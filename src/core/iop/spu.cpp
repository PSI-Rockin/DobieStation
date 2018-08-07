#include <cstdio>
#include "spu.hpp"
#include "../emulator.hpp"

uint16_t SPU::autodma_ctrl = 0;

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
}

bool SPU::running_ADMA()
{
    return ADMA_left >= 0;
}

void SPU::finish_DMA()
{
    status.DMA_finished = true;
    status.DMA_busy = false;
}

void SPU::start_DMA(int size)
{
    size <<= 1;
    if (autodma_ctrl & (1 << (id - 1)))
    {
        printf("ADMA size: $%08X\n", size);
        ADMA_left = size;
    }
    else
        ADMA_left = 0;
}

void SPU::write_DMA(uint32_t value)
{
    //printf("[SPU%d] Write mem $%08X ($%08X)\n", id, value, current_addr);
    //RAM[current_addr] = value & 0xFFFF;
    //RAM[current_addr + 1] = value >> 16;
    current_addr += 2;
    if (ADMA_left >= 0x100)
    {
        ADMA_left -= 2;
        if (ADMA_left < 0x100)
        {
            autodma_ctrl |= ~3;
        }
    }
    status.DMA_busy = true;
    status.DMA_finished = false;
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
    addr &= 0x3FF;
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
    addr &= 0x3FF;
    if (addr >= 0x1C0 && addr < 0x1C0 + (0xC * 24))
    {
        printf("[SPU%d] Write voice %d: $%04X ($%08X)\n", id, (addr - 0x1C0) / 0xC, value, addr);
        return;
    }
    switch (addr)
    {
        case 0x19A:
            printf("[SPU%d] Write Core Att: $%04X\n", id, value);
            core_att = value & 0x7FFF;
            if (value & (1 << 15))
            {
                status.DMA_finished = false;
                status.DMA_busy = false;
            }
            break;
        case 0x19C:
            printf("[SPU%d] Write IRQA_H: $%04X\n", id, value);
            break;
        case 0x19E:
            printf("[SPU%d] Write IRQA_L: $%04X\n", id, value);
            break;
        case 0x1A8:
            transfer_addr &= 0xFFFF;
            transfer_addr |= (value & 0xF) << 16;
            current_addr = transfer_addr;
            printf("[SPU%d] Transfer addr: $%08X (H: $%04X)\n", id, transfer_addr, value);
            break;
        case 0x1AA:
            transfer_addr &= ~0xFFFF;
            transfer_addr |= value;
            current_addr = transfer_addr;
            printf("[SPU%d] Transfer addr: $%08X (L: $%04X)\n", id, transfer_addr, value);
            break;
        case 0x1B0:
            printf("[SPU%d] Write ADMA: $%04X\n", id, value);
            autodma_ctrl = value;
            break;
        default:
            printf("[SPU%d] Unrecognized write16 to addr $%08X of $%04X\n", id, addr, value);
    }
}
