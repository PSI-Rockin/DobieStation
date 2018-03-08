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

DMAC::DMAC(EmotionEngine* cpu, Emulator* e, GraphicsInterface* gif) : cpu(cpu), e(e), gif(gif)
{

}

void DMAC::reset()
{
    //Certain homebrew seem to assume that master enable is turned on
    control.master_enable = true;
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
                    transfer_end(i);
                    continue;
                }
                int mode = (channels[i].control >> 2) & 0x3;
                switch (mode)
                {
                    //Normal - suspend transfer
                    case 0:
                        transfer_end(i);
                        break;
                    case 1:
                        handle_source_chain(i);
                        break;
                    case 2:
                        handle_dest_chain(i);
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
                    gif->send_PATH3(quad);
                    break;
                case SIF1:
                    printf("[DMAC] SIF1 transfer from $%08X\n", channels[i].address - 16);
                    break;
            }
        }
    }
}

void DMAC::transfer_end(int index)
{
    printf("[DMAC] Transfer end: %d\n", index);
    channels[index].control &= ~0x100;

    bool old_stat = interrupt_stat.channel_stat[index];
    interrupt_stat.channel_stat[index];

    //Rising edge INT1
    if (!old_stat & interrupt_stat.channel_mask[index])
        cpu->int1();
}

void DMAC::handle_source_chain(int index)
{
    uint64_t DMAtag = e->read64(channels[index].tag_address);
    printf("[DMAC] DMAtag read $%08X: $%08X_%08X\n", channels[index].tag_address, DMAtag >> 32, DMAtag & 0xFFFFFFFF);

    //Change CTRL to have the upper 16 bits equal to bits 16-31 of the most recently read DMAtag
    channels[index].control &= 0xFFFF;
    channels[index].control |= DMAtag & 0xFFFF0000;

    uint16_t quadword_count = DMAtag & 0xFFFF;
    uint8_t id = (DMAtag >> 28) & 0x7;
    uint32_t addr = (DMAtag >> 32) & 0x7FFFFFFF;
    addr &= ~0xF;
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
        case 3:
            //ref
            channels[index].address = addr;
            channels[index].tag_address += 16;
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

void DMAC::handle_dest_chain(int index)
{

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
        case 0x1000C000:
            reg = channels[SIF0].control;
            break;
        case 0x1000C400:
            reg = channels[SIF1].control;
            break;
        case 0x1000C430:
            reg = channels[SIF1].tag_address;
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
            printf("[DMAC] Unrecognized read32 from $%08X\n", address);
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
            channels[GIF].address = value & ~0xF;
            break;
        case 0x1000A020:
            printf("[DMAC] GIF QWC: $%08X\n", value & 0xFFFF);
            channels[GIF].quadword_count = value & 0xFFFF;
            break;
        case 0x1000A030:
            printf("[DMAC] GIF T_ADR: $%08X\n", value);
            channels[GIF].tag_address = value & ~0xF;
            break;
        case 0x1000C000:
            printf("[DMAC] SIF0 CTRL: $%08X\n", value);
            /*channels[SIF0].control = value;
            if (value & 0x100)
                start_DMA(SIF0);*/
            break;
        case 0x1000C020:
            printf("[DMAC] SIF0 QWC: $%08X\n", value);
            channels[SIF0].quadword_count = value & 0xFFFF;
            break;
        case 0x1000C400:
            printf("[DMAC] SIF1 CTRL: $%08X\n", value);
            channels[SIF1].control = value;
            if (value & 0x100)
                start_DMA(SIF1);
            break;
        case 0x1000C420:
            printf("[DMAC] SIF1 QWC: $%08X\n", value);
            channels[SIF1].quadword_count = value & 0xFFFF;
            break;
        case 0x1000C430:
            printf("[DMAC] SIF1 T_ADR: $%08X\n", value);
            channels[SIF1].tag_address = value & ~0xF;
            break;
        case 0x1000E000:
            printf("[DMAC] Write32 D_CTRL: $%08X\n", value);
            control.master_enable = value & 0x1;
            control.cycle_stealing = value & 0x2;
            control.mem_drain_channel = (value >> 2) & 0x3;
            control.stall_source_channel = (value >> 4) & 0x3;
            control.stall_dest_channel = (value >> 6) & 0x3;
            control.release_cycle = (value >> 8) & 0x7;
            break;
        case 0x1000E010:
            printf("[DMAC] Write32 D_STAT: $%08X\n", value);
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
            printf("[DMAC] Unrecognized write32 of $%08X to $%08X\n", value, address);
            break;
    }
}
