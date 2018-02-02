#include <cstdio>
#include "cop1.hpp"

Cop1::Cop1()
{

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

void Cop1::cvt_s_w(int dest, int source)
{
    //The initial rounding mode set in FCSR is "truncate". x86 uses regular rounding
    //TODO: change this function to reflect the truncate setting
    float bark = (float)gpr[source];
    gpr[dest] = (uint32_t)bark;
    printf("\nCVT_S_W: %f", bark);
}
