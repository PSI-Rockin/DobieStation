#ifndef VU_JITTRANS_HPP
#define VU_JITTRANS_HPP
#include <cstdint>
//#include <vector>
#include "../jitcommon/ir_block.hpp"

class EmotionEngine;

struct EE_InstrInfo
{
    //TODO
};

class EE_JitTranslator
{
private:
    //TODO
    int cycles_this_block;

    EE_InstrInfo instr_info[1024 * 16];
    uint16_t end_PC;
    uint16_t cur_PC;

    void fallback_interpreter(IR::Instruction& instr, uint32_t instr_word, bool is_upper);
public:
    //IR::Block translate(VectorUnit& vu, uint8_t *instr_mem, uint32_t prev_pc);
    void reset_instr_info();
};

#endif // EE_JITTRANS_HPP
