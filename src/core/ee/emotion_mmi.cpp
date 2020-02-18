#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include "emotioninterpreter.hpp"

void EmotionInterpreter::mmi(EE_InstrInfo &info, uint32_t instruction)
{
    int op = instruction & 0x3F;
    switch (op)
    {
        case 0x00:
            info.interpreter_fn = &madd;
            info.pipeline = EE_InstrInfo::Pipeline::MAC0;
            info.latency = 4;
            info.throughput = 2;
            info.instruction_type = EE_InstrInfo::InstructionType::MADD;
            info.add_dependency(DependencyType::Write, RegType::GPR, (uint8_t)EE_SpecialReg::LO);
            info.add_dependency(DependencyType::Write, RegType::GPR, (uint8_t)EE_SpecialReg::HI);
            info.add_dependency(DependencyType::Write, RegType::GPR, (instruction >> 11) & 0x1F);
            info.add_dependency(DependencyType::Read, RegType::GPR, (instruction >> 21) & 0x1F);
            info.add_dependency(DependencyType::Read, RegType::GPR, (instruction >> 16) & 0x1F);
            break;
        case 0x01:
            info.interpreter_fn = &maddu;
            info.pipeline = EE_InstrInfo::Pipeline::MAC0;
            info.latency = 4;
            info.throughput = 2;
            info.instruction_type = EE_InstrInfo::InstructionType::MADD;
            info.add_dependency(DependencyType::Write, RegType::GPR, (uint8_t)EE_SpecialReg::LO);
            info.add_dependency(DependencyType::Write, RegType::GPR, (uint8_t)EE_SpecialReg::HI);
            info.add_dependency(DependencyType::Write, RegType::GPR, (instruction >> 11) & 0x1F);
            info.add_dependency(DependencyType::Read, RegType::GPR, (instruction >> 21) & 0x1F);
            info.add_dependency(DependencyType::Read, RegType::GPR, (instruction >> 16) & 0x1F);
            break;
        case 0x04:
            info.interpreter_fn = &plzcw;
            info.pipeline = EE_InstrInfo::Pipeline::LZC;
            info.add_dependency(DependencyType::Write, RegType::GPR, (instruction >> 11) & 0x1F);
            info.add_dependency(DependencyType::Read, RegType::GPR, (instruction >> 21) & 0x1F);
            break;
        case 0x08:
            mmi0(info, instruction);
            break;
        case 0x09:
            mmi2(info, instruction);
            break;
        case 0x10:
            info.interpreter_fn = &mfhi1;
            info.pipeline = EE_InstrInfo::Pipeline::MAC1;
            info.add_dependency(DependencyType::Write, RegType::GPR, (instruction >> 11) & 0x1F);
            info.add_dependency(DependencyType::Read, RegType::GPR, (uint8_t)EE_SpecialReg::HI1);
            break;
        case 0x11:
            info.interpreter_fn = &mthi1;
            info.pipeline = EE_InstrInfo::Pipeline::MAC1;
            info.add_dependency(DependencyType::Write, RegType::GPR, (uint8_t)EE_SpecialReg::HI1);
            info.add_dependency(DependencyType::Read, RegType::GPR, (instruction >> 21) & 0x1F);
            break;
        case 0x12:
            info.interpreter_fn = &mflo1;
            info.pipeline = EE_InstrInfo::Pipeline::MAC1;
            info.add_dependency(DependencyType::Write, RegType::GPR, (instruction >> 11) & 0x1F);
            info.add_dependency(DependencyType::Read, RegType::GPR, (uint8_t)EE_SpecialReg::LO1);
            break;
        case 0x13:
            info.interpreter_fn = &mtlo1;
            info.pipeline = EE_InstrInfo::Pipeline::MAC1;
            info.add_dependency(DependencyType::Write, RegType::GPR, (uint8_t)EE_SpecialReg::LO1);
            info.add_dependency(DependencyType::Read, RegType::GPR, (instruction >> 21) & 0x1F);
            break;
        case 0x18:
            info.interpreter_fn = &mult1;
            info.pipeline = EE_InstrInfo::Pipeline::MAC1;
            info.latency = 4;
            info.throughput = 2;
            info.instruction_type = EE_InstrInfo::InstructionType::MULT1;
            info.add_dependency(DependencyType::Write, RegType::GPR, (uint8_t)EE_SpecialReg::LO1);
            info.add_dependency(DependencyType::Write, RegType::GPR, (uint8_t)EE_SpecialReg::HI1);
            info.add_dependency(DependencyType::Write, RegType::GPR, (instruction >> 11) & 0x1F);
            info.add_dependency(DependencyType::Read, RegType::GPR, (instruction >> 21) & 0x1F);
            info.add_dependency(DependencyType::Read, RegType::GPR, (instruction >> 16) & 0x1F);
            break;
        case 0x19:
            info.interpreter_fn = &multu1;
            info.pipeline = EE_InstrInfo::Pipeline::MAC1;
            info.latency = 4;
            info.throughput = 2;
            info.instruction_type = EE_InstrInfo::InstructionType::MULT1;
            info.add_dependency(DependencyType::Write, RegType::GPR, (uint8_t)EE_SpecialReg::LO1);
            info.add_dependency(DependencyType::Write, RegType::GPR, (uint8_t)EE_SpecialReg::HI1);
            info.add_dependency(DependencyType::Write, RegType::GPR, (instruction >> 11) & 0x1F);
            info.add_dependency(DependencyType::Read, RegType::GPR, (instruction >> 21) & 0x1F);
            info.add_dependency(DependencyType::Read, RegType::GPR, (instruction >> 16) & 0x1F);
            break;
        case 0x1A:
            info.interpreter_fn = &div1;
            info.pipeline = EE_InstrInfo::Pipeline::MAC1;
            info.latency = 37;
            info.throughput = 37;
            info.instruction_type = EE_InstrInfo::InstructionType::DIV1;
            info.add_dependency(DependencyType::Write, RegType::GPR, (uint8_t)EE_SpecialReg::LO1);
            info.add_dependency(DependencyType::Write, RegType::GPR, (uint8_t)EE_SpecialReg::HI1);
            info.add_dependency(DependencyType::Read, RegType::GPR, (instruction >> 21) & 0x1F);
            info.add_dependency(DependencyType::Read, RegType::GPR, (instruction >> 16) & 0x1F);
            break;
        case 0x1B:
            info.interpreter_fn = &divu1;
            info.pipeline = EE_InstrInfo::Pipeline::MAC1;
            info.latency = 37;
            info.throughput = 37;
            info.instruction_type = EE_InstrInfo::InstructionType::DIV1;
            info.add_dependency(DependencyType::Write, RegType::GPR, (uint8_t)EE_SpecialReg::LO1);
            info.add_dependency(DependencyType::Write, RegType::GPR, (uint8_t)EE_SpecialReg::HI1);
            info.add_dependency(DependencyType::Read, RegType::GPR, (instruction >> 21) & 0x1F);
            info.add_dependency(DependencyType::Read, RegType::GPR, (instruction >> 16) & 0x1F);
            break;
        case 0x20:
            info.interpreter_fn = &madd1;
            info.pipeline = EE_InstrInfo::Pipeline::MAC1;
            info.latency = 4;
            info.throughput = 2;
            info.instruction_type = EE_InstrInfo::InstructionType::MADD;
            info.add_dependency(DependencyType::Write, RegType::GPR, (uint8_t)EE_SpecialReg::LO1);
            info.add_dependency(DependencyType::Write, RegType::GPR, (uint8_t)EE_SpecialReg::HI1);
            info.add_dependency(DependencyType::Write, RegType::GPR, (instruction >> 11) & 0x1F);
            info.add_dependency(DependencyType::Read, RegType::GPR, (instruction >> 21) & 0x1F);
            info.add_dependency(DependencyType::Read, RegType::GPR, (instruction >> 16) & 0x1F);
            break;
        case 0x21:
            info.interpreter_fn = &maddu1;
            info.pipeline = EE_InstrInfo::Pipeline::MAC1;
            info.latency = 4;
            info.throughput = 2;
            info.instruction_type = EE_InstrInfo::InstructionType::MADD;
            info.add_dependency(DependencyType::Write, RegType::GPR, (uint8_t)EE_SpecialReg::LO1);
            info.add_dependency(DependencyType::Write, RegType::GPR, (uint8_t)EE_SpecialReg::HI1);
            info.add_dependency(DependencyType::Write, RegType::GPR, (instruction >> 11) & 0x1F);
            info.add_dependency(DependencyType::Read, RegType::GPR, (instruction >> 21) & 0x1F);
            info.add_dependency(DependencyType::Read, RegType::GPR, (instruction >> 16) & 0x1F);
            break;
        case 0x28:
            mmi1(info, instruction);
            break;
        case 0x29:
            mmi3(info, instruction);
            break;
        case 0x30:
            pmfhlfmt(info, instruction);
            break;
        case 0x31:
            info.interpreter_fn = &pmthllw;
            info.pipeline = EE_InstrInfo::Pipeline::IntWide;
            info.add_dependency(DependencyType::Write, RegType::GPR, (uint8_t)EE_SpecialReg::LO);
            info.add_dependency(DependencyType::Write, RegType::GPR, (uint8_t)EE_SpecialReg::LO1);
            info.add_dependency(DependencyType::Write, RegType::GPR, (uint8_t)EE_SpecialReg::HI);
            info.add_dependency(DependencyType::Write, RegType::GPR, (uint8_t)EE_SpecialReg::HI1);
            info.add_dependency(DependencyType::Read, RegType::GPR, (instruction >> 21) & 0x1F);
            break;
        case 0x34:
            info.interpreter_fn = &psllh;
            info.pipeline = EE_InstrInfo::Pipeline::IntWide;
            info.add_dependency(DependencyType::Write, RegType::GPR, (instruction >> 11) & 0x1F);
            info.add_dependency(DependencyType::Read, RegType::GPR, (instruction >> 16) & 0x1F);
            break;
        case 0x36:
            info.interpreter_fn = &psrlh;
            info.pipeline = EE_InstrInfo::Pipeline::IntWide;
            info.add_dependency(DependencyType::Write, RegType::GPR, (instruction >> 11) & 0x1F);
            info.add_dependency(DependencyType::Read, RegType::GPR, (instruction >> 16) & 0x1F);
            break;
        case 0x37:
            info.interpreter_fn = &psrah;
            info.pipeline = EE_InstrInfo::Pipeline::IntWide;
            info.add_dependency(DependencyType::Write, RegType::GPR, (instruction >> 11) & 0x1F);
            info.add_dependency(DependencyType::Read, RegType::GPR, (instruction >> 16) & 0x1F);
            break;
        case 0x3C:
            info.interpreter_fn = &psllw;
            info.pipeline = EE_InstrInfo::Pipeline::IntWide;
            info.add_dependency(DependencyType::Write, RegType::GPR, (instruction >> 11) & 0x1F);
            info.add_dependency(DependencyType::Read, RegType::GPR, (instruction >> 16) & 0x1F);
            break;
        case 0x3E:
            info.interpreter_fn = &psrlw;
            info.pipeline = EE_InstrInfo::Pipeline::IntWide;
            info.add_dependency(DependencyType::Write, RegType::GPR, (instruction >> 11) & 0x1F);
            info.add_dependency(DependencyType::Read, RegType::GPR, (instruction >> 16) & 0x1F);
            break;
        case 0x3F:
            info.interpreter_fn = &psraw;
            info.pipeline = EE_InstrInfo::Pipeline::IntWide;
            info.add_dependency(DependencyType::Write, RegType::GPR, (instruction >> 11) & 0x1F);
            info.add_dependency(DependencyType::Read, RegType::GPR, (instruction >> 16) & 0x1F);
            break;
        default:
            unknown_op("mmi", instruction, op);
    }
}

void EmotionInterpreter::madd(EmotionEngine &cpu, uint32_t instruction)
{
    int64_t op1 = (instruction >> 21) & 0x1F;
    int64_t op2 = (instruction >> 16) & 0x1F;
    uint64_t dest = (instruction >> 11) & 0x1F;
    op1 = (int64_t)cpu.get_gpr<int32_t>(op1);
    op2 = (int64_t)cpu.get_gpr<int32_t>(op2);

    uint64_t lo = (uint64_t)(uint32_t)cpu.get_LO();
    uint64_t hi = (uint64_t)(uint32_t)cpu.get_HI();
    int64_t temp = (int64_t)((lo | (hi << 32)) + (op1 * op2));

    lo = (int64_t)(int32_t)(temp & 0xFFFFFFFF);
    hi = (int64_t)(int32_t)(temp >> 32);

    cpu.set_LO_HI(lo, hi, false);
    cpu.set_gpr<int64_t>(dest, (int64_t)lo);
}

void EmotionInterpreter::maddu(EmotionEngine &cpu, uint32_t instruction)
{
    uint64_t op1 = (instruction >> 21) & 0x1F;
    uint64_t op2 = (instruction >> 16) & 0x1F;
    uint64_t dest = (instruction >> 11) & 0x1F;
    op1 = (uint64_t)cpu.get_gpr<uint32_t>(op1);
    op2 = (uint64_t)cpu.get_gpr<uint32_t>(op2);

    uint64_t lo = (uint64_t)(uint32_t)cpu.get_LO();
    uint64_t hi = (uint64_t)(uint32_t)cpu.get_HI();
    uint64_t temp = (int64_t)((lo | (hi << 32)) + (op1 * op2));

    lo = (int64_t)(int32_t)(temp & 0xFFFFFFFF);
    hi = (int64_t)(int32_t)(temp >> 32);

    cpu.set_LO_HI(lo, hi, false);
    cpu.set_gpr<int64_t>(dest, (int64_t)lo);
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
            if ((words[i] & (1 << j)) == (high_bit << j))
                bits++;
            else
                break;
        }
        words[i] = bits;
    }

    cpu.set_gpr<uint32_t>(dest, words[0]);
    cpu.set_gpr<uint32_t>(dest, words[1], 1);
}

/**
 * Parallel Shift Left Logical Halfword
 * Splits RT into eight halfwords and shifts them all by the amount specified in the four-bit SA
 */
void EmotionInterpreter::psllh(EmotionEngine &cpu, uint32_t instruction)
{
    uint32_t source = (instruction >> 16) & 0x1F;
    uint32_t dest = (instruction >> 11) & 0x1F;
    uint8_t shift = (instruction >> 6) & 0xF;

    for (int i = 0; i < 8; i++)
    {
        uint16_t halfword = cpu.get_gpr<uint16_t>(source, i);
        halfword <<= shift;
        cpu.set_gpr<uint16_t>(dest, halfword, i);
    }
}

/**
 * Parallel Shift Right Logical Halfword
 */
void EmotionInterpreter::psrlh(EmotionEngine &cpu, uint32_t instruction)
{
    uint32_t source = (instruction >> 16) & 0x1F;
    uint32_t dest = (instruction >> 11) & 0x1F;
    uint8_t shift = (instruction >> 6) & 0xF;

    for (int i = 0; i < 8; i++)
    {
        uint16_t halfword = cpu.get_gpr<uint16_t>(source, i);
        halfword >>= shift;
        cpu.set_gpr<uint16_t>(dest, halfword, i);
    }
}

/**
 * Parallel Shift Right Arithmetic Halfword
 */
void EmotionInterpreter::psrah(EmotionEngine &cpu, uint32_t instruction)
{
    uint32_t source = (instruction >> 16) & 0x1F;
    uint32_t dest = (instruction >> 11) & 0x1F;
    uint8_t shift = (instruction >> 6) & 0xF;

    for (int i = 0; i < 8; i++)
    {
        int16_t halfword = cpu.get_gpr<int16_t>(source, i);
        halfword >>= shift;
        cpu.set_gpr<int16_t>(dest, halfword, i);
    }
}

/**
 * Parallel Shift Left Logical Word
 * Splits RT into four words and shifts them all by the amount specified in SA
 */
void EmotionInterpreter::psllw(EmotionEngine &cpu, uint32_t instruction)
{
    uint32_t source = (instruction >> 16) & 0x1F;
    uint32_t dest = (instruction >> 11) & 0x1F;
    uint8_t shift = (instruction >> 6) & 0x1F;

    for (int i = 0; i < 4; i++)
    {
        uint32_t word = cpu.get_gpr<uint32_t>(source, i);
        word <<= shift;
        cpu.set_gpr<uint32_t>(dest, word, i);
    }
}

/**
 * Parallel Shift Right Logical Word
 */
void EmotionInterpreter::psrlw(EmotionEngine &cpu, uint32_t instruction)
{
    uint32_t source = (instruction >> 16) & 0x1F;
    uint32_t dest = (instruction >> 11) & 0x1F;
    uint8_t shift = (instruction >> 6) & 0x1F;

    for (int i = 0; i < 4; i++)
    {
        uint32_t word = cpu.get_gpr<uint32_t>(source, i);
        word >>= shift;
        cpu.set_gpr<uint32_t>(dest, word, i);
    }
}

/**
 * Parallel Shift Right Arithmetic Word
 */
void EmotionInterpreter::psraw(EmotionEngine &cpu, uint32_t instruction)
{
    uint32_t source = (instruction >> 16) & 0x1F;
    uint32_t dest = (instruction >> 11) & 0x1F;
    uint8_t shift = (instruction >> 6) & 0x1F;

    for (int i = 0; i < 4; i++)
    {
        int32_t word = cpu.get_gpr<int32_t>(source, i);
        word >>= shift;
        cpu.set_gpr<int32_t>(dest, word, i);
    }
}

void EmotionInterpreter::pmfhlfmt(EE_InstrInfo& info, uint32_t instruction)
{
    uint8_t op = (instruction >> 6) & 0x1F;
    switch (op)
    {
        case 0x00:
            info.interpreter_fn = &pmfhllw;
            info.pipeline = EE_InstrInfo::Pipeline::IntWide;
            info.add_dependency(DependencyType::Write, RegType::GPR, (instruction >> 11) & 0x1F);
            info.add_dependency(DependencyType::Read, RegType::GPR, (uint8_t)EE_SpecialReg::LO);
            info.add_dependency(DependencyType::Read, RegType::GPR, (uint8_t)EE_SpecialReg::LO1);
            info.add_dependency(DependencyType::Read, RegType::GPR, (uint8_t)EE_SpecialReg::HI);
            info.add_dependency(DependencyType::Read, RegType::GPR, (uint8_t)EE_SpecialReg::HI1);
            break;
        case 0x01:
            info.interpreter_fn = &pmfhluw;
            info.pipeline = EE_InstrInfo::Pipeline::IntWide;
            info.add_dependency(DependencyType::Write, RegType::GPR, (instruction >> 11) & 0x1F);
            info.add_dependency(DependencyType::Read, RegType::GPR, (uint8_t)EE_SpecialReg::LO);
            info.add_dependency(DependencyType::Read, RegType::GPR, (uint8_t)EE_SpecialReg::LO1);
            info.add_dependency(DependencyType::Read, RegType::GPR, (uint8_t)EE_SpecialReg::HI);
            info.add_dependency(DependencyType::Read, RegType::GPR, (uint8_t)EE_SpecialReg::HI1);
            break;
        case 0x02:
            info.interpreter_fn = &pmfhlslw;
            info.pipeline = EE_InstrInfo::Pipeline::IntWide;
            info.add_dependency(DependencyType::Write, RegType::GPR, (instruction >> 11) & 0x1F);
            info.add_dependency(DependencyType::Read, RegType::GPR, (uint8_t)EE_SpecialReg::LO);
            info.add_dependency(DependencyType::Read, RegType::GPR, (uint8_t)EE_SpecialReg::LO1);
            info.add_dependency(DependencyType::Read, RegType::GPR, (uint8_t)EE_SpecialReg::HI);
            info.add_dependency(DependencyType::Read, RegType::GPR, (uint8_t)EE_SpecialReg::HI1);
            break;
        case 0x03:           
            info.interpreter_fn = &pmfhllh;
            info.pipeline = EE_InstrInfo::Pipeline::IntWide;
            info.add_dependency(DependencyType::Write, RegType::GPR, (instruction >> 11) & 0x1F);
            info.add_dependency(DependencyType::Read, RegType::GPR, (uint8_t)EE_SpecialReg::LO);
            info.add_dependency(DependencyType::Read, RegType::GPR, (uint8_t)EE_SpecialReg::LO1);
            info.add_dependency(DependencyType::Read, RegType::GPR, (uint8_t)EE_SpecialReg::HI);
            info.add_dependency(DependencyType::Read, RegType::GPR, (uint8_t)EE_SpecialReg::HI1);
            break;
        case 0x04:
            info.interpreter_fn = &pmfhlsh;
            info.pipeline = EE_InstrInfo::Pipeline::IntWide;
            info.add_dependency(DependencyType::Write, RegType::GPR, (instruction >> 11) & 0x1F);
            info.add_dependency(DependencyType::Read, RegType::GPR, (uint8_t)EE_SpecialReg::LO);
            info.add_dependency(DependencyType::Read, RegType::GPR, (uint8_t)EE_SpecialReg::LO1);
            info.add_dependency(DependencyType::Read, RegType::GPR, (uint8_t)EE_SpecialReg::HI);
            info.add_dependency(DependencyType::Read, RegType::GPR, (uint8_t)EE_SpecialReg::HI1);
            break;
    }
}

int64_t EmotionInterpreter::clamp_doubleword(int64_t word)
{
    if (word >= (int64_t)0x000000007FFFFFFF)
    {
        return 0x7FFFFFFF;
    }
    else if (word <= (int64_t)0xFFFFFFFF80000000)
    {
        return 0xFFFFFFFF80000000;
    }
    else
    {
        return (int64_t)(int32_t)word;
    }
}

int16_t EmotionInterpreter::clamp_halfword(int32_t word)
{
    if (word > (int32_t)0x00007FFF)
    {
        return 0x7FFF;
    }
    else if (word < (int32_t)0xFFFF80000)
    {
        return 0x8000;
    }
    else
    {
        return (int16_t)word;
    }
}

/*
* Parallel Move To HI/LO Register - Load Word
*/
void EmotionInterpreter::pmthllw(EmotionEngine& cpu, uint32_t instruction)
{
    uint128_t source = cpu.get_gpr<uint128_t>((instruction >> 21) & 0x1F);
    uint128_t lo, hi;

    lo._u64[0] = cpu.get_LO();
    lo._u64[1] = cpu.get_LO1();

    hi._u64[0] = cpu.get_HI();
    hi._u64[1] = cpu.get_HI1();

    for (int i = 0; i < 2; i++)
    {
        lo._u32[i * 2] = source._u32[i * 2];
        hi._u32[i * 2] = source._u32[(i * 2)+1];
    }

    cpu.set_LO_HI(lo._u64[0], hi._u64[0], false);
    cpu.set_LO_HI(lo._u64[1], hi._u64[1], true);
}

/*
* Parallel Move From HI/LO Register - Load Word
*/
void EmotionInterpreter::pmfhllw(EmotionEngine& cpu, uint32_t instruction)
{
    uint64_t dest = (instruction >> 11) & 0x1F;
    uint128_t lo, hi;
    uint128_t data;

    lo._u64[0] = cpu.get_LO();
    lo._u64[1] = cpu.get_LO1();

    hi._u64[0] = cpu.get_HI();
    hi._u64[1] = cpu.get_HI1();

    for (int i = 0; i < 2; i++)
    {
        data._u32[i * 2] = lo._u32[i * 2];
        data._u32[(i * 2) + 1] = hi._u32[i * 2];
    }

    cpu.set_gpr<uint128_t>(dest, data);
}

/*
* Parallel Move From HI/LO Register - Load Word
*/
void EmotionInterpreter::pmfhluw(EmotionEngine& cpu, uint32_t instruction)
{
    uint64_t dest = (instruction >> 11) & 0x1F;
    uint128_t lo, hi;
    uint128_t data;

    lo._u64[0] = cpu.get_LO();
    lo._u64[1] = cpu.get_LO1();

    hi._u64[0] = cpu.get_HI();
    hi._u64[1] = cpu.get_HI1();

    for (int i = 0; i < 2; i++)
    {
        data._u32[i * 2] = lo._u32[(i * 2) + 1];
        data._u32[(i * 2) + 1] = hi._u32[(i * 2) + 1];
    }

    cpu.set_gpr<uint128_t>(dest, data);
}

/*
* Parallel Move From HI/LO Register - Saturate Long Word
*/
void EmotionInterpreter::pmfhlslw(EmotionEngine& cpu, uint32_t instruction)
{
    uint64_t dest = (instruction >> 11) & 0x1F;
    uint128_t lo, hi;
    uint128_t data;

    lo._u64[0] = cpu.get_LO();
    lo._u64[1] = cpu.get_LO1();

    hi._u64[0] = cpu.get_HI();
    hi._u64[1] = cpu.get_HI1();

    data._u64[0] = clamp_doubleword((uint64_t)lo._u32[0] | ((uint64_t)hi._u32[0] << 32));
    data._u64[1] = clamp_doubleword((uint64_t)lo._u32[2] | ((uint64_t)hi._u32[2] << 32));
    
    cpu.set_gpr<uint128_t>(dest, data);
}

/*
 * Parallel Move From HI/LO Register - Load Halfword
 */
void EmotionInterpreter::pmfhllh(EmotionEngine& cpu, uint32_t instruction)
{
    uint64_t dest = (instruction >> 11) & 0x1F;
    uint128_t lo, hi;
    uint128_t data;

    lo._u64[0] = cpu.get_LO();
    lo._u64[1] = cpu.get_LO1();

    hi._u64[0] = cpu.get_HI();
    hi._u64[1] = cpu.get_HI1();

    for (int i = 0; i < 4; i++)
    {
        data._u16[i * 2] = lo._u16[i * 2];
        data._u16[(i * 2) + 1] = hi._u16[i * 2];
    }

    std::swap(data._u16[1], data._u16[2]);
    std::swap(data._u16[5], data._u16[6]);

    cpu.set_gpr<uint128_t>(dest, data);
}

/*
* Parallel Move From HI/LO Register - Saturate Halfword
*/
void EmotionInterpreter::pmfhlsh(EmotionEngine& cpu, uint32_t instruction)
{
    uint64_t dest = (instruction >> 11) & 0x1F;
    uint128_t lo, hi;
    uint128_t data;

    lo._u64[0] = cpu.get_LO();
    lo._u64[1] = cpu.get_LO1();

    hi._u64[0] = cpu.get_HI();
    hi._u64[1] = cpu.get_HI1();

    data._u16[0] = clamp_halfword(lo._u32[0]);
    data._u16[1] = clamp_halfword(lo._u32[1]);
    data._u16[2] = clamp_halfword(hi._u32[0]);
    data._u16[3] = clamp_halfword(hi._u32[1]);
    data._u16[4] = clamp_halfword(lo._u32[2]);
    data._u16[5] = clamp_halfword(lo._u32[3]);
    data._u16[6] = clamp_halfword(hi._u32[2]);
    data._u16[7] = clamp_halfword(hi._u32[3]);
    cpu.set_gpr<uint128_t>(dest, data);
}

void EmotionInterpreter::mmi0(EE_InstrInfo &info, uint32_t instruction)
{
    uint8_t op = (instruction >> 6) & 0x1F;
    switch (op)
    {
        case 0x00:
            info.interpreter_fn = &paddw;
            info.pipeline = EE_InstrInfo::Pipeline::IntWide;
            info.add_dependency(DependencyType::Write, RegType::GPR, (instruction >> 11) & 0x1F);
            info.add_dependency(DependencyType::Read, RegType::GPR, (instruction >> 21) & 0x1F);
            info.add_dependency(DependencyType::Read, RegType::GPR, (instruction >> 16) & 0x1F);
            break;
        case 0x01:
            info.interpreter_fn = &psubw;
            info.pipeline = EE_InstrInfo::Pipeline::IntWide;
            info.add_dependency(DependencyType::Write, RegType::GPR, (instruction >> 11) & 0x1F);
            info.add_dependency(DependencyType::Read, RegType::GPR, (instruction >> 21) & 0x1F);
            info.add_dependency(DependencyType::Read, RegType::GPR, (instruction >> 16) & 0x1F);
            break;
        case 0x02:
            info.interpreter_fn = &pcgtw;
            info.pipeline = EE_InstrInfo::Pipeline::IntWide;
            info.add_dependency(DependencyType::Write, RegType::GPR, (instruction >> 11) & 0x1F);
            info.add_dependency(DependencyType::Read, RegType::GPR, (instruction >> 21) & 0x1F);
            info.add_dependency(DependencyType::Read, RegType::GPR, (instruction >> 16) & 0x1F);
            break;
        case 0x03:
            info.interpreter_fn = &pmaxw;
            info.pipeline = EE_InstrInfo::Pipeline::IntWide;
            info.add_dependency(DependencyType::Write, RegType::GPR, (instruction >> 11) & 0x1F);
            info.add_dependency(DependencyType::Read, RegType::GPR, (instruction >> 21) & 0x1F);
            info.add_dependency(DependencyType::Read, RegType::GPR, (instruction >> 16) & 0x1F);
            break;
        case 0x04:
            info.interpreter_fn = &paddh;
            info.pipeline = EE_InstrInfo::Pipeline::IntWide;
            info.add_dependency(DependencyType::Write, RegType::GPR, (instruction >> 11) & 0x1F);
            info.add_dependency(DependencyType::Read, RegType::GPR, (instruction >> 21) & 0x1F);
            info.add_dependency(DependencyType::Read, RegType::GPR, (instruction >> 16) & 0x1F);
            break;
        case 0x05:
            info.interpreter_fn = &psubh;
            info.pipeline = EE_InstrInfo::Pipeline::IntWide;
            info.add_dependency(DependencyType::Write, RegType::GPR, (instruction >> 11) & 0x1F);
            info.add_dependency(DependencyType::Read, RegType::GPR, (instruction >> 21) & 0x1F);
            info.add_dependency(DependencyType::Read, RegType::GPR, (instruction >> 16) & 0x1F);
            break;
        case 0x06:
            info.interpreter_fn = &pcgth;
            info.pipeline = EE_InstrInfo::Pipeline::IntWide;
            info.add_dependency(DependencyType::Write, RegType::GPR, (instruction >> 11) & 0x1F);
            info.add_dependency(DependencyType::Read, RegType::GPR, (instruction >> 21) & 0x1F);
            info.add_dependency(DependencyType::Read, RegType::GPR, (instruction >> 16) & 0x1F);
            break;
        case 0x07:
            info.interpreter_fn = &pmaxh;
            info.pipeline = EE_InstrInfo::Pipeline::IntWide;
            info.add_dependency(DependencyType::Write, RegType::GPR, (instruction >> 11) & 0x1F);
            info.add_dependency(DependencyType::Read, RegType::GPR, (instruction >> 21) & 0x1F);
            info.add_dependency(DependencyType::Read, RegType::GPR, (instruction >> 16) & 0x1F);
            break;
        case 0x08:
            info.interpreter_fn = &paddb;
            info.pipeline = EE_InstrInfo::Pipeline::IntWide;
            info.add_dependency(DependencyType::Write, RegType::GPR, (instruction >> 11) & 0x1F);
            info.add_dependency(DependencyType::Read, RegType::GPR, (instruction >> 21) & 0x1F);
            info.add_dependency(DependencyType::Read, RegType::GPR, (instruction >> 16) & 0x1F);
            break;
        case 0x09:
            info.interpreter_fn = &psubb;
            info.pipeline = EE_InstrInfo::Pipeline::IntWide;
            info.add_dependency(DependencyType::Write, RegType::GPR, (instruction >> 11) & 0x1F);
            info.add_dependency(DependencyType::Read, RegType::GPR, (instruction >> 21) & 0x1F);
            info.add_dependency(DependencyType::Read, RegType::GPR, (instruction >> 16) & 0x1F);
            break;
        case 0x0A:
            info.interpreter_fn = &pcgtb;
            info.pipeline = EE_InstrInfo::Pipeline::IntWide;
            info.add_dependency(DependencyType::Write, RegType::GPR, (instruction >> 11) & 0x1F);
            info.add_dependency(DependencyType::Read, RegType::GPR, (instruction >> 21) & 0x1F);
            info.add_dependency(DependencyType::Read, RegType::GPR, (instruction >> 16) & 0x1F);
            break;
        case 0x10:
            info.interpreter_fn = &paddsw;
            info.pipeline = EE_InstrInfo::Pipeline::IntWide;
            info.add_dependency(DependencyType::Write, RegType::GPR, (instruction >> 11) & 0x1F);
            info.add_dependency(DependencyType::Read, RegType::GPR, (instruction >> 21) & 0x1F);
            info.add_dependency(DependencyType::Read, RegType::GPR, (instruction >> 16) & 0x1F);
            break;
        case 0x11:
            info.interpreter_fn = &psubsw;
            info.pipeline = EE_InstrInfo::Pipeline::IntWide;
            info.add_dependency(DependencyType::Write, RegType::GPR, (instruction >> 11) & 0x1F);
            info.add_dependency(DependencyType::Read, RegType::GPR, (instruction >> 21) & 0x1F);
            info.add_dependency(DependencyType::Read, RegType::GPR, (instruction >> 16) & 0x1F);
            break;
        case 0x12:
            info.interpreter_fn = &pextlw;
            info.pipeline = EE_InstrInfo::Pipeline::IntWide;
            info.add_dependency(DependencyType::Write, RegType::GPR, (instruction >> 11) & 0x1F);
            info.add_dependency(DependencyType::Read, RegType::GPR, (instruction >> 21) & 0x1F);
            info.add_dependency(DependencyType::Read, RegType::GPR, (instruction >> 16) & 0x1F);
            break;
        case 0x13:
            info.interpreter_fn = &ppacw;
            info.pipeline = EE_InstrInfo::Pipeline::IntWide;
            info.add_dependency(DependencyType::Write, RegType::GPR, (instruction >> 11) & 0x1F);
            info.add_dependency(DependencyType::Read, RegType::GPR, (instruction >> 21) & 0x1F);
            info.add_dependency(DependencyType::Read, RegType::GPR, (instruction >> 16) & 0x1F);
            break;
        case 0x14:
            info.interpreter_fn = &paddsh;
            info.pipeline = EE_InstrInfo::Pipeline::IntWide;
            info.add_dependency(DependencyType::Write, RegType::GPR, (instruction >> 11) & 0x1F);
            info.add_dependency(DependencyType::Read, RegType::GPR, (instruction >> 21) & 0x1F);
            info.add_dependency(DependencyType::Read, RegType::GPR, (instruction >> 16) & 0x1F);
            break;
        case 0x15:
            info.interpreter_fn = &psubsh;
            info.pipeline = EE_InstrInfo::Pipeline::IntWide;
            info.add_dependency(DependencyType::Write, RegType::GPR, (instruction >> 11) & 0x1F);
            info.add_dependency(DependencyType::Read, RegType::GPR, (instruction >> 21) & 0x1F);
            info.add_dependency(DependencyType::Read, RegType::GPR, (instruction >> 16) & 0x1F);
            break;
        case 0x16:
            info.interpreter_fn = &pextlh;
            info.pipeline = EE_InstrInfo::Pipeline::IntWide;
            info.add_dependency(DependencyType::Write, RegType::GPR, (instruction >> 11) & 0x1F);
            info.add_dependency(DependencyType::Read, RegType::GPR, (instruction >> 21) & 0x1F);
            info.add_dependency(DependencyType::Read, RegType::GPR, (instruction >> 16) & 0x1F);
            break;
        case 0x17:
            info.interpreter_fn = &ppach;
            info.pipeline = EE_InstrInfo::Pipeline::IntWide;
            info.add_dependency(DependencyType::Write, RegType::GPR, (instruction >> 11) & 0x1F);
            info.add_dependency(DependencyType::Read, RegType::GPR, (instruction >> 21) & 0x1F);
            info.add_dependency(DependencyType::Read, RegType::GPR, (instruction >> 16) & 0x1F);
            break;
        case 0x18:
            info.interpreter_fn = &paddsb;
            info.pipeline = EE_InstrInfo::Pipeline::IntWide;
            info.add_dependency(DependencyType::Write, RegType::GPR, (instruction >> 11) & 0x1F);
            info.add_dependency(DependencyType::Read, RegType::GPR, (instruction >> 21) & 0x1F);
            info.add_dependency(DependencyType::Read, RegType::GPR, (instruction >> 16) & 0x1F);
            break;
        case 0x19:
            info.interpreter_fn = &psubsb;
            info.pipeline = EE_InstrInfo::Pipeline::IntWide;
            info.add_dependency(DependencyType::Write, RegType::GPR, (instruction >> 11) & 0x1F);
            info.add_dependency(DependencyType::Read, RegType::GPR, (instruction >> 21) & 0x1F);
            info.add_dependency(DependencyType::Read, RegType::GPR, (instruction >> 16) & 0x1F);
            break;
        case 0x1A:
            info.interpreter_fn = &pextlb;
            info.pipeline = EE_InstrInfo::Pipeline::IntWide;
            info.add_dependency(DependencyType::Write, RegType::GPR, (instruction >> 11) & 0x1F);
            info.add_dependency(DependencyType::Read, RegType::GPR, (instruction >> 21) & 0x1F);
            info.add_dependency(DependencyType::Read, RegType::GPR, (instruction >> 16) & 0x1F);
            break;
        case 0x1B:
            info.interpreter_fn = &ppacb;
            info.pipeline = EE_InstrInfo::Pipeline::IntWide;
            info.add_dependency(DependencyType::Write, RegType::GPR, (instruction >> 11) & 0x1F);
            info.add_dependency(DependencyType::Read, RegType::GPR, (instruction >> 21) & 0x1F);
            info.add_dependency(DependencyType::Read, RegType::GPR, (instruction >> 16) & 0x1F);
            break;
        case 0x1E:
            info.interpreter_fn = &pext5;
            info.pipeline = EE_InstrInfo::Pipeline::IntWide;
            info.add_dependency(DependencyType::Write, RegType::GPR, (instruction >> 11) & 0x1F);
            info.add_dependency(DependencyType::Read, RegType::GPR, (instruction >> 16) & 0x1F);
            break;
        case 0x1F:
            info.interpreter_fn = &ppac5;
            info.pipeline = EE_InstrInfo::Pipeline::IntWide;
            info.add_dependency(DependencyType::Write, RegType::GPR, (instruction >> 11) & 0x1F);
            info.add_dependency(DependencyType::Read, RegType::GPR, (instruction >> 16) & 0x1F);
            break;
        default:
            unknown_op("mmi0", instruction, op);
    }
}

/**
 * Parallel Add Word
 */
void EmotionInterpreter::paddw(EmotionEngine &cpu, uint32_t instruction)
{
    uint64_t reg1 = (instruction >> 21) & 0x1F;
    uint64_t reg2 = (instruction >> 16) & 0x1F;
    uint64_t dest = (instruction >> 11) & 0x1F;

    for (int i = 0; i < 4; i++)
    {
        int32_t word = cpu.get_gpr<int32_t>(reg1, i);
        word += cpu.get_gpr<int32_t>(reg2, i);
        cpu.set_gpr<int32_t>(dest, word, i);
    }
}

/**
 * Parallel Subtract Word
 * Splits the 128-bit registers RS and RT into four words each, subtracts them, and places the 128-bit result in RD.
 * TODO: The result of an overflow/underflow is truncated.
 */
void EmotionInterpreter::psubw(EmotionEngine &cpu, uint32_t instruction)
{
    uint64_t reg1 = (instruction >> 21) & 0x1F;
    uint64_t reg2 = (instruction >> 16) & 0x1F;
    uint64_t dest = (instruction >> 11) & 0x1F;

    for (int i = 0; i < 4; i++)
    {
        int32_t word = cpu.get_gpr<int32_t>(reg1, i);
        word -= cpu.get_gpr<int32_t>(reg2, i);
        cpu.set_gpr<int32_t>(dest, word, i);
    }
}

/**
 * Parallel Compare for Greater Word
 * Compares the four signed words in RS and RT. Sets 0xFFFFFFFF if RS > RT, 0 if not.
 */
void EmotionInterpreter::pcgtw(EmotionEngine &cpu, uint32_t instruction)
{
    uint64_t reg1 = (instruction >> 21) & 0x1F;
    uint64_t reg2 = (instruction >> 16) & 0x1F;
    uint64_t dest = (instruction >> 11) & 0x1F;

    uint128_t qw1 = cpu.get_gpr<uint128_t>(reg1);
    uint128_t qw2 = cpu.get_gpr<uint128_t>(reg2);
    uint128_t dest_qw;

    for (int i = 0; i < 4; i++)
    {
        if ((int32_t)qw1._u32[i] > (int32_t)qw2._u32[i])
            dest_qw._u32[i] = 0xFFFFFFFF;
        else
            dest_qw._u32[i] = 0;
    }

    cpu.set_gpr<uint128_t>(dest, dest_qw);
}

/**
 * Parallel Maximize Word
 * Compares the four words in RS and RT each, and stores the greater of each in RD.
 */
void EmotionInterpreter::pmaxw(EmotionEngine &cpu, uint32_t instruction)
{
    uint64_t reg1 = (instruction >> 21) & 0x1F;
    uint64_t reg2 = (instruction >> 16) & 0x1F;
    uint64_t dest = (instruction >> 11) & 0x1F;

    for (int i = 0; i < 4; i++)
    {
        int32_t a = cpu.get_gpr<int32_t>(reg1, i), b = cpu.get_gpr<int32_t>(reg2, i);
        if (a > b)
            cpu.set_gpr<int32_t>(dest, a, i);
        else
            cpu.set_gpr<int32_t>(dest, b, i);
    }
}

/**
 * Parallel Add Halfword
 */
void EmotionInterpreter::paddh(EmotionEngine &cpu, uint32_t instruction)
{
    uint64_t reg1 = (instruction >> 21) & 0x1F;
    uint64_t reg2 = (instruction >> 16) & 0x1F;
    uint64_t dest = (instruction >> 11) & 0x1F;

    for (int i = 0; i < 8; i++)
    {
        int16_t halfword = cpu.get_gpr<int16_t>(reg1, i);
        halfword += cpu.get_gpr<int16_t>(reg2, i);
        cpu.set_gpr<int16_t>(dest, halfword, i);
    }
}

/**
 * Parallel Subtract Halfword
 */
void EmotionInterpreter::psubh(EmotionEngine &cpu, uint32_t instruction)
{
    uint64_t reg1 = (instruction >> 21) & 0x1F;
    uint64_t reg2 = (instruction >> 16) & 0x1F;
    uint64_t dest = (instruction >> 11) & 0x1F;

    for (int i = 0; i < 8; i++)
    {
        int16_t halfword = cpu.get_gpr<int16_t>(reg1, i);
        halfword -= cpu.get_gpr<int16_t>(reg2, i);
        cpu.set_gpr<int16_t>(dest, halfword, i);
    }
}

/**
 * Parallel Compare for Greater Halfword
 * Compares the eight signed halfwords in RS and RT. Sets 0xFFFF if RS > RT, 0 if not.
 */
void EmotionInterpreter::pcgth(EmotionEngine &cpu, uint32_t instruction)
{
    uint64_t reg1 = (instruction >> 21) & 0x1F;
    uint64_t reg2 = (instruction >> 16) & 0x1F;
    uint64_t dest = (instruction >> 11) & 0x1F;

    uint128_t qw1 = cpu.get_gpr<uint128_t>(reg1);
    uint128_t qw2 = cpu.get_gpr<uint128_t>(reg2);
    uint128_t dest_qw;

    for (int i = 0; i < 8; i++)
    {
        if ((int16_t)qw1._u16[i] > (int16_t)qw2._u16[i])
            dest_qw._u16[i] = 0xFFFF;
        else
            dest_qw._u16[i] = 0;
    }

    cpu.set_gpr<uint128_t>(dest, dest_qw);
}

/**
 * Parallel Maximize Halfword
 * Compares the eight halfwords in RS and RT each, and stores the greater of each in RD.
 */
void EmotionInterpreter::pmaxh(EmotionEngine &cpu, uint32_t instruction)
{
    uint64_t reg1 = (instruction >> 21) & 0x1F;
    uint64_t reg2 = (instruction >> 16) & 0x1F;
    uint64_t dest = (instruction >> 11) & 0x1F;

    for (int i = 0; i < 8; i++)
    {
        int16_t a = cpu.get_gpr<int16_t>(reg1, i), b = cpu.get_gpr<int16_t>(reg2, i);
        if (a > b)
            cpu.set_gpr<int16_t>(dest, a, i);
        else
            cpu.set_gpr<int16_t>(dest, b, i);
    }
}

/**
 * Parallel Add Byte
 */
void EmotionInterpreter::paddb(EmotionEngine &cpu, uint32_t instruction)
{
    uint64_t reg1 = (instruction >> 21) & 0x1F;
    uint64_t reg2 = (instruction >> 16) & 0x1F;
    uint64_t dest = (instruction >> 11) & 0x1F;

    for (int i = 0; i < 16; i++)
    {
        int8_t byte = cpu.get_gpr<int8_t>(reg1, i);
        byte += cpu.get_gpr<int8_t>(reg2, i);
        cpu.set_gpr<int8_t>(dest, byte, i);
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
        int8_t byte = cpu.get_gpr<int8_t>(reg1, i);
        byte -= cpu.get_gpr<int8_t>(reg2, i);
        cpu.set_gpr<int8_t>(dest, byte, i);
    }
}

/**
 * Parallel Compare for Greater Byte
 * Compares the sixteen signed bytes in RS and RT. Sets 0xFF if RS > RT, 0 if not.
 */
void EmotionInterpreter::pcgtb(EmotionEngine &cpu, uint32_t instruction)
{
    uint64_t reg1 = (instruction >> 21) & 0x1F;
    uint64_t reg2 = (instruction >> 16) & 0x1F;
    uint64_t dest = (instruction >> 11) & 0x1F;

    uint128_t qw1 = cpu.get_gpr<uint128_t>(reg1);
    uint128_t qw2 = cpu.get_gpr<uint128_t>(reg2);
    uint128_t dest_qw;

    for (int i = 0; i < 16; i++)
    {
        if ((int8_t)qw1._u8[i] > (int8_t)qw2._u8[i])
            dest_qw._u8[i] = 0xFF;
        else
            dest_qw._u8[i] = 0;
    }

    cpu.set_gpr<uint128_t>(dest, dest_qw);
}

void EmotionInterpreter::pextlw(EmotionEngine &cpu, uint32_t instruction)
{
    uint64_t reg1 = (instruction >> 21) & 0x1F;
    uint64_t reg2 = (instruction >> 16) & 0x1F;
    uint64_t dest = (instruction >> 11) & 0x1F;

    uint64_t dw1 = cpu.get_gpr<uint64_t>(reg1);
    uint64_t dw2 = cpu.get_gpr<uint64_t>(reg2);

    cpu.set_gpr<uint32_t>(dest, dw2 & 0xFFFFFFFF, 0);
    cpu.set_gpr<uint32_t>(dest, dw1 & 0xFFFFFFFF, 1);
    cpu.set_gpr<uint32_t>(dest, dw2 >> 32, 2);
    cpu.set_gpr<uint32_t>(dest, dw1 >> 32, 3);
}

void EmotionInterpreter::ppacw(EmotionEngine &cpu, uint32_t instruction)
{
    uint64_t reg1 = (instruction >> 21) & 0x1F;
    uint64_t reg2 = (instruction >> 16) & 0x1F;
    uint64_t dest = (instruction >> 11) & 0x1F;

    uint128_t qw1 = cpu.get_gpr<uint128_t>(reg1);
    uint128_t qw2 = cpu.get_gpr<uint128_t>(reg2);

    cpu.set_gpr<uint32_t>(dest, qw2._u64[0] & 0xFFFFFFFF, 0);
    cpu.set_gpr<uint32_t>(dest, qw2._u64[1] & 0xFFFFFFFF, 1);
    cpu.set_gpr<uint32_t>(dest, qw1._u64[0] & 0xFFFFFFFF, 2);
    cpu.set_gpr<uint32_t>(dest, qw1._u64[1] & 0xFFFFFFFF, 3);
}

void EmotionInterpreter::pextlh(EmotionEngine &cpu, uint32_t instruction)
{
    uint64_t reg1 = (instruction >> 21) & 0x1F;
    uint64_t reg2 = (instruction >> 16) & 0x1F;
    uint64_t dest = (instruction >> 11) & 0x1F;

    uint64_t dw1 = cpu.get_gpr<uint64_t>(reg1);
    uint64_t dw2 = cpu.get_gpr<uint64_t>(reg2);

    for (int i = 0; i < 4; i++)
    {
        cpu.set_gpr<uint16_t>(dest, (dw2 >> (i * 16)) & 0xFFFF, (i * 2));
        cpu.set_gpr<uint16_t>(dest, (dw1 >> (i * 16)) & 0xFFFF, (i * 2) + 1);
    }
}

void EmotionInterpreter::ppach(EmotionEngine &cpu, uint32_t instruction)
{
    uint64_t reg1 = (instruction >> 21) & 0x1F;
    uint64_t reg2 = (instruction >> 16) & 0x1F;
    uint64_t dest = (instruction >> 11) & 0x1F;

    uint128_t qw1 = cpu.get_gpr<uint128_t>(reg1);
    uint128_t qw2 = cpu.get_gpr<uint128_t>(reg2);

    for (int i = 0; i < 4; i++)
    {
        cpu.set_gpr<uint16_t>(dest, qw1._u32[i] & 0xFFFF, i + 4);
        cpu.set_gpr<uint16_t>(dest, qw2._u32[i] & 0xFFFF, i);
    }
}

/**
 * Parallel Add Signed Saturation Word
 */
void EmotionInterpreter::paddsw(EmotionEngine &cpu, uint32_t instruction)
{
    uint64_t reg1 = (instruction >> 21) & 0x1F;
    uint64_t reg2 = (instruction >> 16) & 0x1F;
    uint64_t dest = (instruction >> 11) & 0x1F;

    for (int i = 0; i < 4; i++)
    {
        int64_t value = cpu.get_gpr<int32_t>(reg1, i);
        value += cpu.get_gpr<int32_t>(reg2, i);
        if (value > 0x7FFFFFFF)
            value = 0x7FFFFFFF;
        if (value < (int32_t)0x80000000)
            value = (int32_t)0x80000000;
        cpu.set_gpr<int32_t>(dest, (int32_t)value, i);
    }
}

/**
 * Parallel Subtract Signed Saturation Word
 */
void EmotionInterpreter::psubsw(EmotionEngine &cpu, uint32_t instruction)
{
    uint64_t reg1 = (instruction >> 21) & 0x1F;
    uint64_t reg2 = (instruction >> 16) & 0x1F;
    uint64_t dest = (instruction >> 11) & 0x1F;

    for (int i = 0; i < 4; i++)
    {
        int64_t value = cpu.get_gpr<int32_t>(reg1, i);
        value -= cpu.get_gpr<int32_t>(reg2, i);
        if (value > 0x7FFFFFFF)
            value = 0x7FFFFFFF;
        if (value < (int32_t)0x80000000)
            value = (int32_t)0x80000000;
        cpu.set_gpr<int32_t>(dest, (int32_t)value, i);
    }
}

/**
 * Parallel Add Signed Saturation Halfword
 */
void EmotionInterpreter::paddsh(EmotionEngine &cpu, uint32_t instruction)
{
    uint64_t reg1 = (instruction >> 21) & 0x1F;
    uint64_t reg2 = (instruction >> 16) & 0x1F;
    uint64_t dest = (instruction >> 11) & 0x1F;

    for (int i = 0; i < 8; i++)
    {
        int32_t value = cpu.get_gpr<int16_t>(reg1, i);
        value += cpu.get_gpr<int16_t>(reg2, i);
        if (value > 0x7FFF)
            value = 0x7FFF;
        if (value < -0x8000)
            value = -0x8000;
        cpu.set_gpr<int16_t>(dest, value & 0xFFFF, i);
    }
}

/**
 * Parallel Subtract Signed Saturation Halfword
 */
void EmotionInterpreter::psubsh(EmotionEngine &cpu, uint32_t instruction)
{
    uint64_t reg1 = (instruction >> 21) & 0x1F;
    uint64_t reg2 = (instruction >> 16) & 0x1F;
    uint64_t dest = (instruction >> 11) & 0x1F;

    for (int i = 0; i < 8; i++)
    {
        int32_t value = cpu.get_gpr<int16_t>(reg1, i);
        value -= cpu.get_gpr<int16_t>(reg2, i);
        if (value > 0x7FFF)
            value = 0x7FFF;
        if (value < -0x8000)
            value = -0x8000;
        cpu.set_gpr<int16_t>(dest, value & 0xFFFF, i);
    }
}

/**
 * Parallel Add Signed Saturation Byte
 */
void EmotionInterpreter::paddsb(EmotionEngine &cpu, uint32_t instruction)
{
    uint64_t reg1 = (instruction >> 21) & 0x1F;
    uint64_t reg2 = (instruction >> 16) & 0x1F;
    uint64_t dest = (instruction >> 11) & 0x1F;

    for (int i = 0; i < 16; i++)
    {
        int16_t value = cpu.get_gpr<int8_t>(reg1, i);
        value += cpu.get_gpr<int8_t>(reg2, i);
        if (value > 0x7F)
            value = 0x7F;
        if (value < -0x80)
            value = -0x80;
        cpu.set_gpr<int8_t>(dest, value & 0xFF, i);
    }
}

/**
 * Parallel Subtract Signed Saturation Byte
 */
void EmotionInterpreter::psubsb(EmotionEngine &cpu, uint32_t instruction)
{
    uint64_t reg1 = (instruction >> 21) & 0x1F;
    uint64_t reg2 = (instruction >> 16) & 0x1F;
    uint64_t dest = (instruction >> 11) & 0x1F;

    for (int i = 0; i < 16; i++)
    {
        int16_t value = cpu.get_gpr<int8_t>(reg1, i);
        value -= cpu.get_gpr<int8_t>(reg2, i);
        if (value > 0x7F)
            value = 0x7F;
        if (value < -0x80)
            value = -0x80;
        cpu.set_gpr<int8_t>(dest, value & 0xFF, i);
    }
}

void EmotionInterpreter::pextlb(EmotionEngine &cpu, uint32_t instruction)
{
    uint64_t reg1 = (instruction >> 21) & 0x1F;
    uint64_t reg2 = (instruction >> 16) & 0x1F;
    uint64_t dest = (instruction >> 11) & 0x1F;

    uint64_t dw1 = cpu.get_gpr<uint64_t>(reg1);
    uint64_t dw2 = cpu.get_gpr<uint64_t>(reg2);

    for (int i = 0; i < 8; i++)
    {
        cpu.set_gpr<uint8_t>(dest, (dw2 >> (i * 8)) & 0xFF, (i * 2));
        cpu.set_gpr<uint8_t>(dest, (dw1 >> (i * 8)) & 0xFF, (i * 2) + 1);
    }
}

/**
 * Parallel Pack to Byte
 * Splits RS and RT into eight halfwords each. Stores the lower 8-bits of each halfword in RD.
 */
void EmotionInterpreter::ppacb(EmotionEngine &cpu, uint32_t instruction)
{
    uint64_t reg1 = (instruction >> 21) & 0x1F;
    uint64_t reg2 = (instruction >> 16) & 0x1F;
    uint64_t dest = (instruction >> 11) & 0x1F;

    uint128_t qw1 = cpu.get_gpr<uint128_t>(reg1);
    uint128_t qw2 = cpu.get_gpr<uint128_t>(reg2);

    for (int i = 0; i < 8; i++)
    {
        uint8_t byte1 = qw1._u16[i] & 0xFF;
        uint8_t byte2 = qw2._u16[i] & 0xFF;
        cpu.set_gpr<uint8_t>(dest, byte1, i + 8);
        cpu.set_gpr<uint8_t>(dest, byte2, i);
    }
}

/**
 * Parallel Extend from 5 Bits
 * Splits RT into four words, each in 1-5-5-5 format. Converts each word into 8-8-8-8 format and stores it in RD.
 */
void EmotionInterpreter::pext5(EmotionEngine &cpu, uint32_t instruction)
{
    uint64_t source = (instruction >> 16) & 0x1F;
    uint64_t dest = (instruction >> 11) & 0x1F;

    for (int i = 0; i < 4; i++)
    {
        uint32_t packed = cpu.get_gpr<uint32_t>(source, i) & 0xFFFF;
        uint32_t unpacked = (packed & (1 << 15)) << 16;
        unpacked |= ((packed >> 10) & 0x1F) << 19;
        unpacked |= ((packed >> 5) & 0x1F) << 11;
        unpacked |= (packed & 0x1F) << 3;
        cpu.set_gpr<uint32_t>(dest, unpacked, i);
    }
}

/**
 * Parallel Pack to 5 Bits
 * Splits RT into four words, each in 8-8-8-8 format. Truncates each word into 1-5-5-5 format.
 */
void EmotionInterpreter::ppac5(EmotionEngine &cpu, uint32_t instruction)
{
    uint64_t source = (instruction >> 16) & 0x1F;
    uint64_t dest = (instruction >> 11) & 0x1F;

    for (int i = 0; i < 4; i++)
    {
        uint32_t unpacked = cpu.get_gpr<uint32_t>(source, i);

        uint32_t packed = (unpacked >> 3) & 0x1F;
        packed |= ((unpacked >> 11) & 0x1F) << 5;
        packed |= ((unpacked >> 19) & 0x1F) << 10;
        packed |= (unpacked & (1 << 31)) >> 16;
        cpu.set_gpr<uint32_t>(dest, packed, i);
    }
}

void EmotionInterpreter::mmi1(EE_InstrInfo &info, uint32_t instruction)
{
    uint8_t op = (instruction >> 6) & 0x1F;
    switch (op)
    {
        case 0x01:
            info.interpreter_fn = &pabsw;
            info.pipeline = EE_InstrInfo::Pipeline::IntWide;
            info.add_dependency(DependencyType::Write, RegType::GPR, (instruction >> 11) & 0x1F);
            info.add_dependency(DependencyType::Read, RegType::GPR, (instruction >> 16) & 0x1F);
            break;
        case 0x02:
            info.interpreter_fn = &pceqw;
            info.pipeline = EE_InstrInfo::Pipeline::IntWide;
            info.add_dependency(DependencyType::Write, RegType::GPR, (instruction >> 11) & 0x1F);
            info.add_dependency(DependencyType::Read, RegType::GPR, (instruction >> 16) & 0x1F);
            info.add_dependency(DependencyType::Read, RegType::GPR, (instruction >> 21) & 0x1F);
            break;
        case 0x03:
            info.interpreter_fn = &pminw;
            info.pipeline = EE_InstrInfo::Pipeline::IntWide;
            info.add_dependency(DependencyType::Write, RegType::GPR, (instruction >> 11) & 0x1F);
            info.add_dependency(DependencyType::Read, RegType::GPR, (instruction >> 16) & 0x1F);
            info.add_dependency(DependencyType::Read, RegType::GPR, (instruction >> 21) & 0x1F);
            break;
        case 0x04:
            info.interpreter_fn = &padsbh;
            info.pipeline = EE_InstrInfo::Pipeline::IntWide;
            info.add_dependency(DependencyType::Write, RegType::GPR, (instruction >> 11) & 0x1F);
            info.add_dependency(DependencyType::Read, RegType::GPR, (instruction >> 16) & 0x1F);
            info.add_dependency(DependencyType::Read, RegType::GPR, (instruction >> 21) & 0x1F);
            break;
        case 0x05:
            info.interpreter_fn = &pabsh;
            info.pipeline = EE_InstrInfo::Pipeline::IntWide;
            info.add_dependency(DependencyType::Write, RegType::GPR, (instruction >> 11) & 0x1F);
            info.add_dependency(DependencyType::Read, RegType::GPR, (instruction >> 16) & 0x1F);
            break;
        case 0x06:
            info.interpreter_fn = &pceqh;
            info.pipeline = EE_InstrInfo::Pipeline::IntWide;
            info.add_dependency(DependencyType::Write, RegType::GPR, (instruction >> 11) & 0x1F);
            info.add_dependency(DependencyType::Read, RegType::GPR, (instruction >> 16) & 0x1F);
            info.add_dependency(DependencyType::Read, RegType::GPR, (instruction >> 21) & 0x1F);
            break;
        case 0x07:
            info.interpreter_fn = &pminh;
            info.pipeline = EE_InstrInfo::Pipeline::IntWide;
            info.add_dependency(DependencyType::Write, RegType::GPR, (instruction >> 11) & 0x1F);
            info.add_dependency(DependencyType::Read, RegType::GPR, (instruction >> 16) & 0x1F);
            info.add_dependency(DependencyType::Read, RegType::GPR, (instruction >> 21) & 0x1F);
            break;
        case 0x0A:
            info.interpreter_fn = &pceqb;
            info.pipeline = EE_InstrInfo::Pipeline::IntWide;
            info.add_dependency(DependencyType::Write, RegType::GPR, (instruction >> 11) & 0x1F);
            info.add_dependency(DependencyType::Read, RegType::GPR, (instruction >> 16) & 0x1F);
            info.add_dependency(DependencyType::Read, RegType::GPR, (instruction >> 21) & 0x1F);
            break;
        case 0x10:
            info.interpreter_fn = &padduw;
            info.pipeline = EE_InstrInfo::Pipeline::IntWide;
            info.add_dependency(DependencyType::Write, RegType::GPR, (instruction >> 11) & 0x1F);
            info.add_dependency(DependencyType::Read, RegType::GPR, (instruction >> 16) & 0x1F);
            info.add_dependency(DependencyType::Read, RegType::GPR, (instruction >> 21) & 0x1F);
            break;
        case 0x11:
            info.interpreter_fn = &psubuw;
            info.pipeline = EE_InstrInfo::Pipeline::IntWide;
            info.add_dependency(DependencyType::Write, RegType::GPR, (instruction >> 11) & 0x1F);
            info.add_dependency(DependencyType::Read, RegType::GPR, (instruction >> 16) & 0x1F);
            info.add_dependency(DependencyType::Read, RegType::GPR, (instruction >> 21) & 0x1F);
            break;
        case 0x12:
            info.interpreter_fn = &pextuw;
            info.pipeline = EE_InstrInfo::Pipeline::IntWide;
            info.add_dependency(DependencyType::Write, RegType::GPR, (instruction >> 11) & 0x1F);
            info.add_dependency(DependencyType::Read, RegType::GPR, (instruction >> 16) & 0x1F);
            info.add_dependency(DependencyType::Read, RegType::GPR, (instruction >> 21) & 0x1F);
            break;
        case 0x14:
            info.interpreter_fn = &padduh;
            info.pipeline = EE_InstrInfo::Pipeline::IntWide;
            info.add_dependency(DependencyType::Write, RegType::GPR, (instruction >> 11) & 0x1F);
            info.add_dependency(DependencyType::Read, RegType::GPR, (instruction >> 16) & 0x1F);
            info.add_dependency(DependencyType::Read, RegType::GPR, (instruction >> 21) & 0x1F);
            break;
        case 0x15:
            info.interpreter_fn = &psubuh;
            info.pipeline = EE_InstrInfo::Pipeline::IntWide;
            info.add_dependency(DependencyType::Write, RegType::GPR, (instruction >> 11) & 0x1F);
            info.add_dependency(DependencyType::Read, RegType::GPR, (instruction >> 16) & 0x1F);
            info.add_dependency(DependencyType::Read, RegType::GPR, (instruction >> 21) & 0x1F);
            break;
        case 0x16:
            info.interpreter_fn = &pextuh;
            info.pipeline = EE_InstrInfo::Pipeline::IntWide;
            info.add_dependency(DependencyType::Write, RegType::GPR, (instruction >> 11) & 0x1F);
            info.add_dependency(DependencyType::Read, RegType::GPR, (instruction >> 16) & 0x1F);
            info.add_dependency(DependencyType::Read, RegType::GPR, (instruction >> 21) & 0x1F);
            break;
        case 0x18:
            info.interpreter_fn = &paddub;
            info.pipeline = EE_InstrInfo::Pipeline::IntWide;
            info.add_dependency(DependencyType::Write, RegType::GPR, (instruction >> 11) & 0x1F);
            info.add_dependency(DependencyType::Read, RegType::GPR, (instruction >> 16) & 0x1F);
            info.add_dependency(DependencyType::Read, RegType::GPR, (instruction >> 21) & 0x1F);
            break;
        case 0x19:
            info.interpreter_fn = &psubub;
            info.pipeline = EE_InstrInfo::Pipeline::IntWide;
            info.add_dependency(DependencyType::Write, RegType::GPR, (instruction >> 11) & 0x1F);
            info.add_dependency(DependencyType::Read, RegType::GPR, (instruction >> 16) & 0x1F);
            info.add_dependency(DependencyType::Read, RegType::GPR, (instruction >> 21) & 0x1F);
            break;
        case 0x1A:
            info.interpreter_fn = &pextub;
            info.pipeline = EE_InstrInfo::Pipeline::IntWide;
            info.add_dependency(DependencyType::Write, RegType::GPR, (instruction >> 11) & 0x1F);
            info.add_dependency(DependencyType::Read, RegType::GPR, (instruction >> 16) & 0x1F);
            info.add_dependency(DependencyType::Read, RegType::GPR, (instruction >> 21) & 0x1F);
            break;
        case 0x1B:
            info.interpreter_fn = &qfsrv;
            info.pipeline = EE_InstrInfo::Pipeline::IntWide;
            info.add_dependency(DependencyType::Write, RegType::GPR, (instruction >> 11) & 0x1F);
            info.add_dependency(DependencyType::Read, RegType::GPR, (uint8_t)EE_SpecialReg::SA);
            info.add_dependency(DependencyType::Read, RegType::GPR, (instruction >> 16) & 0x1F);
            info.add_dependency(DependencyType::Read, RegType::GPR, (instruction >> 21) & 0x1F);
            break;
        default:
            return unknown_op("mmi1", instruction, op);
    }
}

/**
 * Parallel Absolute Word
 */
void EmotionInterpreter::pabsw(EmotionEngine &cpu, uint32_t instruction)
{
    uint32_t source = (instruction >> 16) & 0x1F;
    uint32_t dest = (instruction >> 11) & 0x1F;

    for (int i = 0; i < 4; i++)
    {
        uint32_t word = cpu.get_gpr<uint32_t>(source, i);
        int32_t sword = cpu.get_gpr<int32_t>(source, i);
        if (word == 0x80000000)
            cpu.set_gpr<uint32_t>(dest, 0x7FFFFFFF, i);
        else if (sword < 0)
            cpu.set_gpr<uint32_t>(dest, -sword, i);
        else
            cpu.set_gpr<uint32_t>(dest, word, i);
    }
}

/**
 * Parallel Compare for Equal Word
 * Compares the four words in RS and RT. Sets 0xFFFFFFFF if equal, 0 if not.
 */
void EmotionInterpreter::pceqw(EmotionEngine &cpu, uint32_t instruction)
{
    uint64_t reg1 = (instruction >> 21) & 0x1F;
    uint64_t reg2 = (instruction >> 16) & 0x1F;
    uint64_t dest = (instruction >> 11) & 0x1F;

    uint128_t qw1 = cpu.get_gpr<uint128_t>(reg1);
    uint128_t qw2 = cpu.get_gpr<uint128_t>(reg2);
    uint128_t dest_qw;

    for (int i = 0; i < 4; i++)
    {
        if (qw1._u32[i] == qw2._u32[i])
            dest_qw._u32[i] = 0xFFFFFFFF;
        else
            dest_qw._u32[i] = 0;
    }

    cpu.set_gpr<uint128_t>(dest, dest_qw);
}

/**
 * Parallel Minimize Word
 * Compares the four words in RS and RT each, and stores the lesser of each in RD.
 */
void EmotionInterpreter::pminw(EmotionEngine &cpu, uint32_t instruction)
{
    uint64_t reg1 = (instruction >> 21) & 0x1F;
    uint64_t reg2 = (instruction >> 16) & 0x1F;
    uint64_t dest = (instruction >> 11) & 0x1F;

    for (int i = 0; i < 4; i++)
    {
        int32_t a = cpu.get_gpr<int32_t>(reg1, i), b = cpu.get_gpr<int32_t>(reg2, i);
        if (a < b)
            cpu.set_gpr<int32_t>(dest, a, i);
        else
            cpu.set_gpr<int32_t>(dest, b, i);
    }
}

/**
 * Parallel Absolute Halfword
 */
void EmotionInterpreter::pabsh(EmotionEngine &cpu, uint32_t instruction)
{
    uint32_t source = (instruction >> 16) & 0x1F;
    uint32_t dest = (instruction >> 11) & 0x1F;

    for (int i = 0; i < 8; i++)
    {
        int16_t shalfword = cpu.get_gpr<int16_t>(source, i);
        uint16_t halfword = cpu.get_gpr<uint16_t>(source, i);
        if (halfword == 0x8000)
            cpu.set_gpr<uint16_t>(dest, 0x7FFF, i);
        else if (shalfword < 0)
            cpu.set_gpr<uint16_t>(dest, -shalfword, i);
        else
            cpu.set_gpr<uint16_t>(dest, halfword, i);
    }
}

/**
 * Parallel Compare for Equal Halfword
 * Compares the eight halfwords in RS and RT. Sets 0xFFFF if equal, 0 if not.
 */
void EmotionInterpreter::pceqh(EmotionEngine &cpu, uint32_t instruction)
{
    uint64_t reg1 = (instruction >> 21) & 0x1F;
    uint64_t reg2 = (instruction >> 16) & 0x1F;
    uint64_t dest = (instruction >> 11) & 0x1F;

    uint128_t qw1 = cpu.get_gpr<uint128_t>(reg1);
    uint128_t qw2 = cpu.get_gpr<uint128_t>(reg2);
    uint128_t dest_qw;

    for (int i = 0; i < 8; i++)
    {
        if (qw1._u16[i] == qw2._u16[i])
            dest_qw._u16[i] = 0xFFFF;
        else
            dest_qw._u16[i] = 0;
    }

    cpu.set_gpr<uint128_t>(dest, dest_qw);
}

/**
 * Parallel Add/Subtract Halfword
 */
void EmotionInterpreter::padsbh(EmotionEngine &cpu, uint32_t instruction)
{
    uint64_t reg1 = (instruction >> 21) & 0x1F;
    uint64_t reg2 = (instruction >> 16) & 0x1F;
    uint64_t dest = (instruction >> 11) & 0x1F;

    for (int i = 0; i < 4; i++)
    {
        int16_t hw1, hw2;
        hw1 = cpu.get_gpr<int16_t>(reg1, i);
        hw2 = cpu.get_gpr<int16_t>(reg1, i + 4);
        hw1 -= cpu.get_gpr<int16_t>(reg2, i);
        hw2 += cpu.get_gpr<int16_t>(reg2, i + 4);
        cpu.set_gpr<int16_t>(dest, hw1, i);
        cpu.set_gpr<int16_t>(dest, hw2, i + 4);
    }
}

/**
 * Parallel Add with Unsigned Saturation Word
 * Splits the 128-bit registers RS and RT into four unsigned words each and adds them together.
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

/**
 * Parallel Subtract with Unsigned Saturation Word
 * Splits the 128-bit registers RS and RT into four unsigned words each and subtracts them together.
 * If the result is less than 0, it is saturated to that value.
 */
void EmotionInterpreter::psubuw(EmotionEngine &cpu, uint32_t instruction)
{
    uint64_t reg1 = (instruction >> 21) & 0x1F;
    uint64_t reg2 = (instruction >> 16) & 0x1F;
    uint64_t dest = (instruction >> 11) & 0x1F;

    for (int i = 0; i < 4; i++)
    {
        uint64_t result = cpu.get_gpr<uint32_t>(reg1, i);
        result -= cpu.get_gpr<uint32_t>(reg2, i);
        if (result > 0xFFFFFFFF)
            result = 0x0;
        cpu.set_gpr<uint32_t>(dest, (uint32_t)result, i);
    }
}

/**
 * Parallel Minimize Halfword
 * Compares the eight halfwords in RS and RT each, and stores the lesser of each in RD.
 */
void EmotionInterpreter::pminh(EmotionEngine &cpu, uint32_t instruction)
{
    uint64_t reg1 = (instruction >> 21) & 0x1F;
    uint64_t reg2 = (instruction >> 16) & 0x1F;
    uint64_t dest = (instruction >> 11) & 0x1F;

    for (int i = 0; i < 8; i++)
    {
        int16_t a = cpu.get_gpr<int16_t>(reg1, i), b = cpu.get_gpr<int16_t>(reg2, i);
        if (a < b)
            cpu.set_gpr<int16_t>(dest, a, i);
        else
            cpu.set_gpr<int16_t>(dest, b, i);
    }
}

/**
 * Parallel Compare for Equal Byte
 * Compares the sixteen bytes in RS and RT, setting 0xFF if equal and 0 if not.
 */
void EmotionInterpreter::pceqb(EmotionEngine &cpu, uint32_t instruction)
{
    uint64_t reg1 = (instruction >> 21) & 0x1F;
    uint64_t reg2 = (instruction >> 16) & 0x1F;
    uint64_t dest = (instruction >> 11) & 0x1F;

    uint128_t qw1 = cpu.get_gpr<uint128_t>(reg1);
    uint128_t qw2 = cpu.get_gpr<uint128_t>(reg2);
    uint128_t dest_qw;

    for (int i = 0; i < 16; i++)
    {
        if (qw1._u8[i] == qw2._u8[i])
            dest_qw._u8[i] = 0xFF;
        else
            dest_qw._u8[i] = 0;
    }

    cpu.set_gpr<uint128_t>(dest, dest_qw);
}

void EmotionInterpreter::pextuw(EmotionEngine &cpu, uint32_t instruction)
{
    uint64_t reg1 = (instruction >> 21) & 0x1F;
    uint64_t reg2 = (instruction >> 16) & 0x1F;
    uint64_t dest = (instruction >> 11) & 0x1F;

    uint64_t dw1 = cpu.get_gpr<uint64_t>(reg1, 1);
    uint64_t dw2 = cpu.get_gpr<uint64_t>(reg2, 1);

    cpu.set_gpr<uint32_t>(dest, dw2 & 0xFFFFFFFF, 0);
    cpu.set_gpr<uint32_t>(dest, dw1 & 0xFFFFFFFF, 1);
    cpu.set_gpr<uint32_t>(dest, dw2 >> 32, 2);
    cpu.set_gpr<uint32_t>(dest, dw1 >> 32, 3);
}

/**
 * Parallel Add with Unsigned Saturation Halfword
 */
void EmotionInterpreter::padduh(EmotionEngine &cpu, uint32_t instruction)
{
    uint64_t reg1 = (instruction >> 21) & 0x1F;
    uint64_t reg2 = (instruction >> 16) & 0x1F;
    uint64_t dest = (instruction >> 11) & 0x1F;

    for (int i = 0; i < 8; i++)
    {
        uint32_t result = cpu.get_gpr<uint16_t>(reg1, i);
        result += cpu.get_gpr<uint16_t>(reg2, i);
        if (result > 0xFFFF)
            result = 0xFFFF;
        cpu.set_gpr<uint16_t>(dest, (uint16_t)result, i);
    }
}

/**
 * Parallel Subtract with Unsigned Saturation Halfword
 */
void EmotionInterpreter::psubuh(EmotionEngine &cpu, uint32_t instruction)
{
    uint64_t reg1 = (instruction >> 21) & 0x1F;
    uint64_t reg2 = (instruction >> 16) & 0x1F;
    uint64_t dest = (instruction >> 11) & 0x1F;

    for (int i = 0; i < 8; i++)
    {
        uint32_t result = cpu.get_gpr<uint16_t>(reg1, i);
        result -= cpu.get_gpr<uint16_t>(reg2, i);
        if (result > 0xFFFF)
            result = 0x0;
        cpu.set_gpr<uint16_t>(dest, (uint16_t)result, i);
    }
}

void EmotionInterpreter::pextuh(EmotionEngine &cpu, uint32_t instruction)
{
    uint64_t reg1 = (instruction >> 21) & 0x1F;
    uint64_t reg2 = (instruction >> 16) & 0x1F;
    uint64_t dest = (instruction >> 11) & 0x1F;

    uint64_t dw1 = cpu.get_gpr<uint64_t>(reg1, 1);
    uint64_t dw2 = cpu.get_gpr<uint64_t>(reg2, 1);

    for (int i = 0; i < 4; i++)
    {
        cpu.set_gpr<uint16_t>(dest, (dw2 >> (i * 16)) & 0xFFFF, (i * 2));
        cpu.set_gpr<uint16_t>(dest, (dw1 >> (i * 16)) & 0xFFFF, (i * 2) + 1);
    }
}

/**
 * Parallel Add with Unsigned Saturation Byte
 * Splits the 128-bit registers RS and RT into sixteen unsigned bytes each and adds them together.
 * If the result is greater than 0xFF, it is saturated to that value.
 */
void EmotionInterpreter::paddub(EmotionEngine &cpu, uint32_t instruction)
{
    uint64_t reg1 = (instruction >> 21) & 0x1F;
    uint64_t reg2 = (instruction >> 16) & 0x1F;
    uint64_t dest = (instruction >> 11) & 0x1F;

    for (int i = 0; i < 16; i++)
    {
        uint16_t result = cpu.get_gpr<uint8_t>(reg1, i);
        result += cpu.get_gpr<uint8_t>(reg2, i);
        if (result > 0xFF)
            result = 0xFF;
        cpu.set_gpr<uint8_t>(dest, (uint8_t)result, i);
    }
}

/**
 * Parallel Subtract with Unsigned Saturation Byte
 * Splits the 128-bit registers RS and RT into sixteen unsigned bytes each and subtracts them together.
 * If the result is less than 0, it is saturated to that value.
 */
void EmotionInterpreter::psubub(EmotionEngine &cpu, uint32_t instruction)
{
    uint64_t reg1 = (instruction >> 21) & 0x1F;
    uint64_t reg2 = (instruction >> 16) & 0x1F;
    uint64_t dest = (instruction >> 11) & 0x1F;

    for (int i = 0; i < 16; i++)
    {
        uint16_t result = cpu.get_gpr<uint8_t>(reg1, i);
        result -= cpu.get_gpr<uint8_t>(reg2, i);
        if (result > 0xFF)
            result = 0x0;
        cpu.set_gpr<uint8_t>(dest, (uint8_t)result, i);
    }
}

void EmotionInterpreter::pextub(EmotionEngine &cpu, uint32_t instruction)
{
    uint64_t reg1 = (instruction >> 21) & 0x1F;
    uint64_t reg2 = (instruction >> 16) & 0x1F;
    uint64_t dest = (instruction >> 11) & 0x1F;

    uint64_t dw1 = cpu.get_gpr<uint64_t>(reg1, 1);
    uint64_t dw2 = cpu.get_gpr<uint64_t>(reg2, 1);

    for (int i = 0; i < 8; i++)
    {
        cpu.set_gpr<uint8_t>(dest, (dw2 >> (i * 8)) & 0xFF, (i * 2));
        cpu.set_gpr<uint8_t>(dest, (dw1 >> (i * 8)) & 0xFF, (i * 2) + 1);
    }
}

/**
 * Quadword Funnel Shift Right Variable
 * Concatenates RS and RT as a 256-bit integer, then shifts it by the special register SA.
 */
void EmotionInterpreter::qfsrv(EmotionEngine &cpu, uint32_t instruction)
{
    uint64_t reg1 = (instruction >> 21) & 0x1F;
    uint64_t reg2 = (instruction >> 16) & 0x1F;
    uint64_t dest = (instruction >> 11) & 0x1F;

    uint128_t rs = cpu.get_gpr<uint128_t>(reg1);
    uint128_t rt = cpu.get_gpr<uint128_t>(reg2);

    int shift = cpu.get_SA() * 8;
    uint128_t dest_value;
    if (!shift)
        dest_value = rt;
    else
    {
        if (shift < 64)
        {
            dest_value._u64[0] = rt._u64[0] >> shift;
            dest_value._u64[1] = rt._u64[1] >> shift;
            dest_value._u64[0] |= rt._u64[1] << (64 - shift);
            dest_value._u64[1] |= rs._u64[0] << (64 - shift);
        }
        else
        {
            dest_value._u64[0] = rt._u64[1] >> (shift - 64);
            dest_value._u64[1] = rs._u64[0] >> (shift - 64);
            if (shift != 64)
            {
                dest_value._u64[0] |= rs._u64[0] << (128u - shift);
                dest_value._u64[1] |= rs._u64[1] << (128u - shift);
            }
        }
    }
    cpu.set_gpr<uint128_t>(dest, dest_value);
}

void EmotionInterpreter::mmi2(EE_InstrInfo &info, uint32_t instruction)
{
    uint8_t op = (instruction >> 6) & 0x1F;
    switch (op)
    {
        case 0x00:
            info.interpreter_fn = &pmaddw;
            info.pipeline = EE_InstrInfo::Pipeline::IntWide;
            info.latency = 4;
            info.throughput = 2;
            info.instruction_type = EE_InstrInfo::InstructionType::MADD;
            info.add_dependency(DependencyType::Write, RegType::GPR, (uint8_t)EE_SpecialReg::LO);
            info.add_dependency(DependencyType::Write, RegType::GPR, (uint8_t)EE_SpecialReg::LO1);
            info.add_dependency(DependencyType::Write, RegType::GPR, (uint8_t)EE_SpecialReg::HI);
            info.add_dependency(DependencyType::Write, RegType::GPR, (uint8_t)EE_SpecialReg::HI1);
            info.add_dependency(DependencyType::Write, RegType::GPR, (instruction >> 11) & 0x1F);
            info.add_dependency(DependencyType::Read, RegType::GPR, (instruction >> 21) & 0x1F);
            info.add_dependency(DependencyType::Read, RegType::GPR, (instruction >> 16) & 0x1F);
            break;
        case 0x02:
            info.interpreter_fn = &psllvw;
            info.pipeline = EE_InstrInfo::Pipeline::IntWide;
            info.add_dependency(DependencyType::Write, RegType::GPR, (instruction >> 11) & 0x1F);
            info.add_dependency(DependencyType::Read, RegType::GPR, (instruction >> 16) & 0x1F);
            info.add_dependency(DependencyType::Read, RegType::GPR, (instruction >> 21) & 0x1F);
            break;
        case 0x03:
            info.interpreter_fn = &psrlvw;
            info.pipeline = EE_InstrInfo::Pipeline::IntWide;
            info.add_dependency(DependencyType::Write, RegType::GPR, (instruction >> 11) & 0x1F);
            info.add_dependency(DependencyType::Read, RegType::GPR, (instruction >> 16) & 0x1F);
            info.add_dependency(DependencyType::Read, RegType::GPR, (instruction >> 21) & 0x1F);
            break;
        case 0x04:
            info.interpreter_fn = &pmsubw;
            info.pipeline = EE_InstrInfo::Pipeline::IntWide;
            info.latency = 4;
            info.throughput = 2;
            info.instruction_type = EE_InstrInfo::InstructionType::MADD;
            info.add_dependency(DependencyType::Write, RegType::GPR, (uint8_t)EE_SpecialReg::LO);
            info.add_dependency(DependencyType::Write, RegType::GPR, (uint8_t)EE_SpecialReg::LO1);
            info.add_dependency(DependencyType::Write, RegType::GPR, (uint8_t)EE_SpecialReg::HI);
            info.add_dependency(DependencyType::Write, RegType::GPR, (uint8_t)EE_SpecialReg::HI1);
            info.add_dependency(DependencyType::Write, RegType::GPR, (instruction >> 11) & 0x1F);
            info.add_dependency(DependencyType::Read, RegType::GPR, (instruction >> 21) & 0x1F);
            info.add_dependency(DependencyType::Read, RegType::GPR, (instruction >> 16) & 0x1F);
            break;
        case 0x08:
            info.interpreter_fn = &pmfhi;
            info.pipeline = EE_InstrInfo::Pipeline::IntWide;
            info.add_dependency(DependencyType::Write, RegType::GPR, (instruction >> 11) & 0x1F);
            info.add_dependency(DependencyType::Read, RegType::GPR, (uint8_t)EE_SpecialReg::HI);
            info.add_dependency(DependencyType::Read, RegType::GPR, (uint8_t)EE_SpecialReg::HI1);
            break;
        case 0x09:
            info.interpreter_fn = &pmflo;
            info.pipeline = EE_InstrInfo::Pipeline::IntWide;
            info.add_dependency(DependencyType::Write, RegType::GPR, (instruction >> 11) & 0x1F);
            info.add_dependency(DependencyType::Read, RegType::GPR, (uint8_t)EE_SpecialReg::LO);
            info.add_dependency(DependencyType::Read, RegType::GPR, (uint8_t)EE_SpecialReg::LO1);
            break;
        case 0x0A:
            info.interpreter_fn = &pinth;
            info.pipeline = EE_InstrInfo::Pipeline::IntWide;
            info.add_dependency(DependencyType::Write, RegType::GPR, (instruction >> 11) & 0x1F);
            info.add_dependency(DependencyType::Read, RegType::GPR, (instruction >> 16) & 0x1F);
            info.add_dependency(DependencyType::Read, RegType::GPR, (instruction >> 21) & 0x1F);
            break;
        case 0x0C:
            info.interpreter_fn = &pmultw;
            info.pipeline = EE_InstrInfo::Pipeline::IntWide;
            info.latency = 4;
            info.throughput = 2;
            info.instruction_type = EE_InstrInfo::InstructionType::MULT;
            info.add_dependency(DependencyType::Write, RegType::GPR, (uint8_t)EE_SpecialReg::LO);
            info.add_dependency(DependencyType::Write, RegType::GPR, (uint8_t)EE_SpecialReg::LO1);
            info.add_dependency(DependencyType::Write, RegType::GPR, (uint8_t)EE_SpecialReg::HI);
            info.add_dependency(DependencyType::Write, RegType::GPR, (uint8_t)EE_SpecialReg::HI1);
            info.add_dependency(DependencyType::Write, RegType::GPR, (instruction >> 11) & 0x1F);
            info.add_dependency(DependencyType::Read, RegType::GPR, (instruction >> 21) & 0x1F);
            info.add_dependency(DependencyType::Read, RegType::GPR, (instruction >> 16) & 0x1F);
            break;
        case 0x0D:
            info.interpreter_fn = &pdivw;
            info.pipeline = EE_InstrInfo::Pipeline::IntWide;
            info.latency = 37;
            info.throughput = 37;
            info.instruction_type = EE_InstrInfo::InstructionType::DIV;
            info.add_dependency(DependencyType::Write, RegType::GPR, (uint8_t)EE_SpecialReg::LO);
            info.add_dependency(DependencyType::Write, RegType::GPR, (uint8_t)EE_SpecialReg::LO1);
            info.add_dependency(DependencyType::Write, RegType::GPR, (uint8_t)EE_SpecialReg::HI);
            info.add_dependency(DependencyType::Write, RegType::GPR, (uint8_t)EE_SpecialReg::HI1);
            info.add_dependency(DependencyType::Read, RegType::GPR, (instruction >> 21) & 0x1F);
            info.add_dependency(DependencyType::Read, RegType::GPR, (instruction >> 16) & 0x1F);
            break;
        case 0x0E:
            info.interpreter_fn = &pcpyld;
            info.pipeline = EE_InstrInfo::Pipeline::IntWide;
            info.add_dependency(DependencyType::Write, RegType::GPR, (instruction >> 11) & 0x1F);
            info.add_dependency(DependencyType::Read, RegType::GPR, (instruction >> 16) & 0x1F);
            info.add_dependency(DependencyType::Read, RegType::GPR, (instruction >> 21) & 0x1F);
            break;
        case 0x10:
            info.interpreter_fn = &pmaddh;
            info.pipeline = EE_InstrInfo::Pipeline::IntWide;
            info.latency = 4;
            info.throughput = 2;
            info.instruction_type = EE_InstrInfo::InstructionType::MADD;
            info.add_dependency(DependencyType::Write, RegType::GPR, (uint8_t)EE_SpecialReg::LO);
            info.add_dependency(DependencyType::Write, RegType::GPR, (uint8_t)EE_SpecialReg::LO1);
            info.add_dependency(DependencyType::Write, RegType::GPR, (uint8_t)EE_SpecialReg::HI);
            info.add_dependency(DependencyType::Write, RegType::GPR, (uint8_t)EE_SpecialReg::HI1);
            info.add_dependency(DependencyType::Write, RegType::GPR, (instruction >> 11) & 0x1F);
            info.add_dependency(DependencyType::Read, RegType::GPR, (instruction >> 21) & 0x1F);
            info.add_dependency(DependencyType::Read, RegType::GPR, (instruction >> 16) & 0x1F);
            break;
        case 0x11:
            info.interpreter_fn = &phmadh;
            info.pipeline = EE_InstrInfo::Pipeline::IntWide;
            info.latency = 4;
            info.throughput = 2;
            info.instruction_type = EE_InstrInfo::InstructionType::MADD;
            info.add_dependency(DependencyType::Write, RegType::GPR, (uint8_t)EE_SpecialReg::LO);
            info.add_dependency(DependencyType::Write, RegType::GPR, (uint8_t)EE_SpecialReg::LO1);
            info.add_dependency(DependencyType::Write, RegType::GPR, (uint8_t)EE_SpecialReg::HI);
            info.add_dependency(DependencyType::Write, RegType::GPR, (uint8_t)EE_SpecialReg::HI1);
            info.add_dependency(DependencyType::Write, RegType::GPR, (instruction >> 11) & 0x1F);
            info.add_dependency(DependencyType::Read, RegType::GPR, (instruction >> 21) & 0x1F);
            info.add_dependency(DependencyType::Read, RegType::GPR, (instruction >> 16) & 0x1F);
            break;
        case 0x12:
            info.interpreter_fn = &pand;
            info.pipeline = EE_InstrInfo::Pipeline::IntWide;
            info.add_dependency(DependencyType::Write, RegType::GPR, (instruction >> 11) & 0x1F);
            info.add_dependency(DependencyType::Read, RegType::GPR, (instruction >> 16) & 0x1F);
            info.add_dependency(DependencyType::Read, RegType::GPR, (instruction >> 21) & 0x1F);
            break;
        case 0x13:
            info.interpreter_fn = &pxor;
            info.pipeline = EE_InstrInfo::Pipeline::IntWide;
            info.add_dependency(DependencyType::Write, RegType::GPR, (instruction >> 11) & 0x1F);
            info.add_dependency(DependencyType::Read, RegType::GPR, (instruction >> 16) & 0x1F);
            info.add_dependency(DependencyType::Read, RegType::GPR, (instruction >> 21) & 0x1F);
            break;
        case 0x14:
            info.interpreter_fn = &pmsubh;
            info.pipeline = EE_InstrInfo::Pipeline::IntWide;
            info.latency = 4;
            info.throughput = 2;
            info.instruction_type = EE_InstrInfo::InstructionType::MADD;
            info.add_dependency(DependencyType::Write, RegType::GPR, (uint8_t)EE_SpecialReg::LO);
            info.add_dependency(DependencyType::Write, RegType::GPR, (uint8_t)EE_SpecialReg::LO1);
            info.add_dependency(DependencyType::Write, RegType::GPR, (uint8_t)EE_SpecialReg::HI);
            info.add_dependency(DependencyType::Write, RegType::GPR, (uint8_t)EE_SpecialReg::HI1);
            info.add_dependency(DependencyType::Write, RegType::GPR, (instruction >> 11) & 0x1F);
            info.add_dependency(DependencyType::Read, RegType::GPR, (instruction >> 21) & 0x1F);
            info.add_dependency(DependencyType::Read, RegType::GPR, (instruction >> 16) & 0x1F);
            break;
        case 0x15:
            info.interpreter_fn = &phmsbh;
            info.pipeline = EE_InstrInfo::Pipeline::IntWide;
            info.latency = 4;
            info.throughput = 2;
            info.instruction_type = EE_InstrInfo::InstructionType::MADD;
            info.add_dependency(DependencyType::Write, RegType::GPR, (uint8_t)EE_SpecialReg::LO);
            info.add_dependency(DependencyType::Write, RegType::GPR, (uint8_t)EE_SpecialReg::LO1);
            info.add_dependency(DependencyType::Write, RegType::GPR, (uint8_t)EE_SpecialReg::HI);
            info.add_dependency(DependencyType::Write, RegType::GPR, (uint8_t)EE_SpecialReg::HI1);
            info.add_dependency(DependencyType::Write, RegType::GPR, (instruction >> 11) & 0x1F);
            info.add_dependency(DependencyType::Read, RegType::GPR, (instruction >> 21) & 0x1F);
            info.add_dependency(DependencyType::Read, RegType::GPR, (instruction >> 16) & 0x1F);
            break;
        case 0x1A:
            info.interpreter_fn = &pexeh;
            info.pipeline = EE_InstrInfo::Pipeline::IntWide;
            info.add_dependency(DependencyType::Write, RegType::GPR, (instruction >> 11) & 0x1F);
            info.add_dependency(DependencyType::Read, RegType::GPR, (instruction >> 16) & 0x1F);
            break;
        case 0x1B:
            info.interpreter_fn = &prevh;
            info.pipeline = EE_InstrInfo::Pipeline::IntWide;
            info.add_dependency(DependencyType::Write, RegType::GPR, (instruction >> 11) & 0x1F);
            info.add_dependency(DependencyType::Read, RegType::GPR, (instruction >> 16) & 0x1F);
            break;
        case 0x1C:
            info.interpreter_fn = &pmulth;
            info.pipeline = EE_InstrInfo::Pipeline::IntWide;
            info.latency = 4;
            info.throughput = 2;
            info.instruction_type = EE_InstrInfo::InstructionType::MULT;
            info.add_dependency(DependencyType::Write, RegType::GPR, (uint8_t)EE_SpecialReg::LO);
            info.add_dependency(DependencyType::Write, RegType::GPR, (uint8_t)EE_SpecialReg::LO1);
            info.add_dependency(DependencyType::Write, RegType::GPR, (uint8_t)EE_SpecialReg::HI);
            info.add_dependency(DependencyType::Write, RegType::GPR, (uint8_t)EE_SpecialReg::HI1);
            info.add_dependency(DependencyType::Write, RegType::GPR, (instruction >> 11) & 0x1F);
            info.add_dependency(DependencyType::Read, RegType::GPR, (instruction >> 21) & 0x1F);
            info.add_dependency(DependencyType::Read, RegType::GPR, (instruction >> 16) & 0x1F);
            break;
        case 0x1D:
            info.interpreter_fn = &pdivbw;
            info.pipeline = EE_InstrInfo::Pipeline::IntWide;
            info.latency = 37;
            info.throughput = 37;
            info.instruction_type = EE_InstrInfo::InstructionType::MULT;
            info.add_dependency(DependencyType::Write, RegType::GPR, (uint8_t)EE_SpecialReg::LO);
            info.add_dependency(DependencyType::Write, RegType::GPR, (uint8_t)EE_SpecialReg::LO1);
            info.add_dependency(DependencyType::Write, RegType::GPR, (uint8_t)EE_SpecialReg::HI);
            info.add_dependency(DependencyType::Write, RegType::GPR, (uint8_t)EE_SpecialReg::HI1);
            info.add_dependency(DependencyType::Read, RegType::GPR, (instruction >> 21) & 0x1F);
            info.add_dependency(DependencyType::Read, RegType::GPR, (instruction >> 16) & 0x1F);
            break;
        case 0x1E:
            info.interpreter_fn = &pexew;
            info.pipeline = EE_InstrInfo::Pipeline::IntWide;
            info.add_dependency(DependencyType::Write, RegType::GPR, (instruction >> 11) & 0x1F);
            info.add_dependency(DependencyType::Read, RegType::GPR, (instruction >> 16) & 0x1F);
            break;
        case 0x1F:
            info.interpreter_fn = &prot3w;
            info.pipeline = EE_InstrInfo::Pipeline::IntWide;
            info.add_dependency(DependencyType::Write, RegType::GPR, (instruction >> 11) & 0x1F);
            info.add_dependency(DependencyType::Read, RegType::GPR, (instruction >> 16) & 0x1F);
            break;
        default:
            unknown_op("mmi2", instruction, op);
    }
}

/**
* Parallel Multiply-Add Word
- Note! It looks like the PS2 has a multiplication error when calculating the value for HI.
  With some tinkering depending on the mutliplication operands we can get this accurate on the autotests.
*/
void EmotionInterpreter::pmaddw(EmotionEngine &cpu, uint32_t instruction)
{
    uint128_t reg1 = cpu.get_gpr<uint128_t>((instruction >> 21) & 0x1F);
    uint128_t reg2 = cpu.get_gpr<uint128_t>((instruction >> 16) & 0x1F);
    uint32_t dest = (instruction >> 11) & 0x1F;
    uint128_t lo, hi;    
    int64_t result, result2;
    int64_t op1, op2;
    lo._u64[0] = cpu.get_LO();
    lo._u64[1] = cpu.get_LO1();

    hi._u64[0] = cpu.get_HI();
    hi._u64[1] = cpu.get_HI1();

    op1 = (int64_t)reg1._s32[0];
    op2 = (int64_t)reg2._s32[0];
    result = op1 * op2;
        
    result2 = result + ((int64_t)hi._s32[0] << 32);

    //This gets around some one bit errors in the calculation for hi
    if (((op2 & 0x7FFFFFFF) == 0 || (op2 & 0x7FFFFFFF) == 0x7FFFFFFF) && op1 != op2)
        result2 += 0x70000000;

    //4294967296 would be the full >> 32, but there seems to be an error amount
    result2 = (int32_t)(result2 / 4294967295);

    lo._s64[0] = (int32_t)(result & 0xffffffff) + lo._s32[0];
    hi._s64[0] = (int32_t)result2;

    op1 = (int64_t)reg1._s32[2];
    op2 = (int64_t)reg2._s32[2];
    result = op1 * op2;
    
    result2 = result + ((int64_t)hi._s32[2] << 32);
    //4294967296 would be the full >> 32, but there seems to be an error amount
    result2 = (int32_t)(result2 / 4294967295);
    lo._s64[1] = (int32_t)(result & 0xffffffff) + lo._s32[2];
    hi._s64[1] = (int32_t)result2;
   
    cpu.set_gpr<uint32_t>(dest, lo._u32[0]);
    cpu.set_gpr<uint32_t>(dest, hi._u32[0], 1);
    cpu.set_gpr<uint32_t>(dest, lo._u32[2], 2);
    cpu.set_gpr<uint32_t>(dest, hi._u32[2], 3);    

    cpu.set_LO_HI(lo._u64[0], hi._u64[0], false);
    cpu.set_LO_HI(lo._u64[1], hi._u64[1], true);
}

/**
 * Parallel Shift Left Logical Variable Word
 * Splits RT into two doublewords and shifts each lower-order word left. The shift amounts, s and t, are specified
 * by the lower 5 bits of the two doublewords in RS. The resulting 32-bit values are sign-extended.
 */
void EmotionInterpreter::psllvw(EmotionEngine &cpu, uint32_t instruction)
{
    uint32_t shift_reg = (instruction >> 21) & 0x1F;
    uint32_t source = (instruction >> 16) & 0x1F;
    uint32_t dest = (instruction >> 11) & 0x1F;

    //Lower doubleword
    uint8_t s = cpu.get_gpr<uint64_t>(shift_reg) & 0x1F;
    int32_t low_word = (int32_t)(cpu.get_gpr<uint64_t>(source) & 0xFFFFFFFF);
    cpu.set_gpr<int64_t>(dest, low_word << s);

    //Upper doubleword
    uint8_t t = cpu.get_gpr<uint64_t>(shift_reg, 1) & 0x1F;
    int32_t hi_word = (int32_t)(cpu.get_gpr<uint64_t>(source, 1) & 0xFFFFFFFF);
    cpu.set_gpr<int64_t>(dest, hi_word << t, 1);
}

/**
 * Parallel Shift Right Logical Variable Word
 */
void EmotionInterpreter::psrlvw(EmotionEngine &cpu, uint32_t instruction)
{
    uint32_t shift_reg = (instruction >> 21) & 0x1F;
    uint32_t source = (instruction >> 16) & 0x1F;
    uint32_t dest = (instruction >> 11) & 0x1F;

    //Lower doubleword
    uint8_t s = cpu.get_gpr<uint64_t>(shift_reg) & 0x1F;
    uint32_t low_word = cpu.get_gpr<uint64_t>(source) & 0xFFFFFFFF;
    cpu.set_gpr<int64_t>(dest, (int32_t)(low_word >> s));

    //Upper doubleword
    uint8_t t = cpu.get_gpr<uint64_t>(shift_reg, 1) & 0x1F;
    uint32_t hi_word = cpu.get_gpr<uint64_t>(source, 1) & 0xFFFFFFFF;
    cpu.set_gpr<int64_t>(dest, (int32_t)(hi_word >> t), 1);
}

/**
* Parallel Multiply-Subtract Word
- Note! It looks like the PS2 has a multiplication error when calculating the value for HI.
  With some tinkering depending on the mutliplication operands we can get this accurate on the autotests.
*/
void EmotionInterpreter::pmsubw(EmotionEngine &cpu, uint32_t instruction)
{
    uint128_t reg1 = cpu.get_gpr<uint128_t>((instruction >> 21) & 0x1F);
    uint128_t reg2 = cpu.get_gpr<uint128_t>((instruction >> 16) & 0x1F);
    uint32_t dest = (instruction >> 11) & 0x1F;
    uint128_t lo, hi;
    int64_t result, result2;
    int64_t op1, op2;
    lo._u64[0] = cpu.get_LO();
    lo._u64[1] = cpu.get_LO1();

    hi._u64[0] = cpu.get_HI();
    hi._u64[1] = cpu.get_HI1();

    op1 = (int64_t)reg1._s32[0];
    op2 = (int64_t)reg2._s32[0];
    result = op1 * op2;

    result2 = ((int64_t)hi._s32[0] << 32) - result;

    //4294967296 would be the full >> 32, but there seems to be an error amount
    result2 = (int32_t)(result2 / 4294967295);

    lo._s64[0] = lo._s32[0] - (int32_t)(result & 0xffffffff);
    hi._s64[0] = (int32_t)result2;

    op1 = (int64_t)reg1._s32[2];
    op2 = (int64_t)reg2._s32[2];
    result = op1 * op2;

    result2 = ((int64_t)hi._s32[2] << 32) - result;
    //4294967296 would be the full >> 32, but there seems to be an error amount
    result2 = (int32_t)(result2 / 4294967295);
    lo._s64[1] = lo._s32[2] - (int32_t)(result & 0xffffffff);
    hi._s64[1] = (int32_t)result2;

    cpu.set_gpr<uint32_t>(dest, lo._u32[0]);
    cpu.set_gpr<uint32_t>(dest, hi._u32[0], 1);
    cpu.set_gpr<uint32_t>(dest, lo._u32[2], 2);
    cpu.set_gpr<uint32_t>(dest, hi._u32[2], 3);

    cpu.set_LO_HI(lo._u64[0], hi._u64[0], false);
    cpu.set_LO_HI(lo._u64[1], hi._u64[1], true);
}


void EmotionInterpreter::pmfhi(EmotionEngine &cpu, uint32_t instruction)
{
    uint64_t dest = (instruction >> 11) & 0x1F;
    cpu.pmfhi(dest);
}

void EmotionInterpreter::pmflo(EmotionEngine &cpu, uint32_t instruction)
{
    uint64_t dest = (instruction >> 11) & 0x1F;
    cpu.pmflo(dest);
}

void EmotionInterpreter::pinth(EmotionEngine &cpu, uint32_t instruction)
{
    uint64_t reg1 = (instruction >> 21) & 0x1F;
    uint64_t reg2 = (instruction >> 16) & 0x1F;
    uint64_t dest = (instruction >> 11) & 0x1F;

    for (int i = 0; i < 4; i++)
    {
        uint16_t hw1 = cpu.get_gpr<uint16_t>(reg1, i + 4);
        uint16_t hw2 = cpu.get_gpr<uint16_t>(reg2, i);
        cpu.set_gpr<uint16_t>(dest, hw1, (i * 2) + 1);
        cpu.set_gpr<uint16_t>(dest, hw2, i * 2);
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

/**
* Parallel Multiply-Add Halfword
*/
void EmotionInterpreter::pmaddh(EmotionEngine &cpu, uint32_t instruction)
{
    uint128_t reg1 = cpu.get_gpr<uint128_t>((instruction >> 21) & 0x1F);
    uint128_t reg2 = cpu.get_gpr<uint128_t>((instruction >> 16) & 0x1F);
    uint128_t lo, hi;
    uint64_t dest = (instruction >> 11) & 0x1F;

    lo._u64[0] = cpu.get_LO();
    lo._u64[1] = cpu.get_LO1();

    hi._u64[0] = cpu.get_HI();
    hi._u64[1] = cpu.get_HI1();

    lo._u32[0] = (int32_t)((int32_t)lo._u32[0] + (int16_t)reg1._u16[0] * (int16_t)reg2._u16[0]);
    lo._u32[1] = (int32_t)((int32_t)lo._u32[1] + (int16_t)reg1._u16[1] * (int16_t)reg2._u16[1]);
    cpu.set_gpr<uint32_t>(dest, lo._u32[0]);

    hi._u32[0] = (int32_t)((int32_t)hi._u32[0] + (int16_t)reg1._u16[2] * (int16_t)reg2._u16[2]);
    hi._u32[1] = (int32_t)((int32_t)hi._u32[1] + (int16_t)reg1._u16[3] * (int16_t)reg2._u16[3]);
    cpu.set_gpr<uint32_t>(dest, hi._u32[0], 1);

    lo._u32[2] = (int32_t)((int32_t)lo._u32[2] + (int16_t)reg1._u16[4] * (int16_t)reg2._u16[4]);
    lo._u32[3] = (int32_t)((int32_t)lo._u32[3] + (int16_t)reg1._u16[5] * (int16_t)reg2._u16[5]);
    cpu.set_gpr<uint32_t>(dest, lo._u32[2], 2);

    hi._u32[2] = (int32_t)((int32_t)hi._u32[2] + (int16_t)reg1._u16[6] * (int16_t)reg2._u16[6]);
    hi._u32[3] = (int32_t)((int32_t)hi._u32[3] + (int16_t)reg1._u16[7] * (int16_t)reg2._u16[7]);
    cpu.set_gpr<uint32_t>(dest, hi._u32[2], 3);
    
    cpu.set_LO_HI(lo._u64[0], hi._u64[0], false);
    cpu.set_LO_HI(lo._u64[1], hi._u64[1], true);
}

/**
* Parallel Horizontal Multiply-Add Halfword
*/
void EmotionInterpreter::phmadh(EmotionEngine &cpu, uint32_t instruction)
{
    uint128_t reg1 = cpu.get_gpr<uint128_t>((instruction >> 21) & 0x1F);
    uint128_t reg2 = cpu.get_gpr<uint128_t>((instruction >> 16) & 0x1F);
    uint128_t lo, hi;
    uint64_t dest = (instruction >> 11) & 0x1F;
    int32_t mul1;

    mul1 = (int16_t)reg1._u16[1] * (int16_t)reg2._u16[1];
    lo._u32[0] = (int32_t)(mul1 + (int16_t)reg1._u16[0] * (int16_t)reg2._u16[0]);
    lo._u32[1] = mul1;
    cpu.set_gpr<uint32_t>(dest, lo._u32[0]);

    mul1 = (int16_t)reg1._u16[3] * (int16_t)reg2._u16[3];
    hi._u32[0] = (int32_t)(mul1 + (int16_t)reg1._u16[2] * (int16_t)reg2._u16[2]);
    hi._u32[1] = mul1;
    cpu.set_gpr<uint32_t>(dest, hi._u32[0], 1);

    mul1 = (int16_t)reg1._u16[5] * (int16_t)reg2._u16[5];
    lo._u32[2] = (int32_t)(mul1 + (int16_t)reg1._u16[4] * (int16_t)reg2._u16[4]);
    lo._u32[3] = mul1;
    cpu.set_gpr<uint32_t>(dest, lo._u32[2], 2);

    mul1 = (int16_t)reg1._u16[7] * (int16_t)reg2._u16[7];
    hi._u32[2] = (int32_t)(mul1 + (int16_t)reg1._u16[6] * (int16_t)reg2._u16[6]);
    hi._u32[3] = mul1;
    cpu.set_gpr<uint32_t>(dest, hi._u32[2], 3);

    cpu.set_LO_HI(lo._u64[0], hi._u64[0], false);
    cpu.set_LO_HI(lo._u64[1], hi._u64[1], true);
}

void EmotionInterpreter::pand(EmotionEngine &cpu, uint32_t instruction)
{
    uint64_t op1 = (instruction >> 21) & 0x1F;
    uint64_t op2 = (instruction >> 16) & 0x1F;
    uint64_t dest = (instruction >> 11) & 0x1F;

    cpu.set_gpr<uint64_t>(dest, cpu.get_gpr<uint64_t>(op1) & cpu.get_gpr<uint64_t>(op2));
    cpu.set_gpr<uint64_t>(dest, cpu.get_gpr<uint64_t>(op1, 1) & cpu.get_gpr<uint64_t>(op2, 1), 1);
}

void EmotionInterpreter::pxor(EmotionEngine &cpu, uint32_t instruction)
{
    uint64_t op1 = (instruction >> 21) & 0x1F;
    uint64_t op2 = (instruction >> 16) & 0x1F;
    uint64_t dest = (instruction >> 11) & 0x1F;

    cpu.set_gpr<uint64_t>(dest, cpu.get_gpr<uint64_t>(op1) ^ cpu.get_gpr<uint64_t>(op2));
    cpu.set_gpr<uint64_t>(dest, cpu.get_gpr<uint64_t>(op1, 1) ^ cpu.get_gpr<uint64_t>(op2, 1), 1);
}

/**
* Parallel Multiply-Subtract Halfword
*/
void EmotionInterpreter::pmsubh(EmotionEngine &cpu, uint32_t instruction)
{
    uint128_t reg1 = cpu.get_gpr<uint128_t>((instruction >> 21) & 0x1F);
    uint128_t reg2 = cpu.get_gpr<uint128_t>((instruction >> 16) & 0x1F);
    uint128_t lo, hi;
    uint64_t dest = (instruction >> 11) & 0x1F;

    lo._u64[0] = cpu.get_LO();
    lo._u64[1] = cpu.get_LO1();

    hi._u64[0] = cpu.get_HI();
    hi._u64[1] = cpu.get_HI1();

    lo._u32[0] = (int32_t)((int32_t)lo._u32[0] - (int16_t)reg1._u16[0] * (int16_t)reg2._u16[0]);
    lo._u32[1] = (int32_t)((int32_t)lo._u32[1] - (int16_t)reg1._u16[1] * (int16_t)reg2._u16[1]);
    cpu.set_gpr<uint32_t>(dest, lo._u32[0]);

    hi._u32[0] = (int32_t)((int32_t)hi._u32[0] - (int16_t)reg1._u16[2] * (int16_t)reg2._u16[2]);
    hi._u32[1] = (int32_t)((int32_t)hi._u32[1] - (int16_t)reg1._u16[3] * (int16_t)reg2._u16[3]);
    cpu.set_gpr<uint32_t>(dest, hi._u32[0], 1);

    lo._u32[2] = (int32_t)((int32_t)lo._u32[2] - (int16_t)reg1._u16[4] * (int16_t)reg2._u16[4]);
    lo._u32[3] = (int32_t)((int32_t)lo._u32[3] - (int16_t)reg1._u16[5] * (int16_t)reg2._u16[5]);
    cpu.set_gpr<uint32_t>(dest, lo._u32[2], 2);

    hi._u32[2] = (int32_t)((int32_t)hi._u32[2] - (int16_t)reg1._u16[6] * (int16_t)reg2._u16[6]);
    hi._u32[3] = (int32_t)((int32_t)hi._u32[3] - (int16_t)reg1._u16[7] * (int16_t)reg2._u16[7]);
    cpu.set_gpr<uint32_t>(dest, hi._u32[2], 3);

    cpu.set_LO_HI(lo._u64[0], hi._u64[0], false);
    cpu.set_LO_HI(lo._u64[1], hi._u64[1], true);
}

/**
* Parallel Horizontal Multiply-Subtract Halfword
*/
void EmotionInterpreter::phmsbh(EmotionEngine &cpu, uint32_t instruction)
{
    uint128_t reg1 = cpu.get_gpr<uint128_t>((instruction >> 21) & 0x1F);
    uint128_t reg2 = cpu.get_gpr<uint128_t>((instruction >> 16) & 0x1F);
    uint128_t lo, hi;
    uint64_t dest = (instruction >> 11) & 0x1F;
    int32_t mul1;

    mul1 = (int16_t)reg1._u16[1] * (int16_t)reg2._u16[1];
    lo._u32[0] = (int32_t)(mul1 - (int16_t)reg1._u16[0] * (int16_t)reg2._u16[0]);
    lo._u32[1] = ~mul1;
    cpu.set_gpr<uint32_t>(dest, lo._u32[0]);

    mul1 = (int16_t)reg1._u16[3] * (int16_t)reg2._u16[3];
    hi._u32[0] = (int32_t)(mul1 - (int16_t)reg1._u16[2] * (int16_t)reg2._u16[2]);
    hi._u32[1] = ~mul1;
    cpu.set_gpr<uint32_t>(dest, hi._u32[0], 1);

    mul1 = (int16_t)reg1._u16[5] * (int16_t)reg2._u16[5];
    lo._u32[2] = (int32_t)(mul1 - (int16_t)reg1._u16[4] * (int16_t)reg2._u16[4]);
    lo._u32[3] = ~mul1;
    cpu.set_gpr<uint32_t>(dest, lo._u32[2], 2);

    mul1 = (int16_t)reg1._u16[7] * (int16_t)reg2._u16[7];
    hi._u32[2] = (int32_t)(mul1 - (int16_t)reg1._u16[6] * (int16_t)reg2._u16[6]);
    hi._u32[3] = ~mul1;
    cpu.set_gpr<uint32_t>(dest, hi._u32[2], 3);

    cpu.set_LO_HI(lo._u64[0], hi._u64[0], false);
    cpu.set_LO_HI(lo._u64[1], hi._u64[1], true);
}

/**
 * Parallel Exchange Even Halfword
 * Splits RT into two doublewords. Swaps the even-positioned halfwords in each doubleword.
 */
void EmotionInterpreter::pexeh(EmotionEngine &cpu, uint32_t instruction)
{
    uint64_t source = (instruction >> 16) & 0x1F;
    uint64_t dest = (instruction >> 11) & 0x1F;

    uint128_t data = cpu.get_gpr<uint128_t>(source);

    std::swap(data._u16[0], data._u16[2]);
    std::swap(data._u16[4], data._u16[6]);

    cpu.set_gpr<uint128_t>(dest, data);
}

/**
 * Parallel Reverse Halfword
 */
void EmotionInterpreter::prevh(EmotionEngine &cpu, uint32_t instruction)
{
    uint64_t source = (instruction >> 16) & 0x1F;
    uint64_t dest = (instruction >> 11) & 0x1F;

    uint128_t data = cpu.get_gpr<uint128_t>(source);
    std::swap(data._u16[0], data._u16[3]);
    std::swap(data._u16[1], data._u16[2]);

    std::swap(data._u16[4], data._u16[7]);
    std::swap(data._u16[5], data._u16[6]);

    cpu.set_gpr<uint128_t>(dest, data);
}

/**
 * Parallel Multiply Halfword
 */
void EmotionInterpreter::pmulth(EmotionEngine &cpu, uint32_t instruction)
{
    uint128_t reg1 = cpu.get_gpr<uint128_t>((instruction >> 21) & 0x1F);
    uint128_t reg2 = cpu.get_gpr<uint128_t>((instruction >> 16) & 0x1F);
    uint128_t lo, hi;
    uint64_t dest = (instruction >> 11) & 0x1F;

    int32_t result;
    result = (int32_t)(int16_t)reg1._u16[0] * (int16_t)reg2._u16[0];
    lo._u32[0] = (uint32_t)result;
    cpu.set_gpr<int32_t>(dest, result);

    result = (int32_t)(int16_t)reg1._u16[1] * (int16_t)reg2._u16[1];
    lo._u32[1] = (uint32_t)result;

    result = (int32_t)(int16_t)reg1._u16[2] * (int16_t)reg2._u16[2];
    hi._u32[0] = (uint32_t)result;
    cpu.set_gpr<int32_t>(dest, result, 1);

    result = (int32_t)(int16_t)reg1._u16[3] * (int16_t)reg2._u16[3];
    hi._u32[1] = (uint32_t)result;

    result = (int32_t)(int16_t)reg1._u16[4] * (int16_t)reg2._u16[4];
    lo._u32[2] = (uint32_t)result;
    cpu.set_gpr<int32_t>(dest, result, 2);

    result = (int32_t)(int16_t)reg1._u16[5] * (int16_t)reg2._u16[5];
    lo._u32[3] = (uint32_t)result;

    result = (int32_t)(int16_t)reg1._u16[6] * (int16_t)reg2._u16[6];
    hi._u32[2] = (uint32_t)result;
    cpu.set_gpr<int32_t>(dest, result, 3);

    result = (int32_t)(int16_t)reg1._u16[7] * (int16_t)reg2._u16[7];
    hi._u32[3] = (uint32_t)result;

    cpu.set_LO_HI(lo._u64[0], hi._u64[0], false);
    cpu.set_LO_HI(lo._u64[1], hi._u64[1], true);
}

/**
* Parallel Divide with Broadcast Word
*/
void EmotionInterpreter::pdivbw(EmotionEngine &cpu, uint32_t instruction)
{
    uint128_t reg1 = cpu.get_gpr<uint128_t>((instruction >> 21) & 0x1F);
    uint128_t reg2 = cpu.get_gpr<uint128_t>((instruction >> 16) & 0x1F);
    uint128_t lo, hi;

    for (int i = 0; i < 4; i++)
    {
        if (reg1._u32[i] == 0x80000000 && reg2._u16[0] == 0xffff)
        {
            lo._u32[i] = 0x80000000;
            hi._u32[i] = 0;
        }
        else if (reg2._u16[0] != 0)
        {
            lo._u32[i] = (int32_t)reg1._u32[i] / (int16_t)reg2._u16[0];
            hi._u32[i] = (int32_t)reg1._u32[i] % (int16_t)reg2._u16[0];
        }
        else
        {
            if ((int32_t)reg1._u32[i] < 0)
                lo._u32[i] = 1;
            else
                lo._u32[i] = -1;
            hi._u32[i] = (int32_t)reg1._u32[i];
        }
    }

    cpu.set_LO_HI(lo._u64[0], hi._u64[0], false);
    cpu.set_LO_HI(lo._u64[1], hi._u64[1], true);
}

/**
* Parallel Multiply Word
*/
void EmotionInterpreter::pmultw(EmotionEngine &cpu, uint32_t instruction)
{
    uint128_t reg1 = cpu.get_gpr<uint128_t>((instruction >> 21) & 0x1F);
    uint128_t reg2 = cpu.get_gpr<uint128_t>((instruction >> 16) & 0x1F);
    uint128_t lo, hi;
    uint64_t dest = (instruction >> 11) & 0x1F;

    int64_t result;
    result = (int64_t)reg1._s32[0] * (int64_t)reg2._s32[0];
    lo._s64[0] = (int32_t)result;
    hi._s64[0] = (int32_t)(result >> 32);
    cpu.set_gpr<int64_t>(dest, result);

    result = (int64_t)reg1._s32[2] * (int64_t)reg2._s32[2];
    lo._s64[1] = (int32_t)result;
    hi._s64[1] = (int32_t)(result >> 32);
    cpu.set_gpr<int64_t>(dest, result, 1);


    cpu.set_LO_HI(lo._u64[0], hi._u64[0], false);
    cpu.set_LO_HI(lo._u64[1], hi._u64[1], true);
}

/**
* Parallel Divide Word
*/
void EmotionInterpreter::pdivw(EmotionEngine &cpu, uint32_t instruction)
{
    uint128_t reg1 = cpu.get_gpr<uint128_t>((instruction >> 21) & 0x1F);
    uint128_t reg2 = cpu.get_gpr<uint128_t>((instruction >> 16) & 0x1F);
    uint128_t lo, hi;

    for (int i = 0; i < 4; i+=2)
    {
        if (reg1._u32[i] == 0x80000000 && reg2._u32[i] == 0xffffffff)
        {
            lo._u64[i/2] = (int64_t)(int32_t)0x80000000;
            hi._u64[i/2] = 0;
        }
        else if (reg2._u32[i] != 0)
        {
            lo._u64[i/2] = (int64_t)((int32_t)reg1._u32[i] / (int32_t)reg2._u32[i]);
            hi._u64[i/2] = (int64_t)((int32_t)reg1._u32[i] % (int32_t)reg2._u32[i]);
        }
        else
        {
            if ((int32_t)reg1._u32[i] < 0)
                lo._u64[i/2] = 1;
            else
                lo._u64[i/2] = (int64_t)-1;
            hi._u64[i/2] = (int64_t)(int32_t)reg1._u32[i];
        }
    }

    cpu.set_LO_HI(lo._u64[0], hi._u64[0], false);
    cpu.set_LO_HI(lo._u64[1], hi._u64[1], true);
}

/**
 * Parallel Exchange Even Word
 * Splits RT into four words and exchanges the even words.
 */
void EmotionInterpreter::pexew(EmotionEngine &cpu, uint32_t instruction)
{
    uint64_t source = (instruction >> 16) & 0x1F;
    uint64_t dest = (instruction >> 11) & 0x1F;

    uint128_t data = cpu.get_gpr<uint128_t>(source);
    std::swap(data._u32[0], data._u32[2]);
    cpu.set_gpr<uint128_t>(dest, data);
}

/**
 * Parallel Rotate 3 Words Left
 */
void EmotionInterpreter::prot3w(EmotionEngine &cpu, uint32_t instruction)
{
    uint64_t source = (instruction >> 16) & 0x1F;
    uint64_t dest = (instruction >> 11) & 0x1F;

    uint128_t s = cpu.get_gpr<uint128_t>(source);
    uint128_t d;

    d._u32[0] = s._u32[1];
    d._u32[1] = s._u32[2];
    d._u32[2] = s._u32[0];
    d._u32[3] = s._u32[3];

    cpu.set_gpr<uint128_t>(dest, d);
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
    cpu.set_LO_HI((int64_t)(int32_t)(temp & 0xFFFFFFFF), (int64_t)(int32_t)(temp >> 32), true);
    cpu.mflo1(dest);
}

void EmotionInterpreter::multu1(EmotionEngine& cpu, uint32_t instruction)
{
    uint32_t op1 = (instruction >> 21) & 0x1F;
    uint32_t op2 = (instruction >> 16) & 0x1F;
    uint64_t dest = (instruction >> 11) & 0x1F;
    op1 = cpu.get_gpr<uint32_t>(op1);
    op2 = cpu.get_gpr<uint32_t>(op2);
    uint64_t temp = (uint64_t)op1 * op2;
    cpu.set_LO_HI((int64_t)(int32_t)(temp & 0xFFFFFFFF), (int64_t)temp >> 32, true);
    cpu.mflo1(dest);
}

void EmotionInterpreter::div1(EmotionEngine &cpu, uint32_t instruction)
{
    int32_t op1 = (instruction >> 21) & 0x1F;
    int32_t op2 = (instruction >> 16) & 0x1F;
    op1 = cpu.get_gpr<int32_t>(op1);
    op2 = cpu.get_gpr<int32_t>(op2);
    if (op1 == 0x80000000 && op2 == 0xFFFFFFFF)
    {
        cpu.set_LO_HI((int64_t)(int32_t)0x80000000, 0, true);
    }
    else if (op2)
    {
        cpu.set_LO_HI((int64_t)(int32_t)(op1 / op2), (int64_t)(int32_t)(op1 % op2), true);
    }
    else
    {
        int64_t lo;
        if (op1 >= 0)
            lo = -1;
        else
            lo = 1;
        cpu.set_LO_HI(lo, (int64_t)(int32_t)op1, true);
    }
}

void EmotionInterpreter::divu1(EmotionEngine &cpu, uint32_t instruction)
{
    uint32_t op1 = (instruction >> 21) & 0x1F;
    uint32_t op2 = (instruction >> 16) & 0x1F;
    op1 = cpu.get_gpr<uint32_t>(op1);
    op2 = cpu.get_gpr<uint32_t>(op2);
    if (op2)
    {
        cpu.set_LO_HI((int64_t)(int32_t)(op1 / op2), (int64_t)(int32_t)(op1 % op2), true);
    }
    else
    {
        cpu.set_LO_HI((int64_t)-1, (int64_t)(int32_t)op1, true);
    }
}

void EmotionInterpreter::madd1(EmotionEngine &cpu, uint32_t instruction)
{
    int64_t op1 = (instruction >> 21) & 0x1F;
    int64_t op2 = (instruction >> 16) & 0x1F;
    uint64_t dest = (instruction >> 11) & 0x1F;
    op1 = (int64_t)cpu.get_gpr<int32_t>(op1);
    op2 = (int64_t)cpu.get_gpr<int32_t>(op2);

    uint64_t lo = (uint64_t)(uint32_t)cpu.get_LO1();
    uint64_t hi = (uint64_t)(uint32_t)cpu.get_HI1();
    int64_t temp = (int64_t)((lo | (hi << 32)) + (op1 * op2));

    lo = (int64_t)(int32_t)(temp & 0xFFFFFFFF);
    hi = (int64_t)(int32_t)(temp >> 32);

    cpu.set_LO_HI(lo, hi, true);
    cpu.set_gpr<int64_t>(dest, (int64_t)lo);
}

void EmotionInterpreter::maddu1(EmotionEngine &cpu, uint32_t instruction)
{
    uint64_t op1 = (instruction >> 21) & 0x1F;
    uint64_t op2 = (instruction >> 16) & 0x1F;
    uint64_t dest = (instruction >> 11) & 0x1F;
    op1 = (uint64_t)cpu.get_gpr<uint32_t>(op1);
    op2 = (uint64_t)cpu.get_gpr<uint32_t>(op2);

    uint64_t lo = (uint64_t)(uint32_t)cpu.get_LO1();
    uint64_t hi = (uint64_t)(uint32_t)cpu.get_HI1();
    uint64_t temp = (int64_t)((lo | (hi << 32)) + (op1 * op2));

    lo = (int64_t)(int32_t)(temp & 0xFFFFFFFF);
    hi = (int64_t)(int32_t)(temp >> 32);

    cpu.set_LO_HI(lo, hi, true);
    cpu.set_gpr<int64_t>(dest, (int64_t)lo);
}

void EmotionInterpreter::mmi3(EE_InstrInfo& info, uint32_t instruction)
{
    uint8_t op = (instruction >> 6) & 0x1F;
    switch (op)
    {
        case 0x00:
            info.interpreter_fn = &pmadduw;
            info.pipeline = EE_InstrInfo::Pipeline::IntWide;
            info.latency = 4;
            info.throughput = 2;
            info.instruction_type = EE_InstrInfo::InstructionType::MADD;
            info.add_dependency(DependencyType::Write, RegType::GPR, (uint8_t)EE_SpecialReg::LO);
            info.add_dependency(DependencyType::Write, RegType::GPR, (uint8_t)EE_SpecialReg::LO1);
            info.add_dependency(DependencyType::Write, RegType::GPR, (uint8_t)EE_SpecialReg::HI);
            info.add_dependency(DependencyType::Write, RegType::GPR, (uint8_t)EE_SpecialReg::HI1);
            info.add_dependency(DependencyType::Write, RegType::GPR, (instruction >> 11) & 0x1F);
            info.add_dependency(DependencyType::Read, RegType::GPR, (instruction >> 21) & 0x1F);
            info.add_dependency(DependencyType::Read, RegType::GPR, (instruction >> 16) & 0x1F);
            break;
        case 0x03:
            info.interpreter_fn = &psravw;
            info.pipeline = EE_InstrInfo::Pipeline::IntWide;
            info.add_dependency(DependencyType::Write, RegType::GPR, (instruction >> 11) & 0x1F);
            info.add_dependency(DependencyType::Read, RegType::GPR, (instruction >> 16) & 0x1F);
            break;
        case 0x08:
            info.interpreter_fn = &pmthi;
            info.pipeline = EE_InstrInfo::Pipeline::IntWide;
            info.add_dependency(DependencyType::Write, RegType::GPR, (uint8_t)EE_SpecialReg::HI);
            info.add_dependency(DependencyType::Write, RegType::GPR, (uint8_t)EE_SpecialReg::HI1);
            info.add_dependency(DependencyType::Read, RegType::GPR, (instruction >> 21) & 0x1F);
            break;
        case 0x09:
            info.interpreter_fn = &pmtlo;
            info.pipeline = EE_InstrInfo::Pipeline::IntWide;
            info.add_dependency(DependencyType::Write, RegType::GPR, (uint8_t)EE_SpecialReg::LO);
            info.add_dependency(DependencyType::Write, RegType::GPR, (uint8_t)EE_SpecialReg::LO1);
            info.add_dependency(DependencyType::Read, RegType::GPR, (instruction >> 21) & 0x1F);
            break;
        case 0x0A:
            info.interpreter_fn = &pinteh;
            info.pipeline = EE_InstrInfo::Pipeline::IntWide;
            info.add_dependency(DependencyType::Write, RegType::GPR, (instruction >> 11) & 0x1F);
            info.add_dependency(DependencyType::Read, RegType::GPR, (instruction >> 16) & 0x1F);
            info.add_dependency(DependencyType::Read, RegType::GPR, (instruction >> 21) & 0x1F);
            break;
        case 0x0C:
            info.interpreter_fn = &pmultuw;
            info.pipeline = EE_InstrInfo::Pipeline::IntWide;
            info.latency = 4;
            info.throughput = 2;
            info.instruction_type = EE_InstrInfo::InstructionType::MULT;
            info.add_dependency(DependencyType::Write, RegType::GPR, (uint8_t)EE_SpecialReg::LO);
            info.add_dependency(DependencyType::Write, RegType::GPR, (uint8_t)EE_SpecialReg::LO1);
            info.add_dependency(DependencyType::Write, RegType::GPR, (uint8_t)EE_SpecialReg::HI);
            info.add_dependency(DependencyType::Write, RegType::GPR, (uint8_t)EE_SpecialReg::HI1);
            info.add_dependency(DependencyType::Write, RegType::GPR, (instruction >> 11) & 0x1F);
            info.add_dependency(DependencyType::Read, RegType::GPR, (instruction >> 21) & 0x1F);
            info.add_dependency(DependencyType::Read, RegType::GPR, (instruction >> 16) & 0x1F);
            break;
        case 0x0D:
            info.interpreter_fn = &pdivuw;
            info.pipeline = EE_InstrInfo::Pipeline::IntWide;
            info.latency = 4;
            info.throughput = 2;
            info.instruction_type = EE_InstrInfo::InstructionType::DIV;
            info.add_dependency(DependencyType::Write, RegType::GPR, (uint8_t)EE_SpecialReg::LO);
            info.add_dependency(DependencyType::Write, RegType::GPR, (uint8_t)EE_SpecialReg::LO1);
            info.add_dependency(DependencyType::Write, RegType::GPR, (uint8_t)EE_SpecialReg::HI);
            info.add_dependency(DependencyType::Write, RegType::GPR, (uint8_t)EE_SpecialReg::HI1);
            info.add_dependency(DependencyType::Read, RegType::GPR, (instruction >> 21) & 0x1F);
            info.add_dependency(DependencyType::Read, RegType::GPR, (instruction >> 16) & 0x1F);
            break;
        case 0x0E:
            info.interpreter_fn = &pcpyud;
            info.pipeline = EE_InstrInfo::Pipeline::IntWide;
            info.add_dependency(DependencyType::Write, RegType::GPR, (instruction >> 11) & 0x1F);
            info.add_dependency(DependencyType::Read, RegType::GPR, (instruction >> 16) & 0x1F);
            info.add_dependency(DependencyType::Read, RegType::GPR, (instruction >> 21) & 0x1F);
            break;
        case 0x12:
            info.interpreter_fn = &por;
            info.pipeline = EE_InstrInfo::Pipeline::IntWide;
            info.add_dependency(DependencyType::Write, RegType::GPR, (instruction >> 11) & 0x1F);
            info.add_dependency(DependencyType::Read, RegType::GPR, (instruction >> 16) & 0x1F);
            info.add_dependency(DependencyType::Read, RegType::GPR, (instruction >> 21) & 0x1F);
            break;
        case 0x13:
            info.interpreter_fn = &pnor;
            info.pipeline = EE_InstrInfo::Pipeline::IntWide;
            info.add_dependency(DependencyType::Write, RegType::GPR, (instruction >> 11) & 0x1F);
            info.add_dependency(DependencyType::Read, RegType::GPR, (instruction >> 16) & 0x1F);
            info.add_dependency(DependencyType::Read, RegType::GPR, (instruction >> 21) & 0x1F);
            break;
        case 0x1A:
            info.interpreter_fn = &pexch;
            info.pipeline = EE_InstrInfo::Pipeline::IntWide;
            info.add_dependency(DependencyType::Write, RegType::GPR, (instruction >> 11) & 0x1F);
            info.add_dependency(DependencyType::Read, RegType::GPR, (instruction >> 16) & 0x1F);
            break;
        case 0x1B:
            info.interpreter_fn = &pcpyh;
            info.pipeline = EE_InstrInfo::Pipeline::IntWide;
            info.add_dependency(DependencyType::Write, RegType::GPR, (instruction >> 11) & 0x1F);
            info.add_dependency(DependencyType::Read, RegType::GPR, (instruction >> 16) & 0x1F);
            break;
        case 0x1E:
            info.interpreter_fn = &pexcw;
            info.pipeline = EE_InstrInfo::Pipeline::IntWide;
            info.add_dependency(DependencyType::Write, RegType::GPR, (instruction >> 11) & 0x1F);
            info.add_dependency(DependencyType::Read, RegType::GPR, (instruction >> 16) & 0x1F);
            break;
        default:
            unknown_op("mmi3", instruction, op);
    }
}

/**
* Parallel Multiply-Add Unsigned word
*/
void EmotionInterpreter::pmadduw(EmotionEngine &cpu, uint32_t instruction)
{
    uint128_t reg1 = cpu.get_gpr<uint128_t>((instruction >> 21) & 0x1F);
    uint128_t reg2 = cpu.get_gpr<uint128_t>((instruction >> 16) & 0x1F);
    uint128_t lo, hi;
    uint64_t dest = (instruction >> 11) & 0x1F;
    uint64_t result;

    lo._u64[0] = cpu.get_LO();
    lo._u64[1] = cpu.get_LO1();

    hi._u64[0] = cpu.get_HI();
    hi._u64[1] = cpu.get_HI1();

    result = ((uint64_t)lo._u32[0] | (uint64_t)hi._u32[0] << 32) + ((uint64_t)reg1._u32[0] * (uint64_t)reg2._u32[0]);
    lo._u64[0] = (int64_t)(int32_t)(result & 0xffffffff);
    hi._u64[0] = (int64_t)(int32_t)(result >> 32);
    cpu.set_gpr<uint64_t>(dest, result);

    result = ((uint64_t)lo._u32[2] | (uint64_t)hi._u32[2] << 32) + ((uint64_t)reg1._u32[2] * (uint64_t)reg2._u32[2]);
    lo._u64[1] = (int64_t)(int32_t)(result & 0xffffffff);
    hi._u64[1] = (int64_t)(int32_t)(result >> 32);
    cpu.set_gpr<uint64_t>(dest, result, 1);

    cpu.set_LO_HI(lo._u64[0], hi._u64[0], false);
    cpu.set_LO_HI(lo._u64[1], hi._u64[1], true);
}

/**
 * Parallel Shift Right Arithmetic Variable Word
 */
void EmotionInterpreter::psravw(EmotionEngine &cpu, uint32_t instruction)
{
    uint32_t shift_reg = (instruction >> 21) & 0x1F;
    uint32_t source = (instruction >> 16) & 0x1F;
    uint32_t dest = (instruction >> 11) & 0x1F;

    //Lower doubleword
    uint8_t s = cpu.get_gpr<uint64_t>(shift_reg) & 0x1F;
    int32_t low_word = (int32_t)(cpu.get_gpr<uint64_t>(source) & 0xFFFFFFFF);
    cpu.set_gpr<int64_t>(dest, low_word >> s);

    //Upper doubleword
    uint8_t t = cpu.get_gpr<uint64_t>(shift_reg, 1) & 0x1F;
    int32_t hi_word = (int32_t)(cpu.get_gpr<uint64_t>(source, 1) & 0xFFFFFFFF);
    cpu.set_gpr<int64_t>(dest, hi_word >> t, 1);
}

void EmotionInterpreter::pmthi(EmotionEngine &cpu, uint32_t instruction)
{
    uint64_t source = (instruction >> 21) & 0x1F;
    cpu.pmthi(source);
}

void EmotionInterpreter::pmtlo(EmotionEngine &cpu, uint32_t instruction)
{
    uint64_t source = (instruction >> 21) & 0x1F;
    cpu.pmtlo(source);
}

void EmotionInterpreter::pinteh(EmotionEngine &cpu, uint32_t instruction)
{
    uint64_t reg1 = (instruction >> 21) & 0x1F;
    uint64_t reg2 = (instruction >> 16) & 0x1F;
    uint64_t dest = (instruction >> 11) & 0x1F;

    for (int i = 0; i < 4; i++)
    {
        uint16_t hw1 = cpu.get_gpr<uint32_t>(reg1, i) & 0xFFFF;
        uint16_t hw2 = cpu.get_gpr<uint32_t>(reg2, i) & 0xFFFF;
        cpu.set_gpr<uint16_t>(dest, hw1, (i * 2) + 1);
        cpu.set_gpr<uint16_t>(dest, hw2, i * 2);
    }
}

/**
* Parallel Multiply Unsigned Word
*/
void EmotionInterpreter::pmultuw(EmotionEngine &cpu, uint32_t instruction)
{
    uint128_t reg1 = cpu.get_gpr<uint128_t>((instruction >> 21) & 0x1F);
    uint128_t reg2 = cpu.get_gpr<uint128_t>((instruction >> 16) & 0x1F);
    uint128_t lo, hi;
    uint64_t dest = (instruction >> 11) & 0x1F;

    uint64_t result;
    result = (uint64_t)reg1._u32[0] * (uint64_t)reg2._u32[0];
    lo._s64[0] = (int32_t)result;
    hi._s64[0] = (int32_t)(result >> 32);
    cpu.set_gpr<int64_t>(dest, result);

    result = (uint64_t)reg1._u32[2] * (uint64_t)reg2._u32[2];
    lo._s64[1] = (int32_t)result;
    hi._s64[1] = (int32_t)(result >> 32);
    cpu.set_gpr<int64_t>(dest, result, 1);


    cpu.set_LO_HI(lo._u64[0], hi._u64[0], false);
    cpu.set_LO_HI(lo._u64[1], hi._u64[1], true);
}

/**
* Parallel Divide Unsigned Word
*/
void EmotionInterpreter::pdivuw(EmotionEngine &cpu, uint32_t instruction)
{
    uint128_t reg1 = cpu.get_gpr<uint128_t>((instruction >> 21) & 0x1F);
    uint128_t reg2 = cpu.get_gpr<uint128_t>((instruction >> 16) & 0x1F);
    uint128_t lo, hi;

    for (int i = 0; i < 4; i += 2)
    {
        if (reg2._u32[i] != 0)
        {
            lo._u64[i/2] = (int64_t)(int32_t)(reg1._u32[i] / reg2._u32[i]);
            hi._u64[i/2] = (int64_t)(int32_t)(reg1._u32[i] % reg2._u32[i]);
        }
        else
        {
            lo._u64[i/2] = (int64_t)-1;
            hi._u64[i/2] = (int64_t)(int32_t)reg1._u32[i];
        }
    }

    cpu.set_LO_HI(lo._u64[0], hi._u64[0], false);
    cpu.set_LO_HI(lo._u64[1], hi._u64[1], true);
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

    cpu.set_gpr<uint64_t>(dest, high);
    cpu.set_gpr<uint64_t>(dest, low, 1);
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
 * Parallel Exchange Center Halfword
 * Splits RT into two doublewords. The central halfwords in each doubleword are swapped.
 */
void EmotionInterpreter::pexch(EmotionEngine &cpu, uint32_t instruction)
{
    uint64_t source = (instruction >> 16) & 0x1F;
    uint64_t dest = (instruction >> 11) & 0x1F;

    uint16_t data[8];
    for (int i = 0; i < 8; i++)
        data[i] = cpu.get_gpr<uint16_t>(source, i);

    std::swap(data[1], data[2]);
    std::swap(data[5], data[6]);

    for (int i = 0; i < 8; i++)
        cpu.set_gpr<uint16_t>(dest, data[i], i);
}

/**
 * Parallel Copy Halfword
 * Splits RT into two doublewords. The least significant halfwords of each doubleword are copied into the
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

/**
 * Parallel Exchange Center Word
 * Splits RT into four words. The central words are exchanged.
 */
void EmotionInterpreter::pexcw(EmotionEngine &cpu, uint32_t instruction)
{
    uint64_t source = (instruction >> 16) & 0x1F;
    uint64_t dest = (instruction >> 11) & 0x1F;

    uint32_t data[4];
    for (int i = 0; i < 4; i++)
        data[i] = cpu.get_gpr<uint32_t>(source, i);

    std::swap(data[1], data[2]);

    for (int i = 0; i < 4; i++)
        cpu.set_gpr<uint32_t>(dest, data[i], i);
}
