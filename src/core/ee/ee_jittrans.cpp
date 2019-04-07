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

    bool branch_op = false;
    bool branch_delayslot = false;
    int cycle_count = 0;

    while (!branch_delayslot)
    {
        IR::Instruction instr;
        fallback_interpreter(instr, ee.read32(pc));
        block.add_instr(instr);

        if (branch_op)
            branch_delayslot = true;

        branch_op = is_branch(ee.read32(pc));

        pc += 4;
        ++cycle_count;
    }
    
    block.set_cycle_count(cycle_count);
    
    return block;
}

// TENTATIVE
bool EE_JitTranslator::is_branch(uint32_t instr_word) const
{
    uint8_t op = instr_word >> 26;

    switch (op)
    {
    case 0x02:
    case 0x03:
    case 0x04:
    case 0x05:
    case 0x06:
    case 0x07:
    case 0x14:
    case 0x15:
    case 0x16:
    case 0x17:
        return true;
    default:
        break;
    }

    if (op == 0x00)
    {
        op = instr_word & 0x3F;
        switch (op)
        {
        case 0x08:
        case 0x09:
            return true;
        default:
            return false;
        }
    }
    else if (op == 0x01)
    {
        op = (instr_word >> 16) & 0x1F;

        switch (op)
        {
        case 0x00:
        case 0x01:
        case 0x02:
        case 0x03:
        case 0x10:
        case 0x11:
        case 0x12:
        case 0x13:
            return true;
        default:
            return false;
        }
    }

    return false;
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