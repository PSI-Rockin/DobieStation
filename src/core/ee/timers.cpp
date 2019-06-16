#include <algorithm>
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
        timers[i].gated = false;
        timers[i].last_update = 0;
        timers[i].control.enabled = false;
        timers[i].compare = 0;
        timers[i].control.gate_enable = false;
        timers[i].control.gate_VBLANK = false;
    }

    cycle_count = 0;
    next_event = 0xFFFFFFFF;
}

void EmotionTiming::run(int cycles)
{
    cycle_count += cycles;
    if (cycle_count >= next_event)
    {
        update_timers();
        reschedule();
    }
}

void EmotionTiming::update_timers()
{
    for (int i = 0; i < 4; i++)
    {
        if (timers[i].gated || !timers[i].control.enabled)
        {
            timers[i].last_update = cycle_count;
            timers[i].clocks = 0;
            continue;
        }

        uint64_t delta = cycle_count - timers[i].last_update;
        uint64_t counter_delta = delta / timers[i].clock_scale;

        //Keep track of cycles between counter updates
        if (timers[i].clock_scale > 1)
        {
            uint64_t clock_delta = delta % timers[i].clock_scale;

            timers[i].clocks += clock_delta;
            if (timers[i].clocks >= timers[i].clock_scale)
            {
                timers[i].clocks -= timers[i].clock_scale;
                counter_delta++;
            }
        }

        timers[i].counter += counter_delta;

        //Target check
        if ((timers[i].counter - counter_delta) < timers[i].compare && timers[i].counter >= timers[i].compare)
        {
            if (timers[i].control.compare_int_enable)
            {
                if (timers[i].control.clear_on_reference)
                {
                    timers[i].counter -= timers[i].compare;
                    timers[i].compare &= 0xFFFF;
                }
                else
                    timers[i].compare |= 0x10000;

                timers[i].control.compare_int = true;
                intc->assert_IRQ((int)Interrupt::TIMER0 + i);
            }
        }

        //Overflow check
        if (timers[i].counter >= 0x10000)
        {
            while (timers[i].counter >= 0x10000)
                timers[i].counter -= 0x10000;

            timers[i].compare &= 0xFFFF;

            if (timers[i].control.overflow_int_enable)
            {
                timers[i].control.overflow_int = true;
                intc->assert_IRQ((int)Interrupt::TIMER0 + i);
            }
        }

        timers[i].last_update = cycle_count;
    }
}

void EmotionTiming::reschedule()
{
    uint64_t next_event_delta = 0xFFFFFFFF;
    for (int i = 0; i < 4; i++)
    {
        if (timers[i].gated || !timers[i].control.enabled)
            continue;
        uint64_t overflow_mask = 0x10000;
        uint64_t overflow_delta = ((overflow_mask - timers[i].counter) * timers[i].clock_scale) - timers[i].clocks;

        uint64_t target_delta = 0x10000;

        //Don't calculate target delta if the counter is higher, as overflow delta will be reached next
        if (timers[i].compare > timers[i].counter)
            target_delta = ((timers[i].compare - timers[i].counter) * timers[i].clock_scale) - timers[i].clocks;

        next_event_delta = std::min({next_event_delta, overflow_delta, target_delta});
    }

    next_event = cycle_count + next_event_delta;
}

void EmotionTiming::gate(bool VSYNC, bool high)
{
    for (int i = 0; i < 4; i++)
    {
        TimerControl* ctrl = &timers[i].control;
        if (ctrl->gate_enable && ctrl->gate_VBLANK == VSYNC)
        {
            update_timers();
            switch (ctrl->gate_mode)
            {
                case 0:
                    //Pause on HIGH
                    timers[i].gated = high;
                    break;
                case 1:
                    //Reset on HIGH
                    if (high)
                    {
                        timers[i].counter = 0;
                        timers[i].compare &= 0xFFFF;
                    }
                    timers[i].gated = false;
                    break;
                case 2:
                    //Reset on LOW
                    if (!high)
                    {
                        timers[i].counter = 0;
                        timers[i].compare &= 0xFFFF;
                    }
                    timers[i].gated = false;
                    break;
                case 3:
                    //Reset on HIGH or LOW
                    timers[i].counter = 0;
                    timers[i].compare &= 0xFFFF;
                    timers[i].gated = false;
                    break;
            }
            reschedule();
        }
    }
}

uint32_t EmotionTiming::read32(uint32_t addr)
{
    switch (addr)
    {
        case 0x10000000:
            update_timers();
            return timers[0].counter & 0xFFFF;
        case 0x10000010:
            return read_control(0);
        case 0x10000020:
            return timers[0].compare & 0xFFFF;
        case 0x10000800:
            update_timers();
            return timers[1].counter & 0xFFFF;
        case 0x10000810:
            return read_control(1);
        case 0x10000820:
            return timers[1].compare & 0xFFFF;
        case 0x10001000:
            update_timers();
            return timers[2].counter & 0xFFFF;
        case 0x10001010:
            return read_control(2);
        case 0x10001020:
            return timers[2].compare & 0xFFFF;
        case 0x10001800:
            update_timers();
            return timers[3].counter & 0xFFFF;
        case 0x10001810:
            return read_control(3);
        case 0x10001820:
            return timers[3].compare & 0xFFFF;
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
            update_timers();
            printf("[EE Timing] Write32 timer %d counter: $%08X\n", id, value);
            timers[id].counter = value & 0xFFFF;
            if (timers[id].counter < (timers[id].compare & 0xFFFF))
                timers[id].compare &= 0xFFFF;
            reschedule();
            break;
        case 0x10:
            update_timers();
            write_control(id, value);
            reschedule();
            break;
        case 0x20:
            printf("[EE Timing] Write32 timer %d compare: $%08X\n", id, value);
            update_timers();
            timers[id].compare = value & 0xFFFF;
            if(timers[id].compare < timers[id].counter)
                timers[id].compare |= 0x10000;
            reschedule();
            break;
        default:
            printf("[EE Timing] Unrecognized write32 to $%08X of $%08X\n", addr, value);
            break;
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

    update_timers();

    timers[index].control.mode = value & 0x3;
    timers[index].control.gate_enable = value & (1 << 2);
    timers[index].control.gate_VBLANK = value & (1 << 3);
    timers[index].control.gate_mode = (value >> 4) & 0x3;
    timers[index].control.clear_on_reference = value & (1 << 6);
    timers[index].control.enabled = value & (1 << 7);
    timers[index].control.compare_int_enable = value & (1 << 8);
    timers[index].control.overflow_int_enable = value & (1 << 9);
    timers[index].control.compare_int &= !(value & (1 << 10));
    timers[index].control.overflow_int &= !(value & (1 << 11));

    if (timers[index].control.gate_enable && timers[index].control.mode != 3 && !timers[index].control.gate_VBLANK)
        Errors::die("[EE Timing] Timer %d HSYNC gate is on", index);

    switch (timers[index].control.mode)
    {
        case 0:
            //Bus clock
            timers[index].clock_scale = 1;
            break;
        case 1:
            //1/16 bus clock
            timers[index].clock_scale = 16;
            break;
        case 2:
            //1/256 bus clock
            timers[index].clock_scale = 256;
            break;
        case 3:
            //TODO: actual value for HSYNC
            timers[index].clock_scale = 9400;
            break;
    }

    if (timers[index].control.gate_enable && (timers[index].control.mode != 3 || timers[index].control.gate_VBLANK))
        timers[index].gated = true;
    else
        timers[index].gated = false;

    if (timers[index].clock_scale == 1)
        timers[index].clocks = 0;
}
