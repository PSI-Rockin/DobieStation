#ifndef VU_HPP
#define VU_HPP
#include <cstdint>
#include <cstdio>

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
        uint32_t status;
        uint32_t clip_flags;
        VU_R R;
        VU_R I;
        VU_R Q;

        void advance_r();
        void print_vectors(uint8_t a, uint8_t b);
        float convert();
    public:
        VectorUnit(int id);

        void reset();

        static float convert(uint32_t value);

        template <typename T> void write_instr(uint32_t addr, T data);

        uint32_t get_gpr_u(int index, int field);
        void set_gpr_f(int index, int field, float value);
        void set_gpr_u(int index, int field, uint32_t value);
        void set_int(int index, uint16_t value);

        uint32_t qmfc2(int id, int field);

        uint32_t cfc(int index);
        void ctc(int index, uint32_t value);

        void abs(uint8_t field, uint8_t dest, uint8_t source);
        void add(uint8_t field, uint8_t dest, uint8_t reg1, uint8_t reg2);
        void adda(uint8_t field, uint8_t reg1, uint8_t reg2);
        void addabc(uint8_t bc, uint8_t field, uint8_t source, uint8_t bc_reg);
        void addbc(uint8_t bc, uint8_t field, uint8_t dest, uint8_t source, uint8_t bc_reg);
        void addq(uint8_t field, uint8_t dest, uint8_t source);
        void clip(uint8_t reg1, uint8_t reg2);
        void div(uint8_t ftf, uint8_t fsf, uint8_t reg1, uint8_t reg2);
        void ftoi0(uint8_t field, uint8_t dest, uint8_t source);
        void ftoi4(uint8_t field, uint8_t dest, uint8_t source);
        void ftoi12(uint8_t field, uint8_t dest, uint8_t source);
        void ftoi15(uint8_t field, uint8_t dest, uint8_t source);
        void iadd(uint8_t dest, uint8_t reg1, uint8_t reg2);
        void iaddi(uint8_t dest, uint8_t source, int8_t imm);
        void iand(uint8_t dest, uint8_t reg1, uint8_t reg2);
        void iswr(uint8_t field, uint8_t source, uint8_t base);
        void itof0(uint8_t field, uint8_t dest, uint8_t source);
        void itof4(uint8_t field, uint8_t dest, uint8_t source);
        void itof12(uint8_t field, uint8_t dest, uint8_t source);
        void madd(uint8_t field, uint8_t dest, uint8_t reg1, uint8_t reg2);
        void madda(uint8_t field, uint8_t reg1, uint8_t reg2);
        void maddai(uint8_t field, uint8_t source);
        void maddabc(uint8_t bc, uint8_t field, uint8_t source, uint8_t bc_reg);
        void maddbc(uint8_t bc, uint8_t field, uint8_t dest, uint8_t source, uint8_t bc_reg);
        void maxbc(uint8_t bc, uint8_t field, uint8_t dest, uint8_t source, uint8_t bc_reg);
        void mfir(uint8_t field, uint8_t dest, uint8_t source);
        void minibc(uint8_t bc, uint8_t field, uint8_t dest, uint8_t source, uint8_t bc_reg);
        void move(uint8_t field, uint8_t dest, uint8_t source);
        void mr32(uint8_t field, uint8_t dest, uint8_t source);
        void msubabc(uint8_t bc, uint8_t field, uint8_t source, uint8_t bc_reg);
        void msubai(uint8_t field, uint8_t source);
        void msubbc(uint8_t bc, uint8_t field, uint8_t dest, uint8_t source, uint8_t bc_reg);
        void msubi(uint8_t field, uint8_t dest, uint8_t source);
        void mtir(uint8_t fsf, uint8_t dest, uint8_t source);
        void mul(uint8_t field, uint8_t dest, uint8_t reg1, uint8_t reg2);
        void mula(uint8_t field, uint8_t reg1, uint8_t reg2);
        void mulabc(uint8_t bc, uint8_t field, uint8_t source, uint8_t bc_reg);
        void mulai(uint8_t field, uint8_t source);
        void mulbc(uint8_t bc, uint8_t field, uint8_t dest, uint8_t source, uint8_t bc_reg);
        void muli(uint8_t field, uint8_t dest, uint8_t source);
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
        void subi(uint8_t field, uint8_t dest, uint8_t source);
        void subq(uint8_t field, uint8_t dest, uint8_t source);
};

template <typename T>
inline void VectorUnit::write_instr(uint32_t addr, T data)
{
    printf("[VU] Write instr mem $%08X: $%08X\n", addr, data);
    *(T*)&instr_mem[addr & 0x3FFF] = data;
}

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
