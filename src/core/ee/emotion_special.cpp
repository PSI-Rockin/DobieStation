#include <cstdio>
#include <cstdlib>
#include "emotioninterpreter.hpp"

void EmotionInterpreter::special(EmotionEngine &cpu, uint32_t instruction)
{
    int op = instruction & 0x3F;
    switch (op)
    {
        case 0x00:
            sll(cpu, instruction);
            break;
        case 0x02:
            srl(cpu, instruction);
            break;
        case 0x03:
            sra(cpu, instruction);
            break;
        case 0x04:
            sllv(cpu, instruction);
            break;
        case 0x06:
            srlv(cpu, instruction);
            break;
        case 0x07:
            srav(cpu, instruction);
            break;
        case 0x08:
            jr(cpu, instruction);
            break;
        case 0x09:
            jalr(cpu, instruction);
            break;
        case 0x0A:
            movz(cpu, instruction);
            break;
        case 0x0B:
            movn(cpu, instruction);
            break;
        case 0x0C:
            syscall_ee(cpu, instruction);
            break;
        case 0x0F:
            break;
        case 0x10:
            mfhi(cpu, instruction);
            break;
        case 0x11:
            mthi(cpu, instruction);
            break;
        case 0x12:
            mflo(cpu, instruction);
            break;
        case 0x13:
            mtlo(cpu, instruction);
            break;
        case 0x14:
            dsllv(cpu, instruction);
            break;
        case 0x16:
            dsrlv(cpu, instruction);
            break;
        case 0x17:
            dsrav(cpu, instruction);
            break;
        case 0x18:
            mult(cpu, instruction);
            break;
        case 0x19:
            multu(cpu, instruction);
            break;
        case 0x1A:
            div(cpu, instruction);
            break;
        case 0x1B:
            divu(cpu, instruction);
            break;
        case 0x20:
            add(cpu, instruction);
            break;
        case 0x21:
            addu(cpu, instruction);
            break;
        case 0x22:
            sub(cpu, instruction);
            break;
        case 0x23:
            subu(cpu, instruction);
            break;
        case 0x24:
            and_ee(cpu, instruction);
            break;
        case 0x25:
            or_ee(cpu, instruction);
            break;
        case 0x26:
            xor_ee(cpu, instruction);
            break;
        case 0x27:
            nor(cpu, instruction);
            break;
        case 0x28:
            mfsa(cpu, instruction);
            break;
        case 0x29:
            mtsa(cpu, instruction);
            break;
        case 0x2A:
            slt(cpu, instruction);
            break;
        case 0x2B:
            sltu(cpu, instruction);
            break;
        case 0x2C:
            dadd(cpu, instruction);
            break;
        case 0x2D:
            daddu(cpu, instruction);
            break;
        case 0x2F:
            dsubu(cpu, instruction);
            break;
        case 0x38:
            dsll(cpu, instruction);
            break;
        case 0x3A:
            dsrl(cpu, instruction);
            break;
        case 0x3C:
            dsll32(cpu, instruction);
            break;
        case 0x3E:
            dsrl32(cpu, instruction);
            break;
        case 0x3F:
            dsra32(cpu, instruction);
            break;
        default:
            unknown_op("special", instruction, op);
    }
}

void EmotionInterpreter::sll(EmotionEngine &cpu, uint32_t instruction)
{
    uint64_t source = (instruction >> 16) & 0x1F;
    uint64_t dest = (instruction >> 11) & 0x1F;
    uint32_t shift = (instruction >> 6) & 0x1F;
    source = cpu.get_gpr<uint32_t>(source);
    source <<= shift;
    cpu.set_gpr<int64_t>(dest, (int32_t)source);
}

void EmotionInterpreter::srl(EmotionEngine &cpu, uint32_t instruction)
{
    uint64_t source = (instruction >> 16) & 0x1F;
    uint64_t dest = (instruction >> 11) & 0x1F;
    uint32_t shift = (instruction >> 6) & 0x1F;
    source = cpu.get_gpr<uint32_t>(source);
    source >>= shift;
    cpu.set_gpr<int64_t>(dest, (int32_t)source);
}

void EmotionInterpreter::sra(EmotionEngine &cpu, uint32_t instruction)
{
    uint64_t source = (instruction >> 16) & 0x1F;
    uint64_t dest = (instruction >> 11) & 0x1F;
    int32_t shift = (instruction >> 6) & 0x1F;
    source = cpu.get_gpr<int32_t>(source);
    source >>= shift;
    cpu.set_gpr<int64_t>(dest, source);
}

void EmotionInterpreter::sllv(EmotionEngine &cpu, uint32_t instruction)
{
    uint32_t source = (instruction >> 16) & 0x1F;
    uint64_t dest = (instruction >> 11) & 0x1F;
    uint32_t shift = (instruction >> 21) & 0x1F;
    source = cpu.get_gpr<uint32_t>(source);
    source <<= cpu.get_gpr<uint8_t>(shift) & 0x1F;
    cpu.set_gpr<int64_t>(dest, (int32_t)source);
}

void EmotionInterpreter::srlv(EmotionEngine &cpu, uint32_t instruction)
{
    uint32_t source = (instruction >> 16) & 0x1F;
    uint64_t dest = (instruction >> 11) & 0x1F;
    uint32_t shift = (instruction >> 21) & 0x1F;
    source = cpu.get_gpr<uint32_t>(source);
    source >>= cpu.get_gpr<uint8_t>(shift) & 0x1F;
    cpu.set_gpr<int64_t>(dest, (int32_t)source);
}

void EmotionInterpreter::srav(EmotionEngine &cpu, uint32_t instruction)
{
    int32_t source = (instruction >> 16) & 0x1F;
    uint64_t dest = (instruction >> 11) & 0x1F;
    uint32_t shift = (instruction >> 21) & 0x1F;
    source = cpu.get_gpr<int32_t>(source);
    source <<= cpu.get_gpr<uint8_t>(shift) & 0x1F;
    cpu.set_gpr<int64_t>(dest, source);
}

void EmotionInterpreter::jr(EmotionEngine &cpu, uint32_t instruction)
{
    uint32_t address = (instruction >> 21) & 0x1F;
    cpu.jp(cpu.get_gpr<uint32_t>(address));
}

void EmotionInterpreter::jalr(EmotionEngine &cpu, uint32_t instruction)
{
    uint32_t new_addr = (instruction >> 21) & 0x1F;
    uint32_t return_reg = (instruction >> 11) & 0x1F;
    uint32_t return_addr = cpu.get_PC() + 8;
    cpu.jp(cpu.get_gpr<uint32_t>(new_addr));
    cpu.set_gpr<uint32_t>(return_reg, return_addr);
}

void EmotionInterpreter::movz(EmotionEngine &cpu, uint32_t instruction)
{
    uint64_t source = (instruction >> 21) & 0x1F;
    uint64_t test = (instruction >> 16) & 0x1F;
    uint64_t dest = (instruction >> 11) & 0x1F;
    source = cpu.get_gpr<uint64_t>(source);
    if (!cpu.get_gpr<uint64_t>(test))
        cpu.set_gpr<uint64_t>(dest, source);
}

void EmotionInterpreter::movn(EmotionEngine &cpu, uint32_t instruction)
{
    uint64_t source = (instruction >> 21) & 0x1F;
    uint64_t test = (instruction >> 16) & 0x1F;
    uint64_t dest = (instruction >> 11) & 0x1F;
    source = cpu.get_gpr<uint64_t>(source);
    if (cpu.get_gpr<uint64_t>(test))
        cpu.set_gpr<uint64_t>(dest, source);
}

void EmotionInterpreter::syscall_ee(EmotionEngine &cpu, uint32_t instruction)
{
    uint32_t code = (instruction >> 6) & 0xFFFFF;
    //cpu.hle_syscall();
    cpu.syscall_exception();
}

void EmotionInterpreter::mfhi(EmotionEngine &cpu, uint32_t instruction)
{
    uint32_t dest = (instruction >> 11) & 0x1F;
    cpu.mfhi(dest);
}

void EmotionInterpreter::mthi(EmotionEngine &cpu, uint32_t instruction)
{
    uint32_t source = (instruction >> 21) & 0x1F;
    cpu.mthi(source);
}

void EmotionInterpreter::mflo(EmotionEngine &cpu, uint32_t instruction)
{
    uint32_t dest = (instruction >> 11) & 0x1F;
    cpu.mflo(dest);
}

void EmotionInterpreter::mtlo(EmotionEngine &cpu, uint32_t instruction)
{
    uint32_t source = (instruction >> 21) & 0x1F;
    cpu.mtlo(source);
}

void EmotionInterpreter::dsllv(EmotionEngine &cpu, uint32_t instruction)
{
    uint64_t source = (instruction >> 16) & 0x1F;
    uint64_t dest = (instruction >> 11) & 0x1F;
    uint32_t shift = (instruction >> 21) & 0x1F;
    source = cpu.get_gpr<uint64_t>(source);
    source <<= cpu.get_gpr<uint8_t>(shift) & 0x3F;
    cpu.set_gpr<uint64_t>(dest, source);
}

void EmotionInterpreter::dsrlv(EmotionEngine &cpu, uint32_t instruction)
{
    uint64_t source = (instruction >> 16) & 0x1F;
    uint64_t dest = (instruction >> 11) & 0x1F;
    uint32_t shift = (instruction >> 21) & 0x1F;
    source = cpu.get_gpr<uint64_t>(source);
    source >>= cpu.get_gpr<uint8_t>(shift) & 0x3F;
    cpu.set_gpr<int64_t>(dest, source);
}

void EmotionInterpreter::dsrav(EmotionEngine &cpu, uint32_t instruction)
{
    int64_t source = (instruction >> 16) & 0x1F;
    uint64_t dest = (instruction >> 11) & 0x1F;
    uint32_t shift = (instruction >> 21) & 0x1F;
    source = cpu.get_gpr<int64_t>(source);
    source >>= cpu.get_gpr<uint8_t>(shift) & 0x3F;
    cpu.set_gpr<int64_t>(dest, source);
}

void EmotionInterpreter::mult(EmotionEngine &cpu, uint32_t instruction)
{
    int32_t op1 = (instruction >> 21) & 0x1F;
    int32_t op2 = (instruction >> 16) & 0x1F;
    uint64_t dest = (instruction >> 11) & 0x1F;
    op1 = cpu.get_gpr<int32_t>(op1);
    op2 = cpu.get_gpr<int32_t>(op2);
    int64_t temp = (int64_t)op1 * op2;
    cpu.set_LO_HI((int32_t)(temp & 0xFFFFFFFF), (int32_t)(temp >> 32));
    cpu.mflo(dest);
}

void EmotionInterpreter::multu(EmotionEngine& cpu, uint32_t instruction)
{
    uint32_t op1 = (instruction >> 21) & 0x1F;
    uint32_t op2 = (instruction >> 16) & 0x1F;
    uint64_t dest = (instruction >> 11) & 0x1F;
    op1 = cpu.get_gpr<uint32_t>(op1);
    op2 = cpu.get_gpr<uint32_t>(op2);
    uint64_t temp = (uint64_t)op1 * op2;
    cpu.set_LO_HI(temp & 0xFFFFFFFF, temp >> 32);
    cpu.mflo(dest);
}

void EmotionInterpreter::div(EmotionEngine &cpu, uint32_t instruction)
{
    int32_t op1 = (instruction >> 21) & 0x1F;
    int32_t op2 = (instruction >> 16) & 0x1F;
    op1 = cpu.get_gpr<int32_t>(op1);
    op2 = cpu.get_gpr<int32_t>(op2);
    if (!op2)
    {
        exit(1);
    }
    cpu.set_LO_HI(op1 / op2, op1 % op2);
}

void EmotionInterpreter::divu(EmotionEngine &cpu, uint32_t instruction)
{
    uint32_t op1 = (instruction >> 21) & 0x1F;
    uint32_t op2 = (instruction >> 16) & 0x1F;
    op1 = cpu.get_gpr<uint32_t>(op1);
    op2 = cpu.get_gpr<uint32_t>(op2);
    if (!op2)
    {
        exit(1);
    }
    cpu.set_LO_HI(op1 / op2, op1 % op2);
}

void EmotionInterpreter::add(EmotionEngine &cpu, uint32_t instruction)
{
    int32_t op1 = (instruction >> 21) & 0x1F;
    int32_t op2 = (instruction >> 16) & 0x1F;
    uint64_t dest = (instruction >> 11) & 0x1F;
    op1 = cpu.get_gpr<int32_t>(op1);
    op2 = cpu.get_gpr<int32_t>(op2);
    int32_t result = op1 + op2;
    cpu.set_gpr<int64_t>(dest, result);
}

void EmotionInterpreter::addu(EmotionEngine &cpu, uint32_t instruction)
{
    int32_t op1 = (instruction >> 21) & 0x1F;
    int32_t op2 = (instruction >> 16) & 0x1F;
    uint64_t dest = (instruction >> 11) & 0x1F;
    op1 = cpu.get_gpr<int32_t>(op1);
    op2 = cpu.get_gpr<int32_t>(op2);
    int32_t result = op1 + op2;
    cpu.set_gpr<int64_t>(dest, result);
}

void EmotionInterpreter::sub(EmotionEngine &cpu, uint32_t instruction)
{
    //TODO: overflow
    int32_t op1 = (instruction >> 21) & 0x1F;
    int32_t op2 = (instruction >> 16) & 0x1F;
    int64_t dest = (instruction >> 11) & 0x1F;
    op1 = cpu.get_gpr<int32_t>(op1);
    op2 = cpu.get_gpr<int32_t>(op2);
    int32_t result = op1 - op2;
    cpu.set_gpr<int64_t>(dest, result);
}

void EmotionInterpreter::subu(EmotionEngine &cpu, uint32_t instruction)
{
    int32_t op1 = (instruction >> 21) & 0x1F;
    int32_t op2 = (instruction >> 16) & 0x1F;
    int64_t dest = (instruction >> 11) & 0x1F;
    op1 = cpu.get_gpr<int32_t>(op1);
    op2 = cpu.get_gpr<int32_t>(op2);
    int32_t result = op1 - op2;
    cpu.set_gpr<int64_t>(dest, result);
}

void EmotionInterpreter::and_ee(EmotionEngine &cpu, uint32_t instruction)
{
    uint64_t op1 = (instruction >> 21) & 0x1F;
    uint64_t op2 = (instruction >> 16) & 0x1F;
    uint64_t dest = (instruction >> 11) & 0x1F;
    op1 = cpu.get_gpr<uint64_t>(op1);
    op2 = cpu.get_gpr<uint64_t>(op2);
    cpu.set_gpr<uint64_t>(dest, op1 & op2);
}

void EmotionInterpreter::or_ee(EmotionEngine &cpu, uint32_t instruction)
{
    uint64_t op1 = (instruction >> 21) & 0x1F;
    uint64_t op2 = (instruction >> 16) & 0x1F;
    uint64_t dest = (instruction >> 11) & 0x1F;
    op1 = cpu.get_gpr<uint64_t>(op1);
    op2 = cpu.get_gpr<uint64_t>(op2);
    cpu.set_gpr<uint64_t>(dest, op1 | op2);
}

void EmotionInterpreter::xor_ee(EmotionEngine &cpu, uint32_t instruction)
{
    uint64_t op1 = (instruction >> 21) & 0x1F;
    uint64_t op2 = (instruction >> 16) & 0x1F;
    uint64_t dest = (instruction >> 11) & 0x1F;
    op1 = cpu.get_gpr<uint64_t>(op1);
    op2 = cpu.get_gpr<uint64_t>(op2);
    cpu.set_gpr<uint64_t>(dest, op1 ^ op2);
}

void EmotionInterpreter::nor(EmotionEngine &cpu, uint32_t instruction)
{
    uint64_t op1 = (instruction >> 21) & 0x1F;
    uint64_t op2 = (instruction >> 16) & 0x1F;
    uint64_t dest = (instruction >> 11) & 0x1F;
    op1 = cpu.get_gpr<uint64_t>(op1);
    op2 = cpu.get_gpr<uint64_t>(op2);
    cpu.set_gpr<uint64_t>(dest, ~(op1 | op2));
}

void EmotionInterpreter::mfsa(EmotionEngine &cpu, uint32_t instruction)
{
    int dest = (instruction >> 11) & 0x1F;
    cpu.mfsa(dest);
}

void EmotionInterpreter::mtsa(EmotionEngine &cpu, uint32_t instruction)
{
    int source = (instruction >> 21) & 0x1F;
    cpu.mtsa(source);
}

void EmotionInterpreter::slt(EmotionEngine &cpu, uint32_t instruction)
{
    int64_t op1 = (instruction >> 21) & 0x1F;
    int64_t op2 = (instruction >> 16) & 0x1F;
    int64_t dest = (instruction >> 11) & 0x1F;
    op1 = cpu.get_gpr<int64_t>(op1);
    op2 = cpu.get_gpr<int64_t>(op2);
    cpu.set_gpr<uint64_t>(dest, op1 < op2);
}

void EmotionInterpreter::sltu(EmotionEngine &cpu, uint32_t instruction)
{
    uint64_t op1 = (instruction >> 21) & 0x1F;
    uint64_t op2 = (instruction >> 16) & 0x1F;
    uint64_t dest = (instruction >> 11) & 0x1F;
    op1 = cpu.get_gpr<uint64_t>(op1);
    op2 = cpu.get_gpr<uint64_t>(op2);
    cpu.set_gpr<uint64_t>(dest, op1 < op2);
}

void EmotionInterpreter::dadd(EmotionEngine &cpu, uint32_t instruction)
{
    int64_t op1 = (instruction >> 21) & 0x1F;
    int64_t op2 = (instruction >> 16) & 0x1F;
    uint64_t dest = (instruction >> 11) & 0x1F;
    op1 = cpu.get_gpr<int64_t>(op1);
    op2 = cpu.get_gpr<int64_t>(op2);
    cpu.set_gpr<uint64_t>(dest, op1 + op2);
}

void EmotionInterpreter::daddu(EmotionEngine &cpu, uint32_t instruction)
{
    int64_t op1 = (instruction >> 21) & 0x1F;
    int64_t op2 = (instruction >> 16) & 0x1F;
    uint64_t dest = (instruction >> 11) & 0x1F;
    op1 = cpu.get_gpr<int64_t>(op1);
    op2 = cpu.get_gpr<int64_t>(op2);
    cpu.set_gpr<uint64_t>(dest, op1 + op2);
}

void EmotionInterpreter::dsubu(EmotionEngine &cpu, uint32_t instruction)
{
    int64_t op1 = (instruction >> 21) & 0x1F;
    int64_t op2 = (instruction >> 16) & 0x1F;
    uint64_t dest = (instruction >> 11) & 0x1F;
    op1 = cpu.get_gpr<int64_t>(op1);
    op2 = cpu.get_gpr<int64_t>(op2);
    cpu.set_gpr<uint64_t>(dest, op1 - op2);
}

void EmotionInterpreter::dsll(EmotionEngine &cpu, uint32_t instruction)
{
    uint64_t source = (instruction >> 16) & 0x1F;
    uint64_t dest = (instruction >> 11) & 0x1F;
    uint32_t shift = (instruction >> 6) & 0x1F;
    source = cpu.get_gpr<uint64_t>(source);
    source <<= shift;
    cpu.set_gpr<uint64_t>(dest, source);
}

void EmotionInterpreter::dsrl(EmotionEngine &cpu, uint32_t instruction)
{
    uint64_t source = (instruction >> 16) & 0x1F;
    uint64_t dest = (instruction >> 11) & 0x1F;
    uint32_t shift = (instruction >> 6) & 0x1F;
    source = cpu.get_gpr<uint64_t>(source);
    source >>= shift;
    cpu.set_gpr<uint64_t>(dest, source);
}

void EmotionInterpreter::dsll32(EmotionEngine &cpu, uint32_t instruction)
{
    uint64_t source = (instruction >> 16) & 0x1F;
    uint64_t dest = (instruction >> 11) & 0x1F;
    uint32_t shift = (instruction >> 6) & 0x1F;
    source = cpu.get_gpr<uint64_t>(source);
    source <<= (shift + 32);
    cpu.set_gpr<uint64_t>(dest, source);
}

void EmotionInterpreter::dsrl32(EmotionEngine &cpu, uint32_t instruction)
{
    uint64_t source = (instruction >> 16) & 0x1F;
    uint64_t dest = (instruction >> 11) & 0x1F;
    uint32_t shift = (instruction >> 6) & 0x1F;
    source = cpu.get_gpr<uint64_t>(source);
    source >>= (shift + 32);
    cpu.set_gpr<uint64_t>(dest, source);
}

void EmotionInterpreter::dsra32(EmotionEngine& cpu, uint32_t instruction)
{
    int64_t source = (instruction >> 16) & 0x1F;
    uint64_t dest = (instruction >> 11) & 0x1F;
    uint32_t shift = (instruction >> 6) & 0x1F;
    source = cpu.get_gpr<int64_t>(source);
    source >>= (shift + 32);
    cpu.set_gpr<int64_t>(dest, source);
}
