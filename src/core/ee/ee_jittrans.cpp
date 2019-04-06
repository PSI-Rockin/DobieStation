#include <cstring>
#include "ee_jittrans.hpp"
#include "emotioninterpreter.hpp"
#include "../errors.hpp"

void EE_JitTranslator::reset_instr_info()
{
    //TODO
}

IR::Block EE_JitTranslator::translate(EmotionEngine &ee, uint32_t pc)
{
    //TODO
    IR::Block block;
    IR::Instruction fallback(IR::Opcode::FallbackInterpreter);
    block.add_instr(fallback);
    block.set_cycle_count(/*cycles_this_block*/1);
    
    return block;
}

void EE_JitTranslator::fallback_interpreter(IR::Instruction& instr, uint32_t instr_word)
{
    instr.op = IR::Opcode::FallbackInterpreter;
    instr.set_source(instr_word);
}

/**
 * Determine base operation information and which instructions need to be swapped
 */
void EE_JitTranslator::interpreter_pass(EmotionEngine &ee, uint32_t pc)
{
    //TODO
}