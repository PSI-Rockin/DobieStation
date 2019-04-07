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
    std::vector<IR::Instruction> instrs;

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

void EE_JitTranslator::translate_op(std::vector<IR::Instruction>& instrs, uint32_t opcode) const
{
    if (!opcode)
        return; // op is effective nop

    uint8_t op = opcode >> 26;
    IR::Instruction instr;

    switch (op)
    {
        case 0x00:
            // Special Operation
            translate_op_special(instrs, opcode);
            return;
        case 0x01:
            // Regimm Operation
            translate_op_regimm(instrs, opcode);
            return;
        case 0x02:
            // J
            Errors::print_warning("[EE_JIT] Unrecognized op J\n", op);
            fallback_interpreter(instr, opcode);
            break;
        case 0x03:
            // JAL
            Errors::print_warning("[EE_JIT] Unrecognized op JAL\n", op);
            fallback_interpreter(instr, opcode);
            break;
        case 0x04:
            // BEQ
            Errors::print_warning("[EE_JIT] Unrecognized op BEQ\n", op);
            fallback_interpreter(instr, opcode);
            break;
        case 0x05:
            // BNE
            Errors::print_warning("[EE_JIT] Unrecognized op BNE\n", op);
            fallback_interpreter(instr, opcode);
            break;
        case 0x06:
            // BLEZ
            Errors::print_warning("[EE_JIT] Unrecognized op BLEZ\n", op);
            fallback_interpreter(instr, opcode);
            break;
        case 0x07:
            // BGTZ
            Errors::print_warning("[EE_JIT] Unrecognized op BGTZ\n", op);
            fallback_interpreter(instr, opcode);
            break;
        case 0x08:
            // ADDI
            Errors::print_warning("[EE_JIT] Unrecognized op ADDI\n", op);
            fallback_interpreter(instr, opcode);
            break;
        case 0x09:
            // ADDIU
            Errors::print_warning("[EE_JIT] Unrecognized op ADDIU\n", op);
            fallback_interpreter(instr, opcode);
            break;
        case 0x0A:
            // SLTI
            Errors::print_warning("[EE_JIT] Unrecognized op SLTI\n", op);
            fallback_interpreter(instr, opcode);
            break;
        case 0x0B:
            // SLTIU
            Errors::print_warning("[EE_JIT] Unrecognized op SLTIU\n", op);
            fallback_interpreter(instr, opcode);
            break;
        case 0x0C:
            // ANDI
            Errors::print_warning("[EE_JIT] Unrecognized op ANDI\n", op);
            fallback_interpreter(instr, opcode);
            break;
        case 0x0D:
            // ORI
            Errors::print_warning("[EE_JIT] Unrecognized op ORI\n", op);
            fallback_interpreter(instr, opcode);
            break;
        case 0x0E:
            // XORI
            Errors::print_warning("[EE_JIT] Unrecognized op XORI\n", op);
            fallback_interpreter(instr, opcode);
            break;
        case 0x0F:
            // LUI
            Errors::print_warning("[EE_JIT] Unrecognized op LUI\n", op);
            fallback_interpreter(instr, opcode);
            break;
        case 0x10:
        case 0x11:
        case 0x12:
        case 0x13:
            // COP2
            Errors::print_warning("[EE_JIT] Unrecognized op COP2\n", op);
            fallback_interpreter(instr, opcode);
            break;
        case 0x14:
            // BEQL
            Errors::print_warning("[EE_JIT] Unrecognized op BEQL\n", op);
            fallback_interpreter(instr, opcode);
            break;
        case 0x15:
            // BNEL
            Errors::print_warning("[EE_JIT] Unrecognized op BNEL\n", op);
            fallback_interpreter(instr, opcode);
            break;
        case 0x16:
            // BLEZL
            Errors::print_warning("[EE_JIT] Unrecognized op BLEZL\n", op);
            fallback_interpreter(instr, opcode);
            break;
        case 0x17:
            // BGTZL
            Errors::print_warning("[EE_JIT] Unrecognized op BGTZL\n", op);
            fallback_interpreter(instr, opcode);
            break;
        case 0x18:
            // DADDI
            Errors::print_warning("[EE_JIT] Unrecognized op DADDI\n", op);
            fallback_interpreter(instr, opcode);
            break;
        case 0x19:
            // DADDIU
            Errors::print_warning("[EE_JIT] Unrecognized op DADDIU\n", op);
            fallback_interpreter(instr, opcode);
            break;
        case 0x1A:
            // LDL
            Errors::print_warning("[EE_JIT] Unrecognized op LDL\n", op);
            fallback_interpreter(instr, opcode);
            break;
        case 0x1B:
            // LDR
            Errors::print_warning("[EE_JIT] Unrecognized op LDR\n", op);
            fallback_interpreter(instr, opcode);
            break;
        case 0x1C:
            // SLTI
            Errors::print_warning("[EE_JIT] Unrecognized op SLTI\n", op);
            fallback_interpreter(instr, opcode);
            break;
        case 0x1E:
            // LQ
            Errors::print_warning("[EE_JIT] Unrecognized op LQ\n", op);
            fallback_interpreter(instr, opcode);
            break;
        case 0x1F:
            // SQ
            Errors::print_warning("[EE_JIT] Unrecognized op SQ\n", op);
            fallback_interpreter(instr, opcode);
            break;
        case 0x20:
            // LB
            Errors::print_warning("[EE_JIT] Unrecognized op LB\n", op);
            fallback_interpreter(instr, opcode);
            break;
        case 0x21:
            // LH
            Errors::print_warning("[EE_JIT] Unrecognized op LH\n", op);
            fallback_interpreter(instr, opcode);
            break;
        case 0x22:
            // LWL
            Errors::print_warning("[EE_JIT] Unrecognized op LWL\n", op);
            fallback_interpreter(instr, opcode);
            break;
        case 0x23:
            // LW
            Errors::print_warning("[EE_JIT] Unrecognized op LW\n", op);
            fallback_interpreter(instr, opcode);
            break;
        case 0x24:
            // LBU
            Errors::print_warning("[EE_JIT] Unrecognized op LBU\n", op);
            fallback_interpreter(instr, opcode);
            break;
        case 0x25:
            // LHU
            Errors::print_warning("[EE_JIT] Unrecognized op LHU\n", op);
            fallback_interpreter(instr, opcode);
            break;
        case 0x26:
            // LWR
            Errors::print_warning("[EE_JIT] Unrecognized op LWR\n", op);
            fallback_interpreter(instr, opcode);
            break;
        case 0x27:
            // LWU
            Errors::print_warning("[EE_JIT] Unrecognized op LWU\n", op);
            fallback_interpreter(instr, opcode);
            break;
        case 0x28:
            // SB
            Errors::print_warning("[EE_JIT] Unrecognized op SB\n", op);
            fallback_interpreter(instr, opcode);
            break;
        case 0x29:
            // SH
            Errors::print_warning("[EE_JIT] Unrecognized op SH\n", op);
            fallback_interpreter(instr, opcode);
            break;
        case 0x2A:
            // SWL
            Errors::print_warning("[EE_JIT] Unrecognized op SWL\n", op);
            fallback_interpreter(instr, opcode);
            break;
        case 0x2B:
            // SW
            Errors::print_warning("[EE_JIT] Unrecognized op SW\n", op);
            fallback_interpreter(instr, opcode);
            break;
        case 0x2C:
            // SDL
            Errors::print_warning("[EE_JIT] Unrecognized op SDL\n", op);
            fallback_interpreter(instr, opcode);
            break;
        case 0x2D:
            // SDR
            Errors::print_warning("[EE_JIT] Unrecognized op SDR\n", op);
            fallback_interpreter(instr, opcode);
            break;
        case 0x2E:
            // SWR
            Errors::print_warning("[EE_JIT] Unrecognized op SWR\n", op);
            fallback_interpreter(instr, opcode);
            break;
        case 0x2F:
            // CACHE
            Errors::print_warning("[EE_JIT] Unrecognized op CACHE\n", op);
            fallback_interpreter(instr, opcode);
            break;
        case 0x31:
            // LWC1
            Errors::print_warning("[EE_JIT] Unrecognized op LWC1\n", op);
            fallback_interpreter(instr, opcode);
            break;
        case 0x33:
            // PREFETCH
            return;
        case 0x36:
            // LQC2
            Errors::print_warning("[EE_JIT] Unrecognized op LQC2\n", op);
            fallback_interpreter(instr, opcode);
            break;
        case 0x37:
            // LD
            Errors::print_warning("[EE_JIT] Unrecognized op LD\n", op);
            fallback_interpreter(instr, opcode);
            break;
        case 0x39:
            // SWC1
            Errors::print_warning("[EE_JIT] Unrecognized op SWC1\n", op);
            fallback_interpreter(instr, opcode);
            break;
        case 0x3E:
            // SQC2
            Errors::print_warning("[EE_JIT] Unrecognized op SQC2\n", op);
            fallback_interpreter(instr, opcode);
            break;
        case 0x3F:
            // SD
            Errors::print_warning("[EE_JIT] Unrecognized op SD\n", op);
            fallback_interpreter(instr, opcode);
            break;
        default:
            Errors::die("[EE_JIT] Unrecognized op $%02X", op);
            return;
    }
    instrs.push_back(instr);
}

void EE_JitTranslator::translate_op_special(std::vector<IR::Instruction>& instrs, uint32_t opcode) const
{
    uint8_t op = opcode & 0x3F;
    IR::Instruction instr;

    switch (op)
    {
        case 0x00:
            // SLL
            Errors::print_warning("[EE_JIT] Unrecognized special op SLL\n", op);
            fallback_interpreter(instr, opcode);
            break;
        case 0x02:
            // SRL
            Errors::print_warning("[EE_JIT] Unrecognized special op SRL\n", op);
            fallback_interpreter(instr, opcode);
            break;
        case 0x03:
            // SRA
            Errors::print_warning("[EE_JIT] Unrecognized special op SRA\n", op);
            fallback_interpreter(instr, opcode);
            break;
        case 0x04:
            // SLLV
            Errors::print_warning("[EE_JIT] Unrecognized special op SLLV\n", op);
            fallback_interpreter(instr, opcode);
            break;
        case 0x06:
            // SRLV
            Errors::print_warning("[EE_JIT] Unrecognized special op SRLV\n", op);
            fallback_interpreter(instr, opcode);
            break;
        case 0x07:
            // SRAV
            Errors::print_warning("[EE_JIT] Unrecognized special op SRAV\n", op);
            fallback_interpreter(instr, opcode);
            break;
        case 0x08:
            // JR
            Errors::print_warning("[EE_JIT] Unrecognized special op JR\n", op);
            fallback_interpreter(instr, opcode);
            break;
        case 0x09:
            // JALR
            Errors::print_warning("[EE_JIT] Unrecognized special op JALR\n", op);
            fallback_interpreter(instr, opcode);
            break;
        case 0x0A:
            // MOVZ
            Errors::print_warning("[EE_JIT] Unrecognized special op MOVZ\n", op);
            fallback_interpreter(instr, opcode);
            break;
        case 0x0B:
            // MOVN
            Errors::print_warning("[EE_JIT] Unrecognized special op MOVN\n", op);
            fallback_interpreter(instr, opcode);
            break;
        case 0x0C:
            // SYSCALL
            Errors::print_warning("[EE_JIT] Unrecognized special op SYSCALL\n", op);
            fallback_interpreter(instr, opcode);
            break;
        case 0x0D:
            // BREAK
            Errors::print_warning("[EE_JIT] Unrecognized special op BREAK\n", op);
            fallback_interpreter(instr, opcode);
            break;
        case 0x10:
            // MFHI
            Errors::print_warning("[EE_JIT] Unrecognized special op MFHI\n", op);
            fallback_interpreter(instr, opcode);
            break;
        case 0x11:
            // MTHI
            Errors::print_warning("[EE_JIT] Unrecognized special op MTHI\n", op);
            fallback_interpreter(instr, opcode);
            break;
        case 0x12:
            // MFLO
            Errors::print_warning("[EE_JIT] Unrecognized special op MFLO\n", op);
            fallback_interpreter(instr, opcode);
            break;
        case 0x13:
            // MTLO
            Errors::print_warning("[EE_JIT] Unrecognized special op MTLO\n", op);
            fallback_interpreter(instr, opcode);
            break;
        case 0x14:
            // DSLLV
            Errors::print_warning("[EE_JIT] Unrecognized special op DSLLV\n", op);
            fallback_interpreter(instr, opcode);
            break;
        case 0x16:
            // DSRLV
            Errors::print_warning("[EE_JIT] Unrecognized special op DSRLV\n", op);
            fallback_interpreter(instr, opcode);
            break;
        case 0x17:
            // DSRAV
            Errors::print_warning("[EE_JIT] Unrecognized special op DSRAV\n", op);
            fallback_interpreter(instr, opcode);
            break;
        case 0x18:
            // MULT
            Errors::print_warning("[EE_JIT] Unrecognized special op MULT\n", op);
            fallback_interpreter(instr, opcode);
            break;
        case 0x19:
            // MULTU
            Errors::print_warning("[EE_JIT] Unrecognized special op MULTU\n", op);
            fallback_interpreter(instr, opcode);
            break;
        case 0x1A:
            // DIV
            Errors::print_warning("[EE_JIT] Unrecognized special op DIV\n", op);
            fallback_interpreter(instr, opcode);
            break;
        case 0x1B:
            // DIVU
            Errors::print_warning("[EE_JIT] Unrecognized special op DIVU\n", op);
            fallback_interpreter(instr, opcode);
            break;
        case 0x20:
            // ADD
            Errors::print_warning("[EE_JIT] Unrecognized special op ADD\n", op);
            fallback_interpreter(instr, opcode);
            break;
        case 0x21:
            // ADDU
            Errors::print_warning("[EE_JIT] Unrecognized special op ADDU\n", op);
            fallback_interpreter(instr, opcode);
            break;
        case 0x22:
            // SUB
            Errors::print_warning("[EE_JIT] Unrecognized special op SUB\n", op);
            fallback_interpreter(instr, opcode);
            break;
        case 0x23:
            // SUBU
            Errors::print_warning("[EE_JIT] Unrecognized special op SUBU\n", op);
            fallback_interpreter(instr, opcode);
            break;
        case 0x24:
            // AND
            Errors::print_warning("[EE_JIT] Unrecognized special op AND\n", op);
            fallback_interpreter(instr, opcode);
            break;
        case 0x25:
            // OR
            Errors::print_warning("[EE_JIT] Unrecognized special op OR\n", op);
            fallback_interpreter(instr, opcode);
            break;
        case 0x26:
            // XOR
            Errors::print_warning("[EE_JIT] Unrecognized special op XOR\n", op);
            fallback_interpreter(instr, opcode);
            break;
        case 0x27:
            // NOR
            Errors::print_warning("[EE_JIT] Unrecognized special op NOR\n", op);
            fallback_interpreter(instr, opcode);
            break;
        case 0x28:
            // MFSA
            Errors::print_warning("[EE_JIT] Unrecognized special op MFSA\n", op);
            fallback_interpreter(instr, opcode);
            break;
        case 0x29:
            // MTSA
            Errors::print_warning("[EE_JIT] Unrecognized special op MTSA\n", op);
            fallback_interpreter(instr, opcode);
            break;
        case 0x2A:
            // SLT
            Errors::print_warning("[EE_JIT] Unrecognized special op SLT\n", op);
            fallback_interpreter(instr, opcode);
            break;
        case 0x2B:
            // SLTU
            Errors::print_warning("[EE_JIT] Unrecognized special op SLTU\n", op);
            fallback_interpreter(instr, opcode);
            break;
        case 0x2C:
            // DADD
            Errors::print_warning("[EE_JIT] Unrecognized special op DADD\n", op);
            fallback_interpreter(instr, opcode);
            break;
        case 0x2D:
            // DADDU
            Errors::print_warning("[EE_JIT] Unrecognized special op DADDU\n", op);
            fallback_interpreter(instr, opcode);
            break;
        case 0x2E:
            // DSUB
            Errors::print_warning("[EE_JIT] Unrecognized special op DSUB\n", op);
            fallback_interpreter(instr, opcode);
            break;
        case 0x2F:
            // DSUBU
            Errors::print_warning("[EE_JIT] Unrecognized special op DSUBU\n", op);
            fallback_interpreter(instr, opcode);
            break;
        case 0x34:
            // TEQ
            Errors::print_warning("[EE_JIT] Unrecognized special op TEQ\n", op);
            fallback_interpreter(instr, opcode);
            break;
        case 0x38:
            // DSLL
            Errors::print_warning("[EE_JIT] Unrecognized special op DSLL\n", op);
            fallback_interpreter(instr, opcode);
            break;
        case 0x3A:
            // DSRL
            Errors::print_warning("[EE_JIT] Unrecognized special op DSRL\n", op);
            fallback_interpreter(instr, opcode);
            break;
        case 0x3B:
            // DSRA
            Errors::print_warning("[EE_JIT] Unrecognized special op DSRA\n", op);
            fallback_interpreter(instr, opcode);
            break;
        case 0x3C:
            // DSLL32
            Errors::print_warning("[EE_JIT] Unrecognized special op DSLL32\n", op);
            fallback_interpreter(instr, opcode);
            break;
        case 0x3E:
            // DSRL32
            Errors::print_warning("[EE_JIT] Unrecognized special op DSRL32\n", op);
            fallback_interpreter(instr, opcode);
            break;
        case 0x3F:
            // DSRA32
            Errors::print_warning("[EE_JIT] Unrecognized special op DSRA32\n", op);
            fallback_interpreter(instr, opcode);
            break;
        default:
            Errors::die("[EE_JIT] Unrecognized special op $%02X", op);
            return;
    }
    instrs.push_back(instr);
}

void EE_JitTranslator::translate_op_regimm(std::vector<IR::Instruction>& instrs, uint32_t opcode) const
{

}

void EE_JitTranslator::translate_op_mmi(std::vector<IR::Instruction>& instrs, uint32_t opcode) const
{

}

// TENTATIVE
bool EE_JitTranslator::is_branch(uint32_t instr_word) const noexcept
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

void EE_JitTranslator::fallback_interpreter(IR::Instruction& instr, uint32_t instr_word) const noexcept
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