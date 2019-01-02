#ifndef VU_JITTRANS_HPP
#define VU_JITTRANS_HPP
#include <cstdint>
#include <vector>
#include "../jitcommon/ir_block.hpp"

namespace VU_JitTranslator
{
    IR_Block translate(uint32_t PC, uint8_t* instr_mem);

    void translate_upper(std::vector<IR_Instruction>& instrs, uint32_t upper);
    void translate_lower(std::vector<IR_Instruction>& instrs, uint32_t lower);
};

#endif // VU_JITTRANS_HPP
