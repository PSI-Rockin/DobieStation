#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include "intc.hpp"
#include "timers.hpp"
#include "../errors.hpp"
#include "../scheduler.hpp"

EmotionTiming::EmotionTiming(INTC* intc, Scheduler* scheduler) : intc(intc), scheduler(scheduler)
{

}

void EmotionTiming::reset()
{
    for (int i = 0; i < 4; i++)
    {
        timers[i].counter = 0;
        timers[i].clocks = 0;
        timers[i].gated = false;
        timers[i].control.enabled = false;
        timers[i].compare = 0;
        timers[i].control.gate_enable = false;
        timers[i].control.gate_VBLANK = false;
    }

    timer_interrupt_event_id =
            scheduler->register_timer_callback(
                [this] (uint64_t index, bool overflow) { timer_interrupt(index, overflow); });

    for (int i = 0; i < 4; i++)
        events[i] = scheduler->create_timer(timer_interrupt_event_id, 0xFFFF, i);
}

void EmotionTiming::timer_interrupt(int index, bool overflow)
{
    if (!overflow)
    {
        if (timers[index].control.clear_on_reference)
        {
            timers[index].counter = 0;
            scheduler->set_timer_counter(events[index], 0);
        }

        timers[index].control.compare_int = true;
        intc->assert_IRQ((int)Interrupt::TIMER0 + index);
    }
    else
    {
        timers[index].control.overflow_int = true;
        intc->assert_IRQ((int)Interrupt::TIMER0 + index);
    }
}

bool EmotionTiming::is_timer_enabled(int index)
{
    return !timers[index].gated && timers[index].control.enabled;
}

void EmotionTiming::gate(bool VSYNC, bool high)
{
    for (int i = 0; i < 4; i++)
    {
        TimerControl* ctrl = &timers[i].control;
        if (ctrl->gate_enable && ctrl->gate_VBLANK == VSYNC)
        {
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
                        scheduler->set_timer_counter(events[i], 0);
                    }
                    timers[i].gated = false;
                    break;
                case 2:
                    //Reset on LOW
                    if (!high)
                    {
                        timers[i].counter = 0;
                        scheduler->set_timer_counter(events[i], 0);
                    }
                    timers[i].gated = false;
                    break;
                case 3:
                    //Reset on HIGH or LOW
                    timers[i].counter = 0;
                    scheduler->set_timer_counter(events[i], 0);
                    timers[i].gated = false;
                    break;
            }
            scheduler->set_timer_pause(events[i], !is_timer_enabled(i));
        }
    }
}

uint32_t EmotionTiming::read32(uint32_t addr)
{
    int index = (addr >> 11) & 0x3;
    int reg = (addr >> 4) & 0x3;
    switch (reg)
    {
        case 0:
            timers[index].counter = scheduler->get_timer_counter(events[index]);
            return timers[index].counter;
        case 1:
            return read_control(index);
        case 2:
            return timers[index].compare & 0xFFFF;
        default:
            printf("[EE Timing] Unrecognized read32 from $%08X\n", addr);
            return 0;
    }
}

void EmotionTiming::write32(uint32_t addr, uint32_t value)
{
    int id = (addr >> 11) & 0x3;
    int reg = (addr >> 4) & 0x3;
    switch (reg)
    {
        case 0:
            printf("[EE Timing] Write32 timer %d counter: $%08X\n", id, value);
            timers[id].counter = value & 0xFFFF;
            scheduler->set_timer_counter(events[id], timers[id].counter);
            break;
        case 1:
            write_control(id, value);
            scheduler->set_timer_pause(events[id], !is_timer_enabled(id));
            scheduler->set_timer_clockrate(events[id], Scheduler::BUS_CLOCKRATE / timers[id].clock_scale);
            scheduler->set_timer_int_mask(events[id],
                                          timers[id].control.overflow_int_enable,
                                          timers[id].control.compare_int_enable);
            break;
        case 2:
            printf("[EE Timing] Write32 timer %d compare: $%08X\n", id, value);
            timers[id].compare = value & 0xFFFF;
            scheduler->set_timer_target(events[id], timers[id].compare);
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
