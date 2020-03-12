#ifndef SCHEDULER_HPP
#define SCHEDULER_HPP
#include <cstdint>
#include <functional>
#include <list>
#include <vector>

struct CycleCount
{
    int64_t count;
    uint64_t remainder;
};

struct SchedulerEvent
{
    uint64_t event_id;
    int64_t time_to_run;
    uint64_t param;
    int func_id;
    bool pulse;
};

struct SchedulerTimer
{
    uint64_t event_id, param;
    int64_t counter, target, overflow_mask, clockrate, remainder_clocks;
    int64_t last_update, pause_delta;
    int callback_id;
    bool paused;
    bool can_overflow, can_target;
};

class Scheduler
{
    private:
        CycleCount ee_cycles;
        CycleCount bus_cycles;
        CycleCount iop_cycles;

        unsigned int run_cycles;
        uint64_t next_event_id;

        uint64_t timer_event_id;

        std::vector<std::function<void(uint64_t)> > registered_funcs;
        std::vector<std::function<void(uint64_t, bool)> > timer_callbacks;
        std::vector<SchedulerTimer> timers;
        std::list<SchedulerEvent> events;

        int64_t closest_event_time;

        uint64_t convert_to_ee_cycles(uint64_t cycles, uint64_t clockrate);

        int64_t calculate_timer_event_delta(uint64_t timer_id);
        void update_timer_event_time(uint64_t timer_id);
        void update_timer_counter(uint64_t timer_id);

        void timer_event(uint64_t index);

        SchedulerEvent* get_event_ptr(uint64_t event_id);
    public:
        constexpr static uint64_t EE_CLOCKRATE = 294912000; //294.912 MHz
        constexpr static uint64_t BUS_CLOCKRATE = EE_CLOCKRATE / 2;
        constexpr static uint64_t IOP_CLOCKRATE = EE_CLOCKRATE / 8;

        Scheduler();

        void reset();

        unsigned int calculate_run_cycles();
        unsigned int get_bus_run_cycles();
        unsigned int get_iop_run_cycles();

        int64_t get_ee_cycles();
        int64_t get_iop_cycles();

        int register_function(std::function<void(uint64_t)> func);
        int register_timer_callback(std::function<void(uint64_t, bool)> cb);

        uint64_t add_event(int func_id, uint64_t delta, uint64_t param = 0);
        void delete_event(uint64_t event_id);

        uint64_t create_timer(int func_id, uint64_t overflow_mask, uint64_t param = 0);
        void restart_timer(uint64_t timer_id);

        uint64_t get_timer_counter(uint64_t timer_id);
        void set_timer_counter(uint64_t timer_id, uint64_t counter);
        void set_timer_target(uint64_t timer_id, uint64_t target);
        void set_timer_clockrate(uint64_t timer_id, uint64_t clockrate);
        void set_timer_pause(uint64_t timer_id, bool paused);
        void set_timer_int_mask(uint64_t timer_id, bool can_overflow, bool can_target);

        void update_cycle_counts();
        void process_events();

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
