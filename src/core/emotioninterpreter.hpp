#ifndef EMOTIONINTERPRETER_HPP
#define EMOTIONINTERPRETER_HPP
#include "emotion.hpp"

//#define NDISASSEMBLE

namespace EmotionInterpreter
{
    void interpret(EmotionEngine& cpu, uint32_t instruction);

    void special(EmotionEngine& cpu, uint32_t instruction);
    void sll(EmotionEngine& cpu, uint32_t instruction);
    void srl(EmotionEngine& cpu, uint32_t instruction);
    void sra(EmotionEngine& cpu, uint32_t instruction);
    void sllv(EmotionEngine& cpu, uint32_t instruction);
    void srav(EmotionEngine& cpu, uint32_t instruction);
    void jr(EmotionEngine& cpu, uint32_t instruction);
    void jalr(EmotionEngine& cpu, uint32_t instruction);
    void movz(EmotionEngine& cpu, uint32_t instruction);
    void movn(EmotionEngine& cpu, uint32_t instruction);
    void syscall_ee(EmotionEngine& cpu, uint32_t instruction);
    void mfhi(EmotionEngine& cpu, uint32_t instruction);
    void mflo(EmotionEngine& cpu, uint32_t instruction);
    void dsllv(EmotionEngine& cpu, uint32_t instruction);
    void dsrav(EmotionEngine& cpu, uint32_t instruction);
    void mult(EmotionEngine& cpu, uint32_t instruction);
    void div(EmotionEngine& cpu, uint32_t instruction);
    void divu(EmotionEngine& cpu, uint32_t instruction);
    void add(EmotionEngine& cpu, uint32_t instruction);
    void addu(EmotionEngine& cpu, uint32_t instruction);
    void sub(EmotionEngine& cpu, uint32_t instruction);
    void subu(EmotionEngine& cpu, uint32_t instruction);
    void and_ee(EmotionEngine& cpu, uint32_t instruction);
    void or_ee(EmotionEngine& cpu, uint32_t instruction);
    void nor(EmotionEngine& cpu, uint32_t instruction);
    void slt(EmotionEngine& cpu, uint32_t instruction);
    void sltu(EmotionEngine& cpu, uint32_t instruction);
    void daddu(EmotionEngine& cpu, uint32_t instruction);
    void dsll(EmotionEngine& cpu, uint32_t instruction);
    void dsrl(EmotionEngine& cpu, uint32_t instruction);
    void dsll32(EmotionEngine& cpu, uint32_t instruction);
    void dsrl32(EmotionEngine& cpu, uint32_t instruction);
    void dsra32(EmotionEngine& cpu, uint32_t instruction);

    void regimm(EmotionEngine& cpu, uint32_t instruction);
    void bltz(EmotionEngine& cpu, uint32_t instruction);
    void bgez(EmotionEngine& cpu, uint32_t instruction);

    void j(EmotionEngine& cpu, uint32_t instruction);
    void jal(EmotionEngine& cpu, uint32_t instruction);
    void beq(EmotionEngine& cpu, uint32_t instruction);
    void bne(EmotionEngine& cpu, uint32_t instruction);
    void blez(EmotionEngine& cpu, uint32_t instruction);
    void bgtz(EmotionEngine& cpu, uint32_t instruction);
    void addi(EmotionEngine& cpu, uint32_t instruction);
    void addiu(EmotionEngine& cpu, uint32_t instruction);
    void slti(EmotionEngine& cpu, uint32_t instruction);
    void sltiu(EmotionEngine& cpu, uint32_t instruction);
    void andi(EmotionEngine& cpu, uint32_t instruction);
    void ori(EmotionEngine& cpu, uint32_t instruction);
    void xori(EmotionEngine& cpu, uint32_t instruction);
    void lui(EmotionEngine& cpu, uint32_t instruction);
    void beql(EmotionEngine& cpu, uint32_t instruction);
    void bnel(EmotionEngine& cpu, uint32_t instruction);
    void daddiu(EmotionEngine& cpu, uint32_t instruction);
    void lq(EmotionEngine& cpu, uint32_t instruction);
    void sq(EmotionEngine& cpu, uint32_t instruction);
    void lb(EmotionEngine& cpu, uint32_t instruction);
    void lh(EmotionEngine& cpu, uint32_t instruction);
    void lw(EmotionEngine& cpu, uint32_t instruction);
    void lbu(EmotionEngine& cpu, uint32_t instruction);
    void lhu(EmotionEngine& cpu, uint32_t instruction);
    void lwu(EmotionEngine& cpu, uint32_t instruction);
    void sb(EmotionEngine& cpu, uint32_t instruction);
    void sh(EmotionEngine& cpu, uint32_t instruction);
    void sw(EmotionEngine& cpu, uint32_t instruction);
    void lwc1(EmotionEngine& cpu, uint32_t instruction);
    void ld(EmotionEngine& cpu, uint32_t instruction);
    void swc1(EmotionEngine& cpu, uint32_t instruction);
    void sd(EmotionEngine& cpu, uint32_t instruction);

    void cop(EmotionEngine& cpu, uint32_t instruction);
    void cop_mfc(EmotionEngine& cpu, uint32_t instruction);
    void cop_mtc(EmotionEngine& cpu, uint32_t instruction);
    void cop_cvt_s_w(EmotionEngine& cpu, uint32_t instruction);

    void mmi(EmotionEngine& cpu, uint32_t instruction);
    void mult1(EmotionEngine& cpu, uint32_t instruction);
    void mflo1(EmotionEngine& cpu, uint32_t instruction);
    void divu1(EmotionEngine& cpu, uint32_t instruction);

    void mmi3(EmotionEngine& cpu, uint32_t instruction);
    void por(EmotionEngine& cpu, uint32_t instruction);
};

#endif // EMOTIONINTERPRETER_HPP
