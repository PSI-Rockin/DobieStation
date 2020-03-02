#include <cstdio>
#include <cstdlib>
#include "emotioninterpreter.hpp"

void EmotionInterpreter::cop_s(EE_InstrInfo &info, uint32_t instruction)
{
    uint8_t op = instruction & 0x3F;
    switch (op)
    {
        case 0x0:
            info.interpreter_fn = &fpu_add;
            info.pipeline = EE_InstrInfo::Pipeline::COP1;
            info.latency = 4;
            info.add_dependency(DependencyType::Write, RegType::COP1, (instruction >> 6) & 0x1F);
            info.add_dependency(DependencyType::Read, RegType::COP1, (instruction >> 11) & 0x1F);
            info.add_dependency(DependencyType::Read, RegType::COP1, (instruction >> 16) & 0x1F);
            break;
        case 0x1:
            info.interpreter_fn = &fpu_sub;
            info.pipeline = EE_InstrInfo::Pipeline::COP1;
            info.latency = 4;
            info.add_dependency(DependencyType::Write, RegType::COP1, (instruction >> 6) & 0x1F);
            info.add_dependency(DependencyType::Read, RegType::COP1, (instruction >> 11) & 0x1F);
            info.add_dependency(DependencyType::Read, RegType::COP1, (instruction >> 16) & 0x1F);
            break;
        case 0x2:
            info.interpreter_fn = &fpu_mul;
            info.pipeline = EE_InstrInfo::Pipeline::COP1;
            info.latency = 4;
            info.add_dependency(DependencyType::Write, RegType::COP1, (instruction >> 6) & 0x1F);
            info.add_dependency(DependencyType::Read, RegType::COP1, (instruction >> 11) & 0x1F);
            info.add_dependency(DependencyType::Read, RegType::COP1, (instruction >> 16) & 0x1F);
            break;
        case 0x3:
            info.interpreter_fn = &fpu_div;
            info.pipeline = EE_InstrInfo::Pipeline::COP1;
            info.latency = 8;
            info.throughput = 7;
            info.instruction_type = EE_InstrInfo::InstructionType::FPU_DIV;
            info.add_dependency(DependencyType::Write, RegType::COP1, (instruction >> 6) & 0x1F);
            info.add_dependency(DependencyType::Read, RegType::COP1, (instruction >> 11) & 0x1F);
            info.add_dependency(DependencyType::Read, RegType::COP1, (instruction >> 16) & 0x1F);
            break;
        case 0x4:
            info.interpreter_fn = &fpu_sqrt;
            info.pipeline = EE_InstrInfo::Pipeline::COP1;
            info.latency = 8;
            info.throughput = 7;
            info.instruction_type = EE_InstrInfo::InstructionType::FPU_SQRT;
            info.add_dependency(DependencyType::Write, RegType::COP1, (instruction >> 6) & 0x1F);
            info.add_dependency(DependencyType::Read, RegType::COP1, (instruction >> 16) & 0x1F);
            break;
        case 0x5:
            info.interpreter_fn = &fpu_abs;
            info.pipeline = EE_InstrInfo::Pipeline::COP1;
            info.latency = 4;
            info.add_dependency(DependencyType::Write, RegType::COP1, (instruction >> 6) & 0x1F);
            info.add_dependency(DependencyType::Read, RegType::COP1, (instruction >> 11) & 0x1F);
            break;
        case 0x6:
            info.interpreter_fn = &fpu_mov;
            info.pipeline = EE_InstrInfo::Pipeline::COP1;
            info.latency = 4;
            info.add_dependency(DependencyType::Write, RegType::COP1, (instruction >> 6) & 0x1F);
            info.add_dependency(DependencyType::Read, RegType::COP1, (instruction >> 11) & 0x1F);
            break;
        case 0x7:
            info.interpreter_fn = &fpu_neg;
            info.pipeline = EE_InstrInfo::Pipeline::COP1;
            info.latency = 4;
            info.add_dependency(DependencyType::Write, RegType::COP1, (instruction >> 6) & 0x1F);
            info.add_dependency(DependencyType::Read, RegType::COP1, (instruction >> 11) & 0x1F);
            break;
        case 0x16:
            info.interpreter_fn = &fpu_rsqrt;
            info.pipeline = EE_InstrInfo::Pipeline::COP1;
            info.latency = 14;
            info.throughput = 13;
            info.instruction_type = EE_InstrInfo::InstructionType::FPU_RSQRT;
            info.add_dependency(DependencyType::Write, RegType::COP1, (instruction >> 6) & 0x1F);
            info.add_dependency(DependencyType::Read, RegType::COP1, (instruction >> 11) & 0x1F);
            info.add_dependency(DependencyType::Read, RegType::COP1, (instruction >> 16) & 0x1F);
            break;
        case 0x18:
            info.interpreter_fn = &fpu_adda;
            info.pipeline = EE_InstrInfo::Pipeline::COP1;
            info.add_dependency(DependencyType::Read, RegType::COP1, (instruction >> 11) & 0x1F);
            info.add_dependency(DependencyType::Read, RegType::COP1, (instruction >> 16) & 0x1F);
            break;
        case 0x19:
            info.interpreter_fn = &fpu_suba;
            info.pipeline = EE_InstrInfo::Pipeline::COP1;
            info.add_dependency(DependencyType::Read, RegType::COP1, (instruction >> 11) & 0x1F);
            info.add_dependency(DependencyType::Read, RegType::COP1, (instruction >> 16) & 0x1F);
            break;
        case 0x1A:
            info.interpreter_fn = &fpu_mula;
            info.pipeline = EE_InstrInfo::Pipeline::COP1;
            info.add_dependency(DependencyType::Read, RegType::COP1, (instruction >> 11) & 0x1F);
            info.add_dependency(DependencyType::Read, RegType::COP1, (instruction >> 16) & 0x1F);
            break;
        case 0x1C:
            info.interpreter_fn = &fpu_madd;
            info.pipeline = EE_InstrInfo::Pipeline::COP1;
            info.latency = 4;
            info.add_dependency(DependencyType::Write, RegType::COP1, (instruction >> 6) & 0x1F);
            info.add_dependency(DependencyType::Read, RegType::COP1, (instruction >> 11) & 0x1F);
            info.add_dependency(DependencyType::Read, RegType::COP1, (instruction >> 16) & 0x1F);
            break;
        case 0x1D:
            info.interpreter_fn = &fpu_msub;
            info.pipeline = EE_InstrInfo::Pipeline::COP1;
            info.latency = 4;
            info.add_dependency(DependencyType::Write, RegType::COP1, (instruction >> 6) & 0x1F);
            info.add_dependency(DependencyType::Read, RegType::COP1, (instruction >> 11) & 0x1F);
            info.add_dependency(DependencyType::Read, RegType::COP1, (instruction >> 16) & 0x1F);
            break;
        case 0x1E:
            info.interpreter_fn = &fpu_madda;
            info.pipeline = EE_InstrInfo::Pipeline::COP1;
            info.add_dependency(DependencyType::Read, RegType::COP1, (instruction >> 11) & 0x1F);
            info.add_dependency(DependencyType::Read, RegType::COP1, (instruction >> 16) & 0x1F);
            break;
        case 0x1F:
            info.interpreter_fn = &fpu_msuba;
            info.pipeline = EE_InstrInfo::Pipeline::COP1;
            info.add_dependency(DependencyType::Read, RegType::COP1, (instruction >> 11) & 0x1F);
            info.add_dependency(DependencyType::Read, RegType::COP1, (instruction >> 16) & 0x1F);
            break;
        case 0x24:
            info.interpreter_fn = &fpu_cvt_w_s;
            info.pipeline = EE_InstrInfo::Pipeline::COP1;
            info.latency = 4;
            info.add_dependency(DependencyType::Write, RegType::COP1, (instruction >> 6) & 0x1F);
            info.add_dependency(DependencyType::Read, RegType::COP1, (instruction >> 11) & 0x1F);
            break;
        case 0x28:
            info.interpreter_fn = &fpu_max_s;
            info.pipeline = EE_InstrInfo::Pipeline::COP1;
            info.latency = 4;
            info.add_dependency(DependencyType::Write, RegType::COP1, (instruction >> 6) & 0x1F);
            info.add_dependency(DependencyType::Read, RegType::COP1, (instruction >> 11) & 0x1F);
            info.add_dependency(DependencyType::Read, RegType::COP1, (instruction >> 16) & 0x1F);
            break;
        case 0x29:
            info.interpreter_fn = &fpu_min_s;
            info.pipeline = EE_InstrInfo::Pipeline::COP1;
            info.latency = 4;
            info.add_dependency(DependencyType::Write, RegType::COP1, (instruction >> 6) & 0x1F);
            info.add_dependency(DependencyType::Read, RegType::COP1, (instruction >> 11) & 0x1F);
            info.add_dependency(DependencyType::Read, RegType::COP1, (instruction >> 16) & 0x1F);
            break;
        case 0x30:
            info.interpreter_fn = &fpu_c_f_s;
            info.pipeline = EE_InstrInfo::Pipeline::COP1;
            info.latency = 4;
            info.add_dependency(DependencyType::Write, RegType::COP1_CONTROL, (uint8_t)COP1_Control_SpecialReg::CONDITION);
            break;
        case 0x32:
            info.interpreter_fn = &fpu_c_eq_s;
            info.pipeline = EE_InstrInfo::Pipeline::COP1;
            info.latency = 4;
            info.add_dependency(DependencyType::Write, RegType::COP1_CONTROL, (uint8_t)COP1_Control_SpecialReg::CONDITION);
            info.add_dependency(DependencyType::Read, RegType::COP1, (instruction >> 11) & 0x1F);
            info.add_dependency(DependencyType::Read, RegType::COP1, (instruction >> 16) & 0x1F);
            break;
        case 0x34:
            info.interpreter_fn = &fpu_c_lt_s;
            info.pipeline = EE_InstrInfo::Pipeline::COP1;
            info.latency = 4;
            info.add_dependency(DependencyType::Write, RegType::COP1_CONTROL, (uint8_t)COP1_Control_SpecialReg::CONDITION);
            info.add_dependency(DependencyType::Read, RegType::COP1, (instruction >> 11) & 0x1F);
            info.add_dependency(DependencyType::Read, RegType::COP1, (instruction >> 16) & 0x1F);
            break;
        case 0x36:
            info.interpreter_fn = &fpu_c_le_s;
            info.pipeline = EE_InstrInfo::Pipeline::COP1;
            info.latency = 4;
            info.add_dependency(DependencyType::Write, RegType::COP1_CONTROL, (uint8_t)COP1_Control_SpecialReg::CONDITION);
            info.add_dependency(DependencyType::Read, RegType::COP1, (instruction >> 11) & 0x1F);
            info.add_dependency(DependencyType::Read, RegType::COP1, (instruction >> 16) & 0x1F);
            break;
        default:
            unknown_op("FPU", instruction, op);
    }
}


void EmotionInterpreter::fpu_add(EmotionEngine& cpu, uint32_t instruction)
{
    uint32_t dest = (instruction >> 6) & 0x1F;
    uint32_t reg1 = (instruction >> 11) & 0x1F;
    uint32_t reg2 = (instruction >> 16) & 0x1F;
    cpu.get_FPU().add_s(dest, reg1, reg2);
}

void EmotionInterpreter::fpu_sub(EmotionEngine& cpu, uint32_t instruction)
{
    uint32_t dest = (instruction >> 6) & 0x1F;
    uint32_t reg1 = (instruction >> 11) & 0x1F;
    uint32_t reg2 = (instruction >> 16) & 0x1F;
    cpu.get_FPU().sub_s(dest, reg1, reg2);
}

void EmotionInterpreter::fpu_mul(EmotionEngine& cpu, uint32_t instruction)
{
    uint32_t dest = (instruction >> 6) & 0x1F;
    uint32_t reg1 = (instruction >> 11) & 0x1F;
    uint32_t reg2 = (instruction >> 16) & 0x1F;
    cpu.get_FPU().mul_s(dest, reg1, reg2);
}

void EmotionInterpreter::fpu_div(EmotionEngine& cpu, uint32_t instruction)
{
    uint32_t dest = (instruction >> 6) & 0x1F;
    uint32_t reg1 = (instruction >> 11) & 0x1F;
    uint32_t reg2 = (instruction >> 16) & 0x1F;
    cpu.get_FPU().div_s(dest, reg1, reg2);
}

void EmotionInterpreter::fpu_sqrt(EmotionEngine& cpu, uint32_t instruction)
{
    uint32_t dest = (instruction >> 6) & 0x1F;
    uint32_t source = (instruction >> 16) & 0x1F;
    cpu.get_FPU().sqrt_s(dest, source);
}

void EmotionInterpreter::fpu_abs(EmotionEngine& cpu, uint32_t instruction)
{
    uint32_t dest = (instruction >> 6) & 0x1F;
    uint32_t source = (instruction >> 11) & 0x1F;
    cpu.get_FPU().abs_s(dest, source);
}

void EmotionInterpreter::fpu_mov(EmotionEngine& cpu, uint32_t instruction)
{
    uint32_t dest = (instruction >> 6) & 0x1F;
    uint32_t source = (instruction >> 11) & 0x1F;
    cpu.get_FPU().mov_s(dest, source);
}

void EmotionInterpreter::fpu_neg(EmotionEngine& cpu, uint32_t instruction)
{
    uint32_t dest = (instruction >> 6) & 0x1F;
    uint32_t source = (instruction >> 11) & 0x1F;
    cpu.get_FPU().neg_s(dest, source);
}

void EmotionInterpreter::fpu_rsqrt(EmotionEngine& cpu, uint32_t instruction)
{
    uint32_t dest = (instruction >> 6) & 0x1F;
    uint32_t reg1 = (instruction >> 11) & 0x1F;
    uint32_t reg2 = (instruction >> 16) & 0x1F;
    cpu.get_FPU().rsqrt_s(dest, reg1, reg2);
}

void EmotionInterpreter::fpu_adda(EmotionEngine& cpu, uint32_t instruction)
{
    uint32_t reg1 = (instruction >> 11) & 0x1F;
    uint32_t reg2 = (instruction >> 16) & 0x1F;
    cpu.get_FPU().adda_s(reg1, reg2);
}

void EmotionInterpreter::fpu_suba(EmotionEngine& cpu, uint32_t instruction)
{
    uint32_t reg1 = (instruction >> 11) & 0x1F;
    uint32_t reg2 = (instruction >> 16) & 0x1F;
    cpu.get_FPU().suba_s(reg1, reg2);
}

void EmotionInterpreter::fpu_mula(EmotionEngine& cpu, uint32_t instruction)
{
    uint32_t reg1 = (instruction >> 11) & 0x1F;
    uint32_t reg2 = (instruction >> 16) & 0x1F;
    cpu.get_FPU().mula_s(reg1, reg2);
}

void EmotionInterpreter::fpu_madd(EmotionEngine& cpu, uint32_t instruction)
{
    uint32_t dest = (instruction >> 6) & 0x1F;
    uint32_t reg1 = (instruction >> 11) & 0x1F;
    uint32_t reg2 = (instruction >> 16) & 0x1F;
    cpu.get_FPU().madd_s(dest, reg1, reg2);
}

void EmotionInterpreter::fpu_msub(EmotionEngine& cpu, uint32_t instruction)
{
    uint32_t dest = (instruction >> 6) & 0x1F;
    uint32_t reg1 = (instruction >> 11) & 0x1F;
    uint32_t reg2 = (instruction >> 16) & 0x1F;
    cpu.get_FPU().msub_s(dest, reg1, reg2);
}

void EmotionInterpreter::fpu_madda(EmotionEngine& cpu, uint32_t instruction)
{
    uint32_t reg1 = (instruction >> 11) & 0x1F;
    uint32_t reg2 = (instruction >> 16) & 0x1F;
    cpu.get_FPU().madda_s(reg1, reg2);
}

void EmotionInterpreter::fpu_msuba(EmotionEngine& cpu, uint32_t instruction)
{
    uint32_t reg1 = (instruction >> 11) & 0x1F;
    uint32_t reg2 = (instruction >> 16) & 0x1F;
    cpu.get_FPU().msuba_s(reg1, reg2);
}

void EmotionInterpreter::fpu_cvt_w_s(EmotionEngine& cpu, uint32_t instruction)
{
    uint32_t dest = (instruction >> 6) & 0x1F;
    uint32_t source = (instruction >> 11) & 0x1F;
    cpu.get_FPU().cvt_w_s(dest, source);
}

void EmotionInterpreter::fpu_max_s(EmotionEngine& cpu, uint32_t instruction)
{
    uint32_t dest = (instruction >> 6) & 0x1F;
    uint32_t reg1 = (instruction >> 11) & 0x1F;
    uint32_t reg2 = (instruction >> 16) & 0x1F;
    cpu.get_FPU().max_s(dest, reg1, reg2);
}

void EmotionInterpreter::fpu_min_s(EmotionEngine& cpu, uint32_t instruction)
{
    uint32_t dest = (instruction >> 6) & 0x1F;
    uint32_t reg1 = (instruction >> 11) & 0x1F;
    uint32_t reg2 = (instruction >> 16) & 0x1F;
    cpu.get_FPU().min_s(dest, reg1, reg2);
}

void EmotionInterpreter::fpu_c_f_s(EmotionEngine& cpu, uint32_t instruction)
{
    cpu.get_FPU().c_f_s();
}

void EmotionInterpreter::fpu_c_lt_s(EmotionEngine& cpu, uint32_t instruction)
{
    uint32_t reg1 = (instruction >> 11) & 0x1F;
    uint32_t reg2 = (instruction >> 16) & 0x1F;
    cpu.get_FPU().c_lt_s(reg1, reg2);
}

void EmotionInterpreter::fpu_c_eq_s(EmotionEngine& cpu, uint32_t instruction)
{
    uint32_t reg1 = (instruction >> 11) & 0x1F;
    uint32_t reg2 = (instruction >> 16) & 0x1F;
    cpu.get_FPU().c_eq_s(reg1, reg2);
}

void EmotionInterpreter::fpu_c_le_s(EmotionEngine& cpu, uint32_t instruction)
{
    uint32_t reg1 = (instruction >> 11) & 0x1F;
    uint32_t reg2 = (instruction >> 16) & 0x1F;
    cpu.get_FPU().c_le_s(reg1, reg2);
}

void EmotionInterpreter::cop_bc1(EmotionEngine &cpu, uint32_t instruction)
{
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
    cpu.get_FPU().cvt_s_w(dest, source);
}
