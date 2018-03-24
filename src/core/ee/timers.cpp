#include <cstdio>
#include "timers.hpp"

EmotionTiming::EmotionTiming()
{

}

void EmotionTiming::reset()
{
    for (int i = 0; i < 4; i++)
    {
        timers[i].counter = 0;
        timers[i].clocks = 0;
        timers[i].control.enabled = false;
    }
}

void EmotionTiming::run()
{
    for (int i = 0; i < 4; i++)
    {
        if (timers[i].control.enabled)
        {
            timers[i].clocks++;
            switch (timers[i].control.mode)
            {
                case 3:
                    //TODO: actual value for HSYNC
                    if (timers[i].clocks >= 15000)
                    {
                        timers[i].clocks -= 15000;
                        timers[i].counter++;
                    }
                    break;
            }
        }
    }
}

uint32_t EmotionTiming::read32(uint32_t addr)
{
    switch (addr)
    {
        case 0x10000000:
            return timers[0].counter;
        default:
            printf("[EE Timing] Unrecognized read32 from $%08X\n", addr);
            return 0;
    }
}

void EmotionTiming::write32(uint32_t addr, uint32_t value)
{
    switch (addr)
    {
        case 0x10000010:
            printf("[EE Timing] Write32 timer 0 control: $%08X\n", value);
            timers[0].control.mode = value & 0x3;
            timers[0].control.enabled = value & (1 << 7);
            break;
        default:
            printf("[EE Timing] Unrecognized write32 to $%08X of $%08X\n", addr, value);
            break;
    }
}
