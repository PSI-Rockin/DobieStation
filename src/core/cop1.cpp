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
    return gpr[index];
}

void Cop1::mtc(int index, uint32_t value)
{
    //printf("\nMTC1: %d, $%08X", index, value);
    gpr[index] = value;
}

void Cop1::ctc(int index, uint32_t value)
{
    printf("[FPU] CTC1: $%08X (%d)", value, index);
}

void Cop1::cvt_s_w(int dest, int source)
{
    //The initial rounding mode set in FCSR is "truncate". x86 uses regular rounding
    //TODO: change this function to reflect the truncate setting
    float bark = (float)gpr[source];
    gpr[dest] = bark;
    printf("\nSource: $%08X", gpr[source]);
    printf("\nCVT_S_W: %f", bark);
    printf("\n$%08X", gpr[dest]);
}
