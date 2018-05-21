#ifndef VU_HPP
#define VU_HPP
#include <cstdint>

#include "../int128.hpp"

union VU_R
{
    float f;
    uint32_t u;
};

union VU_GPR
{
    float f[4];
    uint32_t u[4];
    int32_t s[4];
};

class VectorUnit
{
    private:
        int id;

        uint8_t instr_mem[1024 * 16];
        uint8_t data_mem[1024 * 16];

        //GPR
        VU_GPR gpr[32];
        uint16_t int_gpr[16];

        //Control registers
        VU_GPR ACC;
        VU_R R;
        float Q;

        void advance_r();
    public:
        VectorUnit(int id);

        void write128(uint32_t addr, uint128_t data);

        uint32_t get_gpr_u(int index, int field);
        void set_gpr_f(int index, int field, float value);
        void set_gpr_u(int index, int field, uint32_t value);
        void set_int(int index, uint16_t value);

        uint32_t qmfc2(int id, int field);

        uint32_t cfc(int index);
        void ctc(int index, uint32_t value);

        void add(uint8_t field, uint8_t dest, uint8_t reg1, uint8_t reg2);
        void addbc(uint8_t bc, uint8_t field, uint8_t dest, uint8_t source, uint8_t bc_reg);
        void addq(uint8_t field, uint8_t dest, uint8_t source);
        void div(uint8_t ftf, uint8_t fsf, uint8_t reg1, uint8_t reg2);
        void ftoi4(uint8_t field, uint8_t dest, uint8_t source);
        void iadd(uint8_t dest, uint8_t reg1, uint8_t reg2);
        void iswr(uint8_t field, uint8_t source, uint8_t base);
        void maddbc(uint8_t bc, uint8_t field, uint8_t dest, uint8_t source, uint8_t bc_reg);
        void maddabc(uint8_t bc, uint8_t field, uint8_t source, uint8_t bc_reg);
        void move(uint8_t field, uint8_t dest, uint8_t source);
        void mr32(uint8_t field, uint8_t dest, uint8_t source);
        void mul(uint8_t field, uint8_t dest, uint8_t reg1, uint8_t reg2);
        void mulabc(uint8_t bc, uint8_t field, uint8_t source, uint8_t bc_reg);
        void mulbc(uint8_t bc, uint8_t field, uint8_t dest, uint8_t source, uint8_t bc_reg);
        void mulq(uint8_t field, uint8_t dest, uint8_t source);
        void opmsub(uint8_t dest, uint8_t reg1, uint8_t reg2);
        void opmula(uint8_t reg1, uint8_t reg2);
        void rinit(uint8_t fsf, uint8_t source);
        void rget(uint8_t field, uint8_t dest);
        void rnext(uint8_t field, uint8_t dest);
        void rsqrt(uint8_t ftf, uint8_t fsf, uint8_t reg1, uint8_t reg2);
        void rxor(uint8_t fsf, uint8_t source);
        void sqi(uint8_t field, uint8_t source, uint8_t base);
        void vu_sqrt(uint8_t ftf, uint8_t source);
        void sub(uint8_t field, uint8_t dest, uint8_t reg1, uint8_t reg2);
        void subbc(uint8_t bc, uint8_t field, uint8_t dest, uint8_t source, uint8_t bc_reg);
};

inline uint32_t VectorUnit::get_gpr_u(int index, int field)
{
    return gpr[index].u[field];
}

inline void VectorUnit::set_gpr_f(int index, int field, float value)
{
    if (index)
        gpr[index].f[field] = value;
}

inline void VectorUnit::set_gpr_u(int index, int field, uint32_t value)
{
    if (index)
        gpr[index].u[field] = value;
}

inline void VectorUnit::set_int(int index, uint16_t value)
{
    if (index)
        int_gpr[index] = value;
}

#endif // VU_HPP
