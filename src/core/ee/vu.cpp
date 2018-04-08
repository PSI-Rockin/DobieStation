#include <cstdio>
#include "vu.hpp"

VectorUnit::VectorUnit(int id) : id(id)
{
    gpr[0].f[0] = 0.0;
    gpr[0].f[1] = 0.0;
    gpr[0].f[2] = 0.0;
    gpr[0].f[3] = 1.0;
    int_gpr[0] = 0;
}

uint32_t VectorUnit::qmfc2(int id, int field)
{
    return gpr[id].u[field];
}

uint32_t VectorUnit::cfc(int index)
{
    if (index < 16)
        return int_gpr[index];
    printf("[COP2] Unrecognized cfc2 from reg %d\n", index);
    return 0;
}

void VectorUnit::ctc(int index, uint32_t value)
{
    if (index >= 1 && index < 16)
    {
        int_gpr[index] = value & 0xFFFF;
        return;
    }
    printf("[COP2] Unrecognized ctc2 of $%08X to reg %d\n", value, index);
}

void VectorUnit::iswr(uint8_t field, uint8_t source, uint8_t base)
{
    uint32_t addr = int_gpr[base] << 4;
    printf("[VU] ISWR to $%08X!\n", addr);
}

void VectorUnit::sub(uint8_t field, uint8_t dest, uint8_t reg1, uint8_t reg2)
{
    printf("[VU] SUB: ");
    for (int i = 0; i < 4; i++)
    {
        if (field & (1 << i))
        {
            float result = gpr[reg1].f[i] - gpr[reg2].f[i];
            set_gpr(dest, i, result);
            printf("%f ", gpr[dest].f[i]);
        }
    }
    printf("\n");
}
