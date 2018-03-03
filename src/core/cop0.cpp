#include <cstdio>
#include "cop0.hpp"

Cop0::Cop0() {}

void Cop0::reset()
{
    for (int i = 0; i < 32; i++)
        gpr[i] = 0;

    //Set processor revision id
    gpr[15] = 0x00002E20;
}

uint32_t Cop0::mfc(int index)
{
    //printf("[COP0] Move from reg%d\n", index);
    return gpr[index];
}

void Cop0::mtc(int index, uint32_t value)
{
    printf("[COP0] Move to reg%d: $%08X\n", index, value);
    gpr[index] = value;
}

bool Cop0::int_enabled()
{
    return (gpr[STATUS] & 0x10001) == 0x10001;
}

void Cop0::set_EIE(bool IE)
{
    gpr[STATUS] &= ~(1 << 16);
    gpr[STATUS] |= IE << 16;
}

void Cop0::set_exception_cause(uint8_t cause)
{
    gpr[CAUSE] &= ~0x24;
    gpr[CAUSE] |= cause << 2;
}

void Cop0::count_up()
{
    gpr[9]++;
}
