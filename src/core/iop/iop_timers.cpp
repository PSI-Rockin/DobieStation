#include <cstdio>
#include "iop_timers.hpp"

IOPTiming::IOPTiming()
{

}

void IOPTiming::reset()
{
    for (int i = 0; i < 6; i++)
        timers[i].counter = 0;
}

void IOPTiming::run()
{
    //timers[5].counter++;
}

uint32_t IOPTiming::read_counter(int index)
{
    return timers[index].counter;
}

void IOPTiming::write_control(int index, uint16_t value)
{
    printf("[IOP Timing] Write timer %d control $%04X\n", index, value);
}
