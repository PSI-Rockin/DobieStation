#ifndef IR_BLOCK_HPP
#define IR_BLOCK_HPP
#include <list>

struct IR_Instruction
{

};

class IR_Block
{
    private:
        std::list<IR_Instruction> instructions;
        int cycle_count;
    public:
        IR_Block();

        void add_instr(IR_Instruction& instr);
        void add_cycle();
};

#endif // IR_BLOCK_HPP
