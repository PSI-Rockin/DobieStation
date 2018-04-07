#include <cstdio>
#include <cstdlib>
#include "emotioninterpreter.hpp"

void EmotionInterpreter::mmi(EmotionEngine &cpu, uint32_t instruction)
{
    int op = instruction & 0x3F;
    switch (op)
    {
        case 0x04:
            plzcw(cpu, instruction);
            break;
        case 0x08:
            mmi0(cpu, instruction);
            break;
        case 0x09:
            mmi2(cpu, instruction);
            break;
        case 0x10:
            mfhi1(cpu, instruction);
            break;
        case 0x11:
            mthi1(cpu, instruction);
            break;
        case 0x12:
            mflo1(cpu, instruction);
            break;
        case 0x13:
            mtlo1(cpu, instruction);
            break;
        case 0x18:
            mult1(cpu, instruction);
            break;
        case 0x1A:
            div1(cpu, instruction);
            break;
        case 0x1B:
            divu1(cpu, instruction);
            break;
        case 0x28:
            mmi1(cpu, instruction);
            break;
        case 0x29:
            mmi3(cpu, instruction);
            break;
        default:
            unknown_op("mmi", instruction, op);
    }
}

/**
 * Parallel Leading Zero or One Count Word
 * Split 64-bit RS into two words, and count the number of leading bits the same as the highest-order bit
 * Store the two results in each word of RD
 * If at == 0x0F00F000_000FFFFF
 * Then after plzcw at, at
 * at == 0x00000003_0x0000000B
 */
void EmotionInterpreter::plzcw(EmotionEngine &cpu, uint32_t instruction)
{
    uint64_t dest = (instruction >> 11) & 0x1F;
    uint64_t reg = (instruction >> 21) & 0x1F;

    uint32_t words[2];
    words[0] = cpu.get_gpr<uint32_t>(reg);
    words[1] = cpu.get_gpr<uint32_t>(reg, 1);

    for (int i = 0; i < 2; i++)
    {
        bool high_bit = words[i] & (1 << 31);
        uint8_t bits = 0;
        for (int j = 30; j >= 0; j--)
        {
            if ((words[i] & (1 << j)) == high_bit)
                bits++;
            else
                break;
        }
        words[i] = bits;
    }

    cpu.set_gpr<uint32_t>(dest, words[0]);
    cpu.set_gpr<uint32_t>(dest, words[1], 1);
}

void EmotionInterpreter::mmi0(EmotionEngine &cpu, uint32_t instruction)
{
    uint8_t op = (instruction >> 6) & 0x1F;
    switch (op)
    {
        case 0x09:
            psubb(cpu, instruction);
            break;
        case 0x12:
            pcgtb(cpu, instruction);
            break;
        default:
            unknown_op("mmi0", instruction, op);
    }
}

/**
 * Parallel Subtract Byte
 * Splits the 128-bit registers RS and RT into sixteen bytes each, subtracts them, and places the 128-bit result in RD.
 * NOTE: This function assumes little-endianness!
 */
void EmotionInterpreter::psubb(EmotionEngine &cpu, uint32_t instruction)
{
    uint64_t reg1 = (instruction >> 21) & 0x1F;
    uint64_t reg2 = (instruction >> 16) & 0x1F;
    uint64_t dest = (instruction >> 11) & 0x1F;

    for (int i = 0; i < 16; i++)
    {
        uint8_t byte = cpu.get_gpr<uint8_t>(reg1, i);
        byte -= cpu.get_gpr<uint8_t>(reg2, i);
        cpu.set_gpr<uint8_t>(dest, byte, i);
    }
}

/**
 * Parallel Compare for Greater Than Byte
 * Compares the 16 bytes in RS to RT. For each byte, if RS > RT, 0xFF is stored in RD's byte. Else, 0x0 is stored.
 */
void EmotionInterpreter::pcgtb(EmotionEngine &cpu, uint32_t instruction)
{
    uint64_t reg1 = (instruction >> 21) & 0x1F;
    uint64_t reg2 = (instruction >> 16) & 0x1F;
    uint64_t dest = (instruction >> 11) & 0x1F;
    for (int i = 0; i < 16; i++)
    {
        uint8_t byte1 = cpu.get_gpr<uint8_t>(reg1, i);
        uint8_t byte2 = cpu.get_gpr<uint8_t>(reg2, i);
        cpu.set_gpr<uint8_t>(dest, (byte1 > byte2) * 0xFF, i);
    }
}

void EmotionInterpreter::mmi1(EmotionEngine &cpu, uint32_t instruction)
{
    uint8_t op = (instruction >> 6) & 0x1F;
    switch (op)
    {
        case 0x10:
            padduw(cpu, instruction);
            break;
        default:
            return unknown_op("mmi1", instruction, op);
    }
}

/**
 * Parallel Add with Unsigned Saturation Word
 * Splits the 128-bit registers RS and RT into four words each and adds them together.
 * If the result is greater than 0xFFFFFFFF, it is saturated to that value.
 */
void EmotionInterpreter::padduw(EmotionEngine &cpu, uint32_t instruction)
{
    uint64_t reg1 = (instruction >> 21) & 0x1F;
    uint64_t reg2 = (instruction >> 16) & 0x1F;
    uint64_t dest = (instruction >> 11) & 0x1F;

    for (int i = 0; i < 4; i++)
    {
        uint64_t result = cpu.get_gpr<uint32_t>(reg1, i);
        result += cpu.get_gpr<uint32_t>(reg2, i);
        if (result > 0xFFFFFFFF)
            result = 0xFFFFFFFF;
        cpu.set_gpr<uint32_t>(dest, (uint32_t)result, i);
    }
}

void EmotionInterpreter::mmi2(EmotionEngine &cpu, uint32_t instruction)
{
    uint8_t op = (instruction >> 6) & 0x1F;
    switch (op)
    {
        case 0x0E:
            pcpyld(cpu, instruction);
            break;
        case 0x12:
            pand(cpu, instruction);
            break;
        default:
            unknown_op("mmi2", instruction, op);
    }
}

/**
 * Parallel Copy from Lower Doubleword
 * The low 64 bits of RS and RT get copied to the high 64 bits and low 64 bits of RD, respectively.
 */
void EmotionInterpreter::pcpyld(EmotionEngine &cpu, uint32_t instruction)
{
    uint64_t reg1 = (instruction >> 21) & 0x1F;
    uint64_t reg2 = (instruction >> 16) & 0x1F;
    uint64_t dest = (instruction >> 11) & 0x1F;

    uint64_t high = cpu.get_gpr<uint64_t>(reg1);
    uint64_t low = cpu.get_gpr<uint64_t>(reg2);

    cpu.set_gpr<uint64_t>(dest, low);
    cpu.set_gpr<uint64_t>(dest, high, 1);
}

void EmotionInterpreter::pand(EmotionEngine &cpu, uint32_t instruction)
{
    uint64_t op1 = (instruction >> 21) & 0x1F;
    uint64_t op2 = (instruction >> 16) & 0x1F;
    uint64_t dest = (instruction >> 11) & 0x1F;

    cpu.set_gpr<uint64_t>(dest, cpu.get_gpr<uint64_t>(op1) & cpu.get_gpr<uint64_t>(op2));
    cpu.set_gpr<uint64_t>(dest, cpu.get_gpr<uint64_t>(op1, 1) & cpu.get_gpr<uint64_t>(op2, 1), 1);
}

void EmotionInterpreter::mfhi1(EmotionEngine &cpu, uint32_t instruction)
{
    uint64_t dest = (instruction >> 11) & 0x1F;
    cpu.mfhi1(dest);
}

void EmotionInterpreter::mthi1(EmotionEngine &cpu, uint32_t instruction)
{
    uint64_t source = (instruction >> 21) & 0x1F;
    cpu.mthi1(source);
}

void EmotionInterpreter::mflo1(EmotionEngine &cpu, uint32_t instruction)
{
    uint64_t dest = (instruction >> 11) & 0x1F;
    cpu.mflo1(dest);
}

void EmotionInterpreter::mtlo1(EmotionEngine &cpu, uint32_t instruction)
{
    uint64_t source = (instruction >> 21) & 0x1F;
    cpu.mtlo1(source);
}

void EmotionInterpreter::mult1(EmotionEngine &cpu, uint32_t instruction)
{
    int32_t op1 = (instruction >> 21) & 0x1F;
    int32_t op2 = (instruction >> 16) & 0x1F;
    uint64_t dest = (instruction >> 11) & 0x1F;
    op1 = cpu.get_gpr<int32_t>(op1);
    op2 = cpu.get_gpr<int32_t>(op2);
    int64_t temp = (int64_t)op1 * op2;
    cpu.set_LO_HI((int32_t)(temp & 0xFFFFFFFF), (int32_t)(temp >> 32), true);
    cpu.mflo1(dest);
}

void EmotionInterpreter::div1(EmotionEngine &cpu, uint32_t instruction)
{
    int32_t op1 = (instruction >> 21) & 0x1F;
    int32_t op2 = (instruction >> 16) & 0x1F;
    op1 = cpu.get_gpr<int32_t>(op1);
    op2 = cpu.get_gpr<int32_t>(op2);
    if (!op2)
    {
        exit(1);
    }
    cpu.set_LO_HI(op1 / op2, op1 % op2, true);
}

void EmotionInterpreter::divu1(EmotionEngine &cpu, uint32_t instruction)
{
    uint32_t op1 = (instruction >> 21) & 0x1F;
    uint32_t op2 = (instruction >> 16) & 0x1F;
    op1 = cpu.get_gpr<uint32_t>(op1);
    op2 = cpu.get_gpr<uint32_t>(op2);
    if (!op2)
    {
        exit(1);
    }
    cpu.set_LO_HI(op1 / op2, op1 % op2, true);
}

void EmotionInterpreter::mmi3(EmotionEngine &cpu, uint32_t instruction)
{
    uint8_t op = (instruction >> 6) & 0x1F;
    switch (op)
    {
        case 0x0E:
            pcpyud(cpu, instruction);
            break;
        case 0x12:
            por(cpu, instruction);
            break;
        case 0x13:
            pnor(cpu, instruction);
            break;
        case 0x1B:
            pcpyh(cpu, instruction);
            break;
        default:
            unknown_op("mmi3", instruction, op);
    }
}

/**
 * Parallel Copy from Upper Doubleword
 * The high 64 bits of RS and RT get copied to the high 64 bits and low 64 bits of RD, respectively.
 */
void EmotionInterpreter::pcpyud(EmotionEngine &cpu, uint32_t instruction)
{
    uint64_t reg1 = (instruction >> 21) & 0x1F;
    uint64_t reg2 = (instruction >> 16) & 0x1F;
    uint64_t dest = (instruction >> 11) & 0x1F;

    uint64_t high = cpu.get_gpr<uint64_t>(reg1, 1);
    uint64_t low = cpu.get_gpr<uint64_t>(reg2, 1);

    cpu.set_gpr<uint64_t>(dest, low);
    cpu.set_gpr<uint64_t>(dest, high, 1);
}

void EmotionInterpreter::por(EmotionEngine &cpu, uint32_t instruction)
{
    uint64_t op1 = (instruction >> 21) & 0x1F;
    uint64_t op2 = (instruction >> 16) & 0x1F;
    uint64_t dest = (instruction >> 11) & 0x1F;

    cpu.set_gpr<uint64_t>(dest, cpu.get_gpr<uint64_t>(op1) | cpu.get_gpr<uint64_t>(op2));
    cpu.set_gpr<uint64_t>(dest, cpu.get_gpr<uint64_t>(op1, 1) | cpu.get_gpr<uint64_t>(op2, 1), 1);
}

void EmotionInterpreter::pnor(EmotionEngine &cpu, uint32_t instruction)
{
    uint64_t op1 = (instruction >> 21) & 0x1F;
    uint64_t op2 = (instruction >> 16) & 0x1F;
    uint64_t dest = (instruction >> 11) & 0x1F;

    cpu.set_gpr<uint64_t>(dest, ~(cpu.get_gpr<uint64_t>(op1) | cpu.get_gpr<uint64_t>(op2)));
    cpu.set_gpr<uint64_t>(dest, ~(cpu.get_gpr<uint64_t>(op1, 1) | cpu.get_gpr<uint64_t>(op2, 1)), 1);
}

/**
 * Parallel Copy Halfword
 * Splits RT into two 64-bit pieces. The least significant halfwords of each doubleword are copied into the
 * halfwords of the two doublewords in RD.
 */
void EmotionInterpreter::pcpyh(EmotionEngine &cpu, uint32_t instruction)
{
    uint64_t source = (instruction >> 16) & 0x1F;
    uint64_t dest = (instruction >> 11) & 0x1F;

    for (int i = 0; i < 4; i++)
    {
        uint16_t value = cpu.get_gpr<uint16_t>(source);
        cpu.set_gpr<uint16_t>(dest, value, i);
    }

    for (int i = 4; i < 8; i++)
    {
        uint16_t value = cpu.get_gpr<uint16_t>(source, 4);
        cpu.set_gpr<uint16_t>(dest, value, i);
    }
}
