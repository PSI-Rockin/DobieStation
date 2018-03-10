#include <cstdio>
#include "iop_dma.hpp"
#include "sif.hpp"

enum CHANNELS
{
    MDECin,
    MDECout,
    GPU,
    CDVD,
    SPU,
    PIO,
    OTC,
    SPU2 = 8,
    SIF0,
    SIF1,
    SIO2in,
    SIO2out
};

IOP_DMA::IOP_DMA(SubsystemInterface* sif) : sif(sif)
{

}

void IOP_DMA::reset(uint8_t* RAM)
{
    this->RAM = RAM;
    for (int i = 0; i < 16; i++)
    {
        channels[i].addr = 0;
        channels[i].block_control = 0;
        channels[i].control.busy = false;
        channels[i].control.sync_mode = 0;

        DPCR.enable[i] = false;
        DPCR.priorities[i] = 7;
        DICR.int_enable[i] = false;
        DICR.int_flag[i] = false;
    }
    DICR.master_int_enable = false;
}

void IOP_DMA::run()
{
    for (int i = 0; i < 16; i++)
    {
        if (DPCR.enable[i] && channels[i].control.busy)
        {
            switch (i)
            {
                case SIF1:
                    if (sif->get_SIF1_size())
                    {
                        uint32_t data = sif->read_SIF1();
                        printf("[IOP DMA] SIF1: $%08X\n", data);

                        *(uint32_t*)&RAM[channels[i].addr] = data;
                        channels[i].addr += 4;
                        uint16_t size = channels[i].block_control & 0xFFFF;
                        size--;
                        set_chan_size(i, size);
                        if (!size)
                            transfer_end(i);
                    }
                    break;
            }
        }
    }
}

void IOP_DMA::transfer_end(int index)
{
    printf("[IOP DMA] Chan%d transfer ended\n", index);
    channels[index].control.busy = false;
    DICR.int_flag[index] = true;
}

uint32_t IOP_DMA::get_DPCR()
{
    uint32_t reg = 0;
    for (int i = 0; i < 8; i++)
    {
        reg |= DPCR.priorities[i] << (i << 2);
        reg |= DPCR.enable[i] << ((i << 2) + 3);
    }
    return reg;
}

uint32_t IOP_DMA::get_DPCR2()
{
    uint32_t reg = 0;
    for (int i = 8; i < 16; i++)
    {
        reg |= DPCR.priorities[i] << (i << 2);
        reg |= DPCR.enable[i] << ((i << 2) + 3);
    }
    return reg;
}

void IOP_DMA::set_DPCR(uint32_t value)
{
    printf("[IOP DMA] Set DPCR: $%08X\n", value);
}

void IOP_DMA::set_DPCR2(uint32_t value)
{
    printf("[IOP DMA] Set DPCR2: $%08X\n", value);
    for (int i = 8; i < 16; i++)
    {
        DPCR.priorities[i] = (value >> (i << 2)) & 0x7;
        DPCR.enable[i] = value & (1 << ((i << 2) + 3));
    }
}

void IOP_DMA::set_DICR(uint32_t value)
{
    printf("[IOP DMA] Set DICR: $%08X\n", value);
}

void IOP_DMA::set_DICR2(uint32_t value)
{
    printf("[IOP DMA] Set DICR2: $%08X\n", value);
}

void IOP_DMA::set_chan_addr(int index, uint32_t value)
{
    printf("[IOP DMA] Chan%d addr: $%08X\n", index, value);
    channels[index].addr = value;
}

void IOP_DMA::set_chan_block(int index, uint32_t value)
{
    printf("[IOP DMA] Chan%d block: $%08X\n", index, value);
    channels[index].block_control = value;
}

void IOP_DMA::set_chan_size(int index, uint16_t value)
{
    printf("[IOP DMA] Chan%d size: $%04X\n", index, value);
    channels[index].block_control &= ~0xFFFF;
    channels[index].block_control |= value;
}

void IOP_DMA::set_chan_count(int index, uint16_t value)
{
    printf("[IOP DMA] Chan%d count: $%04X\n", index, value);
    channels[index].block_control &= ~0xFFFF0000;
    channels[index].block_control |= value;
}

void IOP_DMA::set_chan_control(int index, uint32_t value)
{
    printf("[IOP DMA] Chan%d control: $%08X\n", index, value);
    channels[index].control.direction_from = value & 1;
    channels[index].control.unk8 = value & (1 << 8);
    channels[index].control.sync_mode = (value >> 9) & 0x3;
    channels[index].control.busy = value & (1 << 24);
    channels[index].control.unk30 = value & (1 << 30);
}
