#include "ir_block.hpp"

namespace IR
{

Block::Block()
{
    cycle_count = 0;
}

void Block::add_instr(Instruction &instr)
{
    instructions.push_back(instr);
}

void Block::add_cycle()
{
    cycle_count++;
}

unsigned int Block::get_instruction_count()
{
    return instructions.size();
}

int Block::get_cycle_count()
{
    return cycle_count;
}

Instruction Block::get_next_instr()
{
    Instruction instr = instructions.front();
    instructions.pop_front();
    return instr;
}

};
