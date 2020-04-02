#ifndef __SPU_ENVELOPE_H_
#define __SPU_ENVELOPE_H_
#include <cstdint>


struct Envelope
{
    bool exponential;
    bool rising;
    bool negative_phase;
    uint32_t cycles_left;
    uint8_t shift;
    int8_t step;

    int16_t next_step(int16_t volume);
};

struct ADSR
{
    enum class Stage
    {
        Stopped,
        Attack,
        Decay,
        Sustain,
        Release,
    };

    Envelope envelope;
    Stage stage;
    uint16_t target;
    int16_t volume;
    uint16_t adsr1, adsr2;
    void set_stage(Stage new_stage);
    void advance();
    void update();
};

struct Volume
{
    Envelope envelope;
    int16_t value;
    bool running;

    void advance();
    void set(uint16_t value);
};



#endif // __SPU_ENVELOPE_H_
