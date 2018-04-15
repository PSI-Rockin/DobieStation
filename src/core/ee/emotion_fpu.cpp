#include <cstdio>
#include <cstdlib>
#include "emotioninterpreter.hpp"


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
        case 0x4:
            fpu_sqrt(fpu, instruction);
            break;
        case 0x5:
            fpu_abs(fpu, instruction);
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
        case 0x1C:
            fpu_madd(fpu, instruction);
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
            unknown_op("FPU", instruction, op);
    }
}

void EmotionInterpreter::fpu_add(Cop1 &fpu, uint32_t instruction)
{
    uint32_t dest = (instruction >> 6) & 0x1F;
    uint32_t reg1 = (instruction >> 11) & 0x1F;
    uint32_t reg2 = (instruction >> 16) & 0x1F;
    fpu.add_s(dest, reg1, reg2);
}

void EmotionInterpreter::fpu_sub(Cop1 &fpu, uint32_t instruction)
{
    uint32_t dest = (instruction >> 6) & 0x1F;
    uint32_t reg1 = (instruction >> 11) & 0x1F;
    uint32_t reg2 = (instruction >> 16) & 0x1F;
    fpu.sub_s(dest, reg1, reg2);
}

void EmotionInterpreter::fpu_mul(Cop1 &fpu, uint32_t instruction)
{
    uint32_t dest = (instruction >> 6) & 0x1F;
    uint32_t reg1 = (instruction >> 11) & 0x1F;
    uint32_t reg2 = (instruction >> 16) & 0x1F;
    fpu.mul_s(dest, reg1, reg2);
}

void EmotionInterpreter::fpu_div(Cop1 &fpu, uint32_t instruction)
{
    uint32_t dest = (instruction >> 6) & 0x1F;
    uint32_t reg1 = (instruction >> 11) & 0x1F;
    uint32_t reg2 = (instruction >> 16) & 0x1F;
    fpu.div_s(dest, reg1, reg2);
}

void EmotionInterpreter::fpu_sqrt(Cop1 &fpu, uint32_t instruction)
{
    uint32_t dest = (instruction >> 6) & 0x1F;
    uint32_t source = (instruction >> 11) & 0x1F;
    fpu.sqrt_s(dest, source);
}

void EmotionInterpreter::fpu_abs(Cop1 &fpu, uint32_t instruction)
{
    uint32_t dest = (instruction >> 6) & 0x1F;
    uint32_t source = (instruction >> 11) & 0x1F;
    fpu.abs_s(dest, source);
}

void EmotionInterpreter::fpu_mov(Cop1 &fpu, uint32_t instruction)
{
    uint32_t dest = (instruction >> 6) & 0x1F;
    uint32_t source = (instruction >> 11) & 0x1F;
    fpu.mov_s(dest, source);
}

void EmotionInterpreter::fpu_neg(Cop1 &fpu, uint32_t instruction)
{
    uint32_t dest = (instruction >> 6) & 0x1F;
    uint32_t source = (instruction >> 11) & 0x1F;
    fpu.neg_s(dest, source);
}

void EmotionInterpreter::fpu_adda(Cop1 &fpu, uint32_t instruction)
{
    uint32_t reg1 = (instruction >> 11) & 0x1F;
    uint32_t reg2 = (instruction >> 16) & 0x1F;
    fpu.adda_s(reg1, reg2);
}

void EmotionInterpreter::fpu_madd(Cop1 &fpu, uint32_t instruction)
{
    uint32_t dest = (instruction >> 6) & 0x1F;
    uint32_t reg1 = (instruction >> 11) & 0x1F;
    uint32_t reg2 = (instruction >> 16) & 0x1F;
    fpu.madd_s(dest, reg1, reg2);
}

void EmotionInterpreter::fpu_cvt_w_s(Cop1 &fpu, uint32_t instruction)
{
    uint32_t dest = (instruction >> 6) & 0x1F;
    uint32_t source = (instruction >> 11) & 0x1F;
    fpu.cvt_w_s(dest, source);
}

void EmotionInterpreter::fpu_c_lt_s(Cop1 &fpu, uint32_t instruction)
{
    uint32_t reg1 = (instruction >> 11) & 0x1F;
    uint32_t reg2 = (instruction >> 16) & 0x1F;
    fpu.c_lt_s(reg1, reg2);
}

void EmotionInterpreter::fpu_c_eq_s(Cop1 &fpu, uint32_t instruction)
{
    uint32_t reg1 = (instruction >> 11) & 0x1F;
    uint32_t reg2 = (instruction >> 16) & 0x1F;
    fpu.c_eq_s(reg1, reg2);
}

void EmotionInterpreter::cop_bc1(EmotionEngine &cpu, uint32_t instruction)
{
    const static char* ops[] = {"bc1f", "bc1t", "bc1fl", "bc1tl"};
    const static bool likely[] = {false, false, true, true};
    const static bool op_true[] = {false, true, false, true};
    int32_t offset = ((int16_t)(instruction & 0xFFFF)) << 2;
    uint8_t op = (instruction >> 16) & 0x1F;
    if (op > 3)
    {
        unknown_op("bc1", instruction, op);
    }
    cpu.fpu_bc1(offset, op_true[op], likely[op]);
}

void EmotionInterpreter::cop_cvt_s_w(EmotionEngine &cpu, uint32_t instruction)
{
    int source = (instruction >> 11) & 0x1F;
    int dest = (instruction >> 6) & 0x1F;
    cpu.fpu_cvt_s_w(dest, source);
}
