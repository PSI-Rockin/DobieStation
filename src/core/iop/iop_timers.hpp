#ifndef IOP_TIMERS_HPP
#define IOP_TIMERS_HPP
#include <cstdint>
#include <fstream>

struct IOP_Timer_Control
{
    bool use_gate;
    uint8_t gate_mode;
    bool zero_return;
    bool compare_interrupt_enabled;
    bool overflow_interrupt_enabled;
    bool repeat_int;
    bool toggle_int;
    bool int_enable;
    bool extern_signal;
    uint8_t prescale;
    bool compare_interrupt;
    bool overflow_interrupt;
    bool started;
};

struct IOP_Timer
{
    uint64_t counter;
    IOP_Timer_Control control;
    uint64_t target;
};

class IOP_INTC;
class Scheduler;

class IOPTiming
{
    private:
        IOP_INTC* intc;
        Scheduler* scheduler;

        IOP_Timer timers[6];

        int timer_interrupt_event_id;
        int events[6];

        void timer_interrupt(int index);
        void IRQ_test(int index, bool overflow);
    public:
        IOPTiming(IOP_INTC* intc, Scheduler* scheduler);

        void reset();
        uint32_t read_counter(int index);
        uint16_t read_control(int index);
        uint32_t read_target(int index);

        void write_counter(int index, uint32_t value);
        void write_control(int index, uint16_t value);
        void write_target(int index, uint32_t value);

        void load_state(std::ifstream& state);
        void save_state(std::ofstream& state);
};

#endif // IOP_TIMERS_HPP
