#ifndef TIMERS_HPP
#define TIMERS_HPP
#include <cstdint>

struct TimerControl
{
    uint8_t mode;
    bool gate_enable;
    bool gate_VBLANK;
    uint8_t gate_mode;
    bool clear_on_reference;
    bool enabled;
    bool compare_int_enable;
    bool overflow_int_enable;
    bool compare_int;
    bool overflow_int;
};

struct Timer
{
    uint32_t counter;
    TimerControl control;
    uint32_t compare;

    //Internal variable for holding number of EE clocks
    uint32_t clocks;
};

class INTC;

class EmotionTiming
{
    private:
        INTC* intc;
        Timer timers[4];

        uint32_t read_control(int index);
        void write_control(int index, uint32_t value);
        void count_up(int index, int cycles_per_count);
    public:
        EmotionTiming(INTC* intc);

        void reset();
        void run();

        uint32_t read32(uint32_t addr);
        void write32(uint32_t addr, uint32_t value);
};

#endif // TIMERS_HPP
