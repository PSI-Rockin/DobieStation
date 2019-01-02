#ifndef IR_INSTR_HPP
#define IR_INSTR_HPP
#include <cstdint>

namespace IR
{

enum Opcode
{
    Null,
    LoadConst,
    Jump,
    JumpAndLink,
    BranchEqual,
    BranchNotEqual,
    BranchGreater,
    BranchGreaterOrEqual
};

struct Instruction
{
    Opcode op;
    uint64_t args[5];

    Instruction(Opcode op = Null, uint64_t a0 = 0, uint64_t a1 = 0,
                uint64_t a2 = 0, uint64_t a3 = 0, uint64_t a4 = 0);

    bool is_jump();
};

};

#endif // IR_INSTR_HPP
