#include <cstdio>
#include <cstdlib>
#include "intc.hpp"
#include "timers.hpp"
#include "../errors.hpp"

EmotionTiming::EmotionTiming(INTC* intc) : intc(intc)
{

}

void EmotionTiming::reset()
{
    for (int i = 0; i < 4; i++)
    {
        timers[i].counter = 0;
        timers[i].clocks = 0;
        write_control(i, 0);
    }
}

void EmotionTiming::run()
{
    /*for (int i = 0; i < 4; i++)
    {
        if (timers[i].control.enabled)
        {
            timers[i].clocks++;
            switch (timers[i].control.mode)
            {
                case 0:
                    //Bus clock
                    if (timers[i].clocks >= 2)
                        count_up(i, 2);
                    break;
                case 1:
                    //1/16 bus clock
                    if (timers[i].clocks >= 32)
                        count_up(i, 32);
                    break;
                case 2:
                    //1/256 bus clock
                    if (timers[i].clocks >= 512)
                        count_up(i, 512);
                    break;
                case 3:
                    //TODO: actual value for HSYNC
                    if (timers[i].clocks >= 15000)
                    {
                        count_up(i, 15000);
                    }
                    break;
            }
        }
    }*/
}

void EmotionTiming::run(int cycles)
{
    for (int i = 0; i < 4; i++)
    {
        if (timers[i].control.enabled)
        {
            timers[i].clocks += cycles;
            switch (timers[i].control.mode)
            {
                case 0:
                    //Bus clock
                    while (timers[i].clocks)
                        count_up(i, 1);
                    break;
                case 1:
                    //1/16 bus clock
                    while (timers[i].clocks >= 16)
                        count_up(i, 16);
                    break;
                case 2:
                    //1/256 bus clock
                    while (timers[i].clocks >= 256)
                        count_up(i, 256);
                    break;
                case 3:
                    //TODO: actual value for HSYNC
                    while (timers[i].clocks >= 9400)
                    {
                        count_up(i, 9400);
                    }
                    break;
            }
        }
    }
}

uint32_t EmotionTiming::read32(uint32_t addr)
{
    switch (addr)
    {
        case 0x10000000:
            return timers[0].counter;
        case 0x10000010:
            return read_control(0);
        case 0x10000800:
            return timers[1].counter;
        case 0x10000810:
            return read_control(1);
        case 0x10001000:
            return timers[2].counter;
        case 0x10001010:
            return read_control(2);
        case 0x10001800:
            return timers[3].counter;
        case 0x10001810:
            return read_control(3);
        default:
            printf("[EE Timing] Unrecognized read32 from $%08X\n", addr);
            return 0;
    }
}

void EmotionTiming::write32(uint32_t addr, uint32_t value)
{
    int id = (addr >> 11) & 0x3;
    switch (addr & 0xFF)
    {
        case 0x00:
            printf("[EE Timing] Write32 timer %d counter: $%08X\n", id, value);
            timers[id].counter = value & 0xFFFF;
            break;
        case 0x10:
            write_control(id, value);
            break;
        case 0x20:
            printf("[EE Timing] Write32 timer %d compare: $%08X\n", id, value);
            timers[id].compare = value & 0xFFFF;
            break;
        default:
            printf("[EE Timing] Unrecognized write32 to $%08X of $%08X\n", addr, value);
            break;
    }
}

void EmotionTiming::count_up(int index, int cycles_per_count)
{
    timers[index].clocks -= cycles_per_count;
    timers[index].counter++;

    //Compare check
    if (timers[index].counter == timers[index].compare)
    {
        if (timers[index].control.compare_int_enable)
        {
            if (timers[index].control.clear_on_reference)
                timers[index].counter = 0;
            timers[index].control.compare_int = true;
            intc->assert_IRQ((int)Interrupt::TIMER0 + index);
        }
    }

    //Overflow check
    if (timers[index].counter > 0xFFFF)
    {
        timers[index].counter = 0;
        if (timers[index].control.overflow_int_enable)
        {
            timers[index].control.overflow_int = true;
            intc->assert_IRQ((int)Interrupt::TIMER0 + index);
        }
    }
}

uint32_t EmotionTiming::read_control(int index)
{
    uint32_t reg = 0;
    reg |= timers[index].control.mode;
    reg |= timers[index].control.gate_enable << 2;
    reg |= timers[index].control.gate_VBLANK << 3;
    reg |= timers[index].control.gate_mode << 4;
    reg |= timers[index].control.clear_on_reference << 6;
    reg |= timers[index].control.enabled << 7;
    reg |= timers[index].control.compare_int_enable << 8;
    reg |= timers[index].control.overflow_int_enable << 9;
    reg |= timers[index].control.compare_int << 10;
    reg |= timers[index].control.overflow_int << 11;
    return reg;
}

void EmotionTiming::write_control(int index, uint32_t value)
{
    printf("[EE Timing] Write32 timer %d control: $%08X\n", index, value);
    timers[index].control.mode = value & 0x3;
    timers[index].control.gate_enable = value & (1 << 2);
    if (timers[index].control.gate_enable)
        Errors::die("[EE Timing] Timer %d control.gate_enable is true", index);
    timers[index].control.gate_VBLANK = value & (1 << 3);
    timers[index].control.gate_mode = (value >> 4) & 0x3;
    timers[index].control.clear_on_reference = value & (1 << 6);
    timers[index].control.enabled = value & (1 << 7);
    timers[index].control.compare_int_enable = value & (1 << 8);
    timers[index].control.overflow_int_enable = value & (1 << 9);
    timers[index].control.compare_int &= ~(value & (1 << 10));
    timers[index].control.overflow_int &= ~(value & (1 << 11));
}
