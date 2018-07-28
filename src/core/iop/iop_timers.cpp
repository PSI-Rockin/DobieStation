#include <cstdio>
#include "../emulator.hpp"
#include "iop_timers.hpp"
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
        timers[i].clocks = 0;
        write_control(i, 0);
    }
}

void IOPTiming::run()
{
    for (int i = 4; i < 6; i++)
    {
        timers[i].counter++;
        if (timers[i].counter == timers[i].target)
        {
            timers[i].control.compare_interrupt = true;
            if (timers[i].control.compare_interrupt_enabled)
            {
                IRQ_test(i, false);
                if (timers[i].control.zero_return)
                    timers[i].counter = 0;
            }
        }
        if (timers[i].counter > 0xFFFFFFFF)
        {
            timers[i].counter -= 0xFFFFFFFF;
            if (timers[i].control.overflow_interrupt_enabled)
            {
                IRQ_test(i, true);
            }
        }
    }
}

void IOPTiming::IRQ_test(int index, bool overflow)
{
    if (timers[index].control.int_enable)
    {
        switch (index)
        {
            case 4:
                e->iop_request_IRQ(15);
                break;
            case 5:
                e->iop_request_IRQ(16);
                break;
        }
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
    printf("[IOP Timing] Read timer %d counter: $%08X\n", index, timers[index].counter);
    return timers[index].counter;
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
    reg |= timers[index].control.int_enable << 10;
    reg |= timers[index].control.compare_interrupt << 11;
    reg |= timers[index].control.overflow_interrupt << 12;

    timers[index].control.compare_interrupt = false;
    timers[index].control.overflow_interrupt = false;
    printf("[IOP Timing] Read timer %d control: $%04X\n", index, reg);
    return reg;
}

void IOPTiming::write_counter(int index, uint32_t value)
{
    timers[index].counter = value;
    printf("[IOP Timing] Write timer %d counter: $%08X\n", index, value);
}

void IOPTiming::write_control(int index, uint16_t value)
{
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
    timers[index].counter = 0;
}

void IOPTiming::write_target(int index, uint32_t value)
{
    printf("[IOP Timing] Write timer %d target $%08X\n", index, value);
    timers[index].target = value;
    if (!timers[index].control.toggle_int)
        timers[index].control.int_enable = true;
}
