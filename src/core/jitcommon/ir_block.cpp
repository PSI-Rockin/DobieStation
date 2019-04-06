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

unsigned int Block::get_instruction_count() const
{
    return instructions.size();
}

int Block::get_cycle_count() const
{
    return cycle_count;
}

Instruction Block::get_next_instr()
{
    Instruction instr = instructions.front();
    instructions.pop_front();
    return instr;
}

void Block::set_cycle_count(int cycles)
{
    cycle_count = cycles;
}

};
