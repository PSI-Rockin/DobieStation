#include <cmath>
#include <cstdio>
#include "cop1.hpp"

#define printf(fmt, ...)(0)

Cop1::Cop1()
{

}

//Returns a floating point value that may be changed to accomodate the FPU's non-compliance to the IEEE 754 standard.
float Cop1::convert(float value) const noexcept
{
    uint32_t test;
    memcpy(&test, &value, sizeof(float));
    switch(test & 0x7F800000)
    {
        case 0x0:
            test &= 0x80000000;
            break;
        case 0x7F800000:
            test = (test & 0x80000000)|0x7F7FFFFF;
            break;
        default:
            break;
    }
    memcpy(&value, &test, sizeof(float));
    return value;
}

void Cop1::check_overflow(float& dest, bool set_flags) noexcept
{
    uint32_t test;
    memcpy(&test, &dest, sizeof(float));
    if ((test & ~0x80000000) == 0x7F800000)
    {
        printf("[FPU] Overflow Dest = %x\n", dest);
        test = (test & 0x80000000) | 0x7F7FFFFF;
       
        if (set_flags)
        {
            control.so = true;
            control.o = true;
        }
        memcpy(&dest, &test, sizeof(float));
    }
}

void Cop1::check_underflow(float& dest, bool set_flags) noexcept
{
    uint32_t test;
    memcpy(&test, &dest, sizeof(float));
    if ((test & 0x7F800000) == 0 && (test & 0x7FFFFF) != 0)
    {
        printf("[FPU] Underflow Dest = %x\n", test);
        test &= 0x80000000;
        
        if (set_flags)
        {
            control.su = true;
            control.u = true;
        }
        memcpy(&dest, &test, sizeof(float));
    }
}

void Cop1::reset() noexcept
{
    control.su = false;
    control.so = false;
    control.sd = false;
    control.si = false;
    control.u = false;
    control.o = false;
    control.d = false;
    control.i = false;
    control.condition = false;
}

bool Cop1::get_condition() const noexcept
{
    return control.condition;
}

uint32_t Cop1::get_gpr(int index) const noexcept
{
    uint32_t result;
    memcpy(&result, &gpr[index], sizeof(float));
    return result;
}

void Cop1::mtc(int index, uint32_t value) noexcept
{
    printf("[FPU] MTC1: %d, $%08X\n", index, value);
    memcpy(&gpr[index], &value, sizeof(float));
}

uint32_t Cop1::cfc(int index) noexcept
{
    switch (index)
    {
        case 0:
            return 0x2E00;
        case 31:
            uint32_t reg;
            reg = control.su << 3;
            reg |= control.so << 4;
            reg |= control.sd << 5;
            reg |= control.si << 6;
            reg |= control.u << 14;
            reg |= control.o << 15;
            reg |= control.d << 16;
            reg |= control.i << 17;
            reg |= control.condition << 23;
            printf("[FPU] Read Control Flag %x\n", control.condition);
            return reg;
        default:
            return 0;
    }
}

void Cop1::ctc(int index, uint32_t value) noexcept
{
    printf("[FPU] CTC1: $%08X (%d)\n", value, index);
    switch (index)
    {
        case 31:
            control.su = value & (1 << 3);
            control.so = value & (1 << 4);
            control.sd = value & (1 << 5);
            control.si = value & (1 << 6);
            control.u = value & (1 << 14);
            control.o = value & (1 << 15);
            control.d = value & (1 << 16);
            control.i = value & (1 << 17);
            control.condition = value & (1 << 23);
            break;
    }
}

void Cop1::cvt_s_w(int dest, int source) noexcept
{
    int32_t fixed;
    memcpy(&fixed, &gpr[source], sizeof(float));
    gpr[dest] = convert(static_cast<float>(fixed));
    printf("[FPU] CVT_S_W: %f(%d)\n", gpr[dest], dest);
}

void Cop1::cvt_w_s(int dest, int source) noexcept
{
    uint32_t usource;
    uint32_t udest;
    memcpy(&usource, &gpr[source], sizeof(float));
    if ((usource & 0x7F800000) <= 0x4E800000)
        udest = static_cast<uint32_t>(gpr[source]);
    else if ((usource & 0x80000000) == 0)
        udest = 0x7FFFFFFF;
    else
        udest = 0x80000000;
    memcpy(&gpr[dest], &udest, sizeof(float));
    printf("[FPU] CVT_W_S: $%08X(%d)\n", gpr[dest], dest);
}

void Cop1::add_s(int dest, int reg1, int reg2) noexcept
{
    float op1 = convert(gpr[reg1]);
    float op2 = convert(gpr[reg2]);
    gpr[dest] = op1 + op2;

    check_overflow(gpr[dest], true);
    check_underflow(gpr[dest], true);

    printf("[FPU] add.s: %f(%d) + %f(%d) = %f(%d)\n", op1, reg1, op2, reg2, gpr[dest], dest);
}

void Cop1::sub_s(int dest, int reg1, int reg2) noexcept
{
    float op1 = convert(gpr[reg1]);
    float op2 = convert(gpr[reg2]);
    gpr[dest] = op1 - op2;
        
    check_overflow(gpr[dest], true);
    check_underflow(gpr[dest], true);

    printf("[FPU] sub.s: %f(%d) - %f(%d) = %f(%d)\n", op1, reg1, op2, reg2, gpr[dest], dest);
}

void Cop1::mul_s(int dest, int reg1, int reg2) noexcept
{
    float op1 = convert(gpr[reg1]);
    float op2 = convert(gpr[reg2]);
    gpr[dest] = op1 * op2;

    check_overflow(gpr[dest], true);
    check_underflow(gpr[dest], true);
    printf("[FPU] mul.s: %f(%d) * %f(%d) = %f(%d)\n", op1, reg1, op2, reg2, gpr[dest], dest);
}

void Cop1::div_s(int dest, int reg1, int reg2) noexcept
{
    uint32_t numerator;
    uint32_t denominator;
    memcpy(&numerator, &gpr[reg1], sizeof(float));
    memcpy(&denominator, &gpr[reg2], sizeof(float));
    if ((denominator & 0x7F800000) == 0)
    {
        if ((numerator & 0x7F800000) == 0)
        {
            printf("[FPU] DIV 0/0 Fs = %x Ft = %x\n", numerator, denominator);
            control.i = true;
            control.si = true;
        }
        else
        {
            printf("[FPU] DIV #/0 Fs = %x Ft = %x\n", numerator, denominator);
            control.d = true;
            control.sd = true;
        }
        uint32_t result = ((numerator ^ denominator) & 0x80000000) | 0x7F7FFFFF;
        memcpy(&gpr[dest], &result, sizeof(float));
    }
    else
        gpr[dest] = convert(gpr[reg1]) / convert(gpr[reg2]);

    check_overflow(gpr[dest], false);
    check_underflow(gpr[dest], false);

    printf("[FPU] div.s: %f(%d) / %f(%d) = %f(%d)\n", convert(numerator), reg1, convert(denominator), reg2, gpr[dest], dest);
}

void Cop1::sqrt_s(int dest, int source) noexcept
{
    uint32_t usource;
    uint32_t udest;
    memcpy(&usource, &gpr[source], sizeof(float));
    memcpy(&udest, &gpr[dest], sizeof(float));
    if ((usource & 0x7F800000) == 0)
    {
        udest = usource & 0x80000000;
        memcpy(&gpr[dest], &udest, sizeof(float));
    }
    else
    {
        if (usource & 0x80000000)
        {
            printf("[FPU] Negative Sqrt Fs = %x\n", usource);
            control.i = true;
            control.si = true;
        }

        gpr[dest] = sqrtf(fabsf(convert(gpr[source])));
    }

    control.d = false;
    printf("[FPU] sqrt.s: %f(%d) = %f(%d)\n", gpr[source], source, gpr[dest], dest);
}

void Cop1::abs_s(int dest, int source) noexcept
{
    gpr[dest] = fabsf(gpr[source]);

    control.u = false;
    control.o = false;

    printf("[FPU] abs.s: %f = -%f\n", gpr[source], gpr[dest]);
}

void Cop1::mov_s(int dest, int source) noexcept
{
    gpr[dest] = gpr[source];
    printf("[FPU] mov.s: %d = %d", source, dest);
}

void Cop1::neg_s(int dest, int source) noexcept
{
    gpr[dest] = -gpr[source];

    control.u = false;
    control.o = false;

    printf("[FPU] neg.s: %f = -%f\n", gpr[source], gpr[dest]);
}

void Cop1::rsqrt_s(int dest, int reg1, int reg2) noexcept
{
    uint32_t udest;
    uint32_t ureg1;
    uint32_t ureg2;
    memcpy(&udest, &gpr[dest], sizeof(float));
    memcpy(&ureg1, &gpr[reg1], sizeof(float));
    memcpy(&ureg2, &gpr[reg2], sizeof(float));

    if ((ureg2 & 0x7F800000) == 0)
    {
        udest = (ureg1 ^ ureg2) & 0x80000000;
        memcpy(&gpr[dest], &udest, sizeof(float));
    }
    else
    {
        if (ureg2 & 0x80000000)
        {
            printf("[FPU] Negative RSqrt Fs = %x\n", ureg2);
            control.i = true;
            control.si = true;
        }
        float temp = sqrtf(fabsf(convert(gpr[reg2])));
        gpr[dest] = convert(gpr[reg1]) / convert(temp);
    }

    control.d = false;
    check_overflow(gpr[dest], false);
    check_underflow(gpr[dest], false);
    printf("[FPU] rsqrt.s: %f(%d) = %f(%d)\n", gpr[source], source, gpr[dest], dest);
}

void Cop1::adda_s(int reg1, int reg2) noexcept
{
    float op1 = convert(gpr[reg1]);
    float op2 = convert(gpr[reg2]);
    accumulator = op1 + op2;

    check_overflow(accumulator, true);
    check_underflow(accumulator, true);
    printf("[FPU] adda.s: %f + %f = %f\n", op1, op2, accumulator);
}

void Cop1::suba_s(int reg1, int reg2) noexcept
{
    float op1 = convert(gpr[reg1]);
    float op2 = convert(gpr[reg2]);
    accumulator = op1 - op2;

    check_overflow(accumulator, true);
    check_underflow(accumulator, true);

    printf("[FPU] suba.s: %f - %f = %f\n", op1, op2, accumulator);
}

void Cop1::mula_s(int reg1, int reg2) noexcept
{
    float op1 = convert(gpr[reg1]);
    float op2 = convert(gpr[reg2]);
    accumulator = op1 * op2;

    check_overflow(accumulator, true);
    check_underflow(accumulator, true);
    printf("[FPU] mula.s: %f * %f = %f\n", op1, op2, accumulator);
}

void Cop1::madd_s(int dest, int reg1, int reg2) noexcept
{
    float op1 = convert(gpr[reg1]);
    float op2 = convert(gpr[reg2]);
    float acc = convert(accumulator);
    gpr[dest] = acc + (op1 * op2);
    
    check_overflow(gpr[dest], true);
    check_underflow(gpr[dest], true);
    printf("[FPU] madd.s: %f + %f * %f = %f\n", acc, op1, op2, gpr[dest]);
}

void Cop1::msub_s(int dest, int reg1, int reg2) noexcept
{
    float op1 = convert(gpr[reg1]);
    float op2 = convert(gpr[reg2]);
    float acc = convert(accumulator);
    gpr[dest] = acc - (op1 * op2);

    check_overflow(gpr[dest], true);
    check_underflow(gpr[dest], true);

    printf("[FPU] msub.s: %f - %f * %f = %f\n", acc, op1, op2, gpr[dest]);
}

void Cop1::madda_s(int reg1, int reg2) noexcept
{
    float op1 = convert(gpr[reg1]);
    float op2 = convert(gpr[reg2]);
    accumulator += op1 * op2;

    check_overflow(accumulator, true);
    check_underflow(accumulator, true);
    printf("[FPU] madda.s: %f + (%f * %f) = %f", acc, op1, op2, accumulator);
}

void Cop1::msuba_s(int reg1, int reg2) noexcept
{
    float op1 = convert(gpr[reg1]);
    float op2 = convert(gpr[reg2]);
    accumulator -= op1 * op2;

    check_overflow(accumulator, true);
    check_underflow(accumulator, true);
    printf("[FPU] msuba.s: %f + (%f * %f) = %f", acc, op1, op2, accumulator);
}

void Cop1::max_s(int dest, int reg1, int reg2) noexcept
{
    printf("[FPU] max.s: %f(%d) >= %f(%d)", gpr[reg1], reg1, gpr[reg2], reg2);
    gpr[dest] = fmaxf(gpr[reg1], gpr[reg2]);

    control.u = false;
    control.o = false;
    printf(" Result = %f(%d)\n", gpr[dest], dest);
}

void Cop1::min_s(int dest, int reg1, int reg2) noexcept
{
    printf("[FPU] min.s: %f(%d) <= %f(%d)", gpr[reg1], reg1, gpr[reg2], reg2);
    gpr[dest] = fminf(gpr[reg1], gpr[reg2]);

    control.u = false;
    control.o = false;
    printf(" Result = %f(%d)\n", gpr[dest], dest);
}

void Cop1::c_f_s() noexcept
{
    control.condition = false;
    printf("[FPU] c.f.s\n");
}

void Cop1::c_lt_s(int reg1, int reg2) noexcept
{
    control.condition = convert(gpr[reg1]) < convert(gpr[reg2]);
    printf("[FPU] c.lt.s: %f(%d), %f(%d)\n", gpr[reg1], reg1, gpr[reg2], reg2);
}

void Cop1::c_eq_s(int reg1, int reg2) noexcept
{
    control.condition = convert(gpr[reg1]) == convert(gpr[reg2]);
    printf("[FPU] c.eq.s: %f(%d), %f(%d)\n", gpr[reg1], reg1, gpr[reg2], reg2);
}

void Cop1::c_le_s(int reg1, int reg2) noexcept
{
    control.condition = convert(gpr[reg1]) <= convert(gpr[reg2]);
    printf("[FPU] c.le.s: %f(%d), %f(%d)\n", gpr[reg1], reg1, gpr[reg2], reg2);
}
