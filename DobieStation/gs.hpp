#ifndef GS_HPP
#define GS_HPP
#include <cstdint>

class GraphicsSynthesizer
{
    private:
        uint32_t* local_mem;
    public:
        GraphicsSynthesizer();
        ~GraphicsSynthesizer();

        uint32_t read32(uint32_t addr);
        void write32(uint32_t addr, uint32_t value);
};

#endif // GS_HPP
