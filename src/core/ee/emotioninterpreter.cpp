#include <cstdio>
#include <cstdlib>
#include "emotioninterpreter.hpp"

void EmotionInterpreter::interpret(EmotionEngine &cpu, uint32_t instruction)
{
    if (!instruction)
        return;
    int op = instruction >> 26;
    switch (op)
    {
        case 0x00:
            special(cpu, instruction);
            break;
        case 0x01:
            regimm(cpu, instruction);
            break;
        case 0x02:
            j(cpu, instruction);
            break;
        case 0x03:
            jal(cpu, instruction);
            break;
        case 0x04:
            beq(cpu, instruction);
            break;
        case 0x05:
            bne(cpu, instruction);
            break;
        case 0x06:
            blez(cpu, instruction);
            break;
        case 0x07:
            bgtz(cpu, instruction);
            break;
        case 0x08:
            addi(cpu, instruction);
            break;
        case 0x09:
            addiu(cpu, instruction);
            break;
        case 0x0A:
            slti(cpu, instruction);
            break;
        case 0x0B:
            sltiu(cpu, instruction);
            break;
        case 0x0C:
            andi(cpu, instruction);
            break;
        case 0x0D:
            ori(cpu, instruction);
            break;
        case 0x0E:
            xori(cpu, instruction);
            break;
        case 0x0F:
            lui(cpu, instruction);
            break;
        case 0x10:
        case 0x11:
        case 0x12:
        case 0x13:
            cop(cpu, instruction);
            break;
        case 0x14:
            beql(cpu, instruction);
            break;
        case 0x15:
            bnel(cpu, instruction);
            break;
        case 0x16:
            blezl(cpu, instruction);
            break;
        case 0x17:
            bgtzl(cpu, instruction);
            break;
        case 0x19:
            daddiu(cpu, instruction);
            break;
        case 0x1A:
            ldl(cpu, instruction);
            break;
        case 0x1B:
            ldr(cpu, instruction);
            break;
        case 0x1C:
            mmi(cpu, instruction);
            break;
        case 0x1E:
            lq(cpu, instruction);
            break;
        case 0x1F:
            sq(cpu, instruction);
            break;
        case 0x20:
            lb(cpu, instruction);
            break;
        case 0x21:
            lh(cpu, instruction);
            break;
        case 0x22:
            lwl(cpu, instruction);
            break;
        case 0x23:
            lw(cpu, instruction);
            break;
        case 0x24:
            lbu(cpu, instruction);
            break;
        case 0x25:
            lhu(cpu, instruction);
            break;
        case 0x26:
            lwr(cpu, instruction);
            break;
        case 0x27:
            lwu(cpu, instruction);
            break;
        case 0x28:
            sb(cpu, instruction);
            break;
        case 0x29:
            sh(cpu, instruction);
            break;
        case 0x2A:
            swl(cpu, instruction);
            break;
        case 0x2B:
            sw(cpu, instruction);
            break;
        case 0x2C:
            sdl(cpu, instruction);
            break;
        case 0x2D:
            sdr(cpu, instruction);
            break;
        case 0x2E:
            swr(cpu, instruction);
            break;
        case 0x2F:
            break;
        case 0x31:
            lwc1(cpu, instruction);
            break;
        case 0x33:
            //prefetch
            break;
        case 0x36:
            lqc2(cpu, instruction);
            break;
        case 0x37:
            ld(cpu, instruction);
            break;
        case 0x39:
            swc1(cpu, instruction);
            break;
        case 0x3E:
            sqc2(cpu, instruction);
            break;
        case 0x3F:
            sd(cpu, instruction);
            break;
        default:
            unknown_op("normal", instruction, op);
    }
}

void EmotionInterpreter::regimm(EmotionEngine &cpu, uint32_t instruction)
{
    int op = (instruction >> 16) & 0x1F;
    switch (op)
    {
        case 0x00:
            bltz(cpu, instruction);
            break;
        case 0x01:
            bgez(cpu, instruction);
            break;
        case 0x02:
            bltzl(cpu, instruction);
            break;
        case 0x03:
            bgezl(cpu, instruction);
            break;
        case 0x10:
            bltzal(cpu,instruction);
            break;
        case 0x11:
            bgezal(cpu,instruction);
            break;
        case 0x12:
            bltzall(cpu,instruction);
            break;
        case 0x13:
            bgezall(cpu,instruction);
            break;
        case 0x18:
            mtsab(cpu, instruction);
            break;
        case 0x19:
            mtsah(cpu, instruction);
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
    cpu.set_gpr<uint32_t>(31, cpu.get_PC() + 8);
    cpu.branch(reg < 0, offset);
}

void EmotionInterpreter::bltzall(EmotionEngine &cpu, uint32_t instruction)
{
    int32_t offset = (int16_t)(instruction & 0xFFFF);
    offset <<= 2;
    int64_t reg = (instruction >> 21) & 0x1F;
    reg = cpu.get_gpr<int64_t>(reg);
    cpu.set_gpr<uint32_t>(31, cpu.get_PC() + 8);
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
    cpu.set_gpr<uint32_t>(31, cpu.get_PC() + 8);
    cpu.branch(reg >= 0, offset);
}

void EmotionInterpreter::bgezall(EmotionEngine &cpu, uint32_t instruction)
{
    int32_t offset = (int16_t)(instruction & 0xFFFF);
    offset <<= 2;
    int64_t reg = (instruction >> 21) & 0x1F;
    reg = cpu.get_gpr<int64_t>(reg);
    cpu.set_gpr<uint32_t>(31, cpu.get_PC() + 8);
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
    cpu.set_gpr<uint32_t>(31, PC + 8);
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
    cpu.set_gpr<int64_t>(dest, (int32_t)mem);
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

void EmotionInterpreter::cop(EmotionEngine &cpu, uint32_t instruction)
{
    uint16_t op = (instruction >> 21) & 0x1F;
    uint8_t cop_id = ((instruction >> 26) & 0x3);
    if (cop_id == 2 && op >= 0x10)
    {
        cpu.cop2_special(instruction);
        return;
    }
    switch (op | (cop_id * 0x100))
    {
        case 0x000:
        case 0x100:
            cop_mfc(cpu, instruction);
            break;
        case 0x004:
        case 0x104:
            cop_mtc(cpu, instruction);
            break;
        case 0x010:
        {
            uint8_t op2 = instruction & 0x3F;
            switch (op2)
            {
                case 0x2:
                    break;
                case 0x18:
                    cpu.eret();
                    break;
                case 0x38:
                    cpu.ei();
                    break;
                case 0x39:
                    cpu.di();
                    break;
                default:
                    unknown_op("cop0 type2", instruction, op2);
            }
        }
            break;
        case 0x102:
        case 0x202:
            cop_cfc(cpu, instruction);
            break;
        case 0x106:
        case 0x206:
            cop_ctc(cpu, instruction);
            break;
        case 0x008:
            cop_bc0(cpu, instruction);
            break;
        case 0x108:
            cop_bc1(cpu, instruction);
            break;
        case 0x110:
            cpu.fpu_cop_s(instruction);
            break;
        case 0x114:
            cop_cvt_s_w(cpu, instruction);
            break;
        case 0x201:
            cop2_qmfc2(cpu, instruction);
            break;
        case 0x205:
            cop2_qmtc2(cpu, instruction);
            break;
        default:
            unknown_op("cop", instruction, op | (cop_id * 0x100));
    }
}

void EmotionInterpreter::cop_mfc(EmotionEngine& cpu, uint32_t instruction)
{
    int cop_id = (instruction >> 26) & 0x3;
    int emotion_reg = (instruction >> 16) & 0x1F;
    int cop_reg = (instruction >> 11) & 0x1F;
    cpu.mfc(cop_id, emotion_reg, cop_reg);
}

void EmotionInterpreter::cop_mtc(EmotionEngine& cpu, uint32_t instruction)
{
    int cop_id = (instruction >> 26) & 0x3;
    int emotion_reg = (instruction >> 16) & 0x1F;
    int cop_reg = (instruction >> 11) & 0x1F;
    cpu.mtc(cop_id, emotion_reg, cop_reg);
}

void EmotionInterpreter::cop_cfc(EmotionEngine &cpu, uint32_t instruction)
{
    int cop_id = (instruction >> 26) & 0x3;
    int emotion_reg = (instruction >> 16) & 0x1F;
    int cop_reg = (instruction >> 11) & 0x1F;
    cpu.cfc(cop_id, emotion_reg, cop_reg);
}

void EmotionInterpreter::cop_ctc(EmotionEngine &cpu, uint32_t instruction)
{
    int cop_id = (instruction >> 26) & 0x3;
    int emotion_reg = (instruction >> 16) & 0x1F;
    int cop_reg = (instruction >> 11) & 0x1F;
    cpu.ctc(cop_id, emotion_reg, cop_reg);
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
    int dest = (instruction >> 16) & 0x1F;
    int cop_reg = (instruction >> 11) & 0x1F;
    cpu.qmfc2(dest, cop_reg);
}

void EmotionInterpreter::cop2_qmtc2(EmotionEngine &cpu, uint32_t instruction)
{
    int source = (instruction >> 16) & 0x1F;
    int cop_reg = (instruction >> 11) & 0x1F;
    cpu.qmtc2(source, cop_reg);
}

void EmotionInterpreter::unknown_op(const char *type, uint32_t instruction, uint16_t op)
{
    printf("[EE Interpreter] Unrecognized %s op $%04X\n", type, op);
    printf("[EE Interpreter] Instr: $%08X\n", instruction);
    exit(1);
}
