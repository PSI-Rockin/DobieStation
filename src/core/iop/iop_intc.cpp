#include "iop.hpp"
#include "iop_intc.hpp"

IOP_INTC::IOP_INTC(IOP* iop) : iop(iop)
{

}

void IOP_INTC::int_check()
{
    iop->interrupt_check(I_CTRL && (I_STAT & I_MASK));
}

void IOP_INTC::reset()
{
    I_MASK = 0;
    I_STAT = 0;
    I_CTRL = 0;
}

void IOP_INTC::assert_irq(int id)
{
    I_STAT |= 1 << id;
    int_check();
}

uint32_t IOP_INTC::read_imask()
{
    return I_MASK;
}

uint32_t IOP_INTC::read_istat()
{
    return I_STAT;
}

uint32_t IOP_INTC::read_ictrl()
{
    //I_CTRL disables interrupts when read
    uint32_t value = I_CTRL;
    I_CTRL = 0;
    int_check();
    return value;
}

void IOP_INTC::write_imask(uint32_t value)
{
    I_MASK = value;
    int_check();
}

void IOP_INTC::write_istat(uint32_t value)
{
    I_STAT &= value;
    int_check();
}

void IOP_INTC::write_ictrl(uint32_t value)
{
    I_CTRL = value & 0x1;
    int_check();
}
