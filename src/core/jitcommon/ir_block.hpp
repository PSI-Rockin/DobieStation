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

        unsigned int get_instruction_count();
        int get_cycle_count();
        Instruction get_next_instr();

        void set_cycle_count(int cycles);
};

};

#endif // IR_BLOCK_HPP
