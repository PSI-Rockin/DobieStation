#ifndef VU_JITTRANS_HPP
#define VU_JITTRANS_HPP
#include <cstdint>
//#include <vector>
#include "../jitcommon/ir_block.hpp"

class EmotionEngine;

enum FPU_SpecialReg
{
    VU_Regular = 0,
    ACC = 32
};

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

    bool is_branch(uint32_t instr_word);
    void interpreter_pass(EmotionEngine &ee, uint32_t pc);
    void fallback_interpreter(IR::Instruction& instr, uint32_t instr_word);
public:
    IR::Block translate(EmotionEngine& ee, uint32_t pc);
    void reset_instr_info();
};

#endif // EE_JITTRANS_HPP
