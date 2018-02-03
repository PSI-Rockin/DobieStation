#include <cstdio>
#include <cstdlib>
#include "emotioninterpreter.hpp"

#ifdef NDISASSEMBLE
#define printf(fmt, ...)(0)
#endif

void EmotionInterpreter::interpret(EmotionEngine &cpu, uint32_t instruction)
{
    printf("[$%08X] $%08X - ", cpu.get_PC(), instruction);
    if (!instruction)
    {
        printf("nop\n");
        return;
    }
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
        case 0x19:
            daddiu(cpu, instruction);
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
        case 0x23:
            lw(cpu, instruction);
            break;
        case 0x24:
            lbu(cpu, instruction);
            break;
        case 0x25:
            lhu(cpu, instruction);
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
        case 0x2B:
            sw(cpu, instruction);
            break;
        case 0x2F:
            printf("cache");
            break;
        case 0x31:
            lwc1(cpu, instruction);
            break;
        case 0x37:
            ld(cpu, instruction);
            break;
        case 0x39:
            swc1(cpu, instruction);
            break;
        case 0x3F:
            sd(cpu, instruction);
            break;
        default:
#ifdef NDISASSEMBLE
#undef printf(fmt, ...)(0)
#endif
            printf("Unrecognized op $%02X\n", op);
            exit(1);
    }
#ifdef NDISASSEMBLE
#define printf(fmt, ...)(0)
#endif
    printf("\n");
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
        default:
#ifdef NDISASSEMBLE
#undef printf(fmt, ...)(0)
#endif
            printf("Unrecognized regimm op $%02X", op);
            exit(1);
    }
}

#ifdef NDISASSEMBLE
#define printf(fmt, ...)(0)
#endif

void EmotionInterpreter::bltz(EmotionEngine &cpu, uint32_t instruction)
{
    int32_t offset = (int16_t)(instruction & 0xFFFF);
    offset <<= 2;
    int64_t reg = (instruction >> 21) & 0x1F;
    printf("bltz {%d}, $%08X", reg, cpu.get_PC() + offset + 4);
    reg = cpu.get_gpr<int64_t>(reg);
    cpu.branch(reg < 0, offset);
}

void EmotionInterpreter::bgez(EmotionEngine &cpu, uint32_t instruction)
{
    int32_t offset = (int16_t)(instruction & 0xFFFF);
    offset <<= 2;
    int64_t reg = (instruction >> 21) & 0x1F;
    printf("bgez {%d}, $%08X", reg, cpu.get_PC() + offset + 4);
    reg = cpu.get_gpr<int64_t>(reg);
    cpu.branch(reg >= 0, offset);
}

void EmotionInterpreter::j(EmotionEngine &cpu, uint32_t instruction)
{
    uint32_t addr = (instruction & 0x3FFFFFF) << 2;
    uint32_t PC = cpu.get_PC();
    addr += (PC + 4) & 0xF0000000;
    printf("j $%08X", addr);
    cpu.jp(addr);
}

void EmotionInterpreter::jal(EmotionEngine &cpu, uint32_t instruction)
{
    uint32_t addr = (instruction & 0x3FFFFFF) << 2;
    uint32_t PC = cpu.get_PC();
    addr += (PC + 4) & 0xF0000000;
    printf("jal $%08X", addr);
    cpu.jp(addr);
    cpu.set_gpr<uint32_t>(31, PC + 8);
}

void EmotionInterpreter::beq(EmotionEngine &cpu, uint32_t instruction)
{
    int offset = (int16_t)(instruction & 0xFFFF);
    offset <<= 2;
    uint64_t reg1 = cpu.get_gpr<uint64_t>((instruction >> 21) & 0x1F);
    uint64_t reg2 = cpu.get_gpr<uint64_t>((instruction >> 16) & 0x1F);
    printf("beq {%d}, {%d}, $%08X", (instruction >> 21) & 0x1F, (instruction >> 16) & 0x1F, cpu.get_PC() + offset + 4);
    cpu.branch(reg1 == reg2, offset);
}

void EmotionInterpreter::bne(EmotionEngine &cpu, uint32_t instruction)
{
    int offset = (int16_t)(instruction & 0xFFFF);
    offset <<= 2;
    uint64_t reg1 = cpu.get_gpr<uint64_t>((instruction >> 21) & 0x1F);
    uint64_t reg2 = cpu.get_gpr<uint64_t>((instruction >> 16) & 0x1F);
    printf("bne {%d}, {%d}, $%08X", (instruction >> 21) & 0x1F, (instruction >> 16) & 0x1F, cpu.get_PC() + offset + 4);
    cpu.branch(reg1 != reg2, offset);
}

void EmotionInterpreter::blez(EmotionEngine &cpu, uint32_t instruction)
{
    int32_t offset = (int16_t)(instruction & 0xFFFF);
    offset <<= 2;
    int64_t reg = (instruction >> 21) & 0x1F;
    printf("blez {%d}, $%08X", reg, cpu.get_PC() + offset + 4);
    reg = cpu.get_gpr<int64_t>(reg);
    cpu.branch(reg <= 0, offset);
}

void EmotionInterpreter::bgtz(EmotionEngine &cpu, uint32_t instruction)
{
    int32_t offset = (int16_t)(instruction & 0xFFFF);
    offset <<= 2;
    int64_t reg = (instruction >> 21) & 0x1F;
    printf("bgtz {%d}, $%08X", reg, cpu.get_PC() + offset + 4);
    reg = cpu.get_gpr<int64_t>(reg);
    cpu.branch(reg > 0, offset);
}

void EmotionInterpreter::addi(EmotionEngine &cpu, uint32_t instruction)
{
    //TODO: overflow
    int16_t imm = (int16_t)(instruction & 0xFFFF);
    uint64_t dest = (instruction >> 16) & 0x1F;
    uint32_t source = (instruction >> 21) & 0x1F;
    printf("addi {%d}, {%d}, $%04X", dest, source, imm);
    int32_t result = cpu.get_gpr<int32_t>(source);
    result += imm;
    cpu.set_gpr<int64_t>(dest, result);
}

void EmotionInterpreter::addiu(EmotionEngine &cpu, uint32_t instruction)
{
    int16_t imm = (int16_t)(instruction & 0xFFFF);
    uint64_t dest = (instruction >> 16) & 0x1F;
    uint32_t source = (instruction >> 21) & 0x1F;
    printf("addiu {%d}, {%d}, $%04X", dest, source, imm);
    int32_t result = cpu.get_gpr<int32_t>(source);
    result += imm;
    cpu.set_gpr<int64_t>(dest, result);
}

void EmotionInterpreter::slti(EmotionEngine &cpu, uint32_t instruction)
{
    int64_t imm = (int64_t)(int16_t)(instruction & 0xFFFF);
    uint64_t dest = (instruction >> 16) & 0x1F;
    int64_t source = (instruction >> 21) & 0x1F;
    printf("slti {%d}, {%d}, %lld", dest, source, imm);
    source = cpu.get_gpr<int64_t>(source);
    cpu.set_gpr<uint64_t>(dest, source < imm);
}

void EmotionInterpreter::sltiu(EmotionEngine &cpu, uint32_t instruction)
{
    uint64_t imm = instruction & 0xFFFF;
    uint64_t dest = (instruction >> 16) & 0x1F;
    uint64_t source = (instruction >> 21) & 0x1F;
    printf("sltiu {%d}, {%d}, %lld", dest, source, imm);
    source = cpu.get_gpr<uint64_t>(source);
    cpu.set_gpr<uint64_t>(dest, source < imm);
}

void EmotionInterpreter::andi(EmotionEngine &cpu, uint32_t instruction)
{
    uint64_t imm = instruction & 0xFFFF;
    uint64_t dest = (instruction >> 16) & 0x1F;
    uint64_t source = (instruction >> 21) & 0x1F;
    printf("andi {%d}, {%d}, $%04X", dest, source, imm);
    cpu.set_gpr<uint64_t>(dest, cpu.get_gpr<uint64_t>(source) & imm);
}

void EmotionInterpreter::ori(EmotionEngine &cpu, uint32_t instruction)
{
    uint64_t imm = instruction & 0xFFFF;
    uint64_t dest = (instruction >> 16) & 0x1F;
    uint64_t source = (instruction >> 21) & 0x1F;
    printf("ori {%d}, {%d}, $%04X", dest, source, imm);
    cpu.set_gpr<uint64_t>(dest, cpu.get_gpr<uint64_t>(source) | imm);
}

void EmotionInterpreter::xori(EmotionEngine &cpu, uint32_t instruction)
{
    uint64_t imm = instruction & 0xFFFF;
    uint64_t dest = (instruction >> 16) & 0x1F;
    uint64_t source = (instruction >> 21) & 0x1F;
    printf("xori {%d}, {%d}, $%04X", dest, source, imm);
    cpu.set_gpr<uint64_t>(dest, cpu.get_gpr<uint64_t>(source) ^ imm);
}

void EmotionInterpreter::lui(EmotionEngine &cpu, uint32_t instruction)
{
    int64_t imm = (int64_t)(int32_t)((instruction & 0xFFFF) << 16);
    uint64_t dest = (instruction >> 16) & 0x1F;
    printf("lui {%d}, $%04X", dest, instruction & 0xFFFF);
    cpu.set_gpr<uint64_t>(dest, imm);
}

void EmotionInterpreter::beql(EmotionEngine &cpu, uint32_t instruction)
{
    int offset = (int16_t)(instruction & 0xFFFF);
    offset <<= 2;
    uint64_t reg1 = cpu.get_gpr<uint64_t>((instruction >> 21) & 0x1F);
    uint64_t reg2 = cpu.get_gpr<uint64_t>((instruction >> 16) & 0x1F);
    printf("beql {%d}, {%d}, $%08X", (instruction >> 21) & 0x1F, (instruction >> 16) & 0x1F, cpu.get_PC() + offset + 4);
    cpu.branch_likely(reg1 == reg2, offset);
}

void EmotionInterpreter::bnel(EmotionEngine &cpu, uint32_t instruction)
{
    int offset = (int16_t)(instruction & 0xFFFF);
    offset <<= 2;
    uint64_t reg1 = cpu.get_gpr<uint64_t>((instruction >> 21) & 0x1F);
    uint64_t reg2 = cpu.get_gpr<uint64_t>((instruction >> 16) & 0x1F);
    printf("bnel {%d}, {%d}, $%08X", (instruction >> 21) & 0x1F, (instruction >> 16) & 0x1F, cpu.get_PC() + offset + 4);
    cpu.branch_likely(reg1 != reg2, offset);
}

void EmotionInterpreter::daddiu(EmotionEngine &cpu, uint32_t instruction)
{
    int16_t imm = (int16_t)(instruction & 0xFFFF);
    int64_t source = (instruction >> 21) & 0x1F;
    uint64_t dest = (instruction >> 16) & 0x1F;
    printf("daddiu {%d}, {%d}, $%04X", dest, source, imm);
    source = cpu.get_gpr<int64_t>(source);
    cpu.set_gpr<uint64_t>(dest, source + imm);
}

void EmotionInterpreter::lq(EmotionEngine &cpu, uint32_t instruction)
{
    int16_t imm = (int16_t)(instruction & 0xFFFF);
    uint64_t dest = (instruction >> 16) & 0x1F;
    uint64_t base = (instruction >> 21) & 0x1F;
    printf("lq {%d}, %d{%d}", dest, imm, base);
    uint32_t addr = cpu.get_gpr<uint32_t>(base) + imm;
    cpu.set_gpr<uint64_t>(dest, cpu.read64(addr));
    cpu.set_gpr<uint64_t>(dest, cpu.read64(addr + 8), 1);
}

void EmotionInterpreter::sq(EmotionEngine &cpu, uint32_t instruction)
{
    int16_t imm = (int16_t)(instruction & 0xFFFF);
    uint64_t source = (instruction >> 16) & 0x1F;
    uint64_t base = (instruction >> 21) & 0x1F;
    printf("sq {%d}, %d{%d}", source, imm, base);
    uint32_t addr = cpu.get_gpr<uint32_t>(base) + imm;
    cpu.write64(addr, cpu.get_gpr<uint64_t>(source));
    cpu.write64(addr + 8, cpu.get_gpr<uint64_t>(source, 1));
}

void EmotionInterpreter::lb(EmotionEngine &cpu, uint32_t instruction)
{
    int16_t offset = (int16_t)(instruction & 0xFFFF);
    uint64_t dest = (instruction >> 16) & 0x1F;
    uint32_t base = (instruction >> 21) & 0x1F;
    printf("lb {%d}, %d{%d}", dest, offset, base);
    uint32_t addr = cpu.get_gpr<uint32_t>(base);
    addr += offset;
    cpu.set_gpr<int64_t>(dest, (int8_t)cpu.read8(addr));
}

void EmotionInterpreter::lh(EmotionEngine &cpu, uint32_t instruction)
{
    int16_t offset = (int16_t)(instruction & 0xFFFF);
    uint64_t dest = (instruction >> 16) & 0x1F;
    uint32_t base = (instruction >> 21) & 0x1F;
    printf("lh {%d}, %d{%d}", dest, offset, base);
    uint32_t addr = cpu.get_gpr<uint32_t>(base);
    addr += offset;
    cpu.set_gpr<int64_t>(dest, (int16_t)cpu.read16(addr));
}

void EmotionInterpreter::lw(EmotionEngine &cpu, uint32_t instruction)
{
    int16_t offset = (int16_t)(instruction & 0xFFFF);
    uint64_t dest = (instruction >> 16) & 0x1F;
    uint32_t base = (instruction >> 21) & 0x1F;
    printf("lw {%d}, %d{%d}", dest, offset, base);
    uint32_t addr = cpu.get_gpr<uint32_t>(base);
    addr += offset;
    cpu.set_gpr<int64_t>(dest, (int32_t)cpu.read32(addr));
}

void EmotionInterpreter::lbu(EmotionEngine &cpu, uint32_t instruction)
{
    int16_t offset = (int16_t)(instruction & 0xFFFF);
    uint64_t dest = (instruction >> 16) & 0x1F;
    uint32_t base = (instruction >> 21) & 0x1F;
    printf("lbu {%d}, %d{%d}", dest, offset, base);
    uint32_t addr = cpu.get_gpr<uint32_t>(base);
    addr += offset;
    cpu.set_gpr<uint64_t>(dest, cpu.read8(addr));
}

void EmotionInterpreter::lhu(EmotionEngine &cpu, uint32_t instruction)
{
    int16_t offset = (int16_t)(instruction & 0xFFFF);
    uint64_t dest = (instruction >> 16) & 0x1F;
    uint32_t base = (instruction >> 21) & 0x1F;
    printf("lhu {%d}, %d{%d}", dest, offset, base);
    uint32_t addr = cpu.get_gpr<uint32_t>(base);
    addr += offset;
    cpu.set_gpr<uint64_t>(dest, cpu.read16(addr));
}

void EmotionInterpreter::lwu(EmotionEngine &cpu, uint32_t instruction)
{
    int16_t offset = (int16_t)(instruction & 0xFFFF);
    uint32_t dest = (instruction >> 16) & 0x1F;
    uint32_t base = (instruction >> 21) & 0x1F;
    printf("lwu {%d}, %d{%d}", dest, offset, base);
    uint32_t addr = cpu.get_gpr<uint32_t>(base);
    addr += offset;
    cpu.set_gpr<uint64_t>(dest, cpu.read32(addr));
}

void EmotionInterpreter::sb(EmotionEngine &cpu, uint32_t instruction)
{
    int16_t offset = (int16_t)(instruction & 0xFFFF);
    uint32_t source = (instruction >> 16) & 0x1F;
    uint32_t base = (instruction >> 21) & 0x1F;
    printf("sb {%d}, %d{%d}", source, offset, base);
    uint32_t addr = cpu.get_gpr<uint32_t>(base);
    addr += offset;
    cpu.write8(addr, cpu.get_gpr<uint8_t>(source));
}

void EmotionInterpreter::sh(EmotionEngine &cpu, uint32_t instruction)
{
    int16_t offset = (int16_t)(instruction & 0xFFFF);
    uint32_t source = (instruction >> 16) & 0x1F;
    uint32_t base = (instruction >> 21) & 0x1F;
    printf("sh {%d}, %d{%d}", source, offset, base);
    uint32_t addr = cpu.get_gpr<uint32_t>(base);
    addr += offset;
    cpu.write16(addr, cpu.get_gpr<uint16_t>(source));
}

void EmotionInterpreter::sw(EmotionEngine &cpu, uint32_t instruction)
{
    int16_t offset = (int16_t)(instruction & 0xFFFF);
    uint32_t source = (instruction >> 16) & 0x1F;
    uint32_t base = (instruction >> 21) & 0x1F;
    printf("sw {%d}, %d{%d}", source, offset, base);
    uint32_t addr = cpu.get_gpr<uint32_t>(base);
    addr += offset;
    cpu.write32(addr, cpu.get_gpr<uint32_t>(source));
}

void EmotionInterpreter::lwc1(EmotionEngine &cpu, uint32_t instruction)
{
    int16_t offset = (int16_t)(instruction & 0xFFFF);
    uint32_t dest = (instruction >> 16) & 0x1F;
    uint32_t base = (instruction >> 21) & 0x1F;
    printf("lwc1 {%d}, %d{%d}", dest, offset, base);
    uint32_t addr = cpu.get_gpr<uint32_t>(base);
    addr += offset;
    cpu.lwc1(addr, dest);
}

void EmotionInterpreter::ld(EmotionEngine &cpu, uint32_t instruction)
{
    int16_t offset = (int16_t)(instruction & 0xFFFF);
    uint32_t dest = (instruction >> 16) & 0x1F;
    uint32_t base = (instruction >> 21) & 0x1F;
    printf("ld {%d}, %d{%d}", dest, offset, base);
    uint32_t addr = cpu.get_gpr<uint32_t>(base);
    addr += offset;
    cpu.set_gpr(dest, cpu.read64(addr));
}

void EmotionInterpreter::swc1(EmotionEngine &cpu, uint32_t instruction)
{
    int16_t offset = (int16_t)(instruction & 0xFFFF);
    uint32_t source = (instruction >> 16) & 0x1F;
    uint32_t base = (instruction >> 21) & 0x1F;
    printf("swc1 {%d}, %d{%d}", source, offset, base);
    uint32_t addr = cpu.get_gpr<uint32_t>(base);
    addr += offset;
    cpu.swc1(addr, source);
}

void EmotionInterpreter::sd(EmotionEngine &cpu, uint32_t instruction)
{
    int16_t offset = (int16_t)(instruction & 0xFFFF);
    uint32_t source = (instruction >> 16) & 0x1F;
    uint32_t base = (instruction >> 21) & 0x1F;
    printf("sd {%d}, %d{%d}", source, offset, base);
    uint32_t addr = cpu.get_gpr<uint32_t>(base);
    addr += offset;
    cpu.write64(addr, cpu.get_gpr<uint64_t>(source));
}

void EmotionInterpreter::cop(EmotionEngine &cpu, uint32_t instruction)
{
    uint16_t op = (instruction >> 21) & 0x1F;
    uint8_t cop_id = ((instruction >> 26) & 0x3);
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
            printf("tlbwi");
            break;
        case 0x110:
        {
            uint32_t dest = (instruction >> 6) & 0x1F;
            uint32_t source = (instruction >> 11) & 0x1F;
            printf("mov.s f{%d}, f{%d}", dest, source);
            cpu.fpu_mov_s(dest, source);
        }
            break;
        case 0x114:
            cop_cvt_s_w(cpu, instruction);
            break;
        default:
#ifdef NDISASSEMBLE
#undef printf(fmt, ...)(0)
#endif
            printf("Unrecognized cop op $%d%02X\n", cop_id, op);
            exit(1);
    }
}

#ifdef NDISASSEMBLE
#define printf(fmt, ...)(0)
#endif

void EmotionInterpreter::cop_mfc(EmotionEngine& cpu, uint32_t instruction)
{
    int cop_id = (instruction >> 26) & 0x3;
    int emotion_reg = (instruction >> 16) & 0x1F;
    int cop_reg = (instruction >> 11) & 0x1F;
    printf("mfc%d {%d}, {%d}", cop_id, emotion_reg, cop_reg);
    cpu.mfc(cop_id, emotion_reg, cop_reg);
}

void EmotionInterpreter::cop_mtc(EmotionEngine& cpu, uint32_t instruction)
{
    int cop_id = (instruction >> 26) & 0x3;
    int emotion_reg = (instruction >> 16) & 0x1F;
    int cop_reg = (instruction >> 11) & 0x1F;
    printf("mtc%d {%d}, {%d}", cop_id, emotion_reg, cop_reg);
    cpu.mtc(cop_id, emotion_reg, cop_reg);
}

void EmotionInterpreter::cop_cvt_s_w(EmotionEngine &cpu, uint32_t instruction)
{
    int source = (instruction >> 11) & 0x1F;
    int dest = (instruction >> 6) & 0x1F;
    printf("cvt.s.w f{%d}, f{%d}", dest, source);
    cpu.fpu_cvt_s_w(dest, source);
}
