#include "spu.hpp"

SPU::SPU()
{

}

void SPU::reset()
{
    stat = 0;
}

uint16_t SPU::get_stat()
{
    uint16_t reg = stat;
    stat &= ~0x80; //Clear interrupt flag?
    return reg;
}

void SPU::set_int_flag()
{
    stat |= 0x80;
}
