#include <cstdio>
#include <cstdlib>
#include "emotioninterpreter.hpp"
#include "../errors.hpp"

void EmotionInterpreter::interpret(EmotionEngine &cpu, uint32_t instruction)
{
    EE_InstrInfo instr_info;
    lookup(instr_info, instruction);

    if (instr_info.interpreter_fn == nullptr)
    {
        Errors::die("[EE Interpreter] Lookup returned nullptr interpreter_fn");
    }

    instr_info.interpreter_fn(cpu, instruction);
}

void EmotionInterpreter::lookup(EE_InstrInfo &info, uint32_t instruction)
{
    int op = instruction >> 26;
    switch (op)
    {
        case 0x00:
            special(info, instruction);
            break;
        case 0x01:
            regimm(info, instruction);
            break;
        case 0x02:
            info.interpreter_fn = &j;
            info.pipeline = EE_InstrInfo::Pipeline::Branch;
            break;
        case 0x03:
            info.interpreter_fn = &jal;
            info.pipeline = EE_InstrInfo::Pipeline::Branch;
            info.add_dependency(DependencyType::Write, RegType::GPR, EE_NormalReg::ra);
            break;
        case 0x04:
            info.interpreter_fn = &beq;
            info.pipeline = EE_InstrInfo::Pipeline::Branch;
            info.add_dependency(DependencyType::Read, RegType::GPR, (instruction >> 16) & 0x1F);
            info.add_dependency(DependencyType::Read, RegType::GPR, (instruction >> 21) & 0x1F);
            break;
        case 0x05:
            info.interpreter_fn = &bne;
            info.pipeline = EE_InstrInfo::Pipeline::Branch;
            info.add_dependency(DependencyType::Read, RegType::GPR, (instruction >> 16) & 0x1F);
            info.add_dependency(DependencyType::Read, RegType::GPR, (instruction >> 21) & 0x1F);
            break;
        case 0x06:
            info.interpreter_fn = &blez;
            info.pipeline = EE_InstrInfo::Pipeline::Branch;
            info.add_dependency(DependencyType::Read, RegType::GPR, (instruction >> 21) & 0x1F);
            break;
        case 0x07:
            info.interpreter_fn = &bgtz;
            info.pipeline = EE_InstrInfo::Pipeline::Branch;
            info.add_dependency(DependencyType::Read, RegType::GPR, (instruction >> 21) & 0x1F);
            break;
        case 0x08:
            info.interpreter_fn = &addi;
            info.pipeline = EE_InstrInfo::Pipeline::IntGeneric;
            info.add_dependency(DependencyType::Write, RegType::GPR, (instruction >> 16) & 0x1F);
            info.add_dependency(DependencyType::Read, RegType::GPR, (instruction >> 21) & 0x1F);
            break;
        case 0x09:
            info.interpreter_fn = &addiu;
            info.pipeline = EE_InstrInfo::Pipeline::IntGeneric;
            info.add_dependency(DependencyType::Write, RegType::GPR, (instruction >> 16) & 0x1F);
            info.add_dependency(DependencyType::Read, RegType::GPR, (instruction >> 21) & 0x1F);
            break;
        case 0x0A:
            info.interpreter_fn = &slti;
            info.pipeline = EE_InstrInfo::Pipeline::IntGeneric;
            info.add_dependency(DependencyType::Write, RegType::GPR, (instruction >> 16) & 0x1F);
            info.add_dependency(DependencyType::Read, RegType::GPR, (instruction >> 21) & 0x1F);
            break;
        case 0x0B:
            info.interpreter_fn = &sltiu;
            info.pipeline = EE_InstrInfo::Pipeline::IntGeneric;
            info.add_dependency(DependencyType::Write, RegType::GPR, (instruction >> 16) & 0x1F);
            info.add_dependency(DependencyType::Read, RegType::GPR, (instruction >> 21) & 0x1F);
            break;
        case 0x0C:
            info.interpreter_fn = &andi;
            info.pipeline = EE_InstrInfo::Pipeline::IntGeneric;
            info.add_dependency(DependencyType::Write, RegType::GPR, (instruction >> 16) & 0x1F);
            info.add_dependency(DependencyType::Read, RegType::GPR, (instruction >> 21) & 0x1F);
            break;
        case 0x0D:
            info.interpreter_fn = &ori;
            info.pipeline = EE_InstrInfo::Pipeline::IntGeneric;
            info.add_dependency(DependencyType::Write, RegType::GPR, (instruction >> 16) & 0x1F);
            info.add_dependency(DependencyType::Read, RegType::GPR, (instruction >> 21) & 0x1F);
            break;
        case 0x0E:
            info.interpreter_fn = &xori;
            info.pipeline = EE_InstrInfo::Pipeline::IntGeneric;
            info.add_dependency(DependencyType::Write, RegType::GPR, (instruction >> 16) & 0x1F);
            info.add_dependency(DependencyType::Read, RegType::GPR, (instruction >> 21) & 0x1F);
            break;
        case 0x0F:
            info.interpreter_fn = &lui;
            info.pipeline = EE_InstrInfo::Pipeline::IntGeneric;
            info.add_dependency(DependencyType::Write, RegType::GPR, (instruction >> 16) & 0x1F);
            break;
        case 0x10:
        case 0x11:
        case 0x12:
        case 0x13:
            cop(info, instruction);
            break;
        case 0x14:
            info.interpreter_fn = &beql;
            info.pipeline = EE_InstrInfo::Pipeline::Branch;
            info.add_dependency(DependencyType::Read, RegType::GPR, (instruction >> 16) & 0x1F);
            info.add_dependency(DependencyType::Read, RegType::GPR, (instruction >> 21) & 0x1F);
            break;
        case 0x15:
            info.interpreter_fn = &bnel;
            info.pipeline = EE_InstrInfo::Pipeline::Branch;
            info.add_dependency(DependencyType::Read, RegType::GPR, (instruction >> 16) & 0x1F);
            info.add_dependency(DependencyType::Read, RegType::GPR, (instruction >> 21) & 0x1F);
            break;
        case 0x16:
            info.interpreter_fn = &blezl;
            info.pipeline = EE_InstrInfo::Pipeline::Branch;
            info.add_dependency(DependencyType::Read, RegType::GPR, (instruction >> 21) & 0x1F);
            break;
        case 0x17:
            info.interpreter_fn = &bgtzl;
            info.pipeline = EE_InstrInfo::Pipeline::Branch;
            info.add_dependency(DependencyType::Read, RegType::GPR, (instruction >> 21) & 0x1F);
            break;
        case 0x18:
            info.interpreter_fn = &daddi;
            info.pipeline = EE_InstrInfo::Pipeline::IntGeneric;
            info.add_dependency(DependencyType::Write, RegType::GPR, (instruction >> 16) & 0x1F);
            info.add_dependency(DependencyType::Read, RegType::GPR, (instruction >> 21) & 0x1F);
            break;
        case 0x19:
            info.interpreter_fn = &daddiu;
            info.pipeline = EE_InstrInfo::Pipeline::IntGeneric;
            info.add_dependency(DependencyType::Write, RegType::GPR, (instruction >> 16) & 0x1F);
            info.add_dependency(DependencyType::Read, RegType::GPR, (instruction >> 21) & 0x1F);
            break;
        case 0x1A:
            info.interpreter_fn = &ldl;
            info.pipeline = EE_InstrInfo::Pipeline::LoadStore;
            info.add_dependency(DependencyType::Write, RegType::GPR, (instruction >> 16) & 0x1F);
            info.add_dependency(DependencyType::Read, RegType::GPR, (instruction >> 21) & 0x1F);
            break;
        case 0x1B:
            info.interpreter_fn = &ldr;
            info.pipeline = EE_InstrInfo::Pipeline::LoadStore;
            info.add_dependency(DependencyType::Write, RegType::GPR, (instruction >> 16) & 0x1F);
            info.add_dependency(DependencyType::Read, RegType::GPR, (instruction >> 21) & 0x1F);
            break;
        case 0x1C:
            mmi(info, instruction);
            break;
        case 0x1E:
            info.interpreter_fn = &lq;
            info.pipeline = EE_InstrInfo::Pipeline::LoadStore;
            info.add_dependency(DependencyType::Write, RegType::GPR, (instruction >> 16) & 0x1F);
            info.add_dependency(DependencyType::Read, RegType::GPR, (instruction >> 21) & 0x1F);
            break;
        case 0x1F:
            info.interpreter_fn = &sq;
            info.pipeline = EE_InstrInfo::Pipeline::LoadStore;
            info.add_dependency(DependencyType::Read, RegType::GPR, (instruction >> 16) & 0x1F);
            info.add_dependency(DependencyType::Read, RegType::GPR, (instruction >> 21) & 0x1F);
            break;
        case 0x20:
            info.interpreter_fn = &lb;
            info.pipeline = EE_InstrInfo::Pipeline::LoadStore;
            info.add_dependency(DependencyType::Write, RegType::GPR, (instruction >> 16) & 0x1F);
            info.add_dependency(DependencyType::Read, RegType::GPR, (instruction >> 21) & 0x1F);
            break;
        case 0x21:
            info.interpreter_fn = &lh;
            info.pipeline = EE_InstrInfo::Pipeline::LoadStore;
            info.add_dependency(DependencyType::Write, RegType::GPR, (instruction >> 16) & 0x1F);
            info.add_dependency(DependencyType::Read, RegType::GPR, (instruction >> 21) & 0x1F);
            break;
        case 0x22:
            info.interpreter_fn = &lwl;
            info.pipeline = EE_InstrInfo::Pipeline::LoadStore;
            info.add_dependency(DependencyType::Write, RegType::GPR, (instruction >> 16) & 0x1F);
            info.add_dependency(DependencyType::Read, RegType::GPR, (instruction >> 21) & 0x1F);
            break;
        case 0x23:
            info.interpreter_fn = &lw;
            info.pipeline = EE_InstrInfo::Pipeline::LoadStore;
            info.add_dependency(DependencyType::Write, RegType::GPR, (instruction >> 16) & 0x1F);
            info.add_dependency(DependencyType::Read, RegType::GPR, (instruction >> 21) & 0x1F);
            break;
        case 0x24:
            info.interpreter_fn = &lbu;
            info.pipeline = EE_InstrInfo::Pipeline::LoadStore;
            info.add_dependency(DependencyType::Write, RegType::GPR, (instruction >> 16) & 0x1F);
            info.add_dependency(DependencyType::Read, RegType::GPR, (instruction >> 21) & 0x1F);
            break;
        case 0x25:
            info.interpreter_fn = &lhu;
            info.pipeline = EE_InstrInfo::Pipeline::LoadStore;
            info.add_dependency(DependencyType::Write, RegType::GPR, (instruction >> 16) & 0x1F);
            info.add_dependency(DependencyType::Read, RegType::GPR, (instruction >> 21) & 0x1F);
            break;
        case 0x26:
            info.interpreter_fn = &lwr;
            info.pipeline = EE_InstrInfo::Pipeline::LoadStore;
            info.add_dependency(DependencyType::Write, RegType::GPR, (instruction >> 16) & 0x1F);
            info.add_dependency(DependencyType::Read, RegType::GPR, (instruction >> 21) & 0x1F);
            break;
        case 0x27:
            info.interpreter_fn = &lwu;
            info.pipeline = EE_InstrInfo::Pipeline::LoadStore;
            info.add_dependency(DependencyType::Write, RegType::GPR, (instruction >> 16) & 0x1F);
            info.add_dependency(DependencyType::Read, RegType::GPR, (instruction >> 21) & 0x1F);
            break;
        case 0x28:
            info.interpreter_fn = &sb;
            info.pipeline = EE_InstrInfo::Pipeline::LoadStore;
            info.add_dependency(DependencyType::Read, RegType::GPR, (instruction >> 16) & 0x1F);
            info.add_dependency(DependencyType::Read, RegType::GPR, (instruction >> 21) & 0x1F);
            break;
        case 0x29:
            info.interpreter_fn = &sh;
            info.pipeline = EE_InstrInfo::Pipeline::LoadStore;
            info.add_dependency(DependencyType::Read, RegType::GPR, (instruction >> 16) & 0x1F);
            info.add_dependency(DependencyType::Read, RegType::GPR, (instruction >> 21) & 0x1F);
            break;
        case 0x2A:
            info.interpreter_fn = &swl;
            info.pipeline = EE_InstrInfo::Pipeline::LoadStore;
            info.add_dependency(DependencyType::Read, RegType::GPR, (instruction >> 16) & 0x1F);
            info.add_dependency(DependencyType::Read, RegType::GPR, (instruction >> 21) & 0x1F);
            break;
        case 0x2B:
            info.interpreter_fn = &sw;
            info.pipeline = EE_InstrInfo::Pipeline::LoadStore;
            info.add_dependency(DependencyType::Read, RegType::GPR, (instruction >> 16) & 0x1F);
            info.add_dependency(DependencyType::Read, RegType::GPR, (instruction >> 21) & 0x1F);
            break;
        case 0x2C:
            info.interpreter_fn = &sdl;
            info.pipeline = EE_InstrInfo::Pipeline::LoadStore;
            info.add_dependency(DependencyType::Read, RegType::GPR, (instruction >> 16) & 0x1F);
            info.add_dependency(DependencyType::Read, RegType::GPR, (instruction >> 21) & 0x1F);
            break;
        case 0x2D:
            info.interpreter_fn = &sdr;
            info.pipeline = EE_InstrInfo::Pipeline::LoadStore;
            info.add_dependency(DependencyType::Read, RegType::GPR, (instruction >> 16) & 0x1F);
            info.add_dependency(DependencyType::Read, RegType::GPR, (instruction >> 21) & 0x1F);
            break;
        case 0x2E:
            info.interpreter_fn = &swr;
            info.pipeline = EE_InstrInfo::Pipeline::LoadStore;
            info.add_dependency(DependencyType::Read, RegType::GPR, (instruction >> 16) & 0x1F);
            info.add_dependency(DependencyType::Read, RegType::GPR, (instruction >> 21) & 0x1F);
            break;
        case 0x2F:
            info.interpreter_fn = &cache;
            info.pipeline = EE_InstrInfo::Pipeline::COP0;
            info.add_dependency(DependencyType::Read, RegType::GPR, (instruction >> 21) & 0x1F);
            break;
        case 0x31:
            info.interpreter_fn = &lwc1;
            info.pipeline = EE_InstrInfo::Pipeline::COP1 | EE_InstrInfo::Pipeline::LoadStore;
            info.latency = 2;
            info.add_dependency(DependencyType::Write, RegType::COP1, (instruction >> 16) & 0x1F);
            info.add_dependency(DependencyType::Read, RegType::GPR, (instruction >> 21) & 0x1F);
            break;
        case 0x33:
            //prefetch
            info.interpreter_fn = &nop;
            info.pipeline = EE_InstrInfo::Pipeline::LoadStore;
            info.add_dependency(DependencyType::Read, RegType::GPR, (instruction >> 21) & 0x1F);
            break;
        case 0x36:
            info.interpreter_fn = &lqc2;
            info.pipeline = EE_InstrInfo::Pipeline::COP2 | EE_InstrInfo::Pipeline::LoadStore;
            info.add_dependency(DependencyType::Write, RegType::COP2, (instruction >> 16) & 0x1F);
            info.add_dependency(DependencyType::Read, RegType::GPR, (instruction >> 21) & 0x1F);
            break;
        case 0x37:
            info.interpreter_fn = &ld;
            info.pipeline = EE_InstrInfo::Pipeline::LoadStore;
            info.add_dependency(DependencyType::Write, RegType::GPR, (instruction >> 16) & 0x1F);
            info.add_dependency(DependencyType::Read, RegType::GPR, (instruction >> 21) & 0x1F);
            break;
        case 0x39:
            info.interpreter_fn = &swc1;
            info.pipeline = EE_InstrInfo::Pipeline::COP1 | EE_InstrInfo::Pipeline::LoadStore;
            info.add_dependency(DependencyType::Read, RegType::COP1, (instruction >> 16) & 0x1F);
            info.add_dependency(DependencyType::Read, RegType::GPR, (instruction >> 21) & 0x1F);
            break;
        case 0x3E:
            info.interpreter_fn = &sqc2;
            info.pipeline = EE_InstrInfo::Pipeline::COP2 | EE_InstrInfo::Pipeline::LoadStore;
            info.add_dependency(DependencyType::Read, RegType::COP2, (instruction >> 16) & 0x1F);
            info.add_dependency(DependencyType::Read, RegType::GPR, (instruction >> 21) & 0x1F);
            break;
        case 0x3F:
            info.interpreter_fn = &sd;
            info.pipeline = EE_InstrInfo::Pipeline::LoadStore;
            info.add_dependency(DependencyType::Read, RegType::GPR, (instruction >> 16) & 0x1F);
            info.add_dependency(DependencyType::Read, RegType::GPR, (instruction >> 21) & 0x1F);
            break;
        default:
            unknown_op("normal", instruction, op);
    }
}

void EmotionInterpreter::regimm(EE_InstrInfo &info, uint32_t instruction)
{
    int op = (instruction >> 16) & 0x1F;
    switch (op)
    {
        case 0x00:
            info.interpreter_fn = &bltz;
            info.pipeline = EE_InstrInfo::Pipeline::Branch;
            info.add_dependency(DependencyType::Read, RegType::GPR, (instruction >> 21) & 0x1F);
            break;
        case 0x01:
            info.interpreter_fn = &bgez;
            info.pipeline = EE_InstrInfo::Pipeline::Branch;
            info.add_dependency(DependencyType::Read, RegType::GPR, (instruction >> 21) & 0x1F);
            break;
        case 0x02:
            info.interpreter_fn = &bltzl;
            info.pipeline = EE_InstrInfo::Pipeline::Branch;
            info.add_dependency(DependencyType::Read, RegType::GPR, (instruction >> 21) & 0x1F);
            break;
        case 0x03:
            info.interpreter_fn = &bgezl;
            info.pipeline = EE_InstrInfo::Pipeline::Branch;
            info.add_dependency(DependencyType::Read, RegType::GPR, (instruction >> 21) & 0x1F);
            break;
        case 0x10:
            info.interpreter_fn = &bltzal;
            info.pipeline = EE_InstrInfo::Pipeline::Branch;
            info.add_dependency(DependencyType::Write, RegType::GPR, EE_NormalReg::ra);
            info.add_dependency(DependencyType::Read, RegType::GPR, (instruction >> 21) & 0x1F);
            break;
        case 0x11:
            info.interpreter_fn = &bgezal;
            info.pipeline = EE_InstrInfo::Pipeline::Branch;
            info.add_dependency(DependencyType::Write, RegType::GPR, EE_NormalReg::ra);
            info.add_dependency(DependencyType::Read, RegType::GPR, (instruction >> 21) & 0x1F);
            break;
        case 0x12:
            info.interpreter_fn = &bltzall;
            info.pipeline = EE_InstrInfo::Pipeline::Branch;
            info.add_dependency(DependencyType::Write, RegType::GPR, EE_NormalReg::ra);
            info.add_dependency(DependencyType::Read, RegType::GPR, (instruction >> 21) & 0x1F);
            break;
        case 0x13:
            info.interpreter_fn = &bgezall;
            info.pipeline = EE_InstrInfo::Pipeline::Branch;
            info.add_dependency(DependencyType::Write, RegType::GPR, EE_NormalReg::ra);
            info.add_dependency(DependencyType::Read, RegType::GPR, (instruction >> 21) & 0x1F);
            break;
        case 0x18:
            info.interpreter_fn = &mtsab;
            info.pipeline = EE_InstrInfo::Pipeline::SA;
            info.add_dependency(DependencyType::Write, RegType::GPR, (uint8_t)EE_SpecialReg::SA);
            info.add_dependency(DependencyType::Read, RegType::GPR, (instruction >> 21) & 0x1F);
            break;
        case 0x19:
            info.interpreter_fn = &mtsah;
            info.pipeline = EE_InstrInfo::Pipeline::SA;
            info.add_dependency(DependencyType::Write, RegType::GPR, (uint8_t)EE_SpecialReg::SA);
            info.add_dependency(DependencyType::Read, RegType::GPR, (instruction >> 21) & 0x1F);
            break;
        default:
            unknown_op("regimm", instruction, op);
    }
}


void EmotionInterpreter::bltz(EmotionEngine &cpu, uint32_t instruction)
{
    int32_t offset = (int16_t)(instruction & 0xFFFF);
    offset <<= 2;
    int64_t reg = (instruction >> 21) & 0x1F;
    reg = cpu.get_gpr<int64_t>(reg);
    cpu.branch(reg < 0, offset);
}

void EmotionInterpreter::bltzl(EmotionEngine &cpu, uint32_t instruction)
{
    int32_t offset = (int16_t)(instruction & 0xFFFF);
    offset <<= 2;
    int64_t reg = (instruction >> 21) & 0x1F;
    reg = cpu.get_gpr<int64_t>(reg);
    cpu.branch_likely(reg < 0, offset);
}

void EmotionInterpreter::bltzal(EmotionEngine &cpu, uint32_t instruction)
{
    int32_t offset = (int16_t)(instruction & 0xFFFF);
    offset <<= 2;
    int64_t reg = (instruction >> 21) & 0x1F;
    reg = cpu.get_gpr<int64_t>(reg);
    cpu.set_gpr<uint64_t>(31, cpu.get_PC() + 8);
    cpu.branch(reg < 0, offset);
}

void EmotionInterpreter::bltzall(EmotionEngine &cpu, uint32_t instruction)
{
    int32_t offset = (int16_t)(instruction & 0xFFFF);
    offset <<= 2;
    int64_t reg = (instruction >> 21) & 0x1F;
    reg = cpu.get_gpr<int64_t>(reg);
    cpu.set_gpr<uint64_t>(31, cpu.get_PC() + 8);
    cpu.branch_likely(reg < 0, offset);
}

void EmotionInterpreter::bgez(EmotionEngine &cpu, uint32_t instruction)
{
    int32_t offset = (int16_t)(instruction & 0xFFFF);
    offset <<= 2;
    int64_t reg = (instruction >> 21) & 0x1F;
    reg = cpu.get_gpr<int64_t>(reg);
    cpu.branch(reg >= 0, offset);
}

void EmotionInterpreter::bgezl(EmotionEngine &cpu, uint32_t instruction)
{
    int32_t offset = (int16_t)(instruction & 0xFFFF);
    offset <<= 2;
    int64_t reg = (instruction >> 21) & 0x1F;
    reg = cpu.get_gpr<int64_t>(reg);
    cpu.branch_likely(reg >= 0, offset);
}

void EmotionInterpreter::bgezal(EmotionEngine &cpu, uint32_t instruction)
{
    int32_t offset = (int16_t)(instruction & 0xFFFF);
    offset <<= 2;
    int64_t reg = (instruction >> 21) & 0x1F;
    reg = cpu.get_gpr<int64_t>(reg);
    cpu.set_gpr<uint64_t>(31, cpu.get_PC() + 8);
    cpu.branch(reg >= 0, offset);
}

void EmotionInterpreter::bgezall(EmotionEngine &cpu, uint32_t instruction)
{
    int32_t offset = (int16_t)(instruction & 0xFFFF);
    offset <<= 2;
    int64_t reg = (instruction >> 21) & 0x1F;
    reg = cpu.get_gpr<int64_t>(reg);
    cpu.set_gpr<uint64_t>(31, cpu.get_PC() + 8);
    cpu.branch_likely(reg >= 0, offset);
}

void EmotionInterpreter::mtsab(EmotionEngine &cpu, uint32_t instruction)
{
    uint32_t reg = (instruction >> 21) & 0x1F;
    uint16_t imm = instruction & 0xFFFF;

    reg = cpu.get_gpr<uint32_t>(reg);

    reg = (reg & 0xF) ^ (imm & 0xF);
    cpu.set_SA(reg);
}

void EmotionInterpreter::mtsah(EmotionEngine &cpu, uint32_t instruction)
{
    uint32_t reg = (instruction >> 21) & 0x1F;
    uint16_t imm = instruction & 0xFFFF;

    reg = cpu.get_gpr<uint32_t>(reg);

    reg = (reg & 0x7) ^ (imm & 0x7);
    cpu.set_SA(reg * 2);
}

void EmotionInterpreter::j(EmotionEngine &cpu, uint32_t instruction)
{
    uint32_t addr = (instruction & 0x3FFFFFF) << 2;
    uint32_t PC = cpu.get_PC();
    addr += (PC + 4) & 0xF0000000;
    cpu.jp(addr);
}

void EmotionInterpreter::jal(EmotionEngine &cpu, uint32_t instruction)
{
    uint32_t addr = (instruction & 0x3FFFFFF) << 2;
    uint32_t PC = cpu.get_PC();
    addr += (PC + 4) & 0xF0000000;
    cpu.jp(addr);
    cpu.set_gpr<uint64_t>(31, PC + 8);
}

void EmotionInterpreter::beq(EmotionEngine &cpu, uint32_t instruction)
{
    int offset = (int16_t)(instruction & 0xFFFF);
    offset <<= 2;
    uint64_t reg1 = cpu.get_gpr<uint64_t>((instruction >> 21) & 0x1F);
    uint64_t reg2 = cpu.get_gpr<uint64_t>((instruction >> 16) & 0x1F);
    cpu.branch(reg1 == reg2, offset);
}

void EmotionInterpreter::bne(EmotionEngine &cpu, uint32_t instruction)
{
    int offset = (int16_t)(instruction & 0xFFFF);
    offset <<= 2;
    uint64_t reg1 = cpu.get_gpr<uint64_t>((instruction >> 21) & 0x1F);
    uint64_t reg2 = cpu.get_gpr<uint64_t>((instruction >> 16) & 0x1F);
    cpu.branch(reg1 != reg2, offset);
}

void EmotionInterpreter::blez(EmotionEngine &cpu, uint32_t instruction)
{
    int32_t offset = (int16_t)(instruction & 0xFFFF);
    offset <<= 2;
    int64_t reg = (instruction >> 21) & 0x1F;
    reg = cpu.get_gpr<int64_t>(reg);
    cpu.branch(reg <= 0, offset);
}

void EmotionInterpreter::bgtz(EmotionEngine &cpu, uint32_t instruction)
{
    int32_t offset = (int16_t)(instruction & 0xFFFF);
    offset <<= 2;
    int64_t reg = (instruction >> 21) & 0x1F;
    reg = cpu.get_gpr<int64_t>(reg);
    cpu.branch(reg > 0, offset);
}

void EmotionInterpreter::addi(EmotionEngine &cpu, uint32_t instruction)
{
    //TODO: overflow
    int16_t imm = (int16_t)(instruction & 0xFFFF);
    uint64_t dest = (instruction >> 16) & 0x1F;
    uint32_t source = (instruction >> 21) & 0x1F;
    int32_t result = cpu.get_gpr<int64_t>(source) + imm;
    cpu.set_gpr<int64_t>(dest, result);
}

void EmotionInterpreter::addiu(EmotionEngine &cpu, uint32_t instruction)
{
    int16_t imm = (int16_t)(instruction & 0xFFFF);
    uint64_t dest = (instruction >> 16) & 0x1F;
    uint32_t source = (instruction >> 21) & 0x1F;
    int32_t result = cpu.get_gpr<int64_t>(source) + imm;
    cpu.set_gpr<int64_t>(dest, result);
}

void EmotionInterpreter::slti(EmotionEngine &cpu, uint32_t instruction)
{
    int64_t imm = (int64_t)(int16_t)(instruction & 0xFFFF);
    uint64_t dest = (instruction >> 16) & 0x1F;
    int64_t source = (instruction >> 21) & 0x1F;
    source = cpu.get_gpr<int64_t>(source);
    cpu.set_gpr<uint64_t>(dest, source < imm);
}

void EmotionInterpreter::sltiu(EmotionEngine &cpu, uint32_t instruction)
{
    uint64_t imm = (uint64_t)(int64_t)(int16_t)(instruction & 0xFFFF);
    uint64_t dest = (instruction >> 16) & 0x1F;
    uint64_t source = (instruction >> 21) & 0x1F;
    source = cpu.get_gpr<uint64_t>(source);
    cpu.set_gpr<uint64_t>(dest, source < imm);
}

void EmotionInterpreter::andi(EmotionEngine &cpu, uint32_t instruction)
{
    uint64_t imm = instruction & 0xFFFF;
    uint64_t dest = (instruction >> 16) & 0x1F;
    uint64_t source = (instruction >> 21) & 0x1F;
    cpu.set_gpr<uint64_t>(dest, cpu.get_gpr<uint64_t>(source) & imm);
}

void EmotionInterpreter::ori(EmotionEngine &cpu, uint32_t instruction)
{
    uint64_t imm = instruction & 0xFFFF;
    uint64_t dest = (instruction >> 16) & 0x1F;
    uint64_t source = (instruction >> 21) & 0x1F;
    cpu.set_gpr<uint64_t>(dest, cpu.get_gpr<uint64_t>(source) | imm);
}

void EmotionInterpreter::xori(EmotionEngine &cpu, uint32_t instruction)
{
    uint64_t imm = instruction & 0xFFFF;
    uint64_t dest = (instruction >> 16) & 0x1F;
    uint64_t source = (instruction >> 21) & 0x1F;
    cpu.set_gpr<uint64_t>(dest, cpu.get_gpr<uint64_t>(source) ^ imm);
}

void EmotionInterpreter::lui(EmotionEngine &cpu, uint32_t instruction)
{
    int64_t imm = (int64_t)(int32_t)((instruction & 0xFFFF) << 16);
    uint64_t dest = (instruction >> 16) & 0x1F;
    cpu.set_gpr<uint64_t>(dest, imm);
}

void EmotionInterpreter::beql(EmotionEngine &cpu, uint32_t instruction)
{
    int offset = (int16_t)(instruction & 0xFFFF);
    offset <<= 2;
    uint64_t reg1 = cpu.get_gpr<uint64_t>((instruction >> 21) & 0x1F);
    uint64_t reg2 = cpu.get_gpr<uint64_t>((instruction >> 16) & 0x1F);
    cpu.branch_likely(reg1 == reg2, offset);
}

void EmotionInterpreter::bnel(EmotionEngine &cpu, uint32_t instruction)
{
    int offset = (int16_t)(instruction & 0xFFFF);
    offset <<= 2;
    uint64_t reg1 = cpu.get_gpr<uint64_t>((instruction >> 21) & 0x1F);
    uint64_t reg2 = cpu.get_gpr<uint64_t>((instruction >> 16) & 0x1F);
    cpu.branch_likely(reg1 != reg2, offset);
}

void EmotionInterpreter::blezl(EmotionEngine &cpu, uint32_t instruction)
{
    int32_t offset = (int16_t)(instruction & 0xFFFF);
    offset <<= 2;
    int64_t reg = (instruction >> 21) & 0x1F;
    reg = cpu.get_gpr<int64_t>(reg);
    cpu.branch_likely(reg <= 0, offset);
}

void EmotionInterpreter::bgtzl(EmotionEngine &cpu, uint32_t instruction)
{
    int32_t offset = (int16_t)(instruction & 0xFFFF);
    offset <<= 2;
    int64_t reg = (instruction >> 21) & 0x1F;
    reg = cpu.get_gpr<int64_t>(reg);
    cpu.branch_likely(reg > 0, offset);
}

void EmotionInterpreter::daddi(EmotionEngine &cpu, uint32_t instruction)
{
    int16_t imm = (int16_t)(instruction & 0xFFFF);
    int64_t source = (instruction >> 21) & 0x1F;
    uint64_t dest = (instruction >> 16) & 0x1F;
    source = cpu.get_gpr<int64_t>(source);
    //TODO: 64bit Arithmetic Overflow
    cpu.set_gpr<uint64_t>(dest, source + imm);
}

void EmotionInterpreter::daddiu(EmotionEngine &cpu, uint32_t instruction)
{
    int16_t imm = (int16_t)(instruction & 0xFFFF);
    int64_t source = (instruction >> 21) & 0x1F;
    uint64_t dest = (instruction >> 16) & 0x1F;
    source = cpu.get_gpr<int64_t>(source);
    cpu.set_gpr<uint64_t>(dest, source + imm);
}

void EmotionInterpreter::ldl(EmotionEngine &cpu, uint32_t instruction)
{
    static const uint64_t LDL_MASK[8] =
    {	0x00ffffffffffffffULL, 0x0000ffffffffffffULL, 0x000000ffffffffffULL, 0x00000000ffffffffULL,
        0x0000000000ffffffULL, 0x000000000000ffffULL, 0x00000000000000ffULL, 0x0000000000000000ULL
    };
    static const uint8_t LDL_SHIFT[8] = { 56, 48, 40, 32, 24, 16, 8, 0 };
    int16_t imm = (int16_t)(instruction & 0xFFFF);
    uint64_t dest = (instruction >> 16) & 0x1F;
    uint64_t base = (instruction >> 21) & 0x1F;

    uint32_t addr = cpu.get_gpr<uint32_t>(base) + imm;
    uint32_t shift = addr & 0x7;

    uint64_t mem = cpu.read64(addr & ~0x7);
    uint64_t reg = cpu.get_gpr<uint64_t>(dest);
    cpu.set_gpr<uint64_t>(dest, (reg & LDL_MASK[shift]) | (mem << LDL_SHIFT[shift]));
}

void EmotionInterpreter::ldr(EmotionEngine &cpu, uint32_t instruction)
{
    static const uint64_t LDR_MASK[8] =
    {	0x0000000000000000ULL, 0xff00000000000000ULL, 0xffff000000000000ULL, 0xffffff0000000000ULL,
        0xffffffff00000000ULL, 0xffffffffff000000ULL, 0xffffffffffff0000ULL, 0xffffffffffffff00ULL
    };
    static const uint8_t LDR_SHIFT[8] = { 0, 8, 16, 24, 32, 40, 48, 56 };
    int16_t imm = (int16_t)(instruction & 0xFFFF);
    uint64_t dest = (instruction >> 16) & 0x1F;
    uint64_t base = (instruction >> 21) & 0x1F;

    uint32_t addr = cpu.get_gpr<uint32_t>(base) + imm;
    uint32_t shift = addr & 0x7;

    uint64_t mem = cpu.read64(addr & ~0x7);
    uint64_t reg = cpu.get_gpr<uint64_t>(dest);
    cpu.set_gpr<uint64_t>(dest, (reg & LDR_MASK[shift]) | (mem >> LDR_SHIFT[shift]));
}

void EmotionInterpreter::lq(EmotionEngine &cpu, uint32_t instruction)
{
    int16_t imm = (int16_t)(instruction & 0xFFFF);
    uint64_t dest = (instruction >> 16) & 0x1F;
    uint64_t base = (instruction >> 21) & 0x1F;
    uint32_t addr = cpu.get_gpr<uint32_t>(base) + imm;
    addr &= ~0xF;
    cpu.set_gpr<uint128_t>(dest, cpu.read128(addr));
}

void EmotionInterpreter::sq(EmotionEngine &cpu, uint32_t instruction)
{
    int16_t imm = (int16_t)(instruction & 0xFFFF);
    uint64_t source = (instruction >> 16) & 0x1F;
    uint64_t base = (instruction >> 21) & 0x1F;

    uint32_t addr = cpu.get_gpr<uint32_t>(base) + imm;
    addr &= ~0xF;
    cpu.write128(addr, cpu.get_gpr<uint128_t>(source));
}

void EmotionInterpreter::lb(EmotionEngine &cpu, uint32_t instruction)
{
    int16_t offset = (int16_t)(instruction & 0xFFFF);
    uint64_t dest = (instruction >> 16) & 0x1F;
    uint32_t base = (instruction >> 21) & 0x1F;
    uint32_t addr = cpu.get_gpr<uint32_t>(base);
    addr += offset;
    cpu.set_gpr<int64_t>(dest, (int8_t)cpu.read8(addr));
}

void EmotionInterpreter::lh(EmotionEngine &cpu, uint32_t instruction)
{
    int16_t offset = (int16_t)(instruction & 0xFFFF);
    uint64_t dest = (instruction >> 16) & 0x1F;
    uint32_t base = (instruction >> 21) & 0x1F;
    uint32_t addr = cpu.get_gpr<uint32_t>(base);
    addr += offset;
    cpu.set_gpr<int64_t>(dest, (int16_t)cpu.read16(addr));
}

void EmotionInterpreter::lwl(EmotionEngine &cpu, uint32_t instruction)
{
    static const uint32_t LWL_MASK[4] = { 0xffffff, 0x0000ffff, 0x000000ff, 0x00000000 };
    static const uint8_t LWL_SHIFT[4] = { 24, 16, 8, 0 };
    int16_t offset = (int16_t)(instruction & 0xFFFF);
    uint32_t dest = (instruction >> 16) & 0x1F;
    uint32_t base = (instruction >> 21) & 0x1F;
    uint32_t addr = cpu.get_gpr<uint32_t>(base) + offset;
    int shift = addr & 0x3;

    uint32_t mem = cpu.read32(addr & ~0x3);
    cpu.set_gpr<int64_t>(dest, (int32_t)((cpu.get_gpr<uint32_t>(dest) & LWL_MASK[shift]) | (mem << LWL_SHIFT[shift])));
}

void EmotionInterpreter::lw(EmotionEngine &cpu, uint32_t instruction)
{
    int16_t offset = (int16_t)(instruction & 0xFFFF);
    uint64_t dest = (instruction >> 16) & 0x1F;
    uint32_t base = (instruction >> 21) & 0x1F;
    uint32_t addr = cpu.get_gpr<uint32_t>(base);
    addr += offset;
    cpu.set_gpr<int64_t>(dest, (int32_t)cpu.read32(addr));
}

void EmotionInterpreter::lbu(EmotionEngine &cpu, uint32_t instruction)
{
    int16_t offset = (int16_t)(instruction & 0xFFFF);
    uint64_t dest = (instruction >> 16) & 0x1F;
    uint32_t base = (instruction >> 21) & 0x1F;
    uint32_t addr = cpu.get_gpr<uint32_t>(base);
    addr += offset;
    cpu.set_gpr<uint64_t>(dest, cpu.read8(addr));
}

void EmotionInterpreter::lhu(EmotionEngine &cpu, uint32_t instruction)
{
    int16_t offset = (int16_t)(instruction & 0xFFFF);
    uint64_t dest = (instruction >> 16) & 0x1F;
    uint32_t base = (instruction >> 21) & 0x1F;
    uint32_t addr = cpu.get_gpr<uint32_t>(base);
    addr += offset;
    cpu.set_gpr<uint64_t>(dest, cpu.read16(addr));
}

void EmotionInterpreter::lwr(EmotionEngine &cpu, uint32_t instruction)
{
    static const uint32_t LWR_MASK[4] = { 0x000000, 0xff000000, 0xffff0000, 0xffffff00 };
    static const uint8_t LWR_SHIFT[4] = { 0, 8, 16, 24 };
    int16_t offset = (int16_t)(instruction & 0xFFFF);
    uint32_t dest = (instruction >> 16) & 0x1F;
    uint32_t base = (instruction >> 21) & 0x1F;
    uint32_t addr = cpu.get_gpr<uint32_t>(base) + offset;
    int shift = addr & 0x3;

    uint32_t mem = cpu.read32(addr & ~0x3);
    mem = (cpu.get_gpr<uint32_t>(dest) & LWR_MASK[shift]) | (mem >> LWR_SHIFT[shift]);

    //Only sign-extend the loaded word if the shift is zero
    if (!shift)
        cpu.set_gpr<int64_t>(dest, (int32_t)mem);
    else
        cpu.set_gpr<uint32_t>(dest, mem);
}

void EmotionInterpreter::lwu(EmotionEngine &cpu, uint32_t instruction)
{
    int16_t offset = (int16_t)(instruction & 0xFFFF);
    uint32_t dest = (instruction >> 16) & 0x1F;
    uint32_t base = (instruction >> 21) & 0x1F;
    uint32_t addr = cpu.get_gpr<uint32_t>(base);
    addr += offset;
    cpu.set_gpr<uint64_t>(dest, cpu.read32(addr));
}

void EmotionInterpreter::sb(EmotionEngine &cpu, uint32_t instruction)
{
    int16_t offset = (int16_t)(instruction & 0xFFFF);
    uint32_t source = (instruction >> 16) & 0x1F;
    uint32_t base = (instruction >> 21) & 0x1F;
    uint32_t addr = cpu.get_gpr<uint32_t>(base);
    addr += offset;
    cpu.write8(addr, cpu.get_gpr<uint8_t>(source));
}

void EmotionInterpreter::sh(EmotionEngine &cpu, uint32_t instruction)
{
    int16_t offset = (int16_t)(instruction & 0xFFFF);
    uint32_t source = (instruction >> 16) & 0x1F;
    uint32_t base = (instruction >> 21) & 0x1F;
    uint32_t addr = cpu.get_gpr<uint32_t>(base);
    addr += offset;
    cpu.write16(addr, cpu.get_gpr<uint16_t>(source));
}

void EmotionInterpreter::swl(EmotionEngine& cpu, uint32_t instruction)
{
    static const uint32_t SWL_MASK[4] = { 0xffffff00, 0xffff0000, 0xff000000, 0x00000000 };
    static const uint8_t SWL_SHIFT[4] = { 24, 16, 8, 0 };
    int16_t offset = (int16_t)(instruction & 0xFFFF);
    uint32_t source = (instruction >> 16) & 0x1F;
    uint32_t base = (instruction >> 21) & 0x1F;
    uint32_t addr = cpu.get_gpr<uint32_t>(base) + offset;
    int shift = addr & 3;
    uint32_t mem = cpu.read32(addr & ~3);

    cpu.write32(addr & ~0x3, (cpu.get_gpr<uint32_t>(source) >> SWL_SHIFT[shift]) | (mem & SWL_MASK[shift]));
}

void EmotionInterpreter::sw(EmotionEngine &cpu, uint32_t instruction)
{
    int16_t offset = (int16_t)(instruction & 0xFFFF);
    uint32_t source = (instruction >> 16) & 0x1F;
    uint32_t base = (instruction >> 21) & 0x1F;
    uint32_t addr = cpu.get_gpr<uint32_t>(base);
    addr += offset;
    cpu.write32(addr, cpu.get_gpr<uint32_t>(source));
}

void EmotionInterpreter::sdl(EmotionEngine &cpu, uint32_t instruction)
{
    static const uint64_t SDL_MASK[8] =
    {	0xffffffffffffff00ULL, 0xffffffffffff0000ULL, 0xffffffffff000000ULL, 0xffffffff00000000ULL,
        0xffffff0000000000ULL, 0xffff000000000000ULL, 0xff00000000000000ULL, 0x0000000000000000ULL
    };
    static const uint8_t SDL_SHIFT[8] = { 56, 48, 40, 32, 24, 16, 8, 0 };
    int16_t imm = (int16_t)(instruction & 0xFFFF);
    uint64_t source = (instruction >> 16) & 0x1F;
    uint64_t base = (instruction >> 21) & 0x1F;

    uint32_t addr = cpu.get_gpr<uint32_t>(base) + imm;
    uint32_t shift = addr & 0x7;

    uint64_t mem = cpu.read64(addr & ~0x7);
    mem = (cpu.get_gpr<uint64_t>(source) >> SDL_SHIFT[shift]) |
            (mem & SDL_MASK[shift]);
    cpu.write64(addr & ~0x7, mem);
}

void EmotionInterpreter::sdr(EmotionEngine &cpu, uint32_t instruction)
{
    static const uint64_t SDR_MASK[8] =
    {	0x0000000000000000ULL, 0x00000000000000ffULL, 0x000000000000ffffULL, 0x0000000000ffffffULL,
        0x00000000ffffffffULL, 0x000000ffffffffffULL, 0x0000ffffffffffffULL, 0x00ffffffffffffffULL
    };
    static const uint8_t SDR_SHIFT[8] = { 0, 8, 16, 24, 32, 40, 48, 56 };
    int16_t imm = (int16_t)(instruction & 0xFFFF);
    uint64_t source = (instruction >> 16) & 0x1F;
    uint64_t base = (instruction >> 21) & 0x1F;

    uint32_t addr = cpu.get_gpr<uint32_t>(base) + imm;
    uint32_t shift = addr & 0x7;

    uint64_t mem = cpu.read64(addr & ~0x7);
    mem = (cpu.get_gpr<uint64_t>(source) << SDR_SHIFT[shift]) |
            (mem & SDR_MASK[shift]);
    cpu.write64(addr & ~0x7, mem);
}

void EmotionInterpreter::swr(EmotionEngine &cpu, uint32_t instruction)
{
    static const uint32_t SWR_MASK[4] = { 0x00000000, 0x000000ff, 0x0000ffff, 0x00ffffff };
    static const uint8_t SWR_SHIFT[4] = { 0, 8, 16, 24 };
    int16_t offset = (int16_t)(instruction & 0xFFFF);
    uint32_t source = (instruction >> 16) & 0x1F;
    uint32_t base = (instruction >> 21) & 0x1F;
    uint32_t addr = cpu.get_gpr<uint32_t>(base) + offset;
    int shift = addr & 3;
    uint32_t mem = cpu.read32(addr & ~3);

    cpu.write32(addr & ~0x3, (cpu.get_gpr<uint32_t>(source) << SWR_SHIFT[shift]) | (mem & SWR_MASK[shift]));
}

void EmotionInterpreter::lwc1(EmotionEngine &cpu, uint32_t instruction)
{
    int16_t offset = (int16_t)(instruction & 0xFFFF);
    uint32_t dest = (instruction >> 16) & 0x1F;
    uint32_t base = (instruction >> 21) & 0x1F;
    uint32_t addr = cpu.get_gpr<uint32_t>(base);
    addr += offset;
    cpu.lwc1(addr, dest);
}

void EmotionInterpreter::lqc2(EmotionEngine &cpu, uint32_t instruction)
{
    int16_t offset = (int16_t)(instruction & 0xFFFF);
    uint32_t dest = (instruction >> 16) & 0x1F;
    uint32_t base = (instruction >> 21) & 0x1F;
    uint32_t addr = cpu.get_gpr<uint32_t>(base);
    addr += offset;
    cpu.lqc2(addr, dest);
}

void EmotionInterpreter::ld(EmotionEngine &cpu, uint32_t instruction)
{
    int16_t offset = (int16_t)(instruction & 0xFFFF);
    uint32_t dest = (instruction >> 16) & 0x1F;
    uint32_t base = (instruction >> 21) & 0x1F;
    uint32_t addr = cpu.get_gpr<uint32_t>(base);
    addr += offset;
    cpu.set_gpr(dest, cpu.read64(addr));
}

void EmotionInterpreter::swc1(EmotionEngine &cpu, uint32_t instruction)
{
    int16_t offset = (int16_t)(instruction & 0xFFFF);
    uint32_t source = (instruction >> 16) & 0x1F;
    uint32_t base = (instruction >> 21) & 0x1F;
    uint32_t addr = cpu.get_gpr<uint32_t>(base);
    addr += offset;
    cpu.swc1(addr, source);
}

void EmotionInterpreter::sqc2(EmotionEngine &cpu, uint32_t instruction)
{
    int16_t offset = (int16_t)(instruction & 0xFFFF);
    uint32_t source = (instruction >> 16) & 0x1F;
    uint32_t base = (instruction >> 21) & 0x1F;
    uint32_t addr = cpu.get_gpr<uint32_t>(base);
    addr += offset;
    cpu.sqc2(addr, source);
}

void EmotionInterpreter::sd(EmotionEngine &cpu, uint32_t instruction)
{
    int16_t offset = (int16_t)(instruction & 0xFFFF);
    uint32_t source = (instruction >> 16) & 0x1F;
    uint32_t base = (instruction >> 21) & 0x1F;
    uint32_t addr = cpu.get_gpr<uint32_t>(base);
    addr += offset;
    cpu.write64(addr, cpu.get_gpr<uint64_t>(source));
}

void EmotionInterpreter::cache(EmotionEngine &cpu, uint32_t instruction)
{
    uint16_t op = (instruction >> 16) & 0x1F;
    int16_t offset = (int16_t)(instruction & 0xFFFF);
    uint32_t base = (instruction >> 21) & 0x1F;
    uint32_t addr = cpu.get_gpr<uint32_t>(base);
    addr += offset;
    switch (op)
    {
        case 0x07:
            cpu.invalidate_icache_indexed(addr);
            break;
        default:
            //Do nothing, we don't emulate the other caches
            break;
    }
}


void EmotionInterpreter::tlbwi(EmotionEngine& cpu, uint32_t instruction)
{
    cpu.tlbwi();
}

void EmotionInterpreter::eret(EmotionEngine& cpu, uint32_t instruction)
{
    cpu.eret();
}

void EmotionInterpreter::ei(EmotionEngine& cpu, uint32_t instruction)
{
    cpu.ei();
}

void EmotionInterpreter::di(EmotionEngine& cpu, uint32_t instruction)
{
    cpu.di();
}

void EmotionInterpreter::cop(EE_InstrInfo& info, uint32_t instruction)
{
    uint16_t op = (instruction >> 21) & 0x1F;
    uint8_t cop_id = ((instruction >> 26) & 0x3);

    if (cop_id == 2 && op >= 0x10)
    {
        cop2_special(info, instruction);
        return;
    }

    switch (op | (cop_id * 0x100))
    {
        case 0x000:
            info.interpreter_fn = &cop0_mfc;
            info.pipeline = EE_InstrInfo::Pipeline::COP0;
            info.add_dependency(DependencyType::Write, RegType::GPR, (instruction >> 16) & 0x1F);
            info.add_dependency(DependencyType::Read, RegType::COP0, (instruction >> 11) & 0x1F);
            break;
        case 0x100:
            info.interpreter_fn = &cop1_mfc;
            info.pipeline = EE_InstrInfo::Pipeline::COP1;
            info.latency = 2;
            info.add_dependency(DependencyType::Write, RegType::GPR, (instruction >> 16) & 0x1F);
            info.add_dependency(DependencyType::Read, RegType::COP1, (instruction >> 11) & 0x1F);
            break;
        case 0x004:
            info.interpreter_fn = &cop0_mtc;
            info.pipeline = EE_InstrInfo::Pipeline::COP0;
            info.add_dependency(DependencyType::Write, RegType::GPR, (instruction >> 16) & 0x1F);
            info.add_dependency(DependencyType::Read, RegType::COP0, (instruction >> 11) & 0x1F);
            break;
        case 0x104:
            info.interpreter_fn = &cop1_mtc;
            info.pipeline = EE_InstrInfo::Pipeline::COP1;
            info.latency = 2;
            info.add_dependency(DependencyType::Write, RegType::GPR, (instruction >> 16) & 0x1F);
            info.add_dependency(DependencyType::Read, RegType::COP1, (instruction >> 11) & 0x1F);
            break;
        case 0x010:
        {
            uint8_t op2 = instruction & 0x3F;
            switch (op2)
            {
                case 0x2:
                    info.interpreter_fn = &tlbwi;
                    info.pipeline = EE_InstrInfo::Pipeline::COP0;
                    break;
                case 0x18:
                    info.interpreter_fn = &eret;
                    info.pipeline = EE_InstrInfo::Pipeline::ERET;
                    break;
                case 0x38:
                    info.interpreter_fn = &ei;
                    info.pipeline = EE_InstrInfo::Pipeline::COP0;
                    break;
                case 0x39:
                    info.interpreter_fn = &di;
                    info.pipeline = EE_InstrInfo::Pipeline::COP0;
                    break;
                default:
                    unknown_op("cop0 type2", instruction, op2);
            }
        }
            break;
        case 0x102:
            info.interpreter_fn = &cop1_cfc;
            info.pipeline = EE_InstrInfo::Pipeline::COP1;
            info.add_dependency(DependencyType::Write, RegType::GPR, (instruction >> 16) & 0x1F);
            info.add_dependency(DependencyType::Read, RegType::COP1_CONTROL, (instruction >> 11) & 0x1F);
            break;
        case 0x202:
            info.interpreter_fn = &cop2_cfc;
            info.pipeline = EE_InstrInfo::Pipeline::COP2;
            info.add_dependency(DependencyType::Write, RegType::GPR, (instruction >> 16) & 0x1F);
            info.add_dependency(DependencyType::Read, RegType::COP2_CONTROL, (instruction >> 11) & 0x1F);
            break;
        case 0x106:
            info.interpreter_fn = &cop1_ctc;
            info.pipeline = EE_InstrInfo::Pipeline::COP1;
            info.add_dependency(DependencyType::Write, RegType::GPR, (instruction >> 16) & 0x1F);
            info.add_dependency(DependencyType::Read, RegType::COP1_CONTROL, (instruction >> 11) & 0x1F);
            break;
        case 0x206:
            info.interpreter_fn = &cop2_ctc;
            info.pipeline = EE_InstrInfo::Pipeline::COP2;
            info.add_dependency(DependencyType::Write, RegType::GPR, (instruction >> 16) & 0x1F);
            info.add_dependency(DependencyType::Read, RegType::COP2_CONTROL, (instruction >> 11) & 0x1F);
            break;
        case 0x008:
            info.interpreter_fn = &cop_bc0;
            info.pipeline = EE_InstrInfo::Pipeline::COP0;
            break;
        case 0x108:
            info.interpreter_fn = &cop_bc1;
            info.pipeline = EE_InstrInfo::Pipeline::COP1;
            info.add_dependency(DependencyType::Read, RegType::COP1_CONTROL, (uint8_t)COP1_Control_SpecialReg::CONDITION);
            break;
        case 0x208:
            info.interpreter_fn = &cop2_bc2;
            info.pipeline = EE_InstrInfo::Pipeline::COP2;
            info.add_dependency(DependencyType::Read, RegType::COP2_CONTROL, (uint8_t)COP2_Control_SpecialReg::CONDITION);
            break;
        case 0x110:
            cop_s(info, instruction);
            break;
        case 0x114:
            info.interpreter_fn = &cop_cvt_s_w;
            info.pipeline = EE_InstrInfo::Pipeline::COP1;
            info.latency = 4;
            info.add_dependency(DependencyType::Write, RegType::COP1, (instruction >> 6) & 0x1F);
            info.add_dependency(DependencyType::Read, RegType::COP1, (instruction >> 11) & 0x1F);
            break;
        case 0x201:
            info.interpreter_fn = &cop2_qmfc2;
            info.pipeline = EE_InstrInfo::Pipeline::COP2;
            info.add_dependency(DependencyType::Write, RegType::GPR, (instruction >> 16) & 0x1F);
            info.add_dependency(DependencyType::Read, RegType::COP2, (instruction >> 11) & 0x1F);
            break;
        case 0x205:
            info.interpreter_fn = &cop2_qmtc2;
            info.pipeline = EE_InstrInfo::Pipeline::COP2;
            info.add_dependency(DependencyType::Write, RegType::GPR, (instruction >> 16) & 0x1F);
            info.add_dependency(DependencyType::Read, RegType::COP2, (instruction >> 11) & 0x1F);
            break;
        default:
            unknown_op("cop", instruction, op | (cop_id * 0x100));
    }
}

void EmotionInterpreter::cop0_mfc(EmotionEngine& cpu, uint32_t instruction)
{
    int emotion_reg = (instruction >> 16) & 0x1F;
    int cop_reg = (instruction >> 11) & 0x1F;
    //Special case for MFPS/MFPC
    if (cop_reg == 25)
    {
        if (instruction & 0x1)
        {
            int pc_reg = (instruction >> 1) & 0x1F;
            cpu.mfpc(pc_reg, emotion_reg);
        }
        else
            cpu.mfps(emotion_reg);
    }
    else
        cpu.mfc(0, emotion_reg, cop_reg);
}

void EmotionInterpreter::cop1_mfc(EmotionEngine& cpu, uint32_t instruction)
{
    int emotion_reg = (instruction >> 16) & 0x1F;
    int cop_reg = (instruction >> 11) & 0x1F;

    cpu.mfc(1, emotion_reg, cop_reg);
}

void EmotionInterpreter::cop0_mtc(EmotionEngine& cpu, uint32_t instruction)
{
    int cop_id = (instruction >> 26) & 0x3;
    int emotion_reg = (instruction >> 16) & 0x1F;
    int cop_reg = (instruction >> 11) & 0x1F;

    //Special case for MTPS/MTPC
    if (cop_reg == 25)
    {
        if (instruction & 0x1)
        {
            int pc_reg = (instruction >> 1) & 0x1F;
            cpu.mtpc(pc_reg, emotion_reg);
        }
        else
            cpu.mtps(emotion_reg);
    }
    else
        cpu.mtc(0, emotion_reg, cop_reg);
}

void EmotionInterpreter::cop1_mtc(EmotionEngine& cpu, uint32_t instruction)
{
    int emotion_reg = (instruction >> 16) & 0x1F;
    int cop_reg = (instruction >> 11) & 0x1F;

    cpu.mtc(1, emotion_reg, cop_reg);
}

void EmotionInterpreter::cop1_cfc(EmotionEngine &cpu, uint32_t instruction)
{
    int emotion_reg = (instruction >> 16) & 0x1F;
    int cop_reg = (instruction >> 11) & 0x1F;
    cpu.cfc(1, emotion_reg, cop_reg, instruction);
}

void EmotionInterpreter::cop2_cfc(EmotionEngine &cpu, uint32_t instruction)
{
    cpu.cop2_updatevu0();
    int emotion_reg = (instruction >> 16) & 0x1F;
    int cop_reg = (instruction >> 11) & 0x1F;
    cpu.cfc(2, emotion_reg, cop_reg, instruction);
}

void EmotionInterpreter::cop1_ctc(EmotionEngine &cpu, uint32_t instruction)
{
    int emotion_reg = (instruction >> 16) & 0x1F;
    int cop_reg = (instruction >> 11) & 0x1F;
    cpu.ctc(1, emotion_reg, cop_reg, instruction);
}

void EmotionInterpreter::cop2_ctc(EmotionEngine &cpu, uint32_t instruction)
{
    cpu.cop2_updatevu0();
    int emotion_reg = (instruction >> 16) & 0x1F;
    int cop_reg = (instruction >> 11) & 0x1F;
    cpu.ctc(2, emotion_reg, cop_reg, instruction);
}

void EmotionInterpreter::cop_bc0(EmotionEngine &cpu, uint32_t instruction)
{
    const static bool likely[] = {false, false, true, true};
    const static bool op_true[] = {false, true, false, true};
    int32_t offset = ((int16_t)(instruction & 0xFFFF)) << 2;
    uint8_t op = (instruction >> 16) & 0x1F;
    if (op > 3)
    {
        unknown_op("bc0", instruction, op);
    }
    cpu.cp0_bc0(offset, op_true[op], likely[op]);
}

void EmotionInterpreter::cop2_qmfc2(EmotionEngine &cpu, uint32_t instruction)
{
    cpu.cop2_updatevu0();

    int dest = (instruction >> 16) & 0x1F;
    int cop_reg = (instruction >> 11) & 0x1F;
    int interlock = (instruction & 0x1);
    if (interlock)
    {
        if (cpu.vu0_wait())
        {
            cpu.set_PC(cpu.get_PC() - 4);
            return;
        }
        cpu.clear_interlock();
    }
    cpu.qmfc2(dest, cop_reg);
}

void EmotionInterpreter::cop2_qmtc2(EmotionEngine &cpu, uint32_t instruction)
{
    cpu.cop2_updatevu0();

    int source = (instruction >> 16) & 0x1F;
    int cop_reg = (instruction >> 11) & 0x1F;
    int interlock = (instruction & 0x1);

    if (interlock)
    {
        if (cpu.check_interlock())
        {
            cpu.set_PC(cpu.get_PC() - 4);
            return;
        }
        cpu.clear_interlock();
    }

    cpu.qmtc2(source, cop_reg);
}

void EmotionInterpreter::nop(EmotionEngine &cpu, uint32_t instruction) 
{
    // NULLSUB for sll $zero, $zero, 0 and unimplemented operations like prefetch
}

[[ noreturn ]] void EmotionInterpreter::unknown_op(const char *type, uint32_t instruction, uint16_t op)
{
    Errors::die("[EE Interpreter] Unrecognized %s op $%04X (instr: $%08X)\n", type, op, instruction);
}
