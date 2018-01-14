#ifndef GS_HPP
#define GS_HPP
#include <cstdint>

class GraphicsSynthesizer
{
    public:
        GraphicsSynthesizer();

        void write32(uint32_t addr, uint32_t value);
};

#endif // GS_HPP
