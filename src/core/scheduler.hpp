#ifndef SCHEDULER_HPP
#define SCHEDULER_HPP
#include <cstdint>

struct CycleCount
{
    uint64_t count;
    uint64_t remainder;
};

class Emulator;

struct SchedulerEvent
{
    typedef void(Emulator::*event_func)();

    event_func func;
};

class Scheduler
{
    private:
        CycleCount ee_cycles;
        CycleCount bus_cycles;
        CycleCount iop_cycles;

        unsigned int run_cycles;
    public:
        Scheduler();

        void reset();

        unsigned int calculate_run_cycles();
        void update_cycle_counts();
};

#endif // SCHEDULER_HPP
