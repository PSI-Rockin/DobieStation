#include <cstdio>
#include "../emulator.hpp"
#include "iop_timers.hpp"

IOPTiming::IOPTiming(Emulator* e) : e(e)
{

}

void IOPTiming::reset()
{
    for (int i = 0; i < 6; i++)
    {
        timers[i].counter = 0;
        timers[i].clocks = 0;
        timers[i].control.gate_mode = 0;
        timers[i].control.compare_interrupt = false;
        timers[i].control.overflow_interrupt = false;
    }
}

void IOPTiming::run()
{
    timers[5].clocks++;
    if (timers[5].clocks >= 2)
    {
        timers[5].clocks -= 2;
        timers[5].counter++;
    }
    if (timers[5].counter == timers[5].target)
    {
        timers[5].control.compare_interrupt = true;
        if (timers[5].control.compare_interrupt_enabled)
            e->iop_request_IRQ(16);
    }
    if (timers[5].counter > 0xFFFFFFFF)
    {
        timers[5].control.overflow_interrupt = true;
        if (timers[5].control.overflow_interrupt_enabled)
            e->iop_request_IRQ(16);
    }
}

uint32_t IOPTiming::read_counter(int index)
{
    return timers[index].counter;
}

uint16_t IOPTiming::read_control(int index)
{
    uint16_t reg = 0;
    reg |= timers[index].control.compare_interrupt << 11;
    reg |= timers[index].control.overflow_interrupt << 12;
    printf("[IOP Timing] Read timer %d control: $%04X\n", index, reg);
    return reg;
}

void IOPTiming::write_counter(int index, uint32_t value)
{
    timers[index].counter = value;
}

void IOPTiming::write_control(int index, uint16_t value)
{
    printf("[IOP Timing] Write timer %d control $%04X\n", index, value);
    timers[index].control.compare_interrupt_enabled = value & (1 << 4);
    timers[index].control.overflow_interrupt_enabled = value & (1 << 5);
}

void IOPTiming::write_target(int index, uint32_t value)
{
    printf("[IOP Timing] Write timer %d target $%08X\n", index, value);
    timers[index].target = value;
}
