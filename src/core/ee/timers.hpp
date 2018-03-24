#ifndef TIMERS_HPP
#define TIMERS_HPP
#include <cstdint>

struct TimerControl
{
    uint8_t mode;
    bool enabled;
};

struct Timer
{
    uint16_t counter;
    TimerControl control;

    //Internal variable for holding number of EE clocks
    uint32_t clocks;
};

class EmotionTiming
{
    private:
        Timer timers[4];
    public:
        EmotionTiming();

        void reset();
        void run();

        uint32_t read32(uint32_t addr);
        void write32(uint32_t addr, uint32_t value);
};

#endif // TIMERS_HPP
