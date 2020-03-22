#include <algorithm>
#include <cstdio>
#include <fstream>
#include <memory>
#include <ostream>
#include <sstream>
#include "spu.hpp"
#include "iop_dma.hpp"
#include "iop_intc.hpp"
#include "../audio/ps_adpcm.hpp"
#include "../audio/utils.hpp"

/**
 * Notes on "AutoDMA", as it seems to not be documented anywhere else
 * ADMA is a way of streaming raw PCM to the SPUs.
 * 0x200 (512) bytes are transferred at a 48000 Hz rate
 * Bits 0 and 1 of the ADMA control register determine if the core is transferring data via AutoDMA.
 * Bit 2 seems to be some sort of finish flag?
 */
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
    ADMA_left = 0;
    input_pos = 0;
    key_on = 0;
    key_off = 0xFFFFFF;
    spdif_irq = 0;

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
    //ramdump.write(reinterpret_cast<char const *>(RAM), 1024*1024*2);
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

        ADPCM_info info = {};
        ADPCM_decoder stream = {};

        int dataleft = 1;
        std::vector<int16_t> pcm;

        while (dataleft)
        {
            info.block = stream.read_block((uint8_t*)data);

            if ((info.block.flags & 1) == 1)
            {
                dataleft = 0;
            }

            auto newpcm = stream.decode_samples(info);
            pcm.insert(pcm.end(), newpcm.begin(), newpcm.end());

            data += 8;
        }


        out.append_pcm(pcm);
    }
}

void Voice::set_adsr_phase(adsr_phase phase)
{
    switch (phase)
    {
        case adsr_phase::Attack:
            adsr.target = 0x7fff;
            adsr.mode = ((adsr1 >> 15) & 1) ? adsr_rate::Exponential : adsr_rate::Linear;
            adsr.direction = adsr_dir::Inc;
            adsr.shift = (adsr1 >> 10) & 0x0f;
            if (adsr.direction == adsr_dir::Dec)
            {
                adsr.step = -8 + ((adsr2 >> 8) & 0x03);
            }
            else
            {
                adsr.step = 7 - ((adsr2 >> 8) & 0x03);
            }
            adsr.phase = phase;
            break;
        case adsr_phase::Decay:
            adsr.target = ((adsr1 & 0x0F) + 1) * 0x800;
            adsr.mode = adsr_rate::Exponential;
            adsr.direction = adsr_dir::Dec;
            adsr.shift = (adsr1 >> 4) & 0x0f;
            adsr.step = -8;
            adsr.phase = phase;
            break;
        case adsr_phase::Sustain:
            adsr.target = 0;
            adsr.mode = ((adsr2 >> 15) & 1) ? adsr_rate::Exponential : adsr_rate::Linear;
            adsr.direction = ((adsr2 >> 14) & 1) ? adsr_dir::Dec : adsr_dir::Inc;
            adsr.shift = (adsr2 >> 8) & 0x0f;
            if (adsr.direction == adsr_dir::Dec)
            {
                adsr.step = -8 + ((adsr2 >> 6) & 0x03);
            }
            else
            {
                adsr.step = 7 - ((adsr2 >> 6) & 0x03);
            }
            adsr.phase = phase;
            break;
        case adsr_phase::Release:
            adsr.target = 0;
            adsr.mode = ((adsr2 >> 5) & 1) ? adsr_rate::Exponential : adsr_rate::Linear;
            adsr.direction = adsr_dir::Dec;
            adsr.shift = adsr2 & 0x0f;
            adsr.step = -8;
            adsr.phase = phase;
            break;
    }

}


void SPU::advance_envelope(int voice_id)
{
    adsr_params &adsr = voices[voice_id].adsr;

    if (adsr.cycles > 0)
    {
        adsr.cycles--;
        return;
    }

    adsr.cycles = 1 << std::max(0, adsr.shift-11);

    //if (adsr.phase == adsr_phase::Sustain)



    int16_t step = adsr.step << std::max(0, 11-adsr.shift);

    if (adsr.mode == adsr_rate::Exponential && adsr.direction == adsr_dir::Inc && adsr.envelope > 0x6000)
        adsr.cycles *= 4;

    if (adsr.mode == adsr_rate::Exponential && adsr.direction == adsr_dir::Dec)
        step = step*adsr.envelope/0x8000;

    adsr.envelope = std::max(std::min(adsr.envelope+step, 0x7fff), 0);

    //if (adsr.direction == adsr_dir::Dec)
    //{

    //    adsr.envelope = std::max(std::min(adsr.envelope-step, (uint)0x7fff), (uint)0);
    //}
    //else
    //{
    //    adsr.envelope = std::max(std::min(adsr.envelope+step, (uint)0x7fff), (uint)0);
    //}



   // printf("[SPU%d] EDEBUG %d phase %d\n", id, voice_id, adsr.phase);
   // printf("[SPU%d] EDEBUG %d next tick in %d cycles\n", id, voice_id, adsr.cycles);
   //     printf("[SPU%d] EDEBUG %d target %d\n", id, voice_id, adsr.target);
   //     printf("[SPU%d] EDEBUG %d direction %d\n", id, voice_id, adsr.direction);
   //     printf("[SPU%d] EDEBUG %d shift %d\n", id, voice_id, adsr.shift);
   //     printf("[SPU%d] EDEBUG %d stepvalue %d\n", id, voice_id, adsr.step);
   //     printf("[SPU%d] EDEBUG %d step %d\n", id, voice_id, step);
   //     printf("[SPU%d] EDEBUG %d ENVX %d\n", id, voice_id, adsr.envelope);


    if (adsr.phase != adsr_phase::Sustain)
    {
        if ((adsr.direction == adsr_dir::Inc && adsr.envelope >= adsr.target) ||
            (adsr.direction == adsr_dir::Dec && adsr.envelope <= adsr.target))
        {
            switch (adsr.phase)
            {
                case adsr_phase::Attack:
                    voices[voice_id].set_adsr_phase(adsr_phase::Decay);
                    printf("[SPU%d] EDEBUG Setting ADSR phase of voice %d to %d\n", id, voice_id, 1);
                    break;
                case adsr_phase::Decay:
                    printf("[SPU%d] EDEBUG Setting ADSR phase of voice %d to %d\n", id, voice_id, 2);
                    voices[voice_id].set_adsr_phase(adsr_phase::Sustain);
                    break;
            }
        }
    }
}

stereo_sample SPU::voice_gen_sample(int voice_id)
{
    Voice &voice = voices[voice_id];
    if (voice.key_switch_timeout != 0)
    {
        voice.key_switch_timeout--;
    }

    voice.counter += voice.pitch;
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

            voice.last_pcm = voice.current_pcm;
            voice.adpcm.block = dec.read_block((uint8_t*)(RAM+voice.current_addr));
            voice.current_pcm = dec.decode_samples(voice.adpcm);

            spu_check_irq(voice.current_addr);
            voice.current_addr++;
            voice.current_addr &= 0x000FFFFF;
        }

        voice.old3 = voice.old2;
        voice.old2 = voice.old1;
        voice.old1 = voice.next_sample;

        voice.next_sample = voice.current_pcm.at(voice.block_pos-4);

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
                    printf("[SPU%d] EDEBUG Setting ADSR phase of voice %d to %d\n", id, voice_id, 3);
                    voice.adsr.phase = adsr_phase::Release;
                    voice.adsr.envelope = 0;
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
    if (voice_id == 23 && id == 1)
    printf("%d times %d >> 15 == %d\n", output_sample, voice.adsr.envelope, ((output_sample * voice.adsr.envelope) >>15));
    output_sample = (output_sample * voice.adsr.envelope) >> 15;

    stereo_sample out;
    out.left = (output_sample*voice.left_vol) >> 15; // 15
    out.right = (output_sample*voice.right_vol)>> 15;

    advance_envelope(voice_id);

    return out;
}

void SPU::gen_sample()
{

    stereo_sample core_output = {};

    for (int i = 0; i < 24; i++)
    {
        stereo_sample sample = voice_gen_sample(i);

        core_output.left += sample.left;
        core_output.right += sample.right;
    }

    left_out_pcm.push_back(core_output.left);
    right_out_pcm.push_back(core_output.right);

    if (left_out_pcm.size() > 0x100)
    {
        coreout->append_pcm_stereo(left_out_pcm, right_out_pcm);
        left_out_pcm.clear();
        right_out_pcm.clear();
    }

    if (ADMA_left > 0 && autodma_ctrl & (1 << (id - 1)))
    {
        input_pos++;
        process_ADMA();
        input_pos &= 0x1FF;
    }

    if (running_ADMA())
    {
        if (can_write_ADMA())
            set_dma_req();
        else
            clear_dma_req();
    }
}

void SPU::finish_DMA()
{
    status.DMA_ready = true;
    status.DMA_busy = false;
}

void SPU::start_DMA(int size)
{
    if (autodma_ctrl & (1 << (id - 1)))
    {
        printf("ADMA started with size: $%08X\n", size);
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

void SPU::write_ADMA(uint8_t *RAM)
{
   // printf("[SPU%d] ADMA transfer: $%08X\n", id, ADMA_left);
    ADMA_left += 2;

    if (ADMA_left >= 0x400)
        clear_dma_req();

    status.DMA_busy = true;
    status.DMA_ready = false;
}

void SPU::process_ADMA() 
{
    if (input_pos == 128 || input_pos == 384)
    {
        ADMA_left -= std::min(ADMA_left, 0x200);

        if (ADMA_left < 0x200)
        {
            autodma_ctrl |= 0x4;
        }
        else
            autodma_ctrl &= ~0x4;
    }
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
            printf("[SPU%d] Read V%d VOLL: $%04X\n", id, v, voices[v].left_vol);
            return voices[v].left_vol;
        case 2:
            printf("[SPU%d] Read V%d VOLR: $%04X\n", id, v, voices[v].right_vol);
            return voices[v].right_vol;
        case 4:
            printf("[SPU%d] ReadV%d PITCH: $%04X\n", id, v, voices[v].pitch);
            return voices[v].pitch;
        case 6:
            printf("[SPU%d] Read V%d ADSR1: $%04X\n", id, v, voices[v].adsr1);
            return voices[v].adsr1;
        case 8:
            printf("[SPU%d] Read V%d ADSR2: $%04X\n", id, v, voices[v].adsr2);
            return voices[v].adsr2;
        case 10:
            printf("[SPU%d] Read V%d ENVX: $%04X\n", id, v, voices[v].current_envelope);
            //return voices[v].current_envelope;
            return voices[v].adsr.envelope;
        default:
            printf("[SPU%d] V%d Read $%08X\n", id, v, addr);
            return 0;
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
        printf("[SPU] Write high addr $%04X: $%04X\n", addr, value);
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
    switch (addr)
    {
        case 0x188:
            printf("[SPU%d] Write VMIXLH: $%04X\n", id, value);
            voice_mixdry_left &= 0xFFFF;
            voice_mixdry_left |= value << 16;
            break;
        case 0x18A:
            printf("[SPU%d] Write VMIXLL: $%04X\n", id, value);
            voice_mixdry_left &= ~0xFFFF;
            voice_mixdry_left |= value;
            break;
        case 0x18C:
            printf("[SPU%d] Write VMIXELH: $%04X\n", id, value);
            voice_mixwet_left &= 0xFFFF;
            voice_mixwet_left |= value << 16;
            break;
        case 0x18E:
            printf("[SPU%d] Write VMIXELL: $%04X\n", id, value);
            voice_mixwet_left &= ~0xFFFF;
            voice_mixwet_left |= value;
            break;
        case 0x190:
            printf("[SPU%d] Write VMIXRH: $%04X\n", id, value);
            voice_mixdry_right &= 0xFFFF;
            voice_mixdry_right |= value << 16;
            break;
        case 0x192:
            printf("[SPU%d] Write VMIXRL: $%04X\n", id, value);
            voice_mixdry_right &= ~0xFFFF;
            voice_mixdry_right |= value;
            break;
        case 0x194:
            printf("[SPU%d] Write VMIXERH: $%04X\n", id, value);
            voice_mixwet_right &= 0xFFFF;
            voice_mixwet_right |= value << 16;
            break;
        case 0x196:
            printf("[SPU%d] Write VMIXERL: $%04X\n", id, value);
            voice_mixwet_right &= ~0xFFFF;
            voice_mixwet_right |= value;
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
            voices[v].left_vol = value;
            break;
        case 2:
            printf("[SPU%d] Write V%d VOLR: $%04X\n", id, v, value);
            voices[v].right_vol = value;
            break;
        case 4:
            printf("[SPU%d] Write V%d PITCH: $%04X\n", id, v, value);
            voices[v].pitch = value & 0xFFFF;
            if (voices[v].pitch > 0x4000)
                voices[v].pitch = 0x4000;
            break;
        case 6:
            printf("[SPU%d] Write V%d ADSR1: $%04X\n", id, v, value);
            voices[v].adsr1 = value;
            break;
        case 8:
            printf("[SPU%d] Write V%d ADSR2: $%04X\n", id, v, value);
            voices[v].adsr2 = value;
            break;
        case 10:
            printf("[SPU%d] Write V%d ENVX: $%04X\n", id, v, value);
            //voices[v].current_envelope = value;
            voices[v].adsr.envelope = value;
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
    //voices[v].adsr.phase = adsr_phase::Attack;
    voices[v].set_adsr_phase(adsr_phase::Attack);
    voices[v].adsr.cycles = 0;
    voices[v].adsr.envelope = 0;
    printf("[SPU%d] EDEBUG Setting ADSR phase of voice %d to %d\n", id, v, 0);
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
    //voices[v].adsr.phase = adsr_phase::Release;
    voices[v].set_adsr_phase(adsr_phase::Release);
    voices[v].adsr.cycles = 0;
    printf("[SPU%d] EDEBUG Setting ADSR phase of voice %d to %d\n", id, v, 3);
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
