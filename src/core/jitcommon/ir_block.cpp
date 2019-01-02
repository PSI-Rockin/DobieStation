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

};
