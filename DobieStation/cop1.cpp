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
    gpr[index] = value;
}

void Cop1::cvt_s_w(int dest, int source)
{
    uint32_t bark = gpr[source];
    printf("\nCVT_S_W: $%08X", bark);
}
