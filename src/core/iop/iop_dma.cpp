#include <cstdio>
#include "cdvd.hpp"
#include "iop_dma.hpp"
#include "sio2.hpp"
#include "spu.hpp"

#include "../emulator.hpp"
#include "../sif.hpp"

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
    unk,
    SIF0,
    SIF1,
    SIO2in,
    SIO2out
};

IOP_DMA::IOP_DMA(Emulator* e, CDVD_Drive* cdvd, SubsystemInterface* sif, SIO2* sio2, class SPU* spu, class SPU* spu2) :
    e(e), cdvd(cdvd), sif(sif), sio2(sio2), spu(spu), spu2(spu2)
{

}

const char* IOP_DMA::CHAN(int index)
{
    static const char* borp[] =
    {"MDECin", "MDECout", "GPU", "CDVD", "SPU", "PIO", "OTC", "67", "SPU2", "8", "SIF0", "SIF1", "SIO2in", "SIO2out"};
    return borp[index];
}

void IOP_DMA::reset(uint8_t* RAM)
{
    this->RAM = RAM;
    for (int i = 0; i < 16; i++)
    {
        channels[i].addr = 0;
        channels[i].word_count = 0;
        channels[i].block_size = 0;
        channels[i].control.busy = false;
        channels[i].control.sync_mode = 0;

        DPCR.enable[i] = false;
        DPCR.priorities[i] = 7;
    }
    DICR.STAT[0] = 0;
    DICR.STAT[1] = 0;
    DICR.MASK[0] = 0;
    DICR.MASK[1] = 0;
    DICR.master_int_enable[0] = false;
    DICR.master_int_enable[1] = false;
}

void IOP_DMA::run()
{
    for (int i = 0; i < 16; i++)
    {
        if (DPCR.enable[i] && channels[i].control.busy)
        {
            switch (i)
            {
                case CDVD:
                    process_CDVD();
                    break;
                case SPU:
                    process_SPU();
                    break;
                case SPU2:
                    process_SPU2();
                    break;
                case SIF0:
                    process_SIF0();
                    break;
                case SIF1:
                    process_SIF1();
                    break;
                case SIO2in:
                    process_SIO2in();
                    break;
                case SIO2out:
                    process_SIO2out();
                    break;
            }
        }
    }
}

void IOP_DMA::process_CDVD()
{
    uint32_t count = channels[CDVD].word_count * channels[CDVD].block_size * 4;
    if (cdvd->bytes_left() > 0)
    {
        printf("[IOP DMA] CDVD bytes: $%08X\n", count);
        uint32_t bytes_read = cdvd->read_to_RAM(RAM + channels[CDVD].addr, count);
        if (count <= bytes_read)
        {
            transfer_end(CDVD);
            set_chan_block(CDVD, 0);
            return;
        }
        channels[CDVD].addr += bytes_read;
        channels[CDVD].word_count -= bytes_read / (channels[CDVD].block_size * 4);
    }
}

void IOP_DMA::process_SPU()
{
    if (!channels[SPU].word_count)
    {
        transfer_end(SPU);
        spu->finish_DMA();
    }
    else
    {
        uint32_t value = *(uint32_t*)&RAM[channels[SPU].addr];
        spu->write_DMA(value);
        channels[SPU].word_count--;
        channels[SPU].addr += 4;
    }
}

void IOP_DMA::process_SPU2()
{
    if (!channels[SPU2].word_count)
    {
        transfer_end(SPU2);
        spu2->finish_DMA();
    }
    else
    {
        uint32_t value = *(uint32_t*)&RAM[channels[SPU2].addr];
        spu2->write_DMA(value);
        channels[SPU2].word_count--;
        channels[SPU2].addr += 4;
    }
}

void IOP_DMA::process_SIF0()
{
    if (channels[SIF0].word_count)
    {
        if (sif->get_SIF0_size() < SubsystemInterface::MAX_FIFO_SIZE)
        {
            uint32_t data = *(uint32_t*)&RAM[channels[SIF0].addr];
            sif->write_SIF0(data);

            channels[SIF0].addr += 4;
            channels[SIF0].word_count--;
        }
    }
    else
    {
        if (channels[SIF0].tag_end)
        {
            transfer_end(SIF0);
        }
        //Read tag if there's enough room to transfer the EE's tag
        else if (sif->get_SIF0_size() <= SubsystemInterface::MAX_FIFO_SIZE - 2)
        {
            uint32_t data = *(uint32_t*)&RAM[channels[SIF0].tag_addr];
            uint32_t words = *(uint32_t*)&RAM[channels[SIF0].tag_addr + 4];
            //Transfer EEtag
            sif->write_SIF0(*(uint32_t*)&RAM[channels[SIF0].tag_addr + 8]);
            sif->write_SIF0(*(uint32_t*)&RAM[channels[SIF0].tag_addr + 12]);

            channels[SIF0].addr = data & 0xFFFFFF;
            channels[SIF0].word_count = (words + 3) & 0xFFFFFFFC; //round to nearest 4?

            channels[SIF0].tag_addr += 16;

            printf("[IOP DMA] Read SIF0 DMAtag!\n");
            printf("Data: $%08X\n", data);
            printf("Words: $%08X\n", channels[SIF0].word_count);

            if ((data & (1 << 31)) || (data & (1 << 30)))
                channels[SIF0].tag_end = true;
        }
    }
}

void IOP_DMA::process_SIF1()
{
    if (channels[SIF1].word_count)
    {
        if (sif->get_SIF1_size() > 0)
        {
            uint32_t data = sif->read_SIF1();

            *(uint32_t*)&RAM[channels[SIF1].addr] = data;
            channels[SIF1].addr += 4;
            channels[SIF1].word_count--;
        }
    }
    else
    {
        if (channels[SIF1].tag_end)
        {
            transfer_end(SIF1);
        }
        else if (sif->get_SIF1_size() >= 4)
        {
            //Read IOP DMAtag
            uint32_t data = sif->read_SIF1();
            channels[SIF1].addr = data & 0xFFFFFF;
            channels[SIF1].word_count = sif->read_SIF1();

            //EEtag
            sif->read_SIF1();
            sif->read_SIF1();
            printf("[IOP DMA] Read SIF1 DMAtag!\n");
            printf("Addr: $%08X\n", channels[SIF1].addr);
            printf("Words: $%08X\n", channels[SIF1].word_count);
            if ((data & (1 << 31)) || (data & (1 << 30)))
                channels[SIF1].tag_end = true;
        }
    }
}

void IOP_DMA::process_SIO2in()
{
    for (int i = 0; i < 4; i++)
    {
        sio2->write_serial(RAM[channels[SIO2in].addr + i]);
    }
    channels[SIO2in].addr += 4;
    channels[SIO2in].word_count--;
    if (channels[SIO2in].word_count == 0)
        transfer_end(SIO2in);
}

void IOP_DMA::process_SIO2out()
{
    for (int i = 0; i < 4; i++)
        RAM[channels[SIO2out].addr + i] = sio2->read_serial();
    channels[SIO2out].addr += 4;
    channels[SIO2out].word_count--;
    if (channels[SIO2out].word_count == 0)
        transfer_end(SIO2out);
}

void IOP_DMA::transfer_end(int index)
{
    printf("[IOP DMA] %s transfer ended\n", CHAN(index));
    channels[index].control.busy = false;
    channels[index].tag_end = false;
    bool dicr2 = index > 7;
    if (dicr2)
        index -= 8;
    DICR.STAT[dicr2] |= (1 << index);

    if (DICR.STAT[dicr2] & DICR.MASK[dicr2])
    {
        printf("[IOP DMA] IRQ requested\n");
        e->iop_request_IRQ(3);
    }
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
        int bit = i - 8;
        reg |= DPCR.priorities[i] << (bit << 2);
        reg |= DPCR.enable[i] << ((bit << 2) + 3);
    }
    return reg;
}

uint32_t IOP_DMA::get_DICR()
{
    uint32_t reg = 0;
    reg |= DICR.MASK[0] << 16;
    reg |= DICR.master_int_enable[0] << 23;
    reg |= DICR.STAT[0] << 24;

    bool IRQ;
    if (DICR.master_int_enable[0] && (DICR.MASK[0] & DICR.STAT[0]))
        IRQ = true;
    else
        IRQ = false;
    reg |= IRQ << 31;
    printf("[IOP DMA] Get DICR: $%08X\n", reg);
    return reg;
}

uint32_t IOP_DMA::get_DICR2()
{
    uint32_t reg = 0;
    reg |= DICR.MASK[1] << 16;
    reg |= DICR.master_int_enable[1] << 23;
    reg |= DICR.STAT[1] << 24;

    bool IRQ;
    if (DICR.master_int_enable[1] && (DICR.MASK[1] & DICR.STAT[1]))
        IRQ = true;
    else
        IRQ = false;
    reg |= IRQ << 31;
    printf("[IOP DMA] Get DICR2: $%08X\n", reg);
    return reg;
}

uint32_t IOP_DMA::get_chan_addr(int index)
{
    return channels[index].addr;
}

uint32_t IOP_DMA::get_chan_block(int index)
{
    return channels[index].word_count | (channels[index].block_size << 16);
}

uint32_t IOP_DMA::get_chan_control(int index)
{
    uint32_t reg = 0;
    reg |= channels[index].control.direction_from;
    reg |= channels[index].control.unk8 << 8;
    reg |= channels[index].control.sync_mode << 9;
    reg |= channels[index].control.busy << 24;
    reg |= channels[index].control.unk30 << 30;
    return reg;
}

void IOP_DMA::set_DPCR(uint32_t value)
{
    printf("[IOP DMA] Set DPCR: $%08X\n", value);
    for (int i = 0; i < 8; i++)
    {
        bool old_enable = DPCR.enable[i];
        DPCR.priorities[i] = (value >> (i << 2)) & 0x7;
        DPCR.enable[i] = value & (1 << ((i << 2) + 3));
        if (!old_enable && DPCR.enable[i])
            channels[i].tag_end = false;
    }
}

void IOP_DMA::set_DPCR2(uint32_t value)
{
    printf("[IOP DMA] Set DPCR2: $%08X\n", value);
    for (int i = 8; i < 16; i++)
    {
        int bit = i - 8;
        bool old_enable = DPCR.enable[i];
        DPCR.priorities[i] = (value >> (bit << 2)) & 0x7;
        DPCR.enable[i] = value & (1 << ((bit << 2) + 3));
        if (!old_enable && DPCR.enable[i])
            channels[i].tag_end = false;
    }
}

void IOP_DMA::set_DICR(uint32_t value)
{
    printf("[IOP DMA] Set DICR: $%08X\n", value);
    DICR.MASK[0] = (value >> 16) & 0x7F;
    DICR.master_int_enable[0] = value & (1 << 23);
    DICR.STAT[0] &= ~((value >> 24) & 0x7F);
}

void IOP_DMA::set_DICR2(uint32_t value)
{
    printf("[IOP DMA] Set DICR2: $%08X\n", value);
    DICR.MASK[1] = (value >> 16) & 0x7F;
    DICR.master_int_enable[1] = value & (1 << 23);
    DICR.STAT[1] &= ~((value >> 24) & 0x7F);
}

void IOP_DMA::set_chan_addr(int index, uint32_t value)
{
    printf("[IOP DMA] %s addr: $%08X\n", CHAN(index), value);
    channels[index].addr = value;
}

void IOP_DMA::set_chan_block(int index, uint32_t value)
{
    printf("[IOP DMA] %s block: $%08X\n", CHAN(index), value);
    channels[index].block_size = value & 0xFFFF;
    channels[index].word_count = value >> 16;
}

void IOP_DMA::set_chan_size(int index, uint16_t value)
{
    printf("[IOP DMA] %s size: $%04X\n", CHAN(index), value);
    channels[index].block_size = value;
}

void IOP_DMA::set_chan_count(int index, uint16_t value)
{
    printf("[IOP DMA] %s count: $%04X\n", CHAN(index), value);
    channels[index].word_count = value;
}

void IOP_DMA::set_chan_control(int index, uint32_t value)
{
    printf("[IOP DMA] %s control: $%08X\n", CHAN(index), value);
    channels[index].control.direction_from = value & 1;
    channels[index].control.unk8 = value & (1 << 8);
    channels[index].control.sync_mode = (value >> 9) & 0x3;
    channels[index].control.busy = value & (1 << 24);
    channels[index].control.unk30 = value & (1 << 30);
}

void IOP_DMA::set_chan_tag_addr(int index, uint32_t value)
{
    printf("[IOP DMA] %s tag addr: $%08X\n", CHAN(index), value);
    channels[index].tag_addr = value;
}
