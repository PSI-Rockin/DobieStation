#ifndef IOP_TIMERS_HPP
#define IOP_TIMERS_HPP
#include <cstdint>

struct IOP_Timer_Control
{
    bool check_gate;
    uint8_t gate_mode;

};

struct IOP_Timer
{
    uint32_t counter;
    IOP_Timer_Control control;
};

class IOPTiming
{
    private:
        IOP_Timer timers[6];
    public:
        IOPTiming();

        void reset();
        void run();
        uint32_t read_counter(int index);

        void write_control(int index, uint16_t value);
};

#endif // IOP_TIMERS_HPP
