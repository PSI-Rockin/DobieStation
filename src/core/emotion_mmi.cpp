#include <cstdio>
#include <cstdlib>
#include "emotioninterpreter.hpp"

#ifdef NDISASSEMBLE
#define printf(fmt, ...)(0)
#endif

void EmotionInterpreter::mmi(EmotionEngine &cpu, uint32_t instruction)
{
    int op = instruction & 0x3F;
    switch (op)
    {
        case 0x12:
            mflo1(cpu, instruction);
            break;
        case 0x18:
            mult1(cpu, instruction);
            break;
        case 0x1B:
            divu1(cpu, instruction);
            break;
        case 0x29:
            mmi3(cpu, instruction);
            break;
        default:
            printf("Unrecognized mmi op $%02X", op);
            exit(1);
    }
}

void EmotionInterpreter::mflo1(EmotionEngine &cpu, uint32_t instruction)
{
    uint64_t dest = (instruction >> 11) & 0x1F;
    printf("mflo1 {%d}", dest);
    cpu.mflo1(dest);
}

void EmotionInterpreter::mult1(EmotionEngine &cpu, uint32_t instruction)
{
    int32_t op1 = (instruction >> 21) & 0x1F;
    int32_t op2 = (instruction >> 16) & 0x1F;
    uint64_t dest = (instruction >> 11) & 0x1F;
    printf("mult1 {%d}, {%d}, {%d}", dest, op1, op2);
    op1 = cpu.get_gpr<int32_t>(op1);
    op2 = cpu.get_gpr<int32_t>(op2);
    int64_t temp = (int64_t)op1 * op2;
    cpu.set_LO_HI((int32_t)(temp & 0xFFFFFFFF), (int32_t)(temp >> 32), true);
    cpu.mflo(dest);
}

void EmotionInterpreter::divu1(EmotionEngine &cpu, uint32_t instruction)
{
    uint32_t op1 = (instruction >> 21) & 0x1F;
    uint32_t op2 = (instruction >> 16) & 0x1F;
    printf("divu1 {%d}, {%d}", op1, op2);
    op1 = cpu.get_gpr<uint32_t>(op1);
    op2 = cpu.get_gpr<uint32_t>(op2);
    if (!op2)
    {
        printf("\ndivu1 div by zero");
        exit(1);
    }
    cpu.set_LO_HI(op1 / op2, op1 % op2, true);
}

void EmotionInterpreter::mmi3(EmotionEngine &cpu, uint32_t instruction)
{
    uint8_t op = (instruction >> 6) & 0x3F;
    switch (op)
    {
        case 0x12:
            por(cpu, instruction);
            break;
        default:
            printf("Unrecognized mmi3 op $%02X", op);
            exit(1);
    }
}

void EmotionInterpreter::por(EmotionEngine &cpu, uint32_t instruction)
{
    uint64_t op1 = (instruction >> 21) & 0x1F;
    uint64_t op2 = (instruction >> 16) & 0x1F;
    uint64_t dest = (instruction >> 11) & 0x1F;

    printf("por {%d}, {%d}, {%d}", dest, op1, op2);
    cpu.set_gpr<uint64_t>(dest, cpu.get_gpr<uint64_t>(op1) | cpu.get_gpr<uint64_t>(op2));
    cpu.set_gpr<uint64_t>(dest, cpu.get_gpr<uint64_t>(op1, 1) | cpu.get_gpr<uint64_t>(op2, 1), 1);
}
