#include <cmath>
#include <cstdio>
#include <cstdlib>
#include "vu.hpp"

#define _x(f) f&8
#define _y(f) f&4
#define _z(f) f&2
#define _w(f) f&1

#define printf(fmt, ...)(0)

VectorUnit::VectorUnit(int id) : id(id)
{
    gpr[0].f[0] = 0.0;
    gpr[0].f[1] = 0.0;
    gpr[0].f[2] = 0.0;
    gpr[0].f[3] = 1.0;
    int_gpr[0] = 0;
}

void VectorUnit::reset()
{
    status = 0;
    clip_flags = 0;
}

float VectorUnit::convert(uint32_t value)
{
    switch(value & 0x7f800000)
    {
        case 0x0:
            value &= 0x80000000;
            return *(float*)&value;
        case 0x7f800000:
        {
            uint32_t result = (value & 0x80000000)|0x7f7fffff;
            return *(float*)&result;
        }
    }
    return *(float*)&value;
}

void VectorUnit::print_vectors(uint8_t a, uint8_t b)
{
    printf("A: ");
    for (int i = 0; i < 4; i++)
        printf("%f ", gpr[a].f[i]);
    printf("\nB: ");
    for (int i = 0; i < 4; i++)
        printf("%f ", gpr[b].f[i]);
    printf("\n");
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
        case 16:
            return status;
        case 18:
            return clip_flags;
        case 20:
            return R.u & 0x7FFFFF;
        case 21:
            return I.u;
        case 22:
            return Q.u;
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
        case 20:
            R.u = value;
            break;
        case 21:
            I.u = value;
            printf("[VU] I = %f\n", I.f);
            break;
        case 22:
            Q.u = value;
            break;
        default:
            printf("[COP2] Unrecognized ctc2 of $%08X to reg %d\n", value, index);
    }
}

void VectorUnit::abs(uint8_t field, uint8_t dest, uint8_t source)
{
    printf("[VU] ABS: ");
    for (int i = 0; i < 4; i++)
    {
        if (field & (1 << (3 - i)))
        {
            float result = fabs(convert(gpr[source].u[i]));
            set_gpr_f(dest, i, result);
            printf("(%d)%f ", i, gpr[dest].f[i]);
        }
    }
    printf("\n");
}

void VectorUnit::add(uint8_t field, uint8_t dest, uint8_t reg1, uint8_t reg2)
{
    printf("[VU] ADD: ");
    for (int i = 0; i < 4; i++)
    {
        if (field & (1 << (3 - i)))
        {
            float result = convert(gpr[reg1].u[i]) + convert(gpr[reg2].u[i]);
            set_gpr_f(dest, i, result);
            printf("(%d)%f ", i, gpr[dest].f[i]);
        }
    }
    printf("\n");
}

void VectorUnit::adda(uint8_t field, uint8_t reg1, uint8_t reg2)
{
    printf("[VU] ADDA: ");
    for (int i = 0; i < 4; i++)
    {
        if (field & (1 << (3 - i)))
        {
            ACC.f[i] = convert(gpr[reg1].u[i]) + convert(gpr[reg2].u[i]);
            printf("(%d)%f ", i, ACC.f[i]);
        }
    }
    printf("\n");
}

void VectorUnit::addabc(uint8_t bc, uint8_t field, uint8_t source, uint8_t bc_reg)
{
    printf("[VU] ADDAbc: ");
    float op = convert(gpr[bc_reg].u[bc]);
    for (int i = 0; i < 4; i++)
    {
        if (field & (1 << (3 - i)))
        {
            ACC.f[i] = op + convert(gpr[source].u[i]);
            printf("(%d)%f", i, ACC.f[i]);
        }
    }
    printf("\n");
}

void VectorUnit::addbc(uint8_t bc, uint8_t field, uint8_t dest, uint8_t source, uint8_t bc_reg)
{
    printf("[VU] ADDbc: ");
    float op = convert(gpr[bc_reg].u[bc]);
    for (int i = 0; i < 4; i++)
    {
        if (field & (1 << (3 - i)))
        {
            float temp = op + convert(gpr[source].u[i]);
            set_gpr_f(dest, i, temp);
            printf("(%d)%f ", i, gpr[dest].f[i]);
        }
    }
    printf("\n");
}

void VectorUnit::addq(uint8_t field, uint8_t dest, uint8_t source)
{
    printf("[VU] ADDq: ");
    float value = convert(Q.u);
    for (int i = 0; i < 4; i++)
    {
        if (field & (1 << (3 - i)))
        {
            float temp = value + convert(gpr[source].u[i]);
            set_gpr_f(dest, i, temp);
            printf("(%d)%f ", i, gpr[dest].f[i]);
        }
    }
    printf("\n");
}

void VectorUnit::clip(uint8_t reg1, uint8_t reg2)
{
    printf("[VU] CLIP\n");
    clip_flags <<= 6; //Move previous clipping judgments up

    //Compare x, y, z fields of FS with the w field of FT
    float value = fabs(convert(gpr[reg2].u[3]));

    float x = convert(gpr[reg1].u[0]);
    float y = convert(gpr[reg1].u[1]);
    float z = convert(gpr[reg1].u[2]);

    clip_flags |= (x > +value);
    clip_flags |= (x < -value) << 1;
    clip_flags |= (y > +value) << 2;
    clip_flags |= (y < -value) << 3;
    clip_flags |= (z > +value) << 4;
    clip_flags |= (z < -value) << 5;
    clip_flags &= 0xFFFFFF;
}

void VectorUnit::div(uint8_t ftf, uint8_t fsf, uint8_t reg1, uint8_t reg2)
{
    float num = convert(gpr[reg1].u[fsf]);
    float denom = convert(gpr[reg2].u[ftf]);
    status = (status & 0xFCF) | ((status & 0x30) << 6);
    if (denom == 0.0)
    {
        if (num == 0.0)
            status |= 0x10;
        else
            status |= 0x20;

        if ((gpr[reg1].u[fsf] & 0x80000000) != (gpr[reg2].u[ftf] & 0x80000000))
            Q.u = 0xFF7FFFFF;
        else
            Q.u = 0x7F7FFFFF;
    }
    else
    {
        Q.f = num / denom;
        Q.f = convert(Q.u);
    }
    printf("[VU] DIV: %f\n", Q.f);
    printf("Reg1: %f\n", num);
    printf("Reg2: %f\n", denom);
}

void VectorUnit::ftoi0(uint8_t field, uint8_t dest, uint8_t source)
{
    printf("[VU] FTOI0: ");
    for (int i = 0; i < 4; i++)
    {
        if (field & (1 << (3 - i)))
        {
            gpr[dest].s[i] = (int32_t)convert(gpr[source].u[i]);
            printf("(%d)$%08X ", i, gpr[dest].s[i]);
        }
    }
    printf("\n");
}

void VectorUnit::ftoi4(uint8_t field, uint8_t dest, uint8_t source)
{
    printf("[VU] FTOI4: ");
    for (int i = 0; i < 4; i++)
    {
        if (field & (1 << (3 - i)))
        {
            gpr[dest].s[i] = (int32_t)(convert(gpr[source].u[i]) * (1.0f / 0.0625f));
            printf("(%d)$%08X ", i, gpr[dest].s[i]);
        }
    }
    printf("\n");
}

void VectorUnit::ftoi12(uint8_t field, uint8_t dest, uint8_t source)
{
    printf("[VU] FTOI12: ");
    for (int i = 0; i < 4; i++)
    {
        if (field & (1 << (3 - i)))
        {
            gpr[dest].s[i] = (int32_t)(convert(gpr[source].u[i]) * (1.0f / 0.000244140625f));
            printf("(%d)$%08X ", i, gpr[dest].s[i]);
        }
    }
    printf("\n");
}

void VectorUnit::ftoi15(uint8_t field, uint8_t dest, uint8_t source)
{
    printf("[VU] FTOI15: ");
    for (int i = 0; i < 4; i++)
    {
        if (field & (1 << (3 - i)))
        {
            gpr[dest].s[i] = (int32_t)(convert(gpr[source].u[i]) * (1.0f / 0.000030517578125));
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

void VectorUnit::iaddi(uint8_t dest, uint8_t source, uint8_t imm)
{
    set_int(dest, int_gpr[source] + imm);
    printf("[VU] IADDI: $%04X\n", int_gpr[dest]);
}

void VectorUnit::iand(uint8_t dest, uint8_t reg1, uint8_t reg2)
{
    set_int(dest, int_gpr[reg1] & int_gpr[reg2]);
    printf("[VU] IAND: $%04X\n", int_gpr[dest]);
}

void VectorUnit::iswr(uint8_t field, uint8_t source, uint8_t base)
{
    uint32_t addr = int_gpr[base] << 4;
    printf("[VU] ISWR to $%08X!\n", addr);
}

void VectorUnit::itof0(uint8_t field, uint8_t dest, uint8_t source)
{
    printf("[VU] ITOF0: ");
    for (int i = 0; i < 4; i++)
    {
        if (field & (1 << (3 - i)))
        {
            gpr[dest].f[i] = (float)gpr[source].s[i];
            printf("(%d)%f ", i, gpr[dest].f[i]);
        }
    }
    printf("\n");
}

void VectorUnit::itof4(uint8_t field, uint8_t dest, uint8_t source)
{
    printf("[VU] ITOF4: ");
    for (int i = 0; i < 4; i++)
    {
        if (field & (1 << (3 - i)))
        {
            gpr[dest].f[i] = (float)((float)gpr[source].s[i] * 0.0625f);
            printf("(%d)%f ", i, gpr[dest].f[i]);
        }
    }
    printf("\n");
}

void VectorUnit::itof12(uint8_t field, uint8_t dest, uint8_t source)
{
    printf("[VU] ITOF12: ");
    for (int i = 0; i < 4; i++)
    {
        if (field & (1 << (3 - i)))
        {
            gpr[dest].f[i] = (float)((float)gpr[source].s[i] * 0.000244140625f);
            printf("(%d)%f ", i, gpr[dest].f[i]);
        }
    }
    printf("\n");
}

void VectorUnit::madd(uint8_t field, uint8_t dest, uint8_t reg1, uint8_t reg2)
{
    printf("[VU] MADD: ");
    for (int i = 0; i < 4; i++)
    {
        if (field & (1 << (3 - i)))
        {
            float temp = convert(gpr[reg1].u[i]) * convert(gpr[reg2].u[i]);
            set_gpr_f(dest, i, temp + convert(ACC.u[i]));
            printf("(%d)%f ", i, gpr[dest].f[i]);
        }
    }
    printf("\n");
}

void VectorUnit::madda(uint8_t field, uint8_t reg1, uint8_t reg2)
{
    printf("[VU] MADDA: ");
    for (int i = 0; i < 4; i++)
    {
        if (field & (1 << (3 - i)))
        {
            float temp = convert(gpr[reg1].u[i]) * convert(gpr[reg2].u[i]);
            ACC.f[i] = temp + convert(ACC.u[i]);
            printf("(%d)%f ", i, ACC.f[i]);
        }
    }
    printf("\n");
}

void VectorUnit::maddabc(uint8_t bc, uint8_t field, uint8_t source, uint8_t bc_reg)
{
    printf("[VU] MADDAbc: ");
    float op = convert(gpr[bc_reg].u[bc]);
    for (int i = 0; i < 4; i++)
    {
        if (field & (1 << (3 - i)))
        {
            float temp = op * convert(gpr[source].u[i]);
            ACC.f[i] = temp + convert(ACC.u[i]);
            printf("(%d)%f ", i, ACC.f[i]);
        }
    }
    printf("\n");
}

void VectorUnit::maddai(uint8_t field, uint8_t source)
{
    printf("[VU] MADDAi: ");
    float op = convert(I.u);
    for (int i = 0; i < 4; i++)
    {
        if (field & (1 << (3 - i)))
        {
            float temp = op * convert(gpr[source].u[i]);
            ACC.f[i] = temp + convert(ACC.u[i]);
            printf("(%d)%f ", i, ACC.f[i]);
        }
    }
    printf("\n");
}

void VectorUnit::maddbc(uint8_t bc, uint8_t field, uint8_t dest, uint8_t source, uint8_t bc_reg)
{
    printf("[VU] MADDbc: ");
    float op = convert(gpr[bc_reg].u[bc]);
    for (int i = 0; i < 4; i++)
    {
        if (field & (1 << (3 - i)))
        {
            float temp = op * convert(gpr[source].u[i]);
            set_gpr_f(dest, i, temp + ACC.f[i]);
            printf("(%d)%f ", i, gpr[dest].f[i]);
        }
    }
    printf("\n");
}

void VectorUnit::maxbc(uint8_t bc, uint8_t field, uint8_t dest, uint8_t source, uint8_t bc_reg)
{
    printf("[VU] MAXbc: ");
    float op = convert(gpr[bc_reg].u[bc]);
    for (int i = 0; i < 4; i++)
    {
        if (field & (1 << (3 - i)))
        {
            float op2 = convert(gpr[source].u[i]);
            if (op > op2)
                set_gpr_f(dest, i, op);
            else
                set_gpr_f(dest, i, op2);
            printf("(%d)%f ", i, gpr[dest].f[i]);
        }
    }
    printf("\n");
}

void VectorUnit::mfir(uint8_t field, uint8_t dest, uint8_t source)
{
    for (int i = 0; i < 4; i++)
    {
        if (field & (1 << (3 - i)))
            gpr[dest].s[i] = (int32_t)(int16_t)int_gpr[source];
    }
}

void VectorUnit::minibc(uint8_t bc, uint8_t field, uint8_t dest, uint8_t source, uint8_t bc_reg)
{
    printf("[VU] MINIbc: ");
    float op = convert(gpr[bc_reg].u[bc]);
    for (int i = 0; i < 4; i++)
    {
        if (field & (1 << (3 - i)))
        {
            float op2 = convert(gpr[source].u[i]);
            if (op < op2)
                set_gpr_f(dest, i, op);
            else
                set_gpr_f(dest, i, op2);
            printf("(%d)%f", i, gpr[dest].f[i]);
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
            set_gpr_u(dest, i, gpr[source].u[i]);
    }
    printf("\n");
}

void VectorUnit::mr32(uint8_t field, uint8_t dest, uint8_t source)
{
    printf("[VU] MR32");
    uint32_t x = gpr[source].u[0];
    if (_x(field))
        set_gpr_f(dest, 0, convert(gpr[source].u[1]));
    if (_y(field))
        set_gpr_f(dest, 1, convert(gpr[source].u[2]));
    if (_z(field))
        set_gpr_f(dest, 2, convert(gpr[source].u[3]));
    if (_w(field))
        set_gpr_f(dest, 3, convert(x));
    printf("\n");
}

void VectorUnit::msubabc(uint8_t bc, uint8_t field, uint8_t source, uint8_t bc_reg)
{
    printf("[VU] MSUBAbc: ");
    float op = convert(gpr[bc_reg].u[bc]);
    for (int i = 0; i < 4; i++)
    {
        if (field & (1 << (3 - i)))
        {
            float temp = op * convert(gpr[source].u[i]);
            ACC.f[i] = convert(ACC.u[i]) - temp;
            printf("(%d)%f ", i, ACC.f[i]);
        }
    }
    printf("\n");
}

void VectorUnit::msubai(uint8_t field, uint8_t source)
{
    printf("[VU] MSUBAi: ");
    float op = convert(I.u);
    for (int i = 0; i < 4; i++)
    {
        if (field & (1 << (3 - i)))
        {
            float temp = op * convert(gpr[source].u[i]);
            ACC.f[i] = convert(ACC.u[i]) - temp;
            printf("(%d)%f ", i, ACC.f[i]);
        }
    }
    printf("\n");
}

void VectorUnit::msubbc(uint8_t bc, uint8_t field, uint8_t dest, uint8_t source, uint8_t bc_reg)
{
    printf("[VU] MSUBbc: ");
    float op = convert(gpr[bc_reg].u[bc]);
    for (int i = 0; i < 4; i++)
    {
        if (field & (1 << (3 - i)))
        {
            float temp = op * convert(gpr[source].u[i]);
            set_gpr_f(dest, i, ACC.f[i] - temp);
            printf("(%d)%f ", i, gpr[dest].f[i]);
        }
    }
    printf("\n");
}

void VectorUnit::msubi(uint8_t field, uint8_t dest, uint8_t source)
{
    printf("[VU] MSUBi: ");
    float op = convert(I.u);
    for (int i = 0; i < 4; i++)
    {
        if (field & (1 << (3 - i)))
        {
            float temp = op * convert(gpr[source].u[i]);
            set_gpr_f(dest, i, ACC.f[i] - temp);
            printf("(%d)%f ", i, gpr[dest].f[i]);
        }
    }
    printf("\n");
}

void VectorUnit::mtir(uint8_t fsf, uint8_t dest, uint8_t source)
{
    int_gpr[dest] = (uint16_t)gpr[source].f[fsf];
}

void VectorUnit::mul(uint8_t field, uint8_t dest, uint8_t reg1, uint8_t reg2)
{
    printf("[VU] MUL: ");
    for (int i = 0; i < 4; i++)
    {
        if (field & (1 << (3 - i)))
        {
            float result = convert(gpr[reg1].u[i]) * convert(gpr[reg2].u[i]);
            set_gpr_f(dest, i, result);
            printf("(%d)%f ", i, gpr[dest].f[i]);
        }
    }
    printf("\n");
}

void VectorUnit::mula(uint8_t field, uint8_t reg1, uint8_t reg2)
{
    printf("[VU] MULA: ");
    for (int i = 0; i < 4; i++)
    {
        if (field & (1 << (3 - i)))
        {
            float temp = convert(gpr[reg1].u[i]) * convert(gpr[reg2].u[i]);
            ACC.f[i] = temp;
            printf("(%d)%f ", i, ACC.f[i]);
        }
    }
    printf("\n");
}

void VectorUnit::mulabc(uint8_t bc, uint8_t field, uint8_t source, uint8_t bc_reg)
{
    printf("[VU] MULAbc: ");
    float op = convert(gpr[bc_reg].u[bc]);
    for (int i = 0; i < 4; i++)
    {
        if (field & (1 << (3 - i)))
        {
            float temp = op * convert(gpr[source].u[i]);
            ACC.f[i] = temp;
            printf("(%d)%f ", i, ACC.f[i]);
        }
    }
    printf("\n");
}

void VectorUnit::mulai(uint8_t field, uint8_t source)
{
    printf("[VU] MULAi: ");
    float op = convert(I.u);
    for (int i = 0; i < 4; i++)
    {
        if (field & (1 << (3 - i)))
        {
            float temp = convert(gpr[source].u[i]) * op;
            ACC.f[i] = temp;
            printf("(%d)%f ", i, ACC.f[i]);
        }
    }
    printf("\n");
}

void VectorUnit::mulbc(uint8_t bc, uint8_t field, uint8_t dest, uint8_t source, uint8_t bc_reg)
{
    printf("[VU] MULbc: ");
    float op = convert(gpr[bc_reg].u[bc]);
    for (int i = 0; i < 4; i++)
    {
        if (field & (1 << (3 - i)))
        {
            float temp = op * convert(gpr[source].u[i]);
            set_gpr_f(dest, i, temp);
            printf("(%d)%f ", i, gpr[dest].f[i]);
        }
    }
    printf("\n");
}

void VectorUnit::muli(uint8_t field, uint8_t dest, uint8_t source)
{
    printf("[VU] MULi: ");
    float op = convert(I.u);
    for (int i = 0; i < 4; i++)
    {
        if (field & (1 << (3 - i)))
        {
            float temp = op * convert(gpr[source].u[i]);
            set_gpr_f(dest, i, temp);
            printf("(%d)%f ", i, gpr[dest].f[i]);
        }
    }
    printf("\n");
}

void VectorUnit::mulq(uint8_t field, uint8_t dest, uint8_t source)
{
    printf("[VU] MULq: ");
    float op = convert(Q.u);
    for (int i = 0; i < 4; i++)
    {
        if (field & (1 << (3 - i)))
        {
            float temp = op * convert(gpr[source].u[i]);
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
    set_gpr_f(dest, 0, convert(ACC.u[0]) - convert(gpr[reg1].u[1]) * convert(gpr[reg2].u[2]));
    set_gpr_f(dest, 1, convert(ACC.u[1]) - convert(gpr[reg1].u[2]) * convert(gpr[reg2].u[0]));
    set_gpr_f(dest, 2, convert(ACC.u[2]) - convert(gpr[reg1].u[0]) * convert(gpr[reg2].u[1]));
    printf("[VU] OPMSUB: %f, %f, %f\n", gpr[dest].f[0], gpr[dest].f[1], gpr[dest].f[2]);
}

/**
 * ACCx = FSy * FTz
 * ACCy = FSz * FTx
 * ACCz = FSx * FTy
 */
void VectorUnit::opmula(uint8_t reg1, uint8_t reg2)
{
    ACC.f[0] = convert(gpr[reg1].u[1]) * convert(gpr[reg2].u[2]);
    ACC.f[1] = convert(gpr[reg1].u[2]) * convert(gpr[reg2].u[0]);
    ACC.f[2] = convert(gpr[reg1].u[0]) * convert(gpr[reg2].u[1]);
    printf("[VU] OPMULA: %f, %f, %f\n", ACC.f[0], ACC.f[1], ACC.f[2]);
}

void VectorUnit::rget(uint8_t field, uint8_t dest)
{
    for (int i = 0; i < 4; i++)
    {
        if (field & (1 << (3 - i)))
        {
            set_gpr_u(dest, i, R.u);
        }
    }
    printf("[VU] RGET: %f\n", R.f);
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
            set_gpr_u(dest, i, R.u);
        }
    }
    printf("[VU] RNEXT: %f\n", R.f);
}

void VectorUnit::rsqrt(uint8_t ftf, uint8_t fsf, uint8_t reg1, uint8_t reg2)
{
    float denom = fabs(convert(gpr[reg2].u[ftf]));
    float num = convert(gpr[reg1].u[fsf]);

    status = (status & 0xFCF) | ((status & 0x30) << 6);

    if (!denom)
    {
        printf("[VU] RSQRT by zero!\n");

        if (num == 0.0)
            status |= 0x10;
        else
            status |= 0x20;
        
        if ((gpr[reg1].u[fsf] & 0x80000000) != (gpr[reg2].u[ftf] & 0x80000000))
            Q.u = 0xFF7FFFFF;
        else
            Q.u = 0x7F7FFFFF;
    }
    else
    {
        Q.f = convert(gpr[reg1].u[fsf]);
        Q.f /= sqrt(denom);
    }
    printf("[VU] RSQRT: %f\n", Q.f);
    printf("Reg1: %f\n", gpr[reg1].f[fsf]);
    printf("Reg2: %f\n", gpr[reg2].f[ftf]);
}

void VectorUnit::rxor(uint8_t fsf, uint8_t source)
{
    VU_R temp;
    temp.u = (R.u & 0x007FFFFF) | 0x3F800000;
    R.u = temp.u ^ (gpr[source].u[fsf] & 0x007FFFFF);
    printf("[VU] RXOR: %f\n", R.f);
}

void VectorUnit::sqi(uint8_t field, uint8_t source, uint8_t base)
{
    uint32_t addr = int_gpr[base] << 4;
    printf("[VU] SQI to $%08X!\n", addr);
    int_gpr[base]++;
}

void VectorUnit::vu_sqrt(uint8_t ftf, uint8_t source)
{
    Q.f = sqrt(convert(gpr[source].u[ftf]));
    printf("[VU] SQRT: %f\n", Q.f);
    printf("Source: %f\n", gpr[source].f[ftf]);
}

void VectorUnit::sub(uint8_t field, uint8_t dest, uint8_t reg1, uint8_t reg2)
{
    printf("[VU] SUB: ");
    for (int i = 0; i < 4; i++)
    {
        if (field & (1 << (3 - i)))
        {
            float result = convert(gpr[reg1].u[i]) - convert(gpr[reg2].u[i]);
            set_gpr_f(dest, i, result);
            printf("(%d)%f ", i, gpr[dest].f[i]);
        }
    }
    printf("\n");
}

void VectorUnit::subbc(uint8_t bc, uint8_t field, uint8_t dest, uint8_t source, uint8_t bc_reg)
{
    printf("[VU] SUBbc: ");
    float op = convert(gpr[bc_reg].u[bc]);
    for (int i = 0; i < 4; i++)
    {
        if (field & (1 << (3 - i)))
        {
            float temp = convert(gpr[source].u[i]) - op;
            set_gpr_f(dest, i, temp);
            printf("(%d)%f ", i, gpr[dest].f[i]);
        }
    }
    printf("\n");
}

void VectorUnit::subi(uint8_t field, uint8_t dest, uint8_t source)
{
    printf("[VU] SUBi: ");
    float op = convert(I.u);
    for (int i = 0; i < 4; i++)
    {
        if (field & (1 << (3 - i)))
        {
            float temp = convert(gpr[source].u[i]) - op;
            set_gpr_f(dest, i, temp);
            printf("(%d)%f ", i, gpr[dest].f[i]);
        }
    }
    printf("\n");
}
