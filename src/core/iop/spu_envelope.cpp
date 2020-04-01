#include "spu_envelope.hpp"
#include <algorithm>


int16_t Envelope::next_step(int16_t volume)
{
    if (cycles_left > 0)
    {
        cycles_left--;
        return 0;
    }

    // Need to handle negative phase for volume sweeps eventually

    cycles_left = 1 << std::max(0, shift-11);
    int16_t next_step = static_cast<int16_t>(step << std::max(0, 11-shift));

    if (exponential && rising && volume > 0x6000)
        cycles_left *= 4;

    // This seems quite suspect, wonky in FFXII's no memory card warning screen
    if (exponential && !rising)
        next_step = next_step*volume / 0x8000;

    return next_step;

}

void ADSR::set_stage(ADSR::Stage new_stage)
{
    envelope.cycles_left = 0;
    switch (new_stage)
    {
        case ADSR::Stage::Attack:
            target = 0x7fff;
            envelope.exponential = ((adsr1 >> 15) & 1) ? true : false;
            envelope.negative_phase = false;
            envelope.rising = true;
            envelope.shift = (adsr1 >> 10) & 0x1f;
            envelope.step = (adsr2 >> 8) & 0x03;
            envelope.step = envelope.rising ? (7 - envelope.step) : (-8 + envelope.step);
            stage = new_stage;
            break;
        case ADSR::Stage::Decay:
            target = ((adsr1 & 0x0F) + 1) * 0x800;
            envelope.negative_phase = false;
            envelope.exponential = true;
            envelope.rising = false;
            envelope.shift = (adsr1 >> 4) & 0x1f;
            envelope.step = -8;
            stage = new_stage;
            break;
        case ADSR::Stage::Sustain:
            target = 0;
            envelope.negative_phase = false;
            envelope.exponential = ((adsr2 >> 15) & 1) ? true : false;
            envelope.rising = ((adsr2 >> 14) & 1) ? false : true;
            envelope.shift = (adsr2 >> 8) & 0x1f;
            envelope.step = (adsr2 >> 6) & 0x03;
            envelope.step = envelope.rising ? (7 - envelope.step) : (-8 + envelope.step);
            stage = new_stage;
            break;
        case ADSR::Stage::Release:
            target = 0;
            envelope.negative_phase = false;
            envelope.exponential = ((adsr2 >> 5) & 1) ? true : false;
            envelope.rising = false;
            envelope.shift = adsr2 & 0x1f;
            envelope.step = -8;
            stage = new_stage;
            break;
        case ADSR::Stage::Stopped:
            stage = new_stage;
    }
}

void ADSR::advance()
{
    if (stage == ADSR::Stage::Stopped)
    {
        return;
    }

    int16_t step = envelope.next_step(volume);

    volume = static_cast<int16_t>(std::max(std::min(volume+step, 0x7fff), 0));

    if (stage != Stage::Sustain)
    {
        if ((envelope.rising && volume >= target) ||
            (!envelope.rising && volume <= target))
        {
            switch (stage)
            {
                case Stage::Attack:
                    set_stage(Stage::Decay);
                    break;
                case Stage::Decay:
                    set_stage(Stage::Sustain);
                    break;
                case Stage::Release:
                    set_stage(Stage::Stopped);
                    break;
                default:
                    break;
            }
        }
    }
}

void Volume::set(uint16_t val)
{
    if (!(val & 0x8000))
    {
        value = static_cast<int16_t>(val << 1);
        running = false;
        return;
    }

    running = true;
    envelope.exponential = val & (1 << 14);
    envelope.rising = !(val & (1 << 13));
    envelope.negative_phase = val & (1 << 12);
    envelope.shift = (val >> 2) & 0x1f;
    envelope.step = val & 0x03;
    envelope.step = envelope.rising ? (7 - envelope.step) : (-8 + envelope.step);
}

void Volume::advance()
{
    if (!running)
        return;
    int16_t step = envelope.next_step(value);
    value = static_cast<int16_t>(std::max(std::min(value+step, 0x7fff), -0x8000));
}
