#include <cstdio>
#include "dmac.hpp"
#include "emulator.hpp"

enum CHANNELS
{
    VIF0,
    VIF1,
    GIF,
    IPU_FROM,
    IPU_TO,
    SIF0,
    SIF1,
    SIF2,
    SPR_FROM,
    SPR_TO
};

DMAC::DMAC(Emulator* e, GraphicsSynthesizer* gs) : e(e), gs(gs)
{

}

void DMAC::reset()
{
    control.master_enable = false;
    for (int i = 0; i < 10; i++)
    {
        channels[i].control = 0;
        channels[i].active = false;
        interrupt_stat.channel_mask[i] = false;
        interrupt_stat.channel_stat[i] = false;
    }
}

void DMAC::run()
{
    if (!control.master_enable)
        return;
    for (int i = 0; i < 10; i++)
    {
        if (channels[i].control & 0x100)
        {
            if (((channels[i].control >> 2) & 0x3) != 0)
                continue;
            uint64_t quad[2];
            quad[0] = e->read64(channels[i].address);
            quad[1] = e->read64(channels[i].address + 8);
            channels[i].address += 16;
            channels[i].quadword_count--;

            printf("\n[DMAC] GIF: $%08X_%08X_%08X_%08X", quad[1] >> 32, quad[1] & 0xFFFFFFFF, quad[0] >> 32, quad[0] & 0xFFFFFFFF);
            if (!channels[i].quadword_count)
                channels[i].control &= ~0x100;
        }
    }
}

void DMAC::start_DMA(int index)
{
    printf("\n[DMAC] D%d started: $%08X", index, channels[index].control);
    printf("\nAddr: $%08X", channels[index].address);
    printf("\nMode: %d", (channels[index].control >> 2) & 0x3);
    printf("\nASP: %d", (channels[index].control >> 4) & 0x3);
    printf("\nTTE: %d", channels[index].control & (1 << 6));
}

uint32_t DMAC::read32(uint32_t address)
{
    uint32_t reg = 0;
    switch (address)
    {
        case 0x1000A000:
            reg = channels[GIF].control;
            break;
        case 0x1000E000:
            reg |= control.master_enable;
            reg |= control.cycle_stealing << 1;
            reg |= control.mem_drain_channel << 2;
            reg |= control.stall_source_channel << 4;
            reg |= control.stall_dest_channel << 6;
            reg |= control.release_cycle << 8;
            break;
        case 0x1000E010:
            for (int i = 0; i < 10; i++)
            {
                reg |= interrupt_stat.channel_stat[i] << i;
                reg |= interrupt_stat.channel_mask[i] << (i + 15);
            }
            reg |= interrupt_stat.stall_stat << 13;
            reg |= interrupt_stat.mfifo_stat << 14;
            reg |= interrupt_stat.bus_stat << 15;

            reg |= interrupt_stat.stall_mask << 29;
            reg |= interrupt_stat.mfifo_mask << 30;
            break;
        default:
            printf("\n[DMAC] Unrecognized read32 from $%08X", address);
            break;
    }
    return reg;
}

void DMAC::write32(uint32_t address, uint32_t value)
{
    switch (address)
    {
        case 0x1000A000:
            channels[GIF].control = value;
            if (value & 0x100)
                start_DMA(GIF);
            break;
        case 0x1000A010:
            printf("\n[DMAC] GIF addr: $%08X", value);
            channels[GIF].address = value;
            break;
        case 0x1000A020:
            printf("\n[DMAC] GIF QWC: $%08X", value);
            channels[GIF].quadword_count = value;
            break;
        case 0x1000E000:
            printf("\n[DMAC] Write32 D_CTRL: $%08X", value);
            control.master_enable = value & 0x1;
            control.cycle_stealing = value & 0x2;
            control.mem_drain_channel = (value >> 2) & 0x3;
            control.stall_source_channel = (value >> 4) & 0x3;
            control.stall_dest_channel = (value >> 6) & 0x3;
            control.release_cycle = (value >> 8) & 0x7;
            break;
        case 0x1000E010:
            printf("\n[DMAC] Write32 D_STAT: $%08X", value);
            for (int i = 0; i < 10; i++)
            {
                interrupt_stat.channel_stat[i] &= ~(value & (1 << i));

                //Reverse channel int mask
                if (value & (1 << (i + 15)))
                    interrupt_stat.channel_mask[i] ^= 1;
            }

            interrupt_stat.stall_stat &= ~(value & (1 << 13));
            interrupt_stat.mfifo_stat &= ~(value & (1 << 14));
            interrupt_stat.bus_stat &= ~(value & (1 << 15));

            if (value & (1 << 29))
                interrupt_stat.stall_mask ^= 1;
            if (value & (1 << 30))
                interrupt_stat.mfifo_mask ^= 1;

            break;
        default:
            printf("\n[DMAC] Unrecognized write32 of $%08X to $%08X", value, address);
            break;
    }
}
