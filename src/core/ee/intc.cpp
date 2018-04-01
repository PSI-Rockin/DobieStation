#include "emotion.hpp"
#include "intc.hpp"

INTC::INTC(EmotionEngine* cpu) : cpu(cpu)
{

}

void INTC::reset()
{
    INTC_MASK = 0;
    INTC_STAT = 0;
}

uint32_t INTC::read_mask()
{
    return INTC_MASK;
}

uint32_t INTC::read_stat()
{
    return INTC_STAT;
}

void INTC::write_mask(uint32_t value)
{
    INTC_MASK ^= (value & 0x7FFF);
    int0_check();
}

void INTC::write_stat(uint32_t value)
{
    INTC_STAT &= ~value;
}

void INTC::assert_IRQ(int id)
{
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
    cpu->set_int0_signal(INTC_STAT & INTC_MASK);
}
