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
        case 0x36:
            break;
        case 0x37:
            ld(cpu, instruction);
            break;
        case 0x39:
            swc1(cpu, instruction);
            break;
        case 0x3E:
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

void EmotionInterpreter::mtsah(EmotionEngine &cpu, uint32_t instruction)
{
    uint32_t reg = (instruction >> 21) & 0x1F;
    uint16_t imm = instruction & 0xFFFF;

    reg = cpu.get_gpr<uint32_t>(reg);

    reg = (reg & 0x7) | (imm & 0x7);
    cpu.set_SA(reg * 16);
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
    int32_t result = cpu.get_gpr<int32_t>(source);
    result += imm;
    cpu.set_gpr<int64_t>(dest, result);
}

void EmotionInterpreter::addiu(EmotionEngine &cpu, uint32_t instruction)
{
    int16_t imm = (int16_t)(instruction & 0xFFFF);
    uint64_t dest = (instruction >> 16) & 0x1F;
    uint32_t source = (instruction >> 21) & 0x1F;
    int32_t result = cpu.get_gpr<int32_t>(source);
    result += imm;
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
    uint64_t imm = instruction & 0xFFFF;
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
    int16_t imm = (int16_t)(instruction & 0xFFFF);
    uint64_t dest = (instruction >> 16) & 0x1F;
    uint64_t base = (instruction >> 21) & 0x1F;

    uint32_t addr = cpu.get_gpr<uint32_t>(base) + imm;
    uint64_t dword = cpu.read64(addr & ~0x7);
    uint64_t new_reg = cpu.get_gpr<uint64_t>(dest);
    uint8_t new_bytes = (addr & 0x7) + 1;
    for (int i = 0; i < new_bytes; i++)
    {
        int offset = ((new_bytes - i - 1) << 3);
        int reg_offset = (56 - (i << 3));
        uint64_t byte = (dword >> offset) & 0xFF;
        new_reg &= ~(0xFFUL << reg_offset);
        new_reg |= byte << reg_offset;
    }

    cpu.set_gpr<uint64_t>(dest, new_reg);
}

void EmotionInterpreter::ldr(EmotionEngine &cpu, uint32_t instruction)
{
    int16_t imm = (int16_t)(instruction & 0xFFFF);
    uint64_t dest = (instruction >> 16) & 0x1F;
    uint64_t base = (instruction >> 21) & 0x1F;

    uint32_t addr = cpu.get_gpr<uint32_t>(base) + imm;
    uint64_t dword = cpu.read64(addr & ~0x7);
    uint64_t new_reg = cpu.get_gpr<uint64_t>(dest);
    uint8_t new_bytes = 8 - (addr & 0x7);
    for (int i = 0; i < new_bytes; i++)
    {
        int offset = 56 - (i << 3);
        int reg_offset = ((new_bytes - i - 1) << 3);
        uint64_t byte = (dword >> offset) & 0xFF;
        new_reg &= ~(0xFFUL << reg_offset);
        new_reg |= byte << reg_offset;
    }

    cpu.set_gpr<uint64_t>(dest, new_reg);
}

void EmotionInterpreter::lq(EmotionEngine &cpu, uint32_t instruction)
{
    int16_t imm = (int16_t)(instruction & 0xFFFF);
    uint64_t dest = (instruction >> 16) & 0x1F;
    uint64_t base = (instruction >> 21) & 0x1F;
    uint32_t addr = cpu.get_gpr<uint32_t>(base) + imm;
    addr &= ~0xF;
    cpu.set_gpr<uint64_t>(dest, cpu.read64(addr));
    cpu.set_gpr<uint64_t>(dest, cpu.read64(addr + 8), 1);
}

void EmotionInterpreter::sq(EmotionEngine &cpu, uint32_t instruction)
{
    int16_t imm = (int16_t)(instruction & 0xFFFF);
    uint64_t source = (instruction >> 16) & 0x1F;
    uint64_t base = (instruction >> 21) & 0x1F;

    uint32_t addr = cpu.get_gpr<uint32_t>(base) + imm;
    addr &= ~0xF;
    cpu.write64(addr, cpu.get_gpr<uint64_t>(source));
    cpu.write64(addr + 8, cpu.get_gpr<uint64_t>(source, 1));
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
    int16_t offset = (int16_t)(instruction & 0xFFFF);
    uint32_t dest = (instruction >> 16) & 0x1F;
    uint32_t base = (instruction >> 21) & 0x1F;
    uint32_t addr = cpu.get_gpr<uint32_t>(base) + offset;
    uint32_t word = cpu.read32(addr & ~0x3);
    uint32_t new_reg = cpu.get_gpr<uint32_t>(dest);
    uint8_t new_bytes = (addr & 0x3) + 1;

    for (int i = 0; i < new_bytes; i++)
    {
        int offset = ((new_bytes - i - 1) << 3);
        int reg_offset = (24 - (i << 3));
        uint32_t byte = (word >> offset) & 0xFF;
        new_reg &= ~(0xFF << reg_offset);
        new_reg |= byte << reg_offset;
    }

    cpu.set_gpr<int64_t>(dest, (int32_t)new_reg);
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
    int16_t imm = (int16_t)(instruction & 0xFFFF);
    uint32_t dest = (instruction >> 16) & 0x1F;
    uint32_t base = (instruction >> 21) & 0x1F;

    uint32_t addr = cpu.get_gpr<uint32_t>(base) + imm;
    uint32_t word = cpu.read32(addr & ~0x3);
    uint32_t new_reg = cpu.get_gpr<uint32_t>(dest);
    uint8_t new_bytes = 4 - (addr & 0x3);
    for (int i = 0; i < new_bytes; i++)
    {
        int offset = 24 - (i << 3);
        int reg_offset = ((new_bytes - i - 1) << 3);
        uint32_t byte = (word >> offset) & 0xFF;
        new_reg &= ~(0xFF << reg_offset);
        new_reg |= byte << reg_offset;
    }

    cpu.set_gpr<int64_t>(dest, (int32_t)new_reg);
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
    int16_t imm = (int16_t)(instruction & 0xFFFF);
    uint32_t source = (instruction >> 16) & 0x1F;
    uint32_t base = (instruction >> 21) & 0x1F;

    uint32_t addr = cpu.get_gpr<uint32_t>(base) + imm;
    uint32_t reg = cpu.get_gpr<uint32_t>(source);
    uint32_t new_word = cpu.read32(addr & ~0x3);
    uint8_t new_bytes = (addr & 0x3) + 1;
    for (int i = 0; i < new_bytes; i++)
    {
        int offset = ((new_bytes - i - 1) << 3);
        int reg_offset = (24 - (i << 3));
        uint32_t byte = (reg >> reg_offset) & 0xFF;
        new_word &= ~(0xFF << offset);
        new_word |= byte << offset;
    }

    cpu.write32(addr & ~0x3, new_word);
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
    int16_t imm = (int16_t)(instruction & 0xFFFF);
    uint64_t source = (instruction >> 16) & 0x1F;
    uint64_t base = (instruction >> 21) & 0x1F;

    uint32_t addr = cpu.get_gpr<uint32_t>(base) + imm;
    uint64_t reg = cpu.get_gpr<uint64_t>(source);
    uint64_t new_dword = cpu.read64(addr & ~0x7);
    uint8_t new_bytes = (addr & 0x7) + 1;
    for (int i = 0; i < new_bytes; i++)
    {
        int offset = ((new_bytes - i - 1) << 3);
        int reg_offset = (56 - (i << 3));
        uint64_t byte = (reg >> reg_offset) & 0xFF;
        new_dword &= ~(0xFFUL << offset);
        new_dword |= byte << offset;
    }

    cpu.write64(addr & ~0x7, new_dword);
}

void EmotionInterpreter::sdr(EmotionEngine &cpu, uint32_t instruction)
{
    int16_t imm = (int16_t)(instruction & 0xFFFF);
    uint64_t source = (instruction >> 16) & 0x1F;
    uint64_t base = (instruction >> 21) & 0x1F;

    uint32_t addr = cpu.get_gpr<uint32_t>(base) + imm;
    uint64_t reg = cpu.get_gpr<uint64_t>(source);
    uint64_t new_dword = cpu.read64(addr & ~0x7);
    uint8_t new_bytes = 8 - (addr & 0x7);
    for (int i = 0; i < new_bytes; i++)
    {
        int offset = 56 - (i << 3);
        int reg_offset = ((new_bytes - i - 1) << 3);
        uint64_t byte = (reg >> reg_offset) & 0xFF;
        new_dword &= ~(0xFFUL << offset);
        new_dword |= byte << offset;
    }

    cpu.write64(addr & ~0x7, new_dword);
}

void EmotionInterpreter::swr(EmotionEngine &cpu, uint32_t instruction)
{
    int16_t imm = (int16_t)(instruction & 0xFFFF);
    uint32_t source = (instruction >> 16) & 0x1F;
    uint32_t base = (instruction >> 21) & 0x1F;

    uint32_t addr = cpu.get_gpr<uint32_t>(base) + imm;
    uint32_t reg = cpu.get_gpr<uint32_t>(source);
    uint32_t new_word = cpu.read32(addr & ~0x3);
    uint8_t new_bytes = 4 - (addr & 0x3);
    for (int i = 0; i < new_bytes; i++)
    {
        int offset = 24 - (i << 3);
        int reg_offset = ((new_bytes - i - 1) << 3);
        uint32_t byte = (reg >> reg_offset) & 0xFF;
        new_word &= ~(0xFF << offset);
        new_word |= byte << offset;
    }

    cpu.write32(addr & ~0x3, new_word);
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
                    unknown_op("cop2", instruction, op2);
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

void EmotionInterpreter::cop2_qmfc2(EmotionEngine &cpu, uint32_t instruction)
{
    int dest = (instruction >> 16) & 0x1F;
    int cop_reg = (instruction >> 11) & 0x1F;
    cpu.qmfc2(dest, cop_reg);
}

void EmotionInterpreter::unknown_op(const char *type, uint32_t instruction, uint16_t op)
{
    printf("[EE Interpreter] Unrecognized %s op $%04X\n", type, op);
    printf("[EE Interpreter] Instr: $%08X\n", instruction);
    exit(1);
}
