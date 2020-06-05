#include "ir_instr.hpp"
#include "../errors.hpp"

namespace IR
{

Instruction::Instruction(Opcode op) : op(op)
{
    jump_dest = 0;
    jump_fail_dest = 0;
    return_addr = 0;
    base = 0;
    ninfo = 0;
    cycle_count = 0;
    bc = 0;
    field = 0;
    field2 = 0;
    is_likely = 0;
    is_link = 0;
}

InstructionInfo *Instruction::get_dest_info()
{
    for (int i = 0; i < ninfo; ++i)
    {
        if (info[i].reg_type == RegisterType::Dest)
            return &info[i];
    }

    return nullptr;
}

InstructionInfo *Instruction::get_source_info()
{
    for (int i = 0; i < ninfo; ++i)
    {
        if (info[i].reg_type == RegisterType::Source)
            return &info[i];
    }

    return nullptr;
}

InstructionInfo *Instruction::get_source2_info()
{
    for (int i = 0; i < ninfo; ++i)
    {
        if (info[i].reg_type == RegisterType::Source2)
            return &info[i];
    }

    return nullptr;
}

void Instruction::append_info(uint64_t value, OperandType operandtype, RegisterType regtype)
{
    if ((int)&info[ninfo] - (int)&info[0] >= sizeof(info))
        Errors::die("Buffer overflow on info: %s, %s:%d", __FUNCTION__, __FILE__, __LINE__);

    if (operandtype == OperandType::Immediate)
        info[ninfo].value.immediate = value;
    else if (operandtype == OperandType::Register)
        info[ninfo].value.reg = value;
    info[ninfo].operand_type = operandtype;
    info[ninfo].reg_type = regtype;

    ++ninfo;
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
uint64_t Instruction::get_dest()
{
    InstructionInfo *info = get_dest_info();
    if (info != nullptr)
    {
        if (info->operand_type == OperandType::Register)
            return info->value.reg;
        else if (info->operand_type == OperandType::Immediate)
            return info->value.immediate;
    }

    Errors::die("Value does not exist: %s, %s:%d", __FUNCTION__, __FILE__, __LINE__);
}

int Instruction::get_base() const
{
    return base;
}

uint64_t Instruction::get_source()
{
    InstructionInfo *info = get_source_info();
    if (info != nullptr)
    {
        if (info->operand_type == OperandType::Register)
            return info->value.reg;
        else if (info->operand_type == OperandType::Immediate)
            return info->value.immediate;
    }

    Errors::die("Value does not exist: %s, %s:%d", __FUNCTION__, __FILE__, __LINE__);
}

uint64_t Instruction::get_source2()
{
    InstructionInfo *info = get_source2_info();
    if (info != nullptr)
    {
        if (info->operand_type == OperandType::Register)
            return info->value.reg;
        else if (info->operand_type == OperandType::Immediate)
            return info->value.immediate;
    }

    Errors::die("Value does not exist: %s, %s:%d", __FUNCTION__, __FILE__, __LINE__);
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
    InstructionInfo *info = get_dest_info();
    if (info == nullptr)
        append_info(value, operandtype, RegisterType::Dest);
    else
    {
        if (operandtype == OperandType::Register)
            info->value.reg = value;
        else if (operandtype == OperandType::Immediate)
            info->value.immediate = value;
        info->operand_type = operandtype;
    }
}

void Instruction::set_base(int index)
{
    base = index;
}

void Instruction::set_source(uint64_t value, OperandType operandtype)
{
    InstructionInfo *info = get_source_info();
    if (info == nullptr)
        append_info(value, operandtype, RegisterType::Source);
    else
    {
        if (operandtype == OperandType::Register)
            info->value.reg = value;
        else if (operandtype == OperandType::Immediate)
            info->value.immediate = value;
        info->operand_type = operandtype;
    }
}

void Instruction::set_source2(uint64_t value, OperandType operandtype)
{
    InstructionInfo *info = get_source2_info();
    if (info == nullptr)
        append_info(value, operandtype, RegisterType::Source2);
    else
    {
        if (operandtype == OperandType::Register)
            info->value.reg = value;
        else if (operandtype == OperandType::Immediate)
            info->value.immediate = value;
        info->operand_type = operandtype;
    }
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
