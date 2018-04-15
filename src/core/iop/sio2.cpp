#include <cstdio>
#include "../emulator.hpp"
#include "sio2.hpp"

SIO2::SIO2(Emulator* e) : e(e)
{

}

void SIO2::reset()
{

}

void SIO2::set_control(uint32_t value)
{
    printf("[SIO2] Set control: $%08X\n", value);
    if (value & 0x1)
    {
        e->iop_request_IRQ(17);
    }
}

uint32_t SIO2::get_RECV1()
{
    printf("[SIO2] Read RECV1\n");
    return 0x1D100;
}
