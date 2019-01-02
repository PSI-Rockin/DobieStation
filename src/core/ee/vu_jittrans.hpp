#ifndef VU_JITTRANS_HPP
#define VU_JITTRANS_HPP
#include <cstdint>
#include <vector>
#include "../jitcommon/ir_block.hpp"

enum VU_SpecialReg
{
    I = 32,
    Q
};

namespace VU_JitTranslator
{
    IR::Block translate(uint32_t PC, uint8_t* instr_mem);

    void translate_upper(std::vector<IR::Instruction>& instrs, uint32_t upper);
    void upper_special(std::vector<IR::Instruction>& instrs, uint32_t upper);

    void translate_lower(std::vector<IR::Instruction>& instrs, uint32_t lower, uint32_t PC);
    void lower1(std::vector<IR::Instruction>& instrs, uint32_t lower);
    void lower2(std::vector<IR::Instruction>& instrs, uint32_t lower, uint32_t PC);
};

#endif // VU_JITTRANS_HPP
