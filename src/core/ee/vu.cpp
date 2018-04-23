#include <cmath>
#include <cstdio>
#include <cstdlib>
#include "vu.hpp"

#define _x(f) f&8
#define _y(f) f&4
#define _z(f) f&2
#define _w(f) f&1

//#define printf(fmt, ...)(0)

VectorUnit::VectorUnit(int id) : id(id)
{
    gpr[0].f[0] = 0.0;
    gpr[0].f[1] = 0.0;
    gpr[0].f[2] = 0.0;
    gpr[0].f[3] = 1.0;
    int_gpr[0] = 0;
}

/**
 * Code taken from PCSX2 and adapted to DobieStation
 * https://github.com/PCSX2/pcsx2/blob/1292cd505efe7c68ab87880b4fd6809a96da703c/pcsx2/VUops.cpp#L1795
 */
void VectorUnit::advance_r()
{
    int x = (R.u >> 4) & 1;
    int y = (R.u >> 22) & 1;
    R.u <<= 1;
    R.u ^= x ^ y;
    R.u = (R.u & 0x7FFFFF) | 0x3F800000;
}

uint32_t VectorUnit::qmfc2(int id, int field)
{
    return gpr[id].u[field];
}

uint32_t VectorUnit::cfc(int index)
{
    if (index < 16)
        return int_gpr[index];
    switch (index)
    {
        case 20:
            return R.u;
        case 22:
            return Q;
        default:
            printf("[COP2] Unrecognized cfc2 from reg %d\n", index);
    }
    return 0;
}

void VectorUnit::ctc(int index, uint32_t value)
{
    if (index < 16)
    {
        printf("[COP2] Set vi%d to $%04X\n", index, value);
        set_int(index, value);
        return;
    }
    switch (index)
    {
        default:
            printf("[COP2] Unrecognized ctc2 of $%08X to reg %d\n", value, index);
    }
}

void VectorUnit::add(uint8_t field, uint8_t dest, uint8_t reg1, uint8_t reg2)
{
    printf("[VU] ADD: ");
    for (int i = 0; i < 4; i++)
    {
        if (field & (1 << (3 - i)))
        {
            float result = gpr[reg1].f[i] + gpr[reg2].f[i];
            set_gpr_f(dest, i, result);
            printf("(%d)%f ", i, gpr[dest].f[i]);
        }
    }
    printf("\n");
}

void VectorUnit::addbc(uint8_t bc, uint8_t field, uint8_t dest, uint8_t source, uint8_t bc_reg)
{
    printf("[VU] ADDbc: ");
    float op = gpr[bc_reg].f[bc];
    for (int i = 0; i < 4; i++)
    {
        if (field & (1 << (3 - i)))
        {
            float temp = op + gpr[source].f[i];
            set_gpr_f(dest, i, temp);
            printf("(%d)%f ", i, gpr[dest].f[i]);
        }
    }
    printf("\n");
}

void VectorUnit::addq(uint8_t field, uint8_t dest, uint8_t source)
{
    printf("[VU] ADDq: ");
    for (int i = 0; i < 4; i++)
    {
        if (field & (1 << (3 - i)))
        {
            float temp = Q + gpr[source].f[i];
            set_gpr_f(dest, i, temp);
            printf("(%d)%f ", i, gpr[dest].f[i]);
        }
    }
    printf("\n");
}

void VectorUnit::div(uint8_t ftf, uint8_t fsf, uint8_t reg1, uint8_t reg2)
{
    float denom = gpr[reg2].f[ftf];
    if (!denom)
    {
        printf("[VU] DIV by zero!\n");
        exit(1);
    }
    Q = gpr[reg1].f[fsf];
    Q /= denom;
    printf("[VU] RSQRT: %f\n", Q);
    printf("Reg1: %f\n", gpr[reg1].f[fsf]);
    printf("Reg2: %f\n", gpr[reg2].f[ftf]);
}

void VectorUnit::ftoi4(uint8_t field, uint8_t dest, uint8_t source)
{
    printf("[VU] FTOI4: ");
    for (int i = 0; i < 4; i++)
    {
        if (field & (1 << (3 - i)))
        {
            gpr[dest].s[i] = (int32_t)(gpr[source].f[i] * (1.0f / 0.0625f));
            printf("(%d)$%08X ", i, gpr[dest].s[i]);
        }
    }
    printf("\n");
}

void VectorUnit::iadd(uint8_t dest, uint8_t reg1, uint8_t reg2)
{
    set_int(dest, int_gpr[reg1] + int_gpr[reg2]);
    printf("[VU] IADD: $%04X\n", int_gpr[dest]);
}

void VectorUnit::iswr(uint8_t field, uint8_t source, uint8_t base)
{
    uint32_t addr = int_gpr[base] << 4;
    printf("[VU] ISWR to $%08X!\n", addr);
}

void VectorUnit::maddbc(uint8_t bc, uint8_t field, uint8_t dest, uint8_t source, uint8_t bc_reg)
{
    printf("[VU] MADDbc: ");
    float op = gpr[bc_reg].f[bc];
    for (int i = 0; i < 4; i++)
    {
        if (field & (1 << (3 - i)))
        {
            float temp = op * gpr[source].f[i];
            set_gpr_f(dest, i, temp + ACC.f[i]);
            printf("(%d)%f ", i, gpr[dest].f[i]);
        }
    }
    printf("\n");
}

void VectorUnit::maddabc(uint8_t bc, uint8_t field, uint8_t source, uint8_t bc_reg)
{
    printf("[VU] MADDAbc: ");
    float op = gpr[bc_reg].f[bc];
    for (int i = 0; i < 4; i++)
    {
        if (field & (1 << (3 - i)))
        {
            float temp = op * gpr[source].f[i];
            ACC.f[i] += temp;
            printf("(%d)%f ", i, ACC.f[i]);
        }
    }
    printf("\n");
}

void VectorUnit::mul(uint8_t field, uint8_t dest, uint8_t reg1, uint8_t reg2)
{
    printf("[VU] MUL: ");
    for (int i = 0; i < 4; i++)
    {
        if (field & (1 << (3 - i)))
        {
            float result = gpr[reg1].f[i] * gpr[reg2].f[i];
            set_gpr_f(dest, i, result);
            printf("(%d)%f ", i, gpr[dest].f[i]);
        }
    }
    printf("\n");
}

void VectorUnit::mulbc(uint8_t bc, uint8_t field, uint8_t dest, uint8_t source, uint8_t bc_reg)
{
    printf("[VU] MULbc: ");
    float op = gpr[bc_reg].f[bc];
    for (int i = 0; i < 4; i++)
    {
        if (field & (1 << (3 - i)))
        {
            float temp = op * gpr[source].f[i];
            set_gpr_f(dest, i, temp);
            printf("(%d)%f ", i, gpr[dest].f[i]);
        }
    }
    printf("\n");
}

void VectorUnit::move(uint8_t field, uint8_t dest, uint8_t source)
{
    printf("[VU] MOVE");
    for (int i = 0; i < 4; i++)
    {
        if (field & (1 << (3 - i)))
        {
            float temp = gpr[source].f[i];
            set_gpr_f(dest, i, temp);
        }
    }
    printf("\n");
}

void VectorUnit::mr32(uint8_t field, uint8_t dest, uint8_t source)
{
    printf("[VU] MR32");
    for (int i = 0; i < 4; i++)
    {
        if (field & (1 << (3 - i)))
        {
            float temp = gpr[source].f[(i + 1) & 0x3];
            set_gpr_f(dest, i, temp);
        }
    }
    printf("\n");
}

void VectorUnit::mulabc(uint8_t bc, uint8_t field, uint8_t source, uint8_t bc_reg)
{
    printf("[VU] MULAbc: ");
    float op = gpr[bc_reg].f[bc];
    for (int i = 0; i < 4; i++)
    {
        if (field & (1 << (3 - i)))
        {
            float temp = op * gpr[source].f[i];
            ACC.f[i] = temp;
            printf("(%d)%f ", i, ACC.f[i]);
        }
    }
    printf("\n");
}

void VectorUnit::mulq(uint8_t field, uint8_t dest, uint8_t source)
{
    printf("[VU] MULq: ");
    for (int i = 0; i < 4; i++)
    {
        if (field & (1 << (3 - i)))
        {
            float temp = Q * gpr[source].f[i];
            set_gpr_f(dest, i, temp);
            printf("(%d)%f ", i, gpr[dest].f[i]);
        }
    }
    printf("\n");
}

/**
 * FDx = ACCx - FSy * FTz
 * FDy = ACCy - FSz * FTx
 * FDz = ACCz - FSx * FTy
 */
void VectorUnit::opmsub(uint8_t dest, uint8_t reg1, uint8_t reg2)
{
    gpr[dest].f[0] = ACC.f[0] - gpr[reg1].f[1] * gpr[reg2].f[2];
    gpr[dest].f[1] = ACC.f[1] - gpr[reg1].f[2] * gpr[reg2].f[0];
    gpr[dest].f[2] = ACC.f[2] - gpr[reg1].f[0] * gpr[reg2].f[1];
    printf("[VU] OPMSUB: %f, %f, %f\n", gpr[dest].f[0], gpr[dest].f[1], gpr[dest].f[2]);
}

/**
 * ACCx = FSy * FTz
 * ACCy = FSz * FTx
 * ACCz = FSx * FTy
 */
void VectorUnit::opmula(uint8_t reg1, uint8_t reg2)
{
    ACC.f[0] = gpr[reg1].f[1] * gpr[reg2].f[2];
    ACC.f[1] = gpr[reg1].f[2] * gpr[reg2].f[0];
    ACC.f[2] = gpr[reg1].f[0] * gpr[reg2].f[1];
    printf("[VU] OPMULA: %f, %f, %f\n", ACC.f[0], ACC.f[1], ACC.f[2]);
}

void VectorUnit::rinit(uint8_t fsf, uint8_t source)
{
    R.u = 0x3F800000;
    R.u |= gpr[source].u[fsf] & 0x007FFFFF;
    printf("[VU] RINIT: %f\n", R.f);
}

void VectorUnit::rnext(uint8_t field, uint8_t dest)
{
    advance_r();
    for (int i = 0; i < 4; i++)
    {
        if (field & (1 << (3 - i)))
        {
            set_gpr_f(dest, i, R.f);
        }
    }
    printf("[VU] RNEXT: %f\n", R.f);
}

void VectorUnit::rsqrt(uint8_t ftf, uint8_t fsf, uint8_t reg1, uint8_t reg2)
{
    float denom = fabs(gpr[reg2].f[ftf]);
    if (!denom)
    {
        printf("[VU] RSQRT by zero!\n");
        exit(1);
    }
    Q = gpr[reg1].f[fsf];
    Q /= sqrt(denom);
    printf("[VU] RSQRT: %f\n", Q);
    printf("Reg1: %f\n", gpr[reg1].f[fsf]);
    printf("Reg2: %f\n", gpr[reg2].f[ftf]);
}

void VectorUnit::sqi(uint8_t field, uint8_t source, uint8_t base)
{
    uint32_t addr = int_gpr[base] << 4;
    printf("[VU] SQI to $%08X!\n", addr);
    int_gpr[base]++;
}

void VectorUnit::vu_sqrt(uint8_t ftf, uint8_t source)
{
    Q = sqrt(gpr[source].f[ftf]);
    printf("[VU] SQRT: %f\n", Q);
    printf("Source: %f\n", gpr[source].f[ftf]);
}

void VectorUnit::sub(uint8_t field, uint8_t dest, uint8_t reg1, uint8_t reg2)
{
    printf("[VU] SUB: ");
    for (int i = 0; i < 4; i++)
    {
        if (field & (1 << (3 - i)))
        {
            float result = gpr[reg1].f[i] - gpr[reg2].f[i];
            set_gpr_f(dest, i, result);
            printf("(%d)%f ", i, gpr[dest].f[i]);
        }
    }
    printf("\n");
}

void VectorUnit::subbc(uint8_t bc, uint8_t field, uint8_t dest, uint8_t source, uint8_t bc_reg)
{
    printf("[VU] SUBbc: ");
    float op = gpr[bc_reg].f[bc];
    for (int i = 0; i < 4; i++)
    {
        if (field & (1 << (3 - i)))
        {
            float temp = gpr[source].f[i] - op;
            set_gpr_f(dest, i, temp);
            printf("(%d)%f ", i, gpr[dest].f[i]);
        }
    }
    printf("\n");
}
