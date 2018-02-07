#include <cstdio>
#include <cstdlib>
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
            if (!channels[i].quadword_count)
            {
                if (channels[i].tag_end)
                {
                    channels[i].control &= ~0x100;
                    continue;
                }
                int mode = (channels[i].control >> 2) & 0x3;
                switch (mode)
                {
                    //Normal - suspend transfer
                    case 0:
                        channels[i].control &= ~0x100;
                        break;
                    case 1:
                        handle_source_chain(i);
                        break;
                    default:
                        break;
                }
                continue;
            }


            uint64_t quad[2];
            quad[0] = e->read64(channels[i].address);
            quad[1] = e->read64(channels[i].address + 8);
            channels[i].address += 16;
            channels[i].quadword_count--;

            switch (i)
            {
                case GIF:
                    gs->send_PATH3(quad);
                    break;
            }
        }
    }
}

void DMAC::handle_source_chain(int index)
{
    uint64_t DMAtag = e->read64(channels[index].tag_address);
    printf("\n[DMAC] DMAtag read $%08X: $%08X_%08X", channels[index].tag_address, DMAtag >> 32, DMAtag & 0xFFFFFFFF);

    uint16_t quadword_count = DMAtag & 0xFFFF;
    uint8_t id = (DMAtag >> 28) & 0x7;
    uint32_t addr = (DMAtag >> 32) & 0x7FFFFFFF;
    addr &= ~0x3;
    bool IRQ_after_transfer = DMAtag & (1 << 31);
    channels[index].quadword_count = quadword_count;
    switch (id)
    {
        case 0:
            //refe
            channels[index].address = addr;
            channels[index].tag_end = true;
            break;
        case 1:
            //cnt
            channels[index].address = channels[index].tag_address + 16;
            channels[index].tag_address = channels[index].address + (quadword_count * 16);
            printf("\nNew address: $%08X", channels[index].address);
            printf("\nNew tag addr: $%08X", channels[index].tag_address);
            break;
        case 7:
            //end
            channels[index].address = channels[index].tag_address + 16;
            channels[index].tag_end = true;
            break;
        default:
            printf("\n[DMAC] Unrecognized source chain DMAtag id %d\n", id);
            exit(1);
    }
}

void DMAC::start_DMA(int index)
{
    printf("\n[DMAC] D%d started: $%08X", index, channels[index].control);
    printf("\nAddr: $%08X", channels[index].address);
    printf("\nMode: %d", (channels[index].control >> 2) & 0x3);
    printf("\nASP: %d", (channels[index].control >> 4) & 0x3);
    printf("\nTTE: %d", channels[index].control & (1 << 6));
    channels[index].tag_end = false;
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
            printf("\n[DMAC] GIF M_ADR: $%08X", value);
            channels[GIF].address = value;
            break;
        case 0x1000A020:
            printf("\n[DMAC] GIF QWC: $%08X", value & 0xFFFF);
            channels[GIF].quadword_count = value & 0xFFFF;
            break;
        case 0x1000A030:
            printf("\n[DMAC] GIF T_ADR: $%08X", value);
            channels[GIF].tag_address = value;
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
