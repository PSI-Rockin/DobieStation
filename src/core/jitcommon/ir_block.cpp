#include "ir_block.hpp"

IR_Block::IR_Block()
{
    cycle_count = 0;
}

void IR_Block::add_instr(IR_Instruction &instr)
{
    instructions.push_back(instr);
}

void IR_Block::add_cycle()
{
    cycle_count++;
}
