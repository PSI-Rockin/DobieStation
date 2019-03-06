#ifndef SCHEDULER_HPP
#define SCHEDULER_HPP
#include <cstdint>
#include <list>

class Emulator;

typedef void(Emulator::*event_func)();

enum EE_Event
{
    VBLANK_START,
    VBLANK_END,
    TIMER_INT
};

enum IOP_Event
{
    CDVD_INT
};

struct CycleCount
{
    int64_t count;
    uint64_t remainder;
};

struct SchedulerEvent
{
    int64_t time_to_run;
    event_func func;
};

class Scheduler
{
    private:
        CycleCount ee_cycles;
        CycleCount bus_cycles;
        CycleCount iop_cycles;

        unsigned int run_cycles;

        std::list<SchedulerEvent> ee_events;

        int64_t closest_event_time;
    public:
        Scheduler();

        void reset();

        unsigned int calculate_run_cycles();
        unsigned int get_bus_run_cycles();
        unsigned int get_iop_run_cycles();

        void add_ee_event(event_func func, uint64_t delta_time_to_run);

        void update_cycle_counts();
        void process_events(Emulator* e);
};

#endif // SCHEDULER_HPP
