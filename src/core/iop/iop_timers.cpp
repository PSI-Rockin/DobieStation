#include <algorithm>
#include <cstdio>
#include "iop_timers.hpp"
#include "../emulator.hpp"
#include "../errors.hpp"

IOPTiming::IOPTiming(Emulator* e) : e(e)
{

}

void IOPTiming::reset()
{
    cycles_since_IRQ = 0;
    for (int i = 0; i < 6; i++)
    {
        timers[i].counter = 0;
        timers[i].target = 0;
        timers[i].clocks = 0;
        timers[i].clock_scale = 1;
        timers[i].control.gate_mode = 0;
        timers[i].control.use_gate = 0;
        timers[i].last_update = 0;
        timers[i].control.zero_return = false;
        timers[i].control.compare_interrupt_enabled = false;
        timers[i].control.overflow_interrupt_enabled = false;
        timers[i].control.repeat_int = false;
        timers[i].control.toggle_int = false;
        timers[i].control.int_enable = false;
        timers[i].control.extern_signal = false;
        timers[i].control.compare_interrupt = false;
        timers[i].control.overflow_interrupt = false;
    }

    cycle_count = 0;
    next_event = 0xFFFFFFFF;
}

void IOPTiming::update_timers()
{
    for (int i = 0; i < 6; i++)
    {
        uint64_t delta = cycle_count - timers[i].last_update;
        uint64_t counter_delta = delta / timers[i].clock_scale;

        //Keep track of cycles between counter updates
        if (timers[i].clock_scale > 1)
        {
            uint64_t clock_delta = delta & (timers[i].clock_scale - 1);

            timers[i].clocks += clock_delta;
            if (timers[i].clocks > timers[i].clock_scale)
            {
                timers[i].clocks -= timers[i].clock_scale;
                counter_delta++;
            }
        }

        timers[i].counter += counter_delta;

        //Target check
        if ((timers[i].counter - counter_delta) < timers[i].target && timers[i].counter >= timers[i].target)
        {
            timers[i].control.compare_interrupt = true;
            if (timers[i].control.compare_interrupt_enabled)
            {
                IRQ_test(i, false);
                if (timers[i].control.zero_return)
                    timers[i].counter -= timers[i].target;
            }
        }

        //Overflow check
        uint32_t overflow = (i > 2) ? 0xFFFFFFFF : 0xFFFF;
        if (timers[i].counter > overflow)
        {
            while (timers[i].counter > overflow)
                timers[i].counter -= overflow;
            if (timers[i].control.overflow_interrupt_enabled)
            {
                IRQ_test(i, true);
            }
        }

        timers[i].last_update = cycle_count;
    }
}

void IOPTiming::reschedule()
{
    uint64_t next_event_delta = 0x100000;
    for (int i = 0; i < 6; i++)
    {
        uint64_t overflow_mask = (i > 2) ? 0x100000000UL : 0x10000;
        uint64_t overflow_delta = ((overflow_mask - timers[i].counter) * timers[i].clock_scale) - timers[i].clocks;

        uint64_t target_delta = 0xFFFFFFFF;

        //Don't calculate target delta if the counter is higher, as overflow delta will be reached next
        if (timers[i].target > timers[i].counter)
            target_delta = ((timers[i].target - timers[i].counter) * timers[i].clock_scale) - timers[i].clocks;

        next_event_delta = std::min({next_event_delta, overflow_delta, target_delta});
    }

    next_event = cycle_count + next_event_delta;
}

void IOPTiming::run(int cycles)
{
    /*while (cycles--)
    {
        for (int i = 0; i < 6; i++)
        {
            timers[i].clocks++;
            if (timers[i].clocks >= timers[i].clock_scale)
                count_up(i, timers[i].clock_scale);
        }
    }*/
    cycle_count += cycles;
    if (cycle_count >= next_event)
    {
        update_timers();
        reschedule();
    }
}

void IOPTiming::count_up(int index, int cycles_per_count)
{
    timers[index].clocks -= cycles_per_count;
    timers[index].counter++;
    if (timers[index].counter == timers[index].target)
    {
        timers[index].control.compare_interrupt = true;
        if (timers[index].control.compare_interrupt_enabled)
        {
            IRQ_test(index, false);
            if (timers[index].control.zero_return)
                timers[index].counter = 0;
        }
    }

    //Overflow check
    uint32_t overflow = (index > 2) ? 0xFFFFFFFF : 0xFFFF;
    if (timers[index].counter > overflow)
    {
        timers[index].counter -= overflow;
        if (timers[index].control.overflow_interrupt_enabled)
        {
            IRQ_test(index, true);
        }
    }
}

void IOPTiming::IRQ_test(int index, bool overflow)
{
    if (timers[index].control.int_enable)
    {
        const static int IRQs[] = {4, 5, 6, 14, 15, 16};
        e->iop_request_IRQ(IRQs[index]);

        if (overflow)
            timers[index].control.overflow_interrupt = true;
        else
            timers[index].control.compare_interrupt = true;
    }

    if (timers[index].control.toggle_int)
        timers[index].control.int_enable ^= true;
    else
        timers[index].control.int_enable = false;
}

uint32_t IOPTiming::read_counter(int index)
{
    update_timers();
    printf("[IOP Timing] Read timer %d counter: $%08lX\n", index, timers[index].counter);
    return timers[index].counter;
}

uint32_t IOPTiming::read_target(int index)
{
    printf("[IOP Timing] Read timer %d target: $%08X\n", index, timers[index].target);
    return timers[index].target;
}

uint16_t IOPTiming::read_control(int index)
{
    uint16_t reg = 0;
    reg |= timers[index].control.use_gate;
    reg |= timers[index].control.gate_mode << 1;
    reg |= timers[index].control.zero_return << 3;
    reg |= timers[index].control.compare_interrupt_enabled << 4;
    reg |= timers[index].control.overflow_interrupt_enabled << 5;
    reg |= timers[index].control.repeat_int << 6;
    reg |= timers[index].control.toggle_int << 7;
    reg |= timers[index].control.extern_signal << 8;
    reg |= timers[index].control.int_enable << 10;
    reg |= timers[index].control.compare_interrupt << 11;
    reg |= timers[index].control.overflow_interrupt << 12;
    if (index < 4)
        reg |= timers[index].control.prescale << 9;
    else
        reg |= timers[index].control.prescale << 13;

    timers[index].control.compare_interrupt = false;
    timers[index].control.overflow_interrupt = false;
    printf("[IOP Timing] Read timer %d control: $%04X\n", index, reg);
    return reg;
}

void IOPTiming::write_counter(int index, uint32_t value)
{
    update_timers();
    timers[index].counter = value;
    printf("[IOP Timing] Write timer %d counter: $%08X\n", index, value);
    reschedule();
}

void IOPTiming::write_control(int index, uint16_t value)
{
    update_timers();
    printf("[IOP Timing] Write timer %d control $%04X\n", index, value);
    timers[index].control.use_gate = value & 0x1;
    if (timers[index].control.use_gate)
        Errors::die("IOPTiming timer %d control.use_gate is true", index);
    timers[index].control.gate_mode = (value >> 1) & 0x3;
    timers[index].control.zero_return = value & (1 << 3);
    timers[index].control.compare_interrupt_enabled = value & (1 << 4);
    timers[index].control.overflow_interrupt_enabled = value & (1 << 5);
    timers[index].control.repeat_int = value & (1 << 6);
    timers[index].control.toggle_int = value & (1 << 7);
    timers[index].control.int_enable = true;
    timers[index].control.extern_signal = value & (1 << 8);
    if (index < 4)
        timers[index].control.prescale = (value >> 9) & 0x1;
    else
        timers[index].control.prescale = (value >> 13) & 0x3;

    timers[index].counter = 0;

    uint32_t clock_scale;

    if (timers[index].control.extern_signal)
    {
        switch (index)
        {
            case 0:
                //Pixel Clock
                clock_scale = 3; //Actually too slow, it's more 2.73, but it's an easy value to use.
                break;
            case 1:
                //HBlank
                clock_scale = 2350;
                break;
            case 3:
                //HBlank
                clock_scale = 2350;
                break;
            default:
                //IOP Clock
                clock_scale = 1;
                break;
        }
    }
    else
        clock_scale = 1;

    switch (timers[index].control.prescale)
    {
        case 0:
            //IOP clock
            timers[index].clock_scale = clock_scale;
            break;
        case 1:
            //1/8 IOP clock
            timers[index].clock_scale = clock_scale * 8;
            break;
        case 2:
            //1/16 IOP clock
            timers[index].clock_scale = clock_scale * 16;
            break;
        case 3:
            //1/256 IOP clock
            timers[index].clock_scale = clock_scale * 256;
            break;
    }

    if (clock_scale == 1)
        timers[index].clocks = 0;

    reschedule();
}

void IOPTiming::write_target(int index, uint32_t value)
{
    update_timers();
    printf("[IOP Timing] Write timer %d target $%08X\n", index, value);
    timers[index].target = value;
    if (!timers[index].control.toggle_int)
        timers[index].control.int_enable = true;
    reschedule();
}
