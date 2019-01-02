#include "vu_jittrans.hpp"

namespace VU_JitTranslator
{

IR_Block translate(uint32_t PC, uint8_t* instr_mem)
{
    IR_Block block;

    bool block_end = false;
    bool delay_slot = false;
    while (!block_end)
    {
        std::vector<IR_Instruction> upper_instrs;
        std::vector<IR_Instruction> lower_instrs;
        if (delay_slot)
            block_end = true;

        uint32_t upper = *(uint32_t*)&instr_mem[PC + 4];
        uint32_t lower = *(uint32_t*)&instr_mem[PC];

        PC += 8;

        translate_upper(upper_instrs, upper);

        //LOI - upper goes first, lower gets loaded into I register
        if (upper & (1 << 31))
        {

        }
        else
        {
            translate_lower(lower_instrs, lower);
        }

        block.add_cycle();
    }

    return block;
}

void translate_upper(std::vector<IR_Instruction>& instrs, uint32_t upper)
{

}

void translate_lower(std::vector<IR_Instruction>& instrs, uint32_t lower)
{

}

};
