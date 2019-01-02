#include "ir_instr.hpp"

namespace IR
{

Instruction::Instruction(Opcode op, uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4) : op(op)
{
    args[0] = a0;
    args[1] = a1;
    args[2] = a2;
    args[3] = a3;
    args[4] = a4;
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
