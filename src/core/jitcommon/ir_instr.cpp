#include "ir_instr.hpp"

namespace IR
{

Instruction::Instruction(Opcode op) : op(op)
{

}

uint32_t Instruction::get_jump_dest()
{
    return jump_dest;
}

uint32_t Instruction::get_jump_fail_dest()
{
    return jump_fail_dest;
}

uint32_t Instruction::get_return_addr()
{
    return return_addr;
}

int Instruction::get_dest()
{
    return dest;
}

int Instruction::get_base()
{
    return base;
}

uint64_t Instruction::get_source()
{
    return source;
}

uint64_t Instruction::get_source2()
{
    return source2;
}

uint8_t Instruction::get_bc()
{
    return bc;
}

uint8_t Instruction::get_field()
{
    return field;
}

uint8_t Instruction::get_field2()
{
    return field2;
}

void Instruction::set_jump_dest(uint32_t addr)
{
    jump_dest = addr;
}

void Instruction::set_jump_fail_dest(uint32_t addr)
{
    jump_fail_dest = addr;
}

void Instruction::set_return_addr(uint32_t addr)
{
    return_addr = addr;
}

void Instruction::set_dest(int index)
{
    dest = index;
}

void Instruction::set_base(int index)
{
    base = index;
}

void Instruction::set_source(uint64_t value)
{
    source = value;
}

void Instruction::set_source2(uint64_t value)
{
    source2 = value;
}

void Instruction::set_bc(uint8_t value)
{
    bc = value;
}

void Instruction::set_field(uint8_t value)
{
    field = value;
}

void Instruction::set_field2(uint8_t value)
{
    field2 = value;
}

bool Instruction::is_jump()
{
    return op == Opcode::Jump ||
            op == Opcode::JumpIndirect ||
            op == Opcode::JumpAndLink ||
            op == Opcode::JumpAndLinkIndirect ||
            op == Opcode::BranchEqual ||
            op == Opcode::BranchNotEqual ||
            op == Opcode::BranchLessThanZero ||
            op == Opcode::BranchGreaterThanZero ||
            op == Opcode::BranchLessOrEqualThanZero ||
            op == Opcode::BranchGreaterOrEqualThanZero;
}

};
