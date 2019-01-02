#ifndef IR_BLOCK_HPP
#define IR_BLOCK_HPP
#include <list>
#include "ir_instr.hpp"

namespace IR
{

class Block
{
    private:
        std::list<Instruction> instructions;
        int cycle_count;
    public:
        Block();

        void add_instr(Instruction& instr);
        void add_cycle();
};

};

#endif // IR_BLOCK_HPP
