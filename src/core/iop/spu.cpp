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

    spdif_irq = 0;

    for (int i = 0; i < 24; i++)
    {
        voices[i].counter = 0;
        voices[i].current_addr = 0;
        voices[i].start_addr = 0;
        voices[i].loop_addr = 0;
        voices[i].pitch = 0;
        voices[i].block_pos = 0;
    }

    IRQA = 0x800;

    ENDX = 0;
}

void SPU::spu_irq()
{
    if (spdif_irq & (4 << id))
        return;

    printf("[SPU%d] IRQA interrupt!\n", id);
    spdif_irq |= 4 << id;
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

                if (loop_start)
                    voices[i].loop_addr = voices[i].start_addr;
            }
            if (voices[i].current_addr == IRQA && core_att & (1 << 6))
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
    current_addr++;
    return RAM[current_addr - 1];
}

void SPU::write_mem(uint16_t value)
{
    printf("[SPU%d] Write mem $%04X ($%08X)\n", id, value, current_addr);
    RAM[current_addr] = value;
    current_addr++;
}

uint16_t SPU::read16(uint32_t addr)
{
    uint16_t reg = 0;
    addr &= 0x7FF;
    if (addr == 0x7C2)
    {
        printf("[SPU] Read SPDIF_IRQ: $%04X\n", spdif_irq);
        return spdif_irq;
    }
    addr &= 0x3FF;
    if (addr >= 0x1C0 && addr < 0x1C0 + (0xC * 24))
    {
        int v = (addr - 0x1C0) / 0xC;
        int reg = (addr - 0x1C0) % 0xC;
        switch (reg)
        {
            case 0:
                return (voices[v].start_addr >> 16) & 0x3F;
            case 2:
                return voices[v].start_addr & 0xFFFF;
            case 4:
                return (voices[v].loop_addr >> 16) & 0x3F;
            case 6:
                return voices[v].loop_addr & 0xFFFF;
            case 8:
                return (voices[v].current_addr >> 16) & 0x3F;
            case 10:
                return voices[v].current_addr & 0xFFFF;
        }
    }
    switch (addr)
    {
        case 0x19A:
            printf("[SPU%d] Core Att\n", id);
            return core_att;
        case 0x1AA:
            return transfer_addr & 0xFFFF;
        case 0x1A8:
            return transfer_addr >> 16;
        case 0x1B0:
            printf("[SPU%d] ADMA stat: $%04X\n", id, autodma_ctrl);
            return autodma_ctrl;
        case 0x340:
            reg = ENDX >> 16;
            printf("[SPU%d] ENDXL: $%04X\n", id, reg);
            break;
        case 0x342:
            reg = ENDX & 0xFFFF;
            printf("[SPU%d] ENDXH: $%04X\n", id, reg);
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

void SPU::write16(uint32_t addr, uint16_t value)
{
    addr &= 0x7FF;
    if (addr == 0x7C2)
    {
        printf("[SPU] Write SPDIF_IRQ: $%04X\n", value);
        spdif_irq = value;
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
                printf("[SPU%d] V%d SSA: $%08X (H: $%04X)\n", id, v, voices[v].start_addr, value);
                break;
            case 2: //SSAL
                voices[v].start_addr &= ~0xFFFF;
                voices[v].start_addr |= value;
                printf("[SPU%d] V%d SSA: $%08X (L: $%04X)\n", id, v, voices[v].start_addr, value);
                break;
            case 4: //LSAXH
                voices[v].loop_addr &= 0xFFFF;
                voices[v].loop_addr |= (value & 0x3F) << 16;
                printf("[SPU%d] V%d LSAX: $%08X (H: $%04X)\n", id, v, voices[v].loop_addr, value);
                break;
            case 6: //LSAXL
                voices[v].loop_addr &= ~0xFFFF;
                voices[v].loop_addr |= value;
                printf("[SPU%d] V%d LSAX: $%08X (L: $%04X)\n", id, v, voices[v].loop_addr, value);
                break;
            default:
                printf("[SPU%d] Write voice %d: $%04X ($%08X)\n", id, (addr - 0x1C0) / 0xC, value, addr);
        }
        return;
    }
    switch (addr)
    {
        case 0x19A:
            printf("[SPU%d] Write Core Att: $%04X\n", id, value);
            if (core_att & (1 << 6))
            {
                if (!(value & (1 << 6)))
                    spdif_irq &= ~(4 << id);
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
            printf("[SPU%d] KON0: $%04X\n", id, value);
            for (int i = 0; i < 16; i++)
            {
                if (value & (1 << i))
                    key_on_voice(i);
            }
            break;
        case 0x1A2:
            printf("[SPU%d] KON1: $%04X\n", id, value);
            for (int i = 0; i < 8; i++)
            {
                if (value & (1 << i))
                    key_on_voice(i + 16);
            }
            break;
        case 0x1A4:
            printf("[SPU%d] KOFF0: $%04X\n", id, value);
            for (int i = 0; i < 16; i++)
            {
                if (value & (1 << i))
                    key_off_voice(i);
            }
            break;
        case 0x1A6:
            printf("[SPU%d] KOFF1: $%04X\n", id, value);
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
            printf("[SPU%d] Transfer addr: $%08X (H: $%04X)\n", id, transfer_addr, value);
            break;
        case 0x1AA:
            transfer_addr &= ~0xFFFF;
            transfer_addr |= value;
            current_addr = transfer_addr;
            printf("[SPU%d] Transfer addr: $%08X (L: $%04X)\n", id, transfer_addr, value);
            break;
        case 0x1AC:
            RAM[current_addr] = value;
            current_addr++;
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
            printf("[SPU%d] V%d VOLL: $%04X\n", id, v, value);
            break;
        case 2:
            printf("[SPU%d] V%d VOLR: $%04X\n", id, v, value);
            break;
        case 4:
            printf("[SPU%d] V%d PITCH: $%04X\n", id, v, value);
            voices[v].pitch = value & 0x3FFF;
            break;
        case 6:
            printf("[SPU%d] V%d ADSR1: $%04X\n", id, v, value);
            break;
        case 8:
            printf("[SPU%d] V%d ADSR2: $%04X\n", id, v, value);
            break;
        default:
            printf("[SPU%d] V%d write $%08X: $%04X\n", id, v, addr, value);
    }
}

void SPU::key_on_voice(int v)
{
    //Copy start addr to loop addr, clear ENDX bit, and set envelope to Attack mode
    voices[v].current_addr = voices[v].start_addr;
    voices[v].loop_addr = voices[v].start_addr;
    ENDX &= ~(1 << v);
}

void SPU::key_off_voice(int v)
{
    //Set envelope to Release mode
}
