#include "ir_instr.hpp"
#include "../errors.hpp"

namespace IR
{

Instruction::Instruction(Opcode op) : op(op)
{
    has_dest = false;
    has_source = false;
    has_source2 = false;
    ignore_likely = false;
    jump_dest = 0;
    jump_fail_dest = 0;
    return_addr = 0;
    base = 0;
    cycle_count = 0;
    bc = 0;
    field = 0;
    field2 = 0;
    is_likely = 0;
    is_link = 0;
}

InstructionInfo *Instruction::get_dest_info()
{
    if (!has_dest)
        return nullptr;
    return &info[(int)RegisterType::Dest];
}

InstructionInfo *Instruction::get_source_info()
{
    if (!has_source)
        return nullptr;
    return &info[(int)RegisterType::Source];
}

InstructionInfo *Instruction::get_source2_info()
{
    if (!has_source2)
        return nullptr;
    return &info[(int)RegisterType::Source2];
}

uint32_t Instruction::get_jump_dest() const
{
    return jump_dest;
}

uint32_t Instruction::get_jump_fail_dest() const
{
    return jump_fail_dest;
}

uint32_t Instruction::get_return_addr() const
{
    return return_addr;
}

/*
    Note that destination can be represented as an immediate value in some rare circumstances.
    An example of this would be for store operations, where the actual address could be determined absolutely at compile time
    Particularly for operation pairs such as

    lui $v1, 0x36
    sw $v0, 0x7470($v1)

    Which ideally gets converted into an instruction which stores at the absolute address 0x367470
*/
uint64_t Instruction::get_dest() const
{
    if (!has_dest)
        Errors::die("Value does not exist: %s, %s:%d", __FUNCTION__, __FILE__, __LINE__);

    return info[(int)RegisterType::Dest].value;
}

int Instruction::get_base() const
{
    return base;
}

uint64_t Instruction::get_source() const
{
    if (!has_source)
        Errors::die("Value does not exist: %s, %s:%d", __FUNCTION__, __FILE__, __LINE__);

    return info[(int)RegisterType::Source].value;
}

uint64_t Instruction::get_source2() const
{
    if (!has_source2)
        Errors::die("Value does not exist: %s, %s:%d", __FUNCTION__, __FILE__, __LINE__);

    return info[(int)RegisterType::Source2].value;
}

uint16_t Instruction::get_cycle_count() const
{
    return cycle_count;
}

uint8_t Instruction::get_bc() const
{
    return bc;
}

uint8_t Instruction::get_field() const
{
    return field;
}

uint8_t Instruction::get_field2() const
{
    return field2;
}

bool Instruction::get_is_likely() const
{
    return is_likely;
}

bool Instruction::get_is_link() const
{
    return is_link;
}

uint32_t Instruction::get_opcode() const
{
    return opcode;
}

void (*Instruction::get_interpreter_fallback(void) const)(EmotionEngine&, uint32_t)
{
    return interpreter_fallback;
};

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

void Instruction::set_dest(uint64_t value, OperandType operandtype)
{
    info[(int)RegisterType::Dest].value = value;
    info[(int)RegisterType::Dest].operand_type = operandtype;
    has_dest = true;
}

void Instruction::set_base(int index)
{
    base = index;
}

void Instruction::set_source(uint64_t value, OperandType operandtype)
{
    info[(int)RegisterType::Source].value = value;
    info[(int)RegisterType::Source].operand_type = operandtype;
    has_source = true;
}

void Instruction::set_source2(uint64_t value, OperandType operandtype)
{
    info[(int)RegisterType::Source2].value = value;
    info[(int)RegisterType::Source2].operand_type = operandtype;
    has_source2 = true;
}

void Instruction::set_cycle_count(uint16_t value)
{
    cycle_count = value;
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

void Instruction::set_is_likely(bool value)
{
    is_likely = value;
}

void Instruction::set_is_link(bool value)
{
    is_link = value;
}

void Instruction::set_opcode(uint32_t value)
{
    opcode = value;
}

void Instruction::set_interpreter_fallback(void(*value)(EmotionEngine&, uint32_t))
{
    interpreter_fallback = value;
}

bool Instruction::is_jump()
{
    return op == Opcode::Jump ||
        op == Opcode::JumpIndirect ||
        op == Opcode::BranchCop0 ||
        op == Opcode::BranchCop1 ||
        op == Opcode::BranchCop2 ||
        op == Opcode::BranchEqual ||
        op == Opcode::BranchEqualZero ||
        op == Opcode::BranchGreaterThanOrEqualZero ||
        op == Opcode::BranchGreaterThanZero ||
        op == Opcode::BranchLessThanOrEqualZero ||
        op == Opcode::BranchLessThanZero ||
        op == Opcode::BranchNotEqual ||
        op == Opcode::BranchNotEqualZero;
}

};
