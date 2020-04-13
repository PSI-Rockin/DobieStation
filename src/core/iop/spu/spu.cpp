#include <algorithm>
#include <cstdio>
#include <fstream>
#include <memory>
#include <ostream>
#include <sstream>
#include <cstring>
#include "spu.hpp"
#include "../iop_dma.hpp"
#include "../iop_intc.hpp"
#include "spu_adpcm.hpp"
#include "../../audio/utils.hpp"
#include "../../errors.hpp"

/**
 * Notes on "AutoDMA", as it seems to not be documented anywhere else
 * ADMA is a way of streaming raw PCM to the SPUs.
 * 0x200 (512) bytes are transferred at a 48000 Hz rate
 * Bits 0 and 1 of the ADMA control register determine if the core is transferring data via AutoDMA.
 * Bit 2 seems to be some sort of finish flag?
 */

#define SPU_CORE0_MEMIN 0x2000
#define SPU_CORE1_MEMIN 0x2400
#define SPU_SINL 0x800
#define SPU_SINR 0xA00
#define REVERB_REG_BASE 0x2E4


uint16_t SPU::spdif_irq = 0;
uint16_t SPU::core_att[2];
uint32_t SPU::IRQA[2];
SPU::SPU(int id, IOP_INTC* intc, IOP_DMA* dma) : id(id), intc(intc), dma(dma)
{ 

}

void SPU::reset(uint8_t* RAM)
{
    this->RAM = (uint16_t*)RAM;
    status.DMA_busy = false;
    status.DMA_ready = false;
    transfer_addr = 0;
    current_addr = 0;
    core_att[id-1] = 0;
    autodma_ctrl = 0x0;
    input_pos = 0;
    key_on = 0;
    key_off = 0xFFFFFF;
    spdif_irq = 0;
    ADMA_buf = 0;
    buf_filled = false;
    data_input_volume_l = 0x7FFF;
    data_input_volume_r = 0x7FFF;
    reverb = {};
    effect_enable = 0;
    sin_pos = 0;


    std::ostringstream fname;
    fname << "spu_" << id << "_stream" << ".wav";

    coreout = new WAVWriter(fname.str(), 2);

    clear_dma_req();

    for (int i = 0; i < 24; i++)
    {
        voices[i].reset();
    }

    IRQA[id-1] = 0x800;

    ENDX = 0;
}

void SPU::spu_check_irq(uint32_t address)
{
    for (int j = 0; j < 2; j++)
    {
        if (address == IRQA[j] && (core_att[j] & (1 << 6)))
            spu_irq(j);
    }
}

void SPU::spu_irq(int index)
{
    if (spdif_irq & (4 << index))
        return;

    printf("[SPU%d] IRQA interrupt!\n", index);
    spdif_irq |= 4 << index;
    intc->assert_irq(9);
}

void SPU::dump_voice_data()
{
    //std::ofstream ramdump("spu.ram", std::fstream::out | std::fstream::binary);
    //ramdump.write((char*)RAM, 1024*1024*2);
    //ramdump.close();
    for (int i = 0; i < 24; i++)
    {
        auto &voice = voices[i];
        printf("reading voice %d on spu %d starting at 0x%X\n", i, id, voice.start_addr);
        printf("loop addr is %X\n", voice.loop_addr);
        std::ostringstream fname;
        fname << "spu_" << id << "_voice_" << i << ".wav";

        WAVWriter out(fname.str());
        auto *data = (RAM + voice.start_addr);

        ADPCM_decoder stream = {};

        int dataleft = 1;
        std::vector<int16_t> pcm;

        while (dataleft)
        {
            auto newpcm = stream.decode_block((uint8_t*)data);

            if ((stream.flags & 1) == 1)
            {
                dataleft = 0;
            }

            pcm.insert(pcm.end(), newpcm.begin(), newpcm.end());

            data += 8;
        }


        out.append_pcm(pcm);
    }
}


stereo_sample SPU::voice_gen_sample(int voice_id)
{
    Voice &voice = voices[voice_id];
    if (voice.key_switch_timeout != 0)
    {
        voice.key_switch_timeout--;
    }

    uint32_t step = voice.pitch;
    if ((voice_pitch_mod & (1 < voice_id)) && voice_id != 0)
    {
        Errors::die("GAME USING PITCH MODULATION! Go tell us which game this is\n");
        // Todo: handle the glitchyness described by nocash?
        int factor = voices[voice_id-1].outx;
        factor = factor + 0x8000;
        step = (step * factor) >> 15;
        step = step & 0xFFFF;
    }
    step = std::min(step, 0x3fffu);

    voice.counter += step;
    while (voice.counter >= 0x1000)
    {
        voice.counter -= 0x1000;

        //Read header
        if (voice.block_pos == 0)
        {
            voice.current_addr &= 0x000FFFFF;
            uint16_t header = RAM[voice.current_addr];
            voice.loop_code = (header >> 8) & 0x3;
            bool loop_start = header & (1 << 10);

            if (loop_start && !voice.loop_addr_specified)
                voice.loop_addr = voice.current_addr;

            voice.block_pos = 4;

            voice.pcm = voice.adpcm.decode_block((uint8_t*)(RAM+voice.current_addr));

            spu_check_irq(voice.current_addr);
            voice.current_addr++;
            voice.current_addr &= 0x000FFFFF;
        }

        voice.old3 = voice.old2;
        voice.old2 = voice.old1;
        voice.old1 = voice.next_sample;

        voice.next_sample = voice.pcm[voice.block_pos-4];

        voice.block_pos++;
        if ((voice.block_pos % 4) == 0)
        {
            spu_check_irq(voice.current_addr);
            voice.current_addr++;
            voice.current_addr &= 0x000FFFFF;
        }


        //End of block
        if (voice.block_pos == 32)
        {
            voice.block_pos = 0;
            switch (voice.loop_code)
            {
                //Continue to next block
                case 0:
                case 2:
                    break;
                //No loop specified, set ENDX and mute channel
                case 1:
                    ENDX |= 1 << voice_id;
                    printf("[SPU%d] EDEBUG Setting ADSR phase of voice %d to %d\n", id, voice_id, 4);
                    voice.adsr.set_stage(ADSR::Stage::Release);
                    voice.adsr.volume = 0;
                    break;
                //Jump to loop addr and set ENDX
                case 3:
                    voice.current_addr = voice.loop_addr;
                    ENDX |= 1 << voice_id;
                    break;
            }
        }
    }


    int16_t output_sample = interpolate(voice_id);
    output_sample = (output_sample * voice.adsr.volume) >> 15;
    voice.outx = output_sample;

    unsigned int offset = 0x400;
    if (id == 1)
        offset = 0xC00;

    if (voice_id == 1)
    {
        if (voice.crest_out_pos > 0x1FF)
            voice.crest_out_pos = 0;
        write(offset+voice.crest_out_pos, static_cast<uint16_t>(output_sample));
        voice.crest_out_pos++;
    }
    if (voice_id == 3)
    {
        if (voice.crest_out_pos > 0x1FF)
            voice.crest_out_pos = 0;
        write(offset+0x200+voice.crest_out_pos, static_cast<uint16_t>(output_sample));
        voice.crest_out_pos++;
    }

    stereo_sample out;
    out.left = (output_sample*voice.left_vol.value) >> 15;
    out.right = (output_sample*voice.right_vol.value) >> 15;

    voice.left_vol.advance();
    voice.right_vol.advance();
    voice.adsr.advance();

    return out;
}

void SPU::gen_sample()
{

    stereo_sample voices_dry = {};
    stereo_sample voices_wet = {};
    stereo_sample core_dry = {};
    stereo_sample core_wet = {};
    stereo_sample memin = {};


    for (int i = 0; i < 24; i++)
    {
        stereo_sample sample = voice_gen_sample(i);

        voices_dry.mix(sample, voices[i].mix_state.dry_l, voices[i].mix_state.dry_r);
        voices_wet.mix(sample, voices[i].mix_state.wet_l, voices[i].mix_state.wet_r);
    }


    if (left_out_pcm.size() > 0x100)
    {
        coreout->append_pcm_stereo(left_out_pcm, right_out_pcm);
        left_out_pcm.clear();
        right_out_pcm.clear();
    }

    if (running_ADMA())
    {
        memin = read_memin();

        core_dry.mix(memin, mix_state.memin_dry_l, mix_state.memin_dry_r);
        core_wet.mix(memin, mix_state.memin_wet_l, mix_state.memin_wet_r);

        input_pos++;

        // We need to fill the next buffer
        if (!buf_filled)
        {
            set_dma_req();
            autodma_ctrl |= 0x4;
        }

        // Switch buffer
        if (input_pos == 0x100)
        {
            input_pos &= 0xFF;
            ADMA_buf = 1 - ADMA_buf;
            buf_filled = false;
        }

    }
    if (id == 2)
    {
        stereo_sample sin;
        sin.left  = mulvol(static_cast<int16_t>(read(SPU_SINL + sin_pos)), core_volume_l);
        sin.right = mulvol(static_cast<int16_t>(read(SPU_SINR + sin_pos)), core_volume_r);
        core_dry.mix(sin, mix_state.sin_dry_l, mix_state.sin_dry_r);
        core_wet.mix(sin, mix_state.sin_wet_l, mix_state.sin_wet_r);
    }

    core_dry.mix(voices_dry, mix_state.voice_dry_l, mix_state.voice_dry_r);
    core_wet.mix(voices_wet, mix_state.voice_wet_l, mix_state.voice_wet_r);

    run_reverb(core_wet);

    stereo_sample core_output = {};
    core_output.mix(core_dry, true, true);
    core_output.mix(reverb.Eout, true, true);

    if (id == 1)
    {
        write(SPU_SINL + sin_pos, static_cast<uint16_t>(core_output.left));
        write(SPU_SINR + sin_pos, static_cast<uint16_t>(core_output.right));
    }
    sin_pos++;
    if (sin_pos > 0x1FF)
        sin_pos = 0;

    left_out_pcm.push_back(core_output.left);
    right_out_pcm.push_back(core_output.right);

}

uint32_t SPU::translate_reverb_offset(int offset)
{
    // As far as I understand these offsets are in bytes, since we address by shorts we need to divide
    uint32_t addr = reverb.effect_pos + (offset/2);
    uint32_t size = reverb.effect_area_end - reverb.effect_area_start;
    addr = reverb.effect_area_start + (addr % size);

    if (addr < reverb.effect_area_start || addr > reverb.effect_area_end)
        Errors::die("[SPU%d] RDEBUG reverb addr outside of buffer - VERY BAD\n", id);

    return addr;
}

void SPU::run_reverb(stereo_sample wet)
{
    // Reverb runs at half the rate
    // In reality it alternates producing left and right samples
    auto &r = reverb;

    if (reverb.cycle > 0)
    {
        reverb.cycle--;
        return;
    }

    if (static_cast<int>(reverb.effect_area_end - reverb.effect_area_start) <= 0)
        return;

    int16_t Lin = 0, Rin = 0;
    int16_t Lout = 0, Rout = 0;


#define W(x, value) write(translate_reverb_offset((static_cast<int>(x))), static_cast<uint16_t>((value)))
#define R(x) static_cast<int16_t>(read(translate_reverb_offset((x))))
    //_Input from Mixer (Input volume multiplied with incoming data)_____________
    // Lin = vLIN * LeftInput    ;from any channels that have Reverb enabled
    // Rin = vRIN * RightInput   ;from any channels that have Reverb enabled
    Lin = mulvol(wet.left, r.vLIN);
    Rin = mulvol(wet.right, r.vRIN);

     // ____Same Side Reflection (left-to-left and right-to-right)___________________
    // [mLSAME] = (Lin + [dLSAME]*vWALL - [mLSAME-2])*vIIR + [mLSAME-2]  ;L-to-L
    // [mRSAME] = (Rin + [dRSAME]*vWALL - [mRSAME-2])*vIIR + [mRSAME-2]  ;R-to-R
    int16_t tLSAME = clamp16(Lin + mulvol(R(r.dLSAME), r.vWALL));
    tLSAME = mulvol(clamp16(tLSAME - R(r.mLSAME - 2)), r.vIIR);
    tLSAME = clamp16(tLSAME + R(r.mLSAME - 2));

    if (effect_enable)
        W(r.mLSAME, tLSAME);

    int16_t tRSAME = clamp16(Rin + mulvol(R(r.dRSAME), r.vWALL));
    tRSAME = mulvol(clamp16(tRSAME - R(r.mRSAME - 2)), r.vIIR);
    tRSAME = clamp16(tRSAME + R(r.mRSAME - 2));

    if (effect_enable)
        W(r.mRSAME, tRSAME);

    // ___Different Side Reflection (left-to-right and right-to-left)_______________
    // [mLDIFF] = (Lin + [dRDIFF]*vWALL - [mLDIFF-2])*vIIR + [mLDIFF-2]  ;R-to-L
    // [mRDIFF] = (Rin + [dLDIFF]*vWALL - [mRDIFF-2])*vIIR + [mRDIFF-2]  ;L-to-R
    int16_t tLDIFF = clamp16(Lin + mulvol(R(r.dLDIFF), r.vWALL));
    tLDIFF = mulvol(clamp16(tLDIFF- R(r.mLDIFF - 2)), r.vIIR);
    tLDIFF = clamp16(tLDIFF + R(r.mLDIFF - 2));

    if (effect_enable)
        W(r.mLDIFF, tLDIFF);

    int16_t tRDIFF = clamp16(Rin + mulvol(R(r.dRDIFF), r.vWALL));
    tRDIFF = mulvol(clamp16(tRDIFF- R(r.mRDIFF - 2)), r.vIIR);
    tRDIFF = clamp16(tRDIFF + R(r.mRDIFF - 2));

    if (effect_enable)
        W(r.mRDIFF, tRDIFF);

    // ___Early Echo (Comb Filter, with input from buffer)__________________________
    // Lout=vCOMB1*[mLCOMB1]+vCOMB2*[mLCOMB2]+vCOMB3*[mLCOMB3]+vCOMB4*[mLCOMB4]
    // Rout=vCOMB1*[mRCOMB1]+vCOMB2*[mRCOMB2]+vCOMB3*[mRCOMB3]+vCOMB4*[mRCOMB4]
    Lout = mulvol(r.vCOMB1, R(r.mLCOMB1));
    Lout = clamp16(Lout + mulvol(r.vCOMB2, R(r.mLCOMB2)));
    Lout = clamp16(Lout + mulvol(r.vCOMB3, R(r.mLCOMB3)));
    Lout = clamp16(Lout + mulvol(r.vCOMB4, R(r.mLCOMB4)));

    Rout = mulvol(r.vCOMB1, R(r.mRCOMB1));
    Rout = clamp16(Rout + mulvol(r.vCOMB2, R(r.mRCOMB2)));
    Rout = clamp16(Rout + mulvol(r.vCOMB3, R(r.mRCOMB3)));
    Rout = clamp16(Rout + mulvol(r.vCOMB4, R(r.mRCOMB4)));

    // ___Late Reverb APF1 (All Pass Filter 1, with input from COMB)________________
    // Lout=Lout-vAPF1*[mLAPF1-dAPF1], [mLAPF1]=Lout, Lout=Lout*vAPF1+[mLAPF1-dAPF1]
    // Rout=Rout-vAPF1*[mRAPF1-dAPF1], [mRAPF1]=Rout, Rout=Rout*vAPF1+[mRAPF1-dAPF1]
    Lout = clamp16(Lout - mulvol(r.vAPF1, R(r.mLAPF1-r.dAFP1)));
    if (effect_enable)
        W(r.mLAPF1, Lout);
    Lout = clamp16(mulvol(Lout, r.vAPF1) + R(r.mLAPF1-r.dAFP1));

    Rout = clamp16(Rout - mulvol(r.vAPF1, R(r.mRAPF1-r.dAFP1)));
    if (effect_enable)
        W(r.mRAPF1, Rout);
    Rout = clamp16(mulvol(Rout, r.vAPF1) + R(r.mRAPF1-r.dAFP1));

    // ___Late Reverb APF2 (All Pass Filter 2, with input from APF1)________________
    // Lout=Lout-vAPF2*[mLAPF2-dAPF2], [mLAPF2]=Lout, Lout=Lout*vAPF2+[mLAPF2-dAPF2]
    // Rout=Rout-vAPF2*[mRAPF2-dAPF2], [mRAPF2]=Rout, Rout=Rout*vAPF2+[mRAPF2-dAPF2]
    Lout = clamp16(Lout-mulvol(r.vAPF2, R(r.mLAPF2-r.dAFP2)));
    if (effect_enable)
        W(r.mLAPF2, Lout);
    Lout = clamp16(mulvol(Lout, r.vAPF2)+R(r.mLAPF2-r.dAFP2));

    Rout = clamp16(Rout-mulvol(r.vAPF2, R(r.mRAPF2-r.dAFP2)));
    if (effect_enable)
        W(r.mRAPF2, Rout);
    Rout = clamp16(mulvol(Rout, r.vAPF2)+R(r.mRAPF2-r.dAFP2));
    // ___Output to Mixer (Output volume multiplied with input from APF2)___________
    // LeftOutput  = Lout*vLOUT
    // RightOutput = Rout*vROUT
    //r.Eout.left = Lout;
    //r.Eout.right = Rout;
    r.Eout.left = mulvol(Lout, effect_volume_l);
    r.Eout.right = mulvol(Rout, effect_volume_r);
    // ___Finally, before repeating the above steps_________________________________
    // BufferAddress = MAX(mBASE, (BufferAddress+2) AND 7FFFEh)
    // Wait one 1T, then repeat the above stuff
    r.effect_pos += 1;
    if (r.effect_pos >= (r.effect_area_end-r.effect_area_start+1))
        r.effect_pos = 0;
#undef R
#undef W
#undef MUL

    reverb.cycle = 1;
}

void SPU::finish_DMA()
{
    status.DMA_ready = true;
    status.DMA_busy = false;
}

void SPU::start_DMA(int size)
{
    if (running_ADMA())
    {
        printf("[SPU%d] ADMA started with size: $%08X\n",id, size);
        ADMA_progress = 0;
    }
    status.DMA_ready = false;
}

void SPU::pause_DMA()
{
    status.DMA_busy = false;
    status.DMA_ready = true;
}

uint32_t SPU::read_DMA()
{
    uint32_t value = RAM[current_addr];
    spu_check_irq(current_addr);
    current_addr++;
    current_addr &= 0x000FFFFF;

    value |= ((uint32_t)RAM[current_addr]) << 16;
    spu_check_irq(current_addr);
    current_addr ++;
    current_addr &= 0x000FFFFF;

    status.DMA_busy = true;
    status.DMA_ready = false;
    return value;
}

void SPU::write_DMA(uint32_t value)
{
    //printf("[SPU%d] Write mem $%08X ($%08X)\n", id, value, current_addr);
    RAM[current_addr] = value & 0xFFFF;
    spu_check_irq(current_addr);
    current_addr++;
    current_addr &= 0x000FFFFF;

    RAM[current_addr] = value >> 16;
    spu_check_irq(current_addr);
    current_addr++;
    current_addr &= 0x000FFFFF;

    status.DMA_busy = true;
    status.DMA_ready = false;
}

void SPU::write_ADMA(uint8_t *source_RAM)
{
    int next_buffer = 1 - ADMA_buf;

    //if (ADMA_progress == 0)
    //    printf("[SPU%d] Started recieving ADMA for buffer %d\n", id, next_buffer);

    // Write to whichever buffer we're not currently reading from
    // 2 samples (2 shorts) per write_ADMA

    if (ADMA_progress < 256)
    {
        std::memcpy((RAM+get_memin_addr()+(next_buffer*0x100)+(ADMA_progress)), source_RAM, 4);

    }
    else if (ADMA_progress < 512)
    {
        std::memcpy((RAM+0x200+get_memin_addr()+(next_buffer*0x100)+(ADMA_progress-0x100)), source_RAM, 4);
    }

    ADMA_progress += 2;

    if (ADMA_progress >= 512)
    {
        //printf("[SPU%d] Filled next ADMA buffer\n", id);
        ADMA_progress = 0;
        autodma_ctrl &= ~0x4;
        clear_dma_req();
        buf_filled = true;
    }


    status.DMA_busy = true;
    status.DMA_ready = false;
}

uint16_t SPU::read(uint32_t addr)
{
    spu_check_irq(addr);
    return RAM[addr];
}

void SPU::write(uint32_t addr, uint16_t data)
{
    spu_check_irq(addr);
    RAM[addr] = data;
}

uint16_t SPU::read_mem()
{
    uint16_t return_value = RAM[current_addr];
    printf("[SPU%d] Read mem $%04X ($%08X)\n", id, return_value, current_addr);

    spu_check_irq(current_addr);
    current_addr++;
    current_addr &= 0x000FFFFF;

    return return_value;
}

void SPU::write_mem(uint16_t value)
{
    printf("[SPU%d] Write mem $%04X ($%08X)\n", id, value, current_addr);
    RAM[current_addr] = value;
    
    spu_check_irq(current_addr);
    current_addr++;
    current_addr &= 0x000FFFFF;
}

uint16_t SPU::read16(uint32_t addr)
{
    uint16_t reg = 0;
    addr &= 0x7FF;
    if (addr >= 0x760)
    {
        if (addr == 0x7C2)
        {
            printf("[SPU] Read SPDIF_IRQ: $%04X\n", spdif_irq);
            return spdif_irq;
        }
        printf("[SPU] Read high addr $%04X\n", addr);
        return 0;
    }
   
    addr &= 0x3FF;
    if (addr < 0x180)
    {
        return read_voice_reg(addr);
    }
    if (addr >= 0x1C0 && addr < 0x1C0 + (0xC * 24))
    {
        int v = (addr - 0x1C0) / 0xC;
        int reg = (addr - 0x1C0) % 0xC;
        switch (reg)
        {
            case 0:
                printf("[SPU%d] Read Voice %d SSAH: $%04X\n", id, v, (voices[v].start_addr >> 16) & 0xF);
                return (voices[v].start_addr >> 16) & 0xF;
            case 2:
                printf("[SPU%d] Read Voice %d SSAL: $%04X\n", id, v, voices[v].start_addr & 0xFFFF);
                return voices[v].start_addr & 0xFFFF;
            case 4:
                printf("[SPU%d] Read Voice %d LSAXH: $%04X\n", id, v, (voices[v].loop_addr >> 16) & 0xF);
                return (voices[v].loop_addr >> 16) & 0xF;
            case 6:
                printf("[SPU%d] Read Voice %d LSAXL: $%04X\n", id, v, voices[v].loop_addr & 0xFFFF);
                return voices[v].loop_addr & 0xFFFF;
            case 8:
                printf("[SPU%d] Read Voice %d NAXH: $%04X\n", id, v, (voices[v].current_addr >> 16) & 0xF);
                return (voices[v].current_addr >> 16) & 0xF;
            case 10:
                printf("[SPU%d] Read Voice %d NAXL: $%04X\n", id, v, voices[v].current_addr & 0xFFFF);
                return voices[v].current_addr & 0xFFFF;
        }
    }

    switch (addr)
    {
        case 0x188:
            //printf("[SPU%d] Read VMIXLH: $%04X\n", id, voice_mixdry_left >> 16);
            return voice_mixdry_left >> 16;
        case 0x18A:
            //printf("[SPU%d] Read VMIXLL: $%04X\n", id, voice_mixdry_left & 0xFFFF);
            return voice_mixdry_left & 0xFFFF;
        case 0x18C:
            //printf("[SPU%d] Read VMIXELH: $%04X\n", id, voice_mixwet_left >> 16);
            return voice_mixwet_left >> 16;
        case 0x18E:
            //printf("[SPU%d] Read VMIXELL: $%04X\n", id, voice_mixwet_left & 0xFFFF);
            return voice_mixwet_left & 0xFFFF;
        case 0x190:
            //printf("[SPU%d] Read VMIXRH: $%04X\n", id, voice_mixdry_right >> 16);
            return voice_mixdry_right >> 16;
        case 0x192:
            //printf("[SPU%d] Read VMIXRL: $%04X\n", id, voice_mixdry_right & 0xFFFF);
            return voice_mixdry_right & 0xFFFF;
        case 0x194:
            //printf("[SPU%d] Read VMIXERH: $%04X\n", id, voice_mixwet_right >> 16);
            return voice_mixwet_right >> 16;
        case 0x196:
            //printf("[SPU%d] Read VMIXERL: $%04X\n", id, voice_mixwet_right & 0xFFFF);
            return voice_mixwet_right & 0xFFFF;
        case 0x198:
            printf("[SPU%d] Read MMIX $%04X\n", id, mix_state.reg);
            return mix_state.reg;
        case 0x19A:
            printf("[SPU%d] Read Core Att\n", id);
            return core_att[id-1];
        case 0x1A0:
            printf("[SPU%d] Read KON1: $%04X\n", id, (key_off >> 16));
            return (key_on >> 16);
        case 0x1A2:
            printf("[SPU%d] Read KON0: $%04X\n", id, (key_on & 0xFFFF));
            return (key_on & 0xFFFF);
        case 0x1A4:
            printf("[SPU%d] Read KOFF1: $%04X\n", id, (key_off >> 16));
            return (key_off >> 16);
        case 0x1A6:
            printf("[SPU%d] Read KOFF0: $%04X\n", id, (key_off & 0xFFFF));
            return (key_off & 0xFFFF);
        case 0x1A8:
            printf("[SPU%d] Read TSAH: $%04X\n", id, transfer_addr >> 16);
            return transfer_addr >> 16;
        case 0x1AA:
            printf("[SPU%d] Read TSAL: $%04X\n", id, transfer_addr & 0xFFFF);
            return transfer_addr & 0xFFFF;
        case 0x1AC:            
            return read_mem();
        case 0x1B0:
            printf("[SPU%d] Read ADMA stat: $%04X\n", id, autodma_ctrl);
            return autodma_ctrl;
        case 0x33C:
            printf("[SPU%d] Read EEA: $%04X\n", id, reverb.effect_area_end);
            return reverb.effect_area_end >> 16;
        case 0x340:
            reg = ENDX >> 16;
            //printf("[SPU%d] Read ENDXH: $%04X\n", id, reg);
            break;
        case 0x342:
            reg = ENDX & 0xFFFF;
            //printf("[SPU%d] Read ENDXL: $%04X\n", id, reg);
            break;
        case 0x344:
            reg |= status.DMA_ready << 7;
            reg |= status.DMA_busy << 10;
            printf("[SPU%d] Read status: $%04X\n", id, reg);
            return reg;
        default:
            printf("[SPU%d] Unrecognized read16 from addr $%08X\n", id, addr);
            reg = 0;
    }
    return reg;
}

uint16_t SPU::read_voice_reg(uint32_t addr)
{
    int v = addr / 0x10;
    int reg = addr & 0xF;
    switch (reg)
    {
        case 0:
            printf("[SPU%d] Read V%d VOLL: $%04X\n", id, v, voices[v].left_vol.value);
            return static_cast<uint16_t>(voices[v].left_vol.value);
        case 2:
            printf("[SPU%d] Read V%d VOLR: $%04X\n", id, v, voices[v].right_vol.value);
            return static_cast<uint16_t>(voices[v].right_vol.value);
        case 4:
            printf("[SPU%d] ReadV%d PITCH: $%04X\n", id, v, voices[v].pitch);
            return voices[v].pitch;
        case 6:
            printf("[SPU%d] Read V%d ADSR1: $%04X\n", id, v, voices[v].adsr.adsr1);
            return voices[v].adsr.adsr1;
        case 8:
            printf("[SPU%d] Read V%d ADSR2: $%04X\n", id, v, voices[v].adsr.adsr2);
            return voices[v].adsr.adsr2;
        case 10:
            printf("[SPU%d] Read V%d ENVX: $%04X\n", id, v, voices[v].adsr.volume);
            return static_cast<uint16_t>(voices[v].adsr.volume);
        default:
            printf("[SPU%d] V%d Read $%08X\n", id, v, addr);
            return 0;
    }
}

void SPU::write_reverb_reg32(uint32_t addr, uint16_t value)
{
    // I hate this less than writing a million switch cases

    // changing reverb settings is not allowed while enabled
    if (!effect_enable)
    {
        unsigned int reg = (addr - REVERB_REG_BASE)/2;
        if (reg & 1)
        {
            printf("[SPU%d] Write %04X reverb reg L %04X\n", id, value, reg/2);
            reverb.regs[reg/2] |= value & 0x0000FFFF;
        }
        else
        {
            printf("[SPU%d] Write %04X reverb reg H %04X\n", id, value, reg/2);
            reverb.regs[reg/2] |= static_cast<uint32_t>(value << 16);
        }
    }
}

void SPU::write16(uint32_t addr, uint16_t value)
{
    addr &= 0x7FF;

    if (addr >= 0x760)
    {
        if (addr == 0x7C2)
        {
            printf("[SPU] Write SPDIF_IRQ: $%04X\n", value);
            spdif_irq = value;
            return;
        }

        addr -= (id -1) * 0x28;

        if (addr >= 0x774 && addr <= 0x786)
        {
            // seems these you can set whenever?
            //if (!effect_enable)
            //    return;

            switch (addr)
            {
                case 0x774:
                    reverb.vIIR = static_cast<int16_t>(value);
                    break;
                case 0x776:
                    reverb.vCOMB1 = static_cast<int16_t>(value);
                    break;
                case 0x778:
                    reverb.vCOMB2 = static_cast<int16_t>(value);
                    break;
                case 0x77A:
                    reverb.vCOMB3 = static_cast<int16_t>(value);
                    break;
                case 0x77C:
                    reverb.vCOMB4 = static_cast<int16_t>(value);
                    break;
                case 0x77E:
                    reverb.vWALL = static_cast<int16_t>(value);
                    break;
                case 0x780:
                    reverb.vAPF1 = static_cast<int16_t>(value);
                    break;
                case 0x782:
                    reverb.vAPF2 = static_cast<int16_t>(value);
                    break;
                case 0x784:
                    reverb.vLIN = static_cast<int16_t>(value);
                    break;
                case 0x786:
                    reverb.vRIN = static_cast<int16_t>(value);
                    break;
            }
            return;
        }

        switch (addr)
        {
            case 0x764:
                printf("[SPU%d] Write EVOLL: $%04X\n", id, value);
                effect_volume_l = static_cast<int16_t>(value);
                break;
            case 0x766:
                printf("[SPU%d] Write EVOLR: $%04X\n", id, value);
                effect_volume_r = static_cast<int16_t>(value);
                break;
            case 0x768:
                printf("[SPU%d] Write AVOLL: $%04X\n", id, value);
                core_volume_l = static_cast<int16_t>(value);
                break;
            case 0x76A:
                printf("[SPU%d] Write AVOLR: $%04X\n", id, value);
                core_volume_r = static_cast<int16_t>(value);
                break;
            case 0x76C:
                printf("[SPU%d] Write (ADMA vol) BVOLL: $%04X\n", id, value);
                data_input_volume_l = static_cast<int16_t>(value);
                break;
            case 0x76E:
                printf("[SPU%d] Write (ADMA vol) BVOLR: $%04X\n", id, value);
                data_input_volume_r = static_cast<int16_t>(value);
                break;
            default:
                printf("[SPU] Write high addr $%04X: $%04X\n", addr, value);
                return;
        }

        return;
    }
    addr &= 0x3FF;
    if (addr < 0x180)
    {
        write_voice_reg(addr, value);
        return;
    }
    if (addr >= 0x1C0 && addr < 0x1C0 + (0xC * 24))
    {
        int v = (addr - 0x1C0) / 0xC;
        int reg = (addr - 0x1C0) % 0xC;
        switch (reg)
        {
            case 0: //SSAH
                voices[v].start_addr &= 0xFFFF;
                voices[v].start_addr |= (value & 0xF) << 16;
                printf("[SPU%d] Write V%d SSA: $%08X (H: $%04X)\n", id, v, voices[v].start_addr, value);
                break;
            case 2: //SSAL
                voices[v].start_addr &= ~0xFFFF;
                voices[v].start_addr |= value & 0xFFF8;
                printf("[SPU%d] Write V%d SSA: $%08X (L: $%04X)\n", id, v, voices[v].start_addr, value);
                break;
            case 4: //LSAXH
                voices[v].loop_addr &= 0xFFFF;
                voices[v].loop_addr |= (value & 0xF) << 16;
                voices[v].loop_addr_specified = true;
                printf("[SPU%d] Write V%d LSAX: $%08X (H: $%04X)\n", id, v, voices[v].loop_addr, value);
                break;
            case 6: //LSAXL
                voices[v].loop_addr &= ~0xFFFF;
                voices[v].loop_addr |= value & 0xFFF8;
                voices[v].loop_addr_specified = true;
                printf("[SPU%d] Write V%d LSAX: $%08X (L: $%04X)\n", id, v, voices[v].loop_addr, value);
                break;
            default:
                printf("[SPU%d] Write voice %d: $%04X ($%08X)\n", id, (addr - 0x1C0) / 0xC, value, addr);
        }
        return;
    }
    if (addr >= 0x2E4 && addr <= 0x33A)
    {
        write_reverb_reg32(addr, value);
        return;
    }
    switch (addr)
    {
        case 0x180:
            printf("[SPU%d] Write PMONH: $%04X\n", id, value);
            voice_pitch_mod &= 0xFFFF;
            voice_pitch_mod |= value << 16;
            break;
        case 0x182:
            printf("[SPU%d] Write PMONL: $%04X\n", id, value);
            voice_pitch_mod &= ~0xFFFF;
            voice_pitch_mod |= value;
            break;
        case 0x184:
            printf("[SPU%d] Write NONH: $%04X\n", id, value);
            voice_noise_gen &= 0xFFFF;
            voice_noise_gen |= value << 16;
            break;
        case 0x186:
            printf("[SPU%d] Write NONL: $%04X\n", id, value);
            voice_noise_gen &= ~0xFFFF;
            voice_noise_gen |= value;
            break;
        case 0x188:
            printf("[SPU%d] Write VMIXLH: $%04X\n", id, value);
            voice_mixdry_left &= 0xFFFF;
            voice_mixdry_left |= value << 16;
            for (int i = 0; i < 16; i++)
            {
                voices[i].mix_state.dry_l = value & (1 << i);
            }
            break;
        case 0x18A:
            printf("[SPU%d] Write VMIXLL: $%04X\n", id, value);
            voice_mixdry_left &= ~0xFFFF;
            voice_mixdry_left |= value;
            for (int i = 0; i < 8; i++)
            {
                voices[i+16].mix_state.dry_l = value & (1 << i);
            }
            break;
        case 0x18C:
            printf("[SPU%d] Write VMIXELH: $%04X\n", id, value);
            voice_mixwet_left &= 0xFFFF;
            voice_mixwet_left |= value << 16;
            for (int i = 0; i < 16; i++)
            {
                voices[i].mix_state.wet_l = value & (1 << i);
            }
            break;
        case 0x18E:
            printf("[SPU%d] Write VMIXELL: $%04X\n", id, value);
            voice_mixwet_left &= ~0xFFFF;
            voice_mixwet_left |= value;
            for (int i = 0; i < 8; i++)
            {
                voices[i+16].mix_state.wet_l = value & (1 << i);
            }
            break;
        case 0x190:
            printf("[SPU%d] Write VMIXRH: $%04X\n", id, value);
            voice_mixdry_right &= 0xFFFF;
            voice_mixdry_right |= value << 16;
            for (int i = 0; i < 16; i++)
            {
                voices[i].mix_state.dry_r = value & (1 << i);
            }
            break;
        case 0x192:
            printf("[SPU%d] Write VMIXRL: $%04X\n", id, value);
            voice_mixdry_right &= ~0xFFFF;
            voice_mixdry_right |= value;
            for (int i = 0; i < 8; i++)
            {
                voices[i+16].mix_state.dry_r = value & (1 << i);
            }
            break;
        case 0x194:
            printf("[SPU%d] Write VMIXERH: $%04X\n", id, value);
            voice_mixwet_right &= 0xFFFF;
            voice_mixwet_right |= value << 16;
            for (int i = 0; i < 16; i++)
            {
                voices[i].mix_state.wet_r = value & (1 << i);
            }
            break;
        case 0x196:
            printf("[SPU%d] Write VMIXERL: $%04X\n", id, value);
            voice_mixwet_right &= ~0xFFFF;
            voice_mixwet_right |= value;
            for (int i = 0; i < 8; i++)
            {
                voices[i+16].mix_state.wet_r = value & (1 << i);
            }
            break;
        case 0x198:
            printf("[SPU%d] Write MMIX: $%04X\n", id, value);
            mix_state.read(value);
            break;
        case 0x19A:
            printf("[SPU%d] Write Core Att: $%04X\n", id, value);
            if (core_att[id - 1] & (1 << 6))
            {
                if (!(value & (1 << 6)))
                    spdif_irq &= ~(2 << id);
            }

            if (!running_ADMA())
            {
                if ((value >> 4) & 0x3)
                    set_dma_req();
                else
                    clear_dma_req();
            }
            core_att[id - 1] = value & 0x7FFF;
            if (value & (1 << 15))
            {
                status.DMA_ready = false;
                status.DMA_busy = false;
                clear_dma_req();
            }
            effect_enable = (value & (1 << 7));
            break;
        case 0x19C:
            printf("[SPU%d] Write IRQA_H: $%04X\n", id, value);
            IRQA[id - 1] &= 0xFFFF;
            IRQA[id - 1] |= (value & 0xF) << 16;
            break;
        case 0x19E:
            printf("[SPU%d] Write IRQA_L: $%04X\n", id, value);
            IRQA[id - 1] &= ~0xFFFF;
            IRQA[id - 1] |= value & 0xFFFF;
            break;
        case 0x1A0:
            printf("[SPU%d] Write KON0: $%04X\n", id, value);
            key_on &= ~0xFFFF;
            key_on |= value;
            for (int i = 0; i < 16; i++)
            {
                if (value & (1 << i))
                    key_on_voice(i);
            }
            break;
        case 0x1A2:
            printf("[SPU%d] Write KON1: $%04X\n", id, value);
            key_on &= ~0xFF0000;
            key_on |= (value & 0xFF) << 16;
            for (int i = 0; i < 8; i++)
            {
                if (value & (1 << i))
                    key_on_voice(i + 16);
            }
            break;
        case 0x1A4:
            printf("[SPU%d] Write KOFF0: $%04X\n", id, value);
            key_off &= ~0xFFFF;
            key_off |= value;
            for (int i = 0; i < 16; i++)
            {
                if (value & (1 << i))
                    key_off_voice(i);
            }
            break;
        case 0x1A6:
            printf("[SPU%d] Write KOFF1: $%04X\n", id, value);
            key_off &= ~0xFF0000;
            key_off |= (value & 0xFF) << 16;
            for (int i = 0; i < 8; i++)
            {
                if (value & (1 << i))
                    key_off_voice(i + 16);
            }
            break;
        case 0x1A8:
            transfer_addr &= 0xFFFF;
            transfer_addr |= (value & 0xF) << 16;
            current_addr = transfer_addr;
            printf("[SPU%d] Write Transfer addr: $%08X (H: $%04X)\n", id, transfer_addr, value);
            break;
        case 0x1AA:
            transfer_addr &= ~0xFFFF;
            transfer_addr |= value;
            current_addr = transfer_addr;
            printf("[SPU%d] Write Transfer addr: $%08X (L: $%04X)\n", id, transfer_addr, value);
            break;
        case 0x1AC:
            write_mem(value);
            break;
        case 0x1B0:
            printf("[SPU%d] Write ADMA: $%04X\n", id, value);
            autodma_ctrl = value;
            if (running_ADMA())
                set_dma_req();
            break;
        case 0x2E0:
            if (!effect_enable)
            {
                printf("[SPU%d] Write ESAH: $%04X\n", id, value);
                reverb.effect_pos = 0;
                reverb.effect_area_start &= 0xFFFF;
                reverb.effect_area_start |= (value & 0x3F) << 16;
            }
            break;
        case 0x2E2:
            if (!effect_enable)
            {
                printf("[SPU%d] Write ESAL: $%04X\n", id, value);
                reverb.effect_pos = 0;
                reverb.effect_area_start &= ~0xFFFF;
                reverb.effect_area_start |= value;
            }
            break;
        case 0x33C:
            if (!effect_enable)
            {
                printf("[SPU%d] Write EEA: $%04X\n", id, value);
                reverb.effect_pos = 0;
                reverb.effect_area_end &= 0xFFFF;
                reverb.effect_area_end |= (static_cast<uint32_t>(value & 0x3F) << 16) | 0xFFFF;
            }
            break;
        case 0x340:
            ENDX &= 0xFF0000;
            break;
        case 0x342:
            ENDX &= 0xFFFF;
            break;
        default:
            printf("[SPU%d] Unrecognized write16 to addr $%08X of $%04X\n", id, addr, value);
    }
}

void SPU::write_voice_reg(uint32_t addr, uint16_t value)
{
    int v = addr / 0x10;
    int reg = addr & 0xF;
    switch (reg)
    {
        case 0:
            printf("[SPU%d] Write V%d VOLL: $%04X\n", id, v, value);
            voices[v].left_vol.set(value);
            break;
        case 2:
            printf("[SPU%d] Write V%d VOLR: $%04X\n", id, v, value);
            voices[v].right_vol.set(value);
            break;
        case 4:
            printf("[SPU%d] Write V%d PITCH: $%04X\n", id, v, value);
            voices[v].pitch = value & 0xFFFF;
            if (voices[v].pitch > 0x4000)
                voices[v].pitch = 0x4000;
            break;
        case 6:
            printf("[SPU%d] Write V%d ADSR1: $%04X\n", id, v, value);
            voices[v].adsr.adsr1 = value;
            voices[v].adsr.update();
            break;
        case 8:
            printf("[SPU%d] Write V%d ADSR2: $%04X\n", id, v, value);
            voices[v].adsr.adsr2 = value;
            voices[v].adsr.update();
            break;
        case 10:
            printf("[SPU%d] Write V%d ENVX: $%04X\n", id, v, value);
            voices[v].adsr.volume = static_cast<int16_t>(value);
            break;
        default:
            printf("[SPU%d] V%d write $%08X: $%04X\n", id, v, addr, value);
    }
}

void SPU::key_on_voice(int v)
{
    if (voices[v].key_switch_timeout > 0)
    {
        printf("[SPU%d] Ignored key_on for voice %d, within 2 T of last change\n", id, v);
        return;
    }
    voices[v].key_switch_timeout = 2;

    //Copy start addr to current addr, clear ENDX bit, and set envelope to Attack mode
    voices[v].current_addr = voices[v].start_addr;
    voices[v].block_pos = 0;
    voices[v].loop_addr_specified = false;
    ENDX &= ~(1 << v);
    voices[v].adsr.set_stage(ADSR::Stage::Attack);
    voices[v].adsr.volume = 0;
    printf("[SPU%d] EDEBUG Setting ADSR phase of voice %d to %d\n", id, v, 1);
}

void SPU::key_off_voice(int v)
{
    if (voices[v].key_switch_timeout > 0)
    {
        printf("[SPU%d] ignored key_off for voice %d, within 2 T of last change\n", id, v);
        return;
    }
    voices[v].key_switch_timeout = 2;
    //Set envelope to Release mode
    voices[v].adsr.set_stage(ADSR::Stage::Release);
    printf("[SPU%d] EDEBUG Setting ADSR phase of voice %d to %d\n", id, v, 4);
}

void SPU::clear_dma_req()
{
    if (id == 1)
        dma->clear_DMA_request(IOP_SPU);
    else
        dma->clear_DMA_request(IOP_SPU2);
}

void SPU::set_dma_req()
{
    if (id == 1)
        dma->set_DMA_request(IOP_SPU);
    else
        dma->set_DMA_request(IOP_SPU2);
}

uint32_t SPU::get_memin_addr()
{
    if (id == 1)
        return SPU_CORE0_MEMIN;
    else
        return SPU_CORE1_MEMIN;
}

stereo_sample SPU::read_memin()
{
    stereo_sample sample = {};
    uint32_t pos = get_memin_addr()+(ADMA_buf*0x100)+input_pos;
    sample.left = (static_cast<int16_t>(RAM[pos]) * data_input_volume_l) >> 15;
    sample.right = (static_cast<int16_t>(RAM[pos+0x200]) * data_input_volume_r) >> 15;

    spu_check_irq(pos);
    spu_check_irq(pos+0x200);

    return sample;
}
