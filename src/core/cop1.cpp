#include <cstdio>
#include "cop1.hpp"

Cop1::Cop1()
{

}

void Cop1::reset()
{
    control.condition = false;
}

bool Cop1::get_condition()
{
    return control.condition;
}

uint32_t Cop1::get_gpr(int index)
{
    return gpr[index].u;
}

void Cop1::mtc(int index, uint32_t value)
{
    //printf("\nMTC1: %d, $%08X", index, value);
    gpr[index].u = value;
}

void Cop1::ctc(int index, uint32_t value)
{
    printf("[FPU] CTC1: $%08X (%d)", value, index);
}

void Cop1::cvt_s_w(int dest, int source)
{
    //The initial rounding mode set in FCSR is "truncate". x86 uses regular rounding
    //TODO: change this function to reflect the truncate setting
    float bark = (float)gpr[source].u;
    gpr[dest].f = bark;
    printf("\nCVT_S_W: %f", bark);
}

void Cop1::cvt_w_s(int dest, int source)
{
    gpr[dest].u = (uint32_t)gpr[source].f;
}

void Cop1::add_s(int dest, int reg1, int reg2)
{
    float op1 = gpr[reg1].f;
    float op2 = gpr[reg2].f;
    gpr[dest].f = op1 + op2;
    printf("\n[FPU] add.s: %f + %f = %f", op1, op2, gpr[dest].f);
}

void Cop1::sub_s(int dest, int reg1, int reg2)
{
    float op1 = gpr[reg1].f;
    float op2 = gpr[reg2].f;
    gpr[dest].f = op1 - op2;
    printf("\n[FPU] sub.s: %f - %f = %f", op1, op2, gpr[dest].f);
}

void Cop1::mul_s(int dest, int reg1, int reg2)
{
    float op1 = gpr[reg1].f;
    float op2 = gpr[reg2].f;
    gpr[dest].f = op1 * op2;
    printf("\n[FPU] mul.s: %f * %f = %f", op1, op2, gpr[dest].f);
}

void Cop1::div_s(int dest, int reg1, int reg2)
{
    float numerator = gpr[reg1].f;
    float denominator = gpr[reg2].f;
    gpr[dest].f = numerator / denominator;
    printf("\n[FPU] div.s: %f / %f = %f", numerator, denominator, gpr[dest].f);
}

void Cop1::mov_s(int dest, int source)
{
    gpr[dest].u = gpr[source].u;
}

void Cop1::neg_s(int dest, int source)
{
    gpr[dest].f = -gpr[source].f;
}

void Cop1::c_lt_s(int reg1, int reg2)
{
    control.condition = gpr[reg1].f < gpr[reg2].f;
}
