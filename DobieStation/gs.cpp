#include <cstdio>
#include <cstdlib>
#include "gs.hpp"

GraphicsSynthesizer::GraphicsSynthesizer()
{
    local_mem = new uint32_t[0xFFFFF];
}

GraphicsSynthesizer::~GraphicsSynthesizer()
{
    if (local_mem)
        delete[] local_mem;
}

uint32_t GraphicsSynthesizer::read32(uint32_t addr)
{
    switch (addr)
    {
        case 0x12001000:
            return 0x00000008;
        default:
            printf("\nUnrecognized read32 from GS reg $%08X", addr);
            exit(1);
    }
}

void GraphicsSynthesizer::write32(uint32_t addr, uint32_t value)
{
    switch (addr)
    {
        case 0x12001000:
            printf("\nWrite32 to GS_CSR: $%08X", value);
            break;
        default:
            printf("\nUnrecognized write32 to GS reg $%08X: $%08X", addr, value);
    }
}
