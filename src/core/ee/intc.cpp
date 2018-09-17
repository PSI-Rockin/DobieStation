#include <cstdio>
#include "emotion.hpp"
#include "intc.hpp"

INTC::INTC(EmotionEngine* cpu) : cpu(cpu)
{

}

void INTC::reset()
{
    INTC_MASK = 0;
    INTC_STAT = 0;

    read_stat_count = 0;
    stat_speedhack_active = false;
}

uint32_t INTC::read_mask()
{
    return INTC_MASK;
}

uint32_t INTC::read_stat()
{
    read_stat_count++;
    if (read_stat_count >= 1000)
    {
        cpu->halt();
        stat_speedhack_active = true;
    }
    return INTC_STAT;
}

void INTC::write_mask(uint32_t value)
{
    INTC_MASK ^= (value & 0x7FFF);
    printf("[INTC] New INTC_MASK: $%08X\n", INTC_MASK);
    int0_check();
}

void INTC::write_stat(uint32_t value)
{
    INTC_STAT &= ~value;
    int0_check();
}

void INTC::assert_IRQ(int id)
{
    printf("[INTC] EE IRQ: %d\n", id);
    INTC_STAT |= (1 << id);
    int0_check();
}

void INTC::deassert_IRQ(int id)
{
    INTC_STAT &= ~(1 << id);
    int0_check();
}

void INTC::int0_check()
{
    if (stat_speedhack_active)
        cpu->unhalt();
    read_stat_count = 0;
    cpu->set_int0_signal(INTC_STAT & INTC_MASK);
}
