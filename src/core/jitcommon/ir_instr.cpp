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

int Instruction::get_link_register()
{
    return link_register;
}

uint32_t Instruction::get_return_addr()
{
    return return_addr;
}

int Instruction::get_dest()
{
    return dest;
}

uint32_t Instruction::get_source_u32()
{
    return source;
}

void Instruction::set_jump_dest(uint32_t addr)
{
    jump_dest = addr;
}

void Instruction::set_link_register(int index)
{
    link_register = index;
}

void Instruction::set_return_addr(uint32_t addr)
{
    return_addr = addr;
}

void Instruction::set_dest(int index)
{
    dest = index;
}

void Instruction::set_source_u32(uint32_t value)
{
    source = value;
}

bool Instruction::is_jump()
{
    return op == Opcode::Jump ||
            op == Opcode::JumpAndLink ||
            op == Opcode::BranchEqual ||
            op == Opcode::BranchNotEqual ||
            op == Opcode::BranchGreater ||
            op == Opcode::BranchGreaterOrEqual;
}

};
