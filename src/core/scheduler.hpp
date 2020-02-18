#ifndef SCHEDULER_HPP
#define SCHEDULER_HPP
#include <cstdint>
#include <functional>
#include <list>

class Emulator;

struct CycleCount
{
    int64_t count;
    uint64_t remainder;
};

struct SchedulerEvent
{
    int64_t time_to_run;
    uint64_t param;
    std::function<void(uint64_t)> func;
};

class Scheduler
{
    private:
        CycleCount ee_cycles;
        CycleCount bus_cycles;
        CycleCount iop_cycles;

        unsigned int run_cycles;

        std::list<SchedulerEvent> events;

        int64_t closest_event_time;
    public:
        Scheduler();

        void reset();

        unsigned int calculate_run_cycles();
        unsigned int get_bus_run_cycles();
        unsigned int get_iop_run_cycles();

        int64_t get_ee_cycles();
        int64_t get_iop_cycles();

        void add_event(std::function<void(uint64_t)> func, uint64_t delay, uint64_t param = 0);

        void update_cycle_counts();
        void process_events(Emulator* e);

        void load_state(std::ifstream& state);
        void save_state(std::ofstream& state);
};

inline int64_t Scheduler::get_ee_cycles()
{
    return ee_cycles.count;
}

inline int64_t Scheduler::get_iop_cycles()
{
    return iop_cycles.count;
}

#endif // SCHEDULER_HPP
