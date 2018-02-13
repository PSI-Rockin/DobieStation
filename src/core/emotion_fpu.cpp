#include <cstdio>
#include <cstdlib>
#include "emotioninterpreter.hpp"

#ifdef NDISASSEMBLE
#define printf(fmt, ...)(0)
#endif

void EmotionInterpreter::cop_s(Cop1& fpu, uint32_t instruction)
{
    uint8_t op = instruction & 0x3F;
    switch (op)
    {
        case 0x0:
            fpu_add(fpu, instruction);
            break;
        case 0x1:
            fpu_sub(fpu, instruction);
            break;
        case 0x2:
            fpu_mul(fpu, instruction);
            break;
        case 0x3:
            fpu_div(fpu, instruction);
            break;
        case 0x6:
            fpu_mov(fpu, instruction);
            break;
        case 0x7:
            fpu_neg(fpu, instruction);
            break;
        case 0x18:
            fpu_adda(fpu, instruction);
            break;
        case 0x24:
            fpu_cvt_w_s(fpu, instruction);
            break;
        case 0x32:
            fpu_c_eq_s(fpu, instruction);
            break;
        case 0x34:
            fpu_c_lt_s(fpu, instruction);
            break;
        default:
#ifdef NDISASSEMBLE
#undef printf(fmt, ...)(0)
#endif
            printf("Unrecognized FPU-S op $%02X", op);
            exit(1);
    }
}

#ifdef NDISASSEMBLE
#define printf(fmt, ...)(0)
#endif

void EmotionInterpreter::fpu_add(Cop1 &fpu, uint32_t instruction)
{
    uint32_t dest = (instruction >> 6) & 0x1F;
    uint32_t reg1 = (instruction >> 11) & 0x1F;
    uint32_t reg2 = (instruction >> 16) & 0x1F;
    printf("add.s f{%d}, f{%d}, f{%d}", dest, reg1, reg2);
    fpu.add_s(dest, reg1, reg2);
}

void EmotionInterpreter::fpu_sub(Cop1 &fpu, uint32_t instruction)
{
    uint32_t dest = (instruction >> 6) & 0x1F;
    uint32_t reg1 = (instruction >> 11) & 0x1F;
    uint32_t reg2 = (instruction >> 16) & 0x1F;
    printf("sub.s f{%d}, f{%d}, f{%d}", dest, reg1, reg2);
    fpu.sub_s(dest, reg1, reg2);
}

void EmotionInterpreter::fpu_mul(Cop1 &fpu, uint32_t instruction)
{
    uint32_t dest = (instruction >> 6) & 0x1F;
    uint32_t reg1 = (instruction >> 11) & 0x1F;
    uint32_t reg2 = (instruction >> 16) & 0x1F;
    printf("mul.s f{%d}, f{%d}, f{%d}", dest, reg1, reg2);
    fpu.mul_s(dest, reg1, reg2);
}

void EmotionInterpreter::fpu_div(Cop1 &fpu, uint32_t instruction)
{
    uint32_t dest = (instruction >> 6) & 0x1F;
    uint32_t reg1 = (instruction >> 11) & 0x1F;
    uint32_t reg2 = (instruction >> 16) & 0x1F;
    printf("div.s f{%d}, f{%d}, f{%d}", dest, reg1, reg2);
    fpu.div_s(dest, reg1, reg2);
}

void EmotionInterpreter::fpu_mov(Cop1 &fpu, uint32_t instruction)
{
    uint32_t dest = (instruction >> 6) & 0x1F;
    uint32_t source = (instruction >> 11) & 0x1F;
    printf("mov.s f{%d}, f{%d}", dest, source);
    fpu.mov_s(dest, source);
}

void EmotionInterpreter::fpu_neg(Cop1 &fpu, uint32_t instruction)
{
    uint32_t dest = (instruction >> 6) & 0x1F;
    uint32_t source = (instruction >> 11) & 0x1F;
    printf("neg.s f{%d}, f{%d}", dest, source);
    fpu.neg_s(dest, source);
}

void EmotionInterpreter::fpu_adda(Cop1 &fpu, uint32_t instruction)
{
    uint32_t reg1 = (instruction >> 11) & 0x1F;
    uint32_t reg2 = (instruction >> 16) & 0x1F;
    printf("adda.s f{%d}, f{%d}", reg1, reg2);
    fpu.adda_s(reg1, reg2);
}

void EmotionInterpreter::fpu_cvt_w_s(Cop1 &fpu, uint32_t instruction)
{
    uint32_t dest = (instruction >> 6) & 0x1F;
    uint32_t source = (instruction >> 11) & 0x1F;
    printf("cvt.w.s f{%d}, f{%d}", dest, source);
    fpu.cvt_w_s(dest, source);
}

void EmotionInterpreter::fpu_c_lt_s(Cop1 &fpu, uint32_t instruction)
{
    uint32_t reg1 = (instruction >> 11) & 0x1F;
    uint32_t reg2 = (instruction >> 16) & 0x1F;
    printf("c.lt.s f{%d}, f{%d}", reg1, reg2);
    fpu.c_lt_s(reg1, reg2);
}

void EmotionInterpreter::fpu_c_eq_s(Cop1 &fpu, uint32_t instruction)
{
    uint32_t reg1 = (instruction >> 11) & 0x1F;
    uint32_t reg2 = (instruction >> 16) & 0x1F;
    printf("c.eq.s f{%d}, f{%d}", reg1, reg2);
    fpu.c_eq_s(reg1, reg2);
}

void EmotionInterpreter::cop_bc1(EmotionEngine &cpu, uint32_t instruction)
{
    const static char* ops[] = {"bc1f", "bc1fl", "bc1t", "bc1tl"};
    const static bool likely[] = {false, false, true, true};
    const static bool op_true[] = {false, true, false, true};
    int32_t offset = ((int16_t)(instruction & 0xFFFF)) << 2;
    uint8_t op = (instruction >> 16) & 0x1F;
    if (op > 3)
    {
        printf("\nUnrecognized BC1 op $%02X", op);
        exit(1);
    }
    printf("%s $%08X", ops[op], cpu.get_PC() + 4 + offset);
    cpu.fpu_bc1(offset, op_true[op], likely[op]);
}

void EmotionInterpreter::cop_cvt_s_w(EmotionEngine &cpu, uint32_t instruction)
{
    int source = (instruction >> 11) & 0x1F;
    int dest = (instruction >> 6) & 0x1F;
    printf("cvt.s.w f{%d}, f{%d}", dest, source);
    cpu.fpu_cvt_s_w(dest, source);
}
