#include <cstring>
#include "ee_jittrans.hpp"
#include "emotioninterpreter.hpp"
#include "../errors.hpp"

void EE_JitTranslator::reset_instr_info()
{
    //TODO
}

void EE_JitTranslator::fallback_interpreter(IR::Instruction& instr, uint32_t instr_word, bool is_upper)
{
    instr.op = IR::Opcode::FallbackInterpreter;
    instr.set_source(instr_word);
    instr.set_field(is_upper);
}