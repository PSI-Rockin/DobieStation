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
    public:
        IOPTiming();
};

#endif // IOP_TIMERS_HPP
