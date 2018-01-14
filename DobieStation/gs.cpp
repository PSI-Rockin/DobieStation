#include <cstdio>
#include "gs.hpp"

GraphicsSynthesizer::GraphicsSynthesizer()
{

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
