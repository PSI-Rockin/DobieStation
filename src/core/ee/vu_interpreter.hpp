#ifndef VU_INTERPRETER_HPP
#define VU_INTERPRETER_HPP
#include "vu.hpp"

namespace VU_Interpreter
{
    void interpret(VectorUnit& vu, uint32_t upper_instr, uint32_t lower_instr);

    void upper(VectorUnit& vu, uint32_t instr);
    void addbc(VectorUnit& vu, uint32_t instr);
    void subbc(VectorUnit& vu, uint32_t instr);
    void maddbc(VectorUnit& vu, uint32_t instr);
    void maxbc(VectorUnit& vu, uint32_t instr);
    void minibc(VectorUnit& vu, uint32_t instr);
    void mulbc(VectorUnit& vu, uint32_t instr);
    void mulq(VectorUnit& vu, uint32_t instr);
    void muli(VectorUnit& vu, uint32_t instr);
    void minii(VectorUnit& vu, uint32_t instr);
    void addi(VectorUnit& vu, uint32_t instr);
    void subq(VectorUnit& vu, uint32_t instr);
    void subi(VectorUnit& vu, uint32_t instr);
    void msubi(VectorUnit& vu, uint32_t instr);
    void add(VectorUnit& vu, uint32_t instr);
    void madd(VectorUnit& vu, uint32_t instr);
    void mul(VectorUnit& vu, uint32_t instr);
    void max(VectorUnit& vu, uint32_t instr);
    void sub(VectorUnit& vu, uint32_t instr);
    void opmsub(VectorUnit& vu, uint32_t instr);

    void upper_special(VectorUnit& vu, uint32_t instr);
    void maddabc(VectorUnit& vu, uint32_t instr);
    void itof0(VectorUnit& vu, uint32_t instr);
    void itof4(VectorUnit& vu, uint32_t instr);
    void itof12(VectorUnit& vu, uint32_t instr);
    void ftoi0(VectorUnit& vu, uint32_t instr);
    void ftoi4(VectorUnit& vu, uint32_t instr);
    void ftoi12(VectorUnit& vu, uint32_t instr);
    void ftoi15(VectorUnit& vu, uint32_t instr);
    void mulabc(VectorUnit& vu, uint32_t instr);
    void abs(VectorUnit& vu, uint32_t instr);
    void mulai(VectorUnit& vu, uint32_t instr);
    void clip(VectorUnit& vu, uint32_t instr);
    void maddai(VectorUnit& vu, uint32_t instr);
    void msubai(VectorUnit& vu, uint32_t instr);
    void opmula(VectorUnit& vu, uint32_t instr);

    void lower(VectorUnit& vu, uint32_t instr);
    void lower1(VectorUnit& vu, uint32_t instr);
    void iadd(VectorUnit& vu, uint32_t instr);
    void isub(VectorUnit& vu, uint32_t instr);
    void iaddi(VectorUnit& vu, uint32_t instr);
    void iand(VectorUnit& vu, uint32_t instr);
    void ior(VectorUnit& vu, uint32_t instr);

    void lower1_special(VectorUnit& vu, uint32_t instr);
    void move(VectorUnit& vu, uint32_t instr);
    void lqi(VectorUnit& vu, uint32_t instr);
    void sqi(VectorUnit& vu, uint32_t instr);
    void div(VectorUnit& vu, uint32_t instr);
    void waitq(VectorUnit& vu, uint32_t instr);
    void mtir(VectorUnit& vu, uint32_t instr);
    void mfir(VectorUnit& vu, uint32_t instr);
    void ilwr(VectorUnit& vu, uint32_t instr);
    void mfp(VectorUnit& vu, uint32_t instr);
    void xtop(VectorUnit& vu, uint32_t instr);
    void xitop(VectorUnit& vu, uint32_t instr);
    void xgkick(VectorUnit& vu, uint32_t instr);
    void eleng(VectorUnit& vu, uint32_t instr);
    void erleng(VectorUnit& vu, uint32_t instr);
    void esqrt(VectorUnit& vu, uint32_t instr);

    void lower2(VectorUnit& vu, uint32_t instr);
    void lq(VectorUnit& vu, uint32_t instr);
    void sq(VectorUnit& vu, uint32_t instr);
    void ilw(VectorUnit& vu, uint32_t instr);
    void isw(VectorUnit& vu, uint32_t instr);
    void iaddiu(VectorUnit& vu, uint32_t instr);
    void isubiu(VectorUnit& vu, uint32_t instr);
    void fcset(VectorUnit& vu, uint32_t instr);
    void fcand(VectorUnit& vu, uint32_t instr);
    void fmand(VectorUnit& vu, uint32_t instr);
    void fcget(VectorUnit& vu, uint32_t instr);
    void b(VectorUnit& vu, uint32_t instr);
    void bal(VectorUnit& vu, uint32_t instr);
    void jr(VectorUnit& vu, uint32_t instr);
    void ibeq(VectorUnit& vu, uint32_t instr);
    void ibne(VectorUnit& vu, uint32_t instr);
    void ibltz(VectorUnit& vu, uint32_t instr);
    void ibgtz(VectorUnit& vu, uint32_t instr);
    void iblez(VectorUnit& vu, uint32_t instr);
    void ibgez(VectorUnit& vu, uint32_t instr);

    void unknown_op(const char* type, uint32_t instruction, uint16_t op);
};

#endif // VU_INTERPRETER_HPP
