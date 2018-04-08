#ifndef VU_HPP
#define VU_HPP
#include <cstdint>

union VU_GPR
{
    float f[4];
    uint32_t u[4];
};

class VectorUnit
{
    private:
        int id;

        VU_GPR gpr[32];
        uint16_t int_gpr[16];
    public:
        VectorUnit(int id);

        void set_gpr(int index, int field, float value);

        uint32_t qmfc2(int id, int field);

        uint32_t cfc(int index);
        void ctc(int index, uint32_t value);

        void iswr(uint8_t field, uint8_t source, uint8_t base);
        void sub(uint8_t field, uint8_t dest, uint8_t reg1, uint8_t reg2);
};

inline void VectorUnit::set_gpr(int index, int field, float value)
{
    if (index)
        gpr[index].f[field] = value;
}

#endif // VU_HPP
