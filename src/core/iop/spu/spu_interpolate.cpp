#include "spu.hpp"
#include <cmath>
#include <cfenv>
#ifndef M_PI
    #define M_PI 3.14159265358979323846
#endif

int16_t gaussianTable[512];

// Generates the table for gaussian interpolation
// research by nocash and Ryphecha, implementation borrowed from Near
void SPU::gaussianConstructTable() {
    const int originalRounding = fegetround();
    fesetround(FE_TONEAREST);
    double table[512];

    for(uint32_t n = 0; n < 512; n++)
    {
        double k = 0.5 + n;
        double s = (sin(M_PI * k * 2.048 / 1024));
        double t = (cos(M_PI * k * 2.000 / 1023) - 1) * 0.50;
        double u = (cos(M_PI * k * 4.000 / 1023) - 1) * 0.08;
        double r = s * (t + u + 1.0) / k;
        table[511 - n] = r;
    }

    double sum = 0.0;

    for(uint32_t n = 0; n < 512; n++)
    {
      sum += table[n];
    }

    double scale = 0x7f80 * 128 / sum;

    for(uint32_t n = 0; n < 512; n++)
    {
      table[n] *= scale;
    }

    for(uint32_t phase = 0; phase < 128; phase++)
    {
        double sum = 0.0;
        sum += table[phase +   0];
        sum += table[phase + 256];
        sum += table[511 - phase];
        sum += table[255 - phase];
        double diff = (sum - 0x7f80) / 4;
        gaussianTable[phase +   0] = nearbyint(table[phase +   0] - diff);
        gaussianTable[phase + 256] = nearbyint(table[phase + 256] - diff);
        gaussianTable[511 - phase] = nearbyint(table[511 - phase] - diff);
        gaussianTable[255 - phase] = nearbyint(table[255 - phase] - diff);
    }
    fesetround(originalRounding);
}


int16_t SPU::interpolate(int voice)
{
    int16_t i = (voices[voice].counter & 0x0ff0) >> 4;

    int16_t out = 0;

    out =  ((gaussianTable[0x0FF-i] * (voices[voice].old3)) >> 15);
    out += ((gaussianTable[0x1FF-i] * (voices[voice].old2)) >> 15);
    out += ((gaussianTable[0x100+i] * (voices[voice].old1)) >> 15);
    out += ((gaussianTable[0x000+i] * (voices[voice].next_sample)) >> 15);

    return out;
}
