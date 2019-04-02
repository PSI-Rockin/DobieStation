#include <algorithm>
#include "errors.hpp"
#include "emulator.hpp"
#include "scheduler.hpp"

Scheduler::Scheduler()
{

}

void Scheduler::reset()
{
    ee_cycles.count = 0;
    bus_cycles.count = 0;
    iop_cycles.count = 0;

    ee_cycles.remainder = 0;
    bus_cycles.remainder = 0;
    iop_cycles.remainder = 0;

    closest_event_time = 0x7FFFFFFFULL << 32ULL;

    events.clear();
}

unsigned int Scheduler::calculate_run_cycles()
{
    if (!events.size())
        Errors::die("[Scheduler] No events registered");
    const static int MAX_CYCLES = 16;
    if (ee_cycles.count + MAX_CYCLES <= closest_event_time)
        run_cycles = MAX_CYCLES;
    else
    {
        int64_t delta = closest_event_time - ee_cycles.count;
        if (delta > 0)
            run_cycles = delta;
        else
            run_cycles = 0;
    }

    return run_cycles;
}

unsigned int Scheduler::get_bus_run_cycles()
{
    unsigned int bus_run_cycles = run_cycles >> 1;
    if (bus_cycles.remainder && (run_cycles & 0x1))
        bus_run_cycles++;

    return bus_run_cycles;
}

unsigned int Scheduler::get_iop_run_cycles()
{
    unsigned int iop_run_cycles = run_cycles >> 3;
    if (iop_cycles.remainder + (run_cycles & 0x7) >= 8)
        iop_run_cycles++;

    return iop_run_cycles;
}

void Scheduler::add_event(SchedulerEvent& event)
{
    closest_event_time = std::min(event.time_to_run, closest_event_time);

    events.push_back(event);
}

void Scheduler::update_cycle_counts()
{
    ee_cycles.count += run_cycles;
    bus_cycles.count += run_cycles >> 1;
    iop_cycles.count += run_cycles >> 3;

    bus_cycles.remainder += run_cycles & 0x1;
    if (bus_cycles.remainder > 1)
    {
        bus_cycles.count++;
        bus_cycles.remainder = 0;
    }

    iop_cycles.remainder += run_cycles & 0x7;
    if (iop_cycles.remainder >= 8)
    {
        iop_cycles.count++;
        iop_cycles.remainder -= 8;
    }
}

void Scheduler::process_events(Emulator* e)
{
    if (ee_cycles.count >= closest_event_time)
    {
        int64_t new_time = 0x7FFFFFFFULL << 32ULL;
        for (auto it = events.begin(); it != events.end(); )
        {
            if (it->time_to_run <= closest_event_time)
            {
                (e->*it->func)();
                it = events.erase(it);
            }
            else
            {
                new_time = std::min(it->time_to_run, new_time);
                it++;
            }
        }
        closest_event_time = new_time;
    }
}
