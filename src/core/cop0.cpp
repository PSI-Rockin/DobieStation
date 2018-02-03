#include "cop0.hpp"

Cop0::Cop0()
{
    gpr[15] = 0x00002E20;
    //gpr[15] = 0;
}

uint32_t Cop0::mfc(int index)
{
    return gpr[index];
}

void Cop0::mtc(int index, uint32_t value)
{
    gpr[index] = value;
}

void Cop0::count_up()
{
    gpr[9]++;
}
