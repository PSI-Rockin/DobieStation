#include <algorithm>
#include <cstdio>
#include "iop_timers.hpp"
#include "iop_intc.hpp"
#include "../emulator.hpp"
#include "../errors.hpp"
#include "../scheduler.hpp"

IOPTiming::IOPTiming(IOP_INTC* intc, Scheduler* scheduler) : intc(intc), scheduler(scheduler)
{

}

void IOPTiming::reset()
{
    for (int i = 0; i < 6; i++)
    {
        timers[i].counter = 0;
        timers[i].control.gate_mode = 0;
        timers[i].control.use_gate = 0;
        timers[i].control.zero_return = false;
        timers[i].control.compare_interrupt_enabled = false;
        timers[i].control.overflow_interrupt_enabled = false;
        timers[i].control.repeat_int = false;
        timers[i].control.toggle_int = false;
        timers[i].control.int_enable = false;
        timers[i].control.extern_signal = false;
        timers[i].control.compare_interrupt = false;
        timers[i].control.overflow_interrupt = false;
        timers[i].target = 0;
    }

    timer_interrupt_event_id =
            scheduler->register_timer_callback(
                [this] (uint64_t index, bool overflow) { IRQ_test(index, overflow); });

    for (int i = 0; i < 3; i++)
        events[i] = scheduler->create_timer(timer_interrupt_event_id, 0xFFFF, i);

    for (int i = 3; i < 6; i++)
        events[i] = scheduler->create_timer(timer_interrupt_event_id, 0xFFFFFFFF, i);

    for (int i = 0; i < 6; i++)
        scheduler->set_timer_pause(events[i], false);
}

void IOPTiming::IRQ_test(int index, bool overflow)
{
    if (!overflow)
    {
        if (timers[index].control.zero_return)
        {
            timers[index].counter = 0;
            scheduler->set_timer_counter(events[index], 0);
        }
    }

    if (timers[index].control.int_enable)
    {
        const static int IRQs[] = {4, 5, 6, 14, 15, 16};
        intc->assert_irq(IRQs[index]);

        if (overflow)
            timers[index].control.overflow_interrupt = true;
        else
            timers[index].control.compare_interrupt = true;
    }
    else
    {
        if (!timers[index].control.repeat_int)
            return;
    }

    if (timers[index].control.toggle_int)
        timers[index].control.int_enable ^= true;
    else
        timers[index].control.int_enable = false;
}

uint32_t IOPTiming::read_counter(int index)
{
    timers[index].counter = scheduler->get_timer_counter(events[index]);
    //printf("[IOP Timing] Read timer %d counter: $%08lX\n", index, timers[index].counter);
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
    timers[index].counter = value;
    scheduler->set_timer_counter(events[index], value);

    printf("[IOP Timing] Write timer %d counter: $%08X\n", index, value);
}

void IOPTiming::write_control(int index, uint16_t value)
{
    printf("[IOP Timing] Write timer %d control $%04X\n", index, value);

    timers[index].control.use_gate = value & 0x1;
    if (timers[index].control.use_gate)
    {
        Errors::die("IOPTiming timer %d control.use_gate is true", index);
    }
    else
        timers[index].control.started = true;
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

    uint32_t clockrate;

    if (timers[index].control.extern_signal)
    {
        switch (index)
        {
            case 0:
                //Pixel clock - is this correct?
                clockrate = 15 * 1000 * 1000;
                break;
            case 1:
                //HBlank
                clockrate = Scheduler::IOP_CLOCKRATE / 2350;
                break;
            case 3:
                //HBlank
                clockrate = Scheduler::IOP_CLOCKRATE / 2350;
                break;
            default:
                //IOP Clock
                clockrate = Scheduler::IOP_CLOCKRATE;
                break;
        }
    }
    else
        clockrate = Scheduler::IOP_CLOCKRATE;

    switch (timers[index].control.prescale)
    {
        case 0:
            //IOP clock
            break;
        case 1:
            //1/8 IOP clock
            clockrate /= 8;
            break;
        case 2:
            //1/16 IOP clock
            clockrate /= 16;
            break;
        case 3:
            //1/256 IOP clock
            clockrate /= 256;
            break;
    }

    timers[index].counter = 0;

    scheduler->set_timer_clockrate(events[index], clockrate);
    scheduler->set_timer_counter(events[index], 0);
    scheduler->set_timer_pause(events[index], !timers[index].control.started);
    scheduler->set_timer_int_mask(events[index],
                                  timers[index].control.overflow_interrupt_enabled,
                                  timers[index].control.compare_interrupt_enabled);
}

void IOPTiming::write_target(int index, uint32_t value)
{
    printf("[IOP Timing] Write timer %d target $%08X\n", index, value);
    timers[index].target = value;
    scheduler->set_timer_target(events[index], value);

    if (!timers[index].control.toggle_int)
        timers[index].control.int_enable = true;
}
