#include <cstdio>
#include "spu.hpp"
#include "../emulator.hpp"

/**
 * Notes on "AutoDMA", as it seems to not be documented anywhere else
 * ADMA is a way of streaming raw PCM to the SPUs.
 * 0x200 (512) bytes are transferred at a 48000 Hz rate
 * Bits 0 and 1 of the ADMA control register determine if the core is transferring data via AutoDMA.
 * Bit 2 seems to be some sort of finish flag?
 */
uint16_t SPU::spdif_irq = 0;

SPU::SPU(int id, Emulator* e) : id(id), e(e)
{ 

}

void SPU::reset(uint8_t* RAM)
{
    this->RAM = (uint16_t*)RAM;
    status.DMA_busy = false;
    status.DMA_finished = false;
    transfer_addr = 0;
    current_addr = 0;
    core_att = 0;
    autodma_ctrl = 0x0;
    ADMA_left = 0;
    input_pos = 0;
    key_on = 0;
    key_off = 0xFFFFFF;
    spdif_irq = 0;

    for (int i = 0; i < 24; i++)
    {
        voices[i].counter = 0;
        voices[i].current_addr = 0;
        voices[i].start_addr = 0;
        voices[i].loop_addr = 0;
        voices[i].pitch = 0;
        voices[i].block_pos = 0;
        voices[i].loop_addr_specified = false;
    }

    IRQA = 0x800;

    ENDX = 0;
}

void SPU::spu_irq()
{
    if (spdif_irq & (2 << id))
        return;

    printf("[SPU%d] IRQA interrupt!\n", id);
    spdif_irq |= 2 << id;
    e->iop_request_IRQ(9);
}

void SPU::update(int cycles_to_run)
{
    cycles += cycles_to_run;

    //Generate a sample every 768 IOP cycles
    if (cycles >= 768)
    {
        cycles -= 768;
        gen_sample();
    }
}

void SPU::gen_sample()
{
    for (int i = 0; i < 24; i++)
    {
        voices[i].counter += voices[i].pitch;
        if (voices[i].counter >= 0x1000)
        {
            voices[i].counter -= 0x1000;

            //Read header
            if (voices[i].block_pos == 0)
            {
                voices[i].current_addr &= 0xFFFFF;
                uint16_t header = RAM[voices[i].current_addr];
                voices[i].loop_code = (header >> 8) & 0x3;
                bool loop_start = header & (1 << 10);

                if (loop_start && !voices[i].loop_addr_specified)
                    voices[i].loop_addr = voices[i].current_addr;
            }
            if (voices[i].current_addr == IRQA && (core_att & (1 << 6)))
                spu_irq();
            voices[i].block_pos++;
            voices[i].current_addr++;

            //End of block
            if (voices[i].block_pos == 8)
            {
                voices[i].block_pos = 0;
                switch (voices[i].loop_code)
                {
                    //Continue to next block
                    case 0:
                    case 2:
                        break;
                    //Jump to loop addr, set ENDX, and go to Release mode
                    case 1:
                        voices[i].current_addr = voices[i].loop_addr;
                        ENDX |= 1 << i;
                        break;
                    //Jump to loop addr and set ENDX
                    case 3:
                        voices[i].current_addr = voices[i].loop_addr;
                        ENDX |= 1 << i;
                        break;
                }
            }
        }
    }

    if (ADMA_left > 0 && autodma_ctrl & (1 << (id - 1)))
    {
        input_pos++;
        process_ADMA();
        input_pos &= 0x1FF;
    }
}

bool SPU::running_ADMA()
{
    return (autodma_ctrl & (1 << (id - 1)));
}

bool SPU::can_write_ADMA()
{
    return (input_pos <= 128 || input_pos >= 384) && (ADMA_left < 0x400);
}

void SPU::finish_DMA()
{
    status.DMA_finished = true;
    status.DMA_busy = false;
}

void SPU::start_DMA(int size)
{
    if (autodma_ctrl & (1 << (id - 1)))
    {
        printf("ADMA started with size: $%08X\n", size);
    }
    status.DMA_finished = false;
}

void SPU::write_DMA(uint32_t value)
{
    //printf("[SPU%d] Write mem $%08X ($%08X)\n", id, value, current_addr);
    RAM[current_addr] = value & 0xFFFF;
    RAM[current_addr + 1] = value >> 16;

    if ((current_addr == IRQA || (current_addr + 1) == IRQA) && (core_att & (1 << 6)))
        spu_irq();

    current_addr += 2;
    current_addr &= 0x000FFFFF;
    status.DMA_busy = true;
    status.DMA_finished = false;
}

void SPU::write_ADMA(uint8_t *RAM)
{
   // printf("[SPU%d] ADMA transfer: $%08X\n", id, ADMA_left);
    ADMA_left += 2;

    status.DMA_busy = true;
    status.DMA_finished = false;
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
    printf("[SPU%d] Read mem $%04X ($%08X)\n", id, RAM[current_addr], current_addr);
    if (current_addr == IRQA && (core_att & (1 << 6)))
        spu_irq();
    current_addr++;
    return RAM[current_addr - 1];
}

void SPU::write_mem(uint16_t value)
{
    printf("[SPU%d] Write mem $%04X ($%08X)\n", id, value, current_addr);
    RAM[current_addr] = value;
    if (current_addr == IRQA && (core_att & (1 << 6)))
        spu_irq();
    current_addr++;
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
                printf("[SPU%d] Read Voice %d SSAH: $%04X\n", id, v, (voices[v].start_addr >> 16) & 0x3F);
                return (voices[v].start_addr >> 16) & 0x3F;
            case 2:
                printf("[SPU%d] Read Voice %d SSAL: $%04X\n", id, v, voices[v].start_addr & 0xFFFF);
                return voices[v].start_addr & 0xFFFF;
            case 4:
                printf("[SPU%d] Read Voice %d LSAXH: $%04X\n", id, v, (voices[v].loop_addr >> 16) & 0x3F);
                return (voices[v].loop_addr >> 16) & 0x3F;
            case 6:
                printf("[SPU%d] Read Voice %d LSAXL: $%04X\n", id, v, voices[v].loop_addr & 0xFFFF);
                return voices[v].loop_addr & 0xFFFF;
            case 8:
                printf("[SPU%d] Read Voice %d NAXH: $%04X\n", id, v, (voices[v].current_addr >> 16) & 0x3F);
                return (voices[v].current_addr >> 16) & 0x3F;
            case 10:
                printf("[SPU%d] Read Voice %d NAXL: $%04X\n", id, v, voices[v].current_addr & 0xFFFF);
                return voices[v].current_addr & 0xFFFF;
        }
    }
    switch (addr)
    {
        case 0x188:
            printf("[SPU%d] Read VMIXLH: $%04X\n", id, voice_mixdry_left >> 16);
            return voice_mixdry_left >> 16;
        case 0x18A:
            printf("[SPU%d] Read VMIXLL: $%04X\n", id, voice_mixdry_left & 0xFFFF);
            return voice_mixdry_left & 0xFFFF;
        case 0x18C:
            printf("[SPU%d] Read VMIXELH: $%04X\n", id, voice_mixwet_left >> 16);
            return voice_mixwet_left >> 16;
        case 0x18E:
            printf("[SPU%d] Read VMIXELL: $%04X\n", id, voice_mixwet_left & 0xFFFF);
            return voice_mixwet_left & 0xFFFF;
        case 0x190:
            printf("[SPU%d] Read VMIXRH: $%04X\n", id, voice_mixdry_right >> 16);
            return voice_mixdry_right >> 16;
        case 0x192:
            printf("[SPU%d] Read VMIXRL: $%04X\n", id, voice_mixdry_right & 0xFFFF);
            return voice_mixdry_right & 0xFFFF;
        case 0x194:
            printf("[SPU%d] Read VMIXERH: $%04X\n", id, voice_mixwet_right >> 16);
            return voice_mixwet_right >> 16;
        case 0x196:
            printf("[SPU%d] Read VMIXERL: $%04X\n", id, voice_mixwet_right & 0xFFFF);
            return voice_mixwet_right & 0xFFFF;
        case 0x19A:
            printf("[SPU%d] Read Core Att\n", id);
            return core_att;
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
            printf("[SPU%d] Read ENDXH: $%04X\n", id, reg);
            break;
        case 0x342:
            reg = ENDX & 0xFFFF;
            printf("[SPU%d] Read ENDXL: $%04X\n", id, reg);
            break;
        case 0x344:
            reg |= status.DMA_finished << 7;
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
        return voices[v].current_envelope;
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
                voices[v].start_addr |= (value & 0x3F) << 16;
                printf("[SPU%d] Write V%d SSA: $%08X (H: $%04X)\n", id, v, voices[v].start_addr, value);
                break;
            case 2: //SSAL
                voices[v].start_addr &= ~0xFFFF;
                voices[v].start_addr |= value;
                printf("[SPU%d] Write V%d SSA: $%08X (L: $%04X)\n", id, v, voices[v].start_addr, value);
                break;
            case 4: //LSAXH
                voices[v].loop_addr &= 0xFFFF;
                voices[v].loop_addr |= (value & 0x3F) << 16;
                voices[v].loop_addr_specified = true;
                printf("[SPU%d] Write V%d LSAX: $%08X (H: $%04X)\n", id, v, voices[v].loop_addr, value);
                break;
            case 6: //LSAXL
                voices[v].loop_addr &= ~0xFFFF;
                voices[v].loop_addr |= value;
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
            if (core_att & (1 << 6))
            {
                if (!(value & (1 << 6)))
                    spdif_irq &= ~(2 << id);
            }
            core_att = value & 0x7FFF;
            if (value & (1 << 15))
            {
                status.DMA_finished = false;
                status.DMA_busy = false;
            }
            break;
        case 0x19C:
            printf("[SPU%d] Write IRQA_H: $%04X\n", id, value);
            IRQA &= 0xFFFF;
            IRQA |= (value & 0x3F) << 16;
            break;
        case 0x19E:
            printf("[SPU%d] Write IRQA_L: $%04X\n", id, value);
            IRQA &= ~0xFFFF;
            IRQA |= value & 0xFFFF;
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
            transfer_addr |= (value & 0x3F) << 16;
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
            break;
        case 2:
            printf("[SPU%d] Write V%d VOLR: $%04X\n", id, v, value);
            break;
        case 4:
            printf("[SPU%d] Write V%d PITCH: $%04X\n", id, v, value);
            voices[v].pitch = value & 0x3FFF;
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
            voices[v].current_envelope = value;
            break;
        default:
            printf("[SPU%d] V%d write $%08X: $%04X\n", id, v, addr, value);
    }
}

void SPU::key_on_voice(int v)
{
    //Copy start addr to current addr, clear ENDX bit, and set envelope to Attack mode
    voices[v].current_addr = voices[v].start_addr;
    voices[v].block_pos = 0;
    voices[v].loop_addr_specified = false;
    ENDX &= ~(1 << v);
}

void SPU::key_off_voice(int v)
{
    //Set envelope to Release mode
}
