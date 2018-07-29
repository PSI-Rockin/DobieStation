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
    bool compare_interrupt;
    bool overflow_interrupt;
};

struct IOP_Timer
{
    uint64_t counter;
    IOP_Timer_Control control;
    uint32_t target;
    uint32_t clocks;
};

class Emulator;

class IOPTiming
{
    private:
        Emulator* e;
        uint32_t cycles_since_IRQ;
        IOP_Timer timers[6];

        void IRQ_test(int index, bool overflow);
    public:
        IOPTiming(Emulator* e);

        void reset();
        void run();
        uint32_t read_counter(int index);
        uint16_t read_control(int index);

        void write_counter(int index, uint32_t value);
        void write_control(int index, uint16_t value);
        void write_target(int index, uint32_t value);

        void load_state(std::ifstream& state);
        void save_state(std::ofstream& state);
};

#endif // IOP_TIMERS_HPP
