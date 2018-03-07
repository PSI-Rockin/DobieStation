#ifndef IOP_INTERPRETER_HPP
#define IOP_INTERPRETER_HPP
#include "iop.hpp"

namespace IOP_Interpreter
{
    void interpret(IOP& cpu, uint32_t instruction);

    void j(IOP& cpu, uint32_t instruction);
    void jal(IOP& cpu, uint32_t instruction);
    void beq(IOP& cpu, uint32_t instruction);
    void bne(IOP& cpu, uint32_t instruction);
    void blez(IOP& cpu, uint32_t instruction);
    void bgtz(IOP& cpu, uint32_t instruction);
    void addi(IOP& cpu, uint32_t instruction);
    void addiu(IOP& cpu, uint32_t instruction);
    void slti(IOP& cpu, uint32_t instruction);
    void sltiu(IOP& cpu, uint32_t instruction);
    void andi(IOP& cpu, uint32_t instruction);
    void ori(IOP& cpu, uint32_t instruction);
    void lui(IOP& cpu, uint32_t instruction);
    void lb(IOP& cpu, uint32_t instruction);
    void lh(IOP& cpu, uint32_t instruction);
    void lwl(IOP& cpu, uint32_t instruction);
    void lw(IOP& cpu, uint32_t instruction);
    void lbu(IOP& cpu, uint32_t instruction);
    void lhu(IOP& cpu, uint32_t instruction);
    void lwr(IOP& cpu, uint32_t instruction);
    void sb(IOP& cpu, uint32_t instruction);
    void sh(IOP& cpu, uint32_t instruction);
    void swl(IOP& cpu, uint32_t instruction);
    void sw(IOP& cpu, uint32_t instruction);
    void swr(IOP& cpu, uint32_t instruction);

    void special(IOP& cpu, uint32_t instruction);
    void sll(IOP& cpu, uint32_t instruction);
    void srl(IOP& cpu, uint32_t instruction);
    void sra(IOP& cpu, uint32_t instruction);
    void sllv(IOP& cpu, uint32_t instruction);
    void srlv(IOP& cpu, uint32_t instruction);
    void srav(IOP& cpu, uint32_t instruction);
    void jr(IOP& cpu, uint32_t instruction);
    void jalr(IOP& cpu, uint32_t instruction);
    void syscall(IOP& cpu, uint32_t instruction);
    void mfhi(IOP& cpu, uint32_t instruction);
    void mthi(IOP& cpu, uint32_t instruction);
    void mflo(IOP& cpu, uint32_t instruction);
    void mtlo(IOP& cpu, uint32_t instruction);
    void mult(IOP& cpu, uint32_t instruction);
    void multu(IOP& cpu, uint32_t instruction);
    void div(IOP& cpu, uint32_t instruction);
    void divu(IOP& cpu, uint32_t instruction);
    void add(IOP& cpu, uint32_t instruction);
    void addu(IOP& cpu, uint32_t instruction);
    void subu(IOP& cpu, uint32_t instruction);
    void and_cpu(IOP& cpu, uint32_t instruction);
    void or_cpu(IOP& cpu, uint32_t instruction);
    void xor_cpu(IOP& cpu, uint32_t instruction);
    void nor(IOP& cpu, uint32_t instruction);
    void slt(IOP& cpu, uint32_t instruction);
    void sltu(IOP& cpu, uint32_t instruction);

    void regimm(IOP& cpu, uint32_t instruction);
    void bltz(IOP& cpu, uint32_t instruction);
    void bgez(IOP& cpu, uint32_t instruction);

    void cop(IOP& cpu, uint32_t instruction);
    void mfc(IOP& cpu, uint32_t instruction);
    void mtc(IOP& cpu, uint32_t instruction);
    void rfe(IOP& cpu, uint32_t instruction);

    void unknown_op(const char* type, uint16_t op);
};

#endif // IOP_INTERPRETER_HPP
