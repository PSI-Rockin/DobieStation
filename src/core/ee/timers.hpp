#ifndef TIMERS_HPP
#define TIMERS_HPP
#include <cstdint>
#include <fstream>

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
    int clocks;

    //Indicates if the timer is paused by a gate
    bool gated;

    uint32_t clock_scale;
    uint64_t last_update;
};

class INTC;

class EmotionTiming
{
    private:
        INTC* intc;
        Timer timers[4];

        uint64_t cycle_count;
        uint64_t next_event;

        uint32_t read_control(int index);
        void write_control(int index, uint32_t value);

        uint64_t get_time_to_interrupt(int index);

        void update_timers();
        void reschedule();
    public:
        EmotionTiming(INTC* intc);

        void reset();
        void run(int cycles);

        void gate(bool VSYNC, bool high);

        uint32_t read32(uint32_t addr);
        void write32(uint32_t addr, uint32_t value);

        void load_state(std::ifstream& state);
        void save_state(std::ofstream& state);
};

#endif // TIMERS_HPP
