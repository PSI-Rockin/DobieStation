#include <cstring>
#include "ee_jittrans.hpp"
#include "emotioninterpreter.hpp"
#include "../errors.hpp"

uint32_t branch_offset_ee(uint32_t instr, uint32_t PC)
{
    int32_t i = (int16_t)(instr);
    i <<= 2;
    return PC + i + 4;
}

uint32_t jump_offset_ee(uint32_t instr, uint32_t PC)
{
    uint32_t addr = (instr & 0x3FFFFFF) << 2;
    addr += (PC + 4) & 0xF0000000;
    return addr;
}

IR::Block EE_JitTranslator::translate(EmotionEngine &ee)
{
    //TODO
    IR::Block block;
    std::vector<IR::Instruction> instrs;
    uint32_t pc = ee.get_PC();

    bool branch_op = false;
    bool branch_delayslot = false;
    int cycle_count = 0;

    eret_op = false;

    while (!branch_delayslot && !eret_op)
    {
        uint32_t opcode = ee.read32(pc);
        IR::Instruction instr = translate_op(opcode, pc);

        if (branch_op)
            branch_delayslot = true;

        branch_op = is_branch(instr);
        instr.set_cycle_count(cycle_count);

        if (instr.op != IR::Opcode::Null)
            block.add_instr(instr);

        pc += 4;
        cycle_count++;
    }

    block.set_cycle_count(cycle_count);

    return block;
}

IR::Instruction EE_JitTranslator::translate_op(uint32_t opcode, uint32_t PC)
{
    uint8_t op = opcode >> 26;
    IR::Instruction instr;

    switch (op)
    {
        case 0x00:
            // Special Operation
            return translate_op_special(opcode, PC);
        case 0x01:
            // Regimm Operation
            return translate_op_regimm(opcode, PC);
        case 0x02:
            // J
            instr.op = IR::Opcode::Jump;
            instr.set_jump_dest(jump_offset_ee(opcode, PC));
            instr.set_is_link(false);
            return instr;
        case 0x03:
            // JAL
            instr.op = IR::Opcode::Jump;
            instr.set_jump_dest(jump_offset_ee(opcode, PC));
            instr.set_return_addr(PC + 8);
            instr.set_is_link(true);
            return instr;
        case 0x04:
            // BEQ
        {
            uint8_t source = (opcode >> 21) & 0x1F;
            uint8_t source2 = (opcode >> 16) & 0x1F;

            if (source == source2)
            {
                // B
                instr.op = IR::Opcode::Jump;
                instr.set_jump_dest(branch_offset_ee(opcode, PC));
                return instr;
            }
            if (!source)
            {
                // BEQZ
                instr.op = IR::Opcode::BranchEqualZero;
                instr.set_source(source2);
                instr.set_jump_dest(branch_offset_ee(opcode, PC));
                instr.set_jump_fail_dest(PC + 8);
                instr.set_is_likely(false);
                return instr;
            }
            if (!source2)
            {
                // BEQZ
                instr.op = IR::Opcode::BranchEqualZero;
                instr.set_source(source);
                instr.set_jump_dest(branch_offset_ee(opcode, PC));
                instr.set_jump_fail_dest(PC + 8);
                instr.set_is_likely(false);
                return instr;
            }

            instr.op = IR::Opcode::BranchEqual;
            instr.set_source((opcode >> 21) & 0x1F);
            instr.set_source2((opcode >> 16) & 0x1F);
            instr.set_jump_dest(branch_offset_ee(opcode, PC));
            instr.set_jump_fail_dest(PC + 8);
            instr.set_is_likely(false);
            return instr;
        }
        case 0x05:
            // BNE
        {
            uint8_t source = (opcode >> 21) & 0x1F;
            uint8_t source2 = (opcode >> 16) & 0x1F;

            if (source == source2)
            {
                instr.op = IR::Opcode::Null;
                return instr;
            }
            if (!source)
            {
                // BNEZ
                instr.op = IR::Opcode::BranchNotEqualZero;
                instr.set_source(source2);
                instr.set_jump_dest(branch_offset_ee(opcode, PC));
                instr.set_jump_fail_dest(PC + 8);
                instr.set_is_likely(false);
                return instr;
            }
            if (!source2)
            {
                // BNEZ
                instr.op = IR::Opcode::BranchNotEqualZero;
                instr.set_source(source);
                instr.set_jump_dest(branch_offset_ee(opcode, PC));
                instr.set_jump_fail_dest(PC + 8);
                instr.set_is_likely(false);
                return instr;
            }

            instr.op = IR::Opcode::BranchNotEqual;
            instr.set_source((opcode >> 21) & 0x1F);
            instr.set_source2((opcode >> 16) & 0x1F);
            instr.set_jump_dest(branch_offset_ee(opcode, PC));
            instr.set_jump_fail_dest(PC + 8);
            instr.set_is_likely(false);
            return instr;
        }
        case 0x06:
            // BLEZ
            instr.op = IR::Opcode::BranchLessThanOrEqualZero;
            instr.set_source((opcode >> 21) & 0x1F);
            instr.set_jump_dest(branch_offset_ee(opcode, PC));
            instr.set_jump_fail_dest(PC + 8);
            instr.set_is_likely(false);
            return instr;
        case 0x07:
            // BGTZ
            instr.op = IR::Opcode::BranchGreaterThanZero;
            instr.set_source((opcode >> 21) & 0x1F);
            instr.set_jump_dest(branch_offset_ee(opcode, PC));
            instr.set_jump_fail_dest(PC + 8);
            instr.set_is_likely(false);
            return instr;
        case 0x08:
            // ADDI
            // TODO: Overflow?
        case 0x09:
            // ADDIU
        {
            uint8_t dest = (opcode >> 16) & 0x1F;
            uint8_t source = (opcode >> 21) & 0x1F;
            int16_t immediate = opcode & 0xFFFF;
            if (!dest)
            {
                instr.op = IR::Opcode::Null;
                return instr;
            }

            if (!source)
            {
                instr.op = IR::Opcode::LoadConst;
                instr.set_dest(dest);
                instr.set_source(immediate);
            }

            instr.op = IR::Opcode::AddWordImm;
            instr.set_dest(dest);
            instr.set_source(source);
            instr.set_source2((int64_t)immediate);
            return instr;
        }
        case 0x0A:
            // SLTI
        {
            uint8_t dest = (opcode >> 16) & 0x1F;

            if (!dest)
            {
                instr.op = IR::Opcode::Null;
                return instr;
            }

            int64_t imm = (int16_t)(opcode & 0xFFFF);
            instr.op = IR::Opcode::SetOnLessThanImmediate;
            instr.set_dest(dest);
            instr.set_source((opcode >> 21) & 0x1F);
            instr.set_source2(imm);
            return instr;
        }
        case 0x0B:
            // SLTIU
        {
            uint8_t dest = (opcode >> 16) & 0x1F;

            if (!dest)
            {
                instr.op = IR::Opcode::Null;
                return instr;
            }

            int64_t imm = (int16_t)(opcode & 0xFFFF);
            instr.op = IR::Opcode::SetOnLessThanImmediateUnsigned;
            instr.set_dest(dest);
            instr.set_source((opcode >> 21) & 0x1F);
            instr.set_source2(imm);
            return instr;
        }
        case 0x0C:
            // ANDI
        {
            uint8_t dest = (opcode >> 16) & 0x1F;
            if (!dest)
            {
                instr.op = IR::Opcode::Null;
                return instr;
            }

            instr.op = IR::Opcode::AndImm;
            instr.set_dest(dest);
            instr.set_source((opcode >> 21) & 0x1F);
            instr.set_source2(opcode & 0xFFFF);
            return instr;
        }
        case 0x0D:
            // ORI
        {
            uint8_t dest = (opcode >> 16) & 0x1F;
            if (!dest)
            {
                instr.op = IR::Opcode::Null;
                return instr;
            }

            instr.op = IR::Opcode::OrImm;
            instr.set_dest((opcode >> 16) & 0x1F);
            instr.set_source((opcode >> 21) & 0x1F);
            instr.set_source2(opcode & 0xFFFF);
            return instr;
        }
        case 0x0E:
            // XORI
        {
            uint8_t dest = (opcode >> 16) & 0x1F;
            if (!dest)
            {
                instr.op = IR::Opcode::Null;
                return instr;
            }

            instr.op = IR::Opcode::XorImm;
            instr.set_dest(dest);
            instr.set_source((opcode >> 21) & 0x1F);
            instr.set_source2(opcode & 0xFFFF);
            return instr;
        }
        case 0x0F:
            // LUI
        {
            uint8_t dest = (opcode >> 16) & 0x1F;
            if (!dest)
            {
                instr.op = IR::Opcode::Null;
                return instr;
            }

            instr.op = IR::Opcode::LoadConst;
            instr.set_dest(dest);
            instr.set_source((int64_t)(int32_t)((opcode & 0xFFFF) << 16));
            return instr;
        }
        case 0x10:
            // COP0 (System Coprocessor)
            return translate_op_cop0(opcode, PC);
        case 0x11:
            // COP1 (Floating Point Unit)
            return translate_op_cop1(opcode, PC);
        case 0x12:
            // COP2 (Vector Unit 0)
            return translate_op_cop2(opcode, PC);
        case 0x13:
            // COP3 (unimplemented)
            Errors::die("[EE_JIT] Unrecognized cop3 opcode $%08X", opcode);
            return instr;
        case 0x14:
            // BEQL
        {
            uint8_t source = (opcode >> 21) & 0x1F;
            uint8_t source2 = (opcode >> 16) & 0x1F;

            if (!source)
            {
                // BEQZL
                instr.op = IR::Opcode::BranchEqualZero;
                instr.set_source(source2);
                instr.set_jump_dest(branch_offset_ee(opcode, PC));
                instr.set_jump_fail_dest(PC + 8);
                instr.set_is_likely(true);
                return instr;
            }
            if (!source2)
            {
                // BEQZL
                instr.op = IR::Opcode::BranchEqualZero;
                instr.set_source(source);
                instr.set_jump_dest(branch_offset_ee(opcode, PC));
                instr.set_jump_fail_dest(PC + 8);
                instr.set_is_likely(true);
                return instr;
            }

            instr.op = IR::Opcode::BranchEqual;
            instr.set_source((opcode >> 21) & 0x1F);
            instr.set_source2((opcode >> 16) & 0x1F);
            instr.set_jump_dest(branch_offset_ee(opcode, PC));
            instr.set_jump_fail_dest(PC + 8);
            instr.set_is_likely(true);
            return instr;
        }
        case 0x15:
            // BNEL
        {
            uint8_t source = (opcode >> 21) & 0x1F;
            uint8_t source2 = (opcode >> 16) & 0x1F;

            if (source == source2)
            {
                instr.op = IR::Opcode::Null;
                return instr;
            }
            if (!source)
            {
                // BNEZL
                instr.op = IR::Opcode::BranchNotEqualZero;
                instr.set_source(source2);
                instr.set_jump_dest(branch_offset_ee(opcode, PC));
                instr.set_jump_fail_dest(PC + 8);
                instr.set_is_likely(true);
                return instr;
            }
            if (!source2)
            {
                // BNEZL
                instr.op = IR::Opcode::BranchNotEqualZero;
                instr.set_source(source);
                instr.set_jump_dest(branch_offset_ee(opcode, PC));
                instr.set_jump_fail_dest(PC + 8);
                instr.set_is_likely(true);
                return instr;
            }

            instr.op = IR::Opcode::BranchNotEqual;
            instr.set_source((opcode >> 21) & 0x1F);
            instr.set_source2((opcode >> 16) & 0x1F);
            instr.set_jump_dest(branch_offset_ee(opcode, PC));
            instr.set_jump_fail_dest(PC + 8);
            instr.set_is_likely(true);
            return instr;
        }
        case 0x16:
            // BLEZL
            instr.op = IR::Opcode::BranchLessThanOrEqualZero;
            instr.set_source((opcode >> 21) & 0x1F);
            instr.set_jump_dest(branch_offset_ee(opcode, PC));
            instr.set_jump_fail_dest(PC + 8);
            instr.set_is_likely(true);
            return instr;
        case 0x17:
            // BGTZL
            instr.op = IR::Opcode::BranchGreaterThanZero;
            instr.set_source((opcode >> 21) & 0x1F);
            instr.set_jump_dest(branch_offset_ee(opcode, PC));
            instr.set_jump_fail_dest(PC + 8);
            instr.set_is_likely(true);
            return instr;
        case 0x18:
            // DADDI
            // TODO: Overflow?
        case 0x19:
            // DADDIU
        {
            uint8_t dest = (opcode >> 16) & 0x1F;
            uint8_t source = (opcode >> 21) & 0x1F;
            int16_t immediate = opcode & 0xFFFF;
            if (!dest)
            {
                instr.op = IR::Opcode::Null;
                return instr;
            }

            if (!source)
            {
                instr.op = IR::Opcode::LoadConst;
                instr.set_dest(dest);
                instr.set_source(immediate);
            }

            instr.op = IR::Opcode::AddDoublewordImm;
            instr.set_dest(dest);
            instr.set_source(source);
            instr.set_source2((int64_t)immediate);
            return instr;
        }
        case 0x1A:
            // LDL
            Errors::print_warning("[EE_JIT] Unrecognized op LDL\n", op);
            fallback_interpreter(instr, opcode);
            return instr;
        case 0x1B:
            // LDR
            Errors::print_warning("[EE_JIT] Unrecognized op LDR\n", op);
            fallback_interpreter(instr, opcode);
            return instr;
        case 0x1C:
            // MMI Operation
            return translate_op_mmi(opcode, PC);
        case 0x1E:
            // LQ
            Errors::print_warning("[EE_JIT] Unrecognized op LQ\n", op);
            fallback_interpreter(instr, opcode);
            return instr;
        case 0x1F:
            // SQ
            Errors::print_warning("[EE_JIT] Unrecognized op SQ\n", op);
            fallback_interpreter(instr, opcode);
            return instr;
        case 0x20:
            // LB
        {
            uint8_t dest = (opcode >> 16) & 0x1F;
            if (!dest)
            {
                instr.op = IR::Opcode::Null;
                return instr;
            }
            instr.op = IR::Opcode::LoadByte;
            instr.set_dest(dest);
            instr.set_source((opcode >> 21) & 0x1F);
            instr.set_source2((int64_t)(int16_t)(opcode & 0xFFFF));
            return instr;
        }
        case 0x21:
            // LH
        {
            uint8_t dest = (opcode >> 16) & 0x1F;
            if (!dest)
            {
                instr.op = IR::Opcode::Null;
                return instr;
            }
            instr.op = IR::Opcode::LoadHalfword;
            instr.set_dest(dest);
            instr.set_source((opcode >> 21) & 0x1F);
            instr.set_source2((int64_t)(int16_t)(opcode & 0xFFFF));
            return instr;
        }
        case 0x22:
            // LWL
            Errors::print_warning("[EE_JIT] Unrecognized op LWL\n", op);
            fallback_interpreter(instr, opcode);
            return instr;
        case 0x23:
            // LW
        {
            uint8_t dest = (opcode >> 16) & 0x1F;
            if (!dest)
            {
                instr.op = IR::Opcode::Null;
                return instr;
            }
            instr.op = IR::Opcode::LoadWord;
            instr.set_dest(dest);
            instr.set_source((opcode >> 21) & 0x1F);
            instr.set_source2((int64_t)(int16_t)(opcode & 0xFFFF));
            return instr;
        }
        case 0x24:
            // LBU
        {
            uint8_t dest = (opcode >> 16) & 0x1F;
            if (!dest)
            {
                instr.op = IR::Opcode::Null;
                return instr;
            }
            instr.op = IR::Opcode::LoadByteUnsigned;
            instr.set_dest(dest);
            instr.set_source((opcode >> 21) & 0x1F);
            instr.set_source2((int64_t)(int16_t)(opcode & 0xFFFF));
            return instr;
        }
        case 0x25:
            // LHU
        {
            uint8_t dest = (opcode >> 16) & 0x1F;
            if (!dest)
            {
                instr.op = IR::Opcode::Null;
                return instr;
            }
            instr.op = IR::Opcode::LoadHalfwordUnsigned;
            instr.set_dest(dest);
            instr.set_source((opcode >> 21) & 0x1F);
            instr.set_source2((int64_t)(int16_t)(opcode & 0xFFFF));
            return instr;
        }
        case 0x26:
            // LWR
            Errors::print_warning("[EE_JIT] Unrecognized op LWR\n", op);
            fallback_interpreter(instr, opcode);
            return instr;
        case 0x27:
            // LWU
        {
            uint8_t dest = (opcode >> 16) & 0x1F;
            if (!dest)
            {
                instr.op = IR::Opcode::Null;
                return instr;
            }
            instr.op = IR::Opcode::LoadWordUnsigned;
            instr.set_dest(dest);
            instr.set_source((opcode >> 21) & 0x1F);
            instr.set_source2((int64_t)(int16_t)(opcode & 0xFFFF));
            return instr;
        }
        case 0x28:
            // SB
        {
            instr.op = IR::Opcode::StoreByte;
            instr.set_dest((opcode >> 21) & 0x1F);
            instr.set_source((opcode >> 16) & 0x1F);
            instr.set_source2((int64_t)(int16_t)(opcode & 0xFFFF));
            return instr;
        }
        case 0x29:
            // SH
        {
            instr.op = IR::Opcode::StoreHalfword;
            instr.set_dest((opcode >> 21) & 0x1F);
            instr.set_source((opcode >> 16) & 0x1F);
            instr.set_source2((int64_t)(int16_t)(opcode & 0xFFFF));
            return instr;
        }
        case 0x2A:
            // SWL
            Errors::print_warning("[EE_JIT] Unrecognized op SWL\n", op);
            fallback_interpreter(instr, opcode);
            return instr;
        case 0x2B:
            // SW
        {
            instr.op = IR::Opcode::StoreWord;
            instr.set_dest((opcode >> 21) & 0x1F);
            instr.set_source((opcode >> 16) & 0x1F);
            instr.set_source2((int64_t)(int16_t)(opcode & 0xFFFF));
            return instr;
        }
        case 0x2C:
            // SDL
            Errors::print_warning("[EE_JIT] Unrecognized op SDL\n", op);
            fallback_interpreter(instr, opcode);
            return instr;
        case 0x2D:
            // SDR
            Errors::print_warning("[EE_JIT] Unrecognized op SDR\n", op);
            fallback_interpreter(instr, opcode);
            return instr;
        case 0x2E:
            // SWR
            Errors::print_warning("[EE_JIT] Unrecognized op SWR\n", op);
            fallback_interpreter(instr, opcode);
            return instr;
        case 0x2F:
            // CACHE
            fallback_interpreter(instr, opcode);
            return instr;
        case 0x31:
            // LWC1
            Errors::print_warning("[EE_JIT] Unrecognized op LWC1\n", op);
            fallback_interpreter(instr, opcode);
            return instr;
        case 0x33:
            // PREFETCH
            fallback_interpreter(instr, opcode);
            return instr;
        case 0x36:
            // LQC2
            Errors::print_warning("[EE_JIT] Unrecognized op LQC2\n", op);
            fallback_interpreter(instr, opcode);
            return instr;
        case 0x37:
            // LD
        {
            uint8_t dest = (opcode >> 16) & 0x1F;
            if (!dest)
            {
                instr.op = IR::Opcode::Null;
                return instr;
            }
            instr.op = IR::Opcode::LoadDoubleword;
            instr.set_dest(dest);
            instr.set_source((opcode >> 21) & 0x1F);
            instr.set_source2((int64_t)(int16_t)(opcode & 0xFFFF));
            return instr;
        }
        case 0x39:
            // SWC1
            Errors::print_warning("[EE_JIT] Unrecognized op SWC1\n", op);
            fallback_interpreter(instr, opcode);
            return instr;
        case 0x3E:
            // SQC2
            Errors::print_warning("[EE_JIT] Unrecognized op SQC2\n", op);
            fallback_interpreter(instr, opcode);
            return instr;
        case 0x3F:
            // SD
        {
            instr.op = IR::Opcode::StoreDoubleword;
            instr.set_dest((opcode >> 21) & 0x1F);
            instr.set_source((opcode >> 16) & 0x1F);
            instr.set_source2((int64_t)(int16_t)(opcode & 0xFFFF));
            return instr;
        }
        default:
            Errors::die("[EE_JIT] Unrecognized op $%02X", op);
            return instr;
    }
}

IR::Instruction EE_JitTranslator::translate_op_special(uint32_t opcode, uint32_t PC)
{
    uint8_t op = opcode & 0x3F;
    IR::Instruction instr;

    switch (op)
    {
        case 0x00:
            // SLL
        {
            uint8_t dest = (opcode >> 11) & 0x1F;
            if (!dest)
            {
                instr.op = IR::Opcode::Null;
                return instr;
            }

            instr.op = IR::Opcode::ShiftLeftLogical;
            instr.set_dest(dest);
            instr.set_source((opcode >> 16) & 0x1F);
            instr.set_source2((opcode >> 6) & 0x1F);
            return instr;
        }
        case 0x02:
            // SRL
        {
            uint8_t dest = (opcode >> 11) & 0x1F;
            if (!dest)
            {
                instr.op = IR::Opcode::Null;
                return instr;
            }

            instr.op = IR::Opcode::ShiftRightLogical;
            instr.set_dest(dest);
            instr.set_source((opcode >> 16) & 0x1F);
            instr.set_source2((opcode >> 6) & 0x1F);
            return instr;
        }
        case 0x03:
            // SRA
        {
            uint8_t dest = (opcode >> 11) & 0x1F;
            if (!dest)
            {
                instr.op = IR::Opcode::Null;
                return instr;
            }

            instr.op = IR::Opcode::ShiftRightArithmetic;
            instr.set_dest(dest);
            instr.set_source((opcode >> 16) & 0x1F);
            instr.set_source2((opcode >> 6) & 0x1F);
            return instr;
        }
        case 0x04:
            // SLLV
        {
            uint8_t dest = (opcode >> 11) & 0x1F;
            if (!dest)
            {
                instr.op = IR::Opcode::Null;
                return instr;
            }

            instr.op = IR::Opcode::ShiftLeftLogicalVariable;
            instr.set_dest(dest);
            instr.set_source((opcode >> 16) & 0x1F);
            instr.set_source2((opcode >> 21) & 0x1F);
            return instr;
        }
        case 0x06:
            // SRLV
        {
            uint8_t dest = (opcode >> 11) & 0x1F;
            if (!dest)
            {
                instr.op = IR::Opcode::Null;
                return instr;
            }

            instr.op = IR::Opcode::ShiftRightLogicalVariable;
            instr.set_dest(dest);
            instr.set_source((opcode >> 16) & 0x1F);
            instr.set_source2((opcode >> 21) & 0x1F);
            return instr;
        }
        case 0x07:
            // SRAV
        {
            uint8_t dest = (opcode >> 11) & 0x1F;
            if (!dest)
            {
                instr.op = IR::Opcode::Null;
                return instr;
            }

            instr.op = IR::Opcode::ShiftRightArithmeticVariable;
            instr.set_dest(dest);
            instr.set_source((opcode >> 16) & 0x1F);
            instr.set_source2((opcode >> 21) & 0x1F);
            return instr;
        }
        case 0x08:
            // JR
            instr.op = IR::Opcode::JumpIndirect;
            instr.set_source((opcode >> 21) & 0x1F);
            instr.set_is_link(false);
            return instr;
        case 0x09:
            // JALR
            instr.op = IR::Opcode::JumpIndirect;
            instr.set_source((opcode >> 21) & 0x1F);
            instr.set_return_addr(PC + 8);
            instr.set_dest((opcode >> 11) & 0x1F);
            instr.set_is_link(true);
            return instr;
        case 0x0A:
            // MOVZ
        {
            uint8_t dest = (opcode >> 11) & 0x1F;
            uint8_t source = (opcode >> 16) & 0x1F;
            uint8_t source2 = (opcode >> 21) & 0x1F;
            if (!dest)
            {
                instr.op = IR::Opcode::Null;
                return instr;
            }
            if (!source)
            {
                instr.op = IR::Opcode::MoveDoublewordReg;
                instr.set_dest(dest);
                instr.set_source(source2);
                return instr;
            }

            instr.op = IR::Opcode::MoveConditionalOnZero;
            instr.set_dest(dest);
            instr.set_source(source);
            instr.set_source2(source2);
            return instr;
        }
        case 0x0B:
            // MOVN
        {
            uint8_t dest = (opcode >> 11) & 0x1F;
            uint8_t source = (opcode >> 16) & 0x1F;
            uint8_t source2 = (opcode >> 21) & 0x1F;
            if (!dest || !source)
            {
                instr.op = IR::Opcode::Null;
                return instr;
            }

            instr.op = IR::Opcode::MoveConditionalOnNotZero;
            instr.set_dest(dest);
            instr.set_source(source);
            instr.set_source2(source2);
            return instr;
        }
        case 0x0C:
            // SYSCALL
            instr.op = IR::Opcode::SystemCall;
            instr.set_return_addr(PC);
            eret_op = true;
            return instr;
        case 0x0D:
            // BREAK
            fallback_interpreter(instr, opcode);
            return instr;
        case 0x0F:
            // SYNC
            fallback_interpreter(instr, opcode);
            return instr;
        case 0x10:
            // MFHI
        {
            uint8_t dest = (opcode >> 11) & 0x1F;
            instr.op = IR::Opcode::MoveDoublewordReg;
            instr.set_source((int)EE_SpecialReg::HI);
            instr.set_dest(dest);
            return instr;
        }
        case 0x11:
            // MTHI
        {
            uint8_t source = (opcode >> 21) & 0x1F;
            instr.op = IR::Opcode::MoveDoublewordReg;
            instr.set_source(source);
            instr.set_dest((int)EE_SpecialReg::HI);
            return instr;
        }
        case 0x12:
            // MFLO
        {
            uint8_t dest = (opcode >> 11) & 0x1F;
            instr.op = IR::Opcode::MoveDoublewordReg;
            instr.set_source((int)EE_SpecialReg::LO);
            instr.set_dest(dest);
            return instr;
        }
        case 0x13:
            // MTLO
        {
            uint8_t source = (opcode >> 21) & 0x1F;
            instr.op = IR::Opcode::MoveDoublewordReg;
            instr.set_source(source);
            instr.set_dest((int)EE_SpecialReg::LO);
            return instr;
        }
        case 0x14:
            // DSLLV
        {
            uint8_t dest = (opcode >> 11) & 0x1F;
            if (!dest)
            {
                instr.op = IR::Opcode::Null;
                return instr;
            }

            instr.op = IR::Opcode::DoublewordShiftLeftLogicalVariable;
            instr.set_dest(dest);
            instr.set_source((opcode >> 16) & 0x1F);
            instr.set_source2((opcode >> 21) & 0x1F);
            return instr;
        }
        case 0x16:
            // DSRLV
        {
            uint8_t dest = (opcode >> 11) & 0x1F;
            if (!dest)
            {
                instr.op = IR::Opcode::Null;
                return instr;
            }

            instr.op = IR::Opcode::DoublewordShiftRightLogicalVariable;
            instr.set_dest(dest);
            instr.set_source((opcode >> 16) & 0x1F);
            instr.set_source2((opcode >> 21) & 0x1F);
            return instr;
        }
        case 0x17:
            // DSRAV
        {
            uint8_t dest = (opcode >> 11) & 0x1F;
            if (!dest)
            {
                instr.op = IR::Opcode::Null;
                return instr;
            }

            instr.op = IR::Opcode::DoublewordShiftRightArithmeticVariable;
            instr.set_dest(dest);
            instr.set_source((opcode >> 16) & 0x1F);
            instr.set_source2((opcode >> 21) & 0x1F);
            return instr;
        }
        case 0x18:
            // MULT
        {
            instr.op = IR::Opcode::MultiplyWord;
            instr.set_dest((opcode >> 11) & 0x1F);
            instr.set_source((opcode >> 21) & 0x1F);
            instr.set_source2((opcode >> 16) & 0x1F);
            return instr;
        }
        case 0x19:
            // MULTU
        {
            instr.op = IR::Opcode::MultiplyUnsignedWord;
            instr.set_dest((opcode >> 11) & 0x1F);
            instr.set_source((opcode >> 21) & 0x1F);
            instr.set_source2((opcode >> 16) & 0x1F);
            return instr;
        }
        case 0x1A:
            // DIV
        {
            instr.op = IR::Opcode::DivideWord;
            instr.set_source((opcode >> 21) & 0x1F);
            instr.set_source2((opcode >> 16) & 0x1F);
            return instr;
        }
        case 0x1B:
            // DIVU
        {
            instr.op = IR::Opcode::DivideUnsignedWord;
            instr.set_source((opcode >> 21) & 0x1F);
            instr.set_source2((opcode >> 16) & 0x1F);
            return instr;
        }
        case 0x20:
            // ADD
            // TODO: Overflow?
        case 0x21:
            // ADDU
        {
            uint8_t dest = (opcode >> 11) & 0x1F;
            uint8_t source = (opcode >> 21) & 0x1F;
            uint8_t source2 = (opcode >> 16) & 0x1F;

            if (!source)
            {
                instr.op = IR::Opcode::MoveWordReg;
                instr.set_dest(dest);
                instr.set_source(source2);
                return instr;
            }
            if (!source2)
            {
                instr.op = IR::Opcode::MoveWordReg;
                instr.set_dest(dest);
                instr.set_source(source);
                return instr;
            }
            if (!dest)
            {
                instr.op = IR::Opcode::Null;
                return instr;
            }
            instr.op = IR::Opcode::AddWordReg;
            instr.set_dest(dest);
            instr.set_source(source);
            instr.set_source2(source2);
            return instr;
        }
        case 0x22:
            // SUB
            // TODO: Overflow?
        case 0x23:
            // SUBU
        {
            uint8_t dest = (opcode >> 11) & 0x1F;
            uint8_t source = (opcode >> 21) & 0x1F;
            uint8_t source2 = (opcode >> 16) & 0x1F;
            if (!dest)
            {
                instr.op = IR::Opcode::Null;
                return instr;
            }
            if (!source)
            {
                instr.op = IR::Opcode::NegateWordReg;
                instr.set_dest(dest);
                instr.set_source(source2);
            }
            if (source == source2)
            {
                instr.op = IR::Opcode::ClearWordReg;
                instr.set_dest(dest);
            }

            instr.op = IR::Opcode::SubWordReg;
            instr.set_dest(dest);
            instr.set_source(source);
            instr.set_source2(source2);
            return instr;
        }
        case 0x24:
            // AND
        {
            uint8_t dest = (opcode >> 11) & 0x1F;
            uint8_t source = (opcode >> 21) & 0x1F;
            uint8_t source2 = (opcode >> 16) & 0x1F;
            if (!dest)
            {
                instr.op = IR::Opcode::Null;
                return instr;
            }
            if (source == source2)
            {
                instr.op = IR::Opcode::MoveDoublewordReg;
                instr.set_dest(dest);
                instr.set_source(source);
                return instr;
            }

            instr.op = IR::Opcode::AndReg;
            instr.set_dest(dest);
            instr.set_source(source);
            instr.set_source2(source2);
            return instr;
        }
        case 0x25:
            // OR
        {
            uint8_t dest = (opcode >> 11) & 0x1F;
            uint8_t source = (opcode >> 21) & 0x1F;
            uint8_t source2 = (opcode >> 16) & 0x1F;
            if (!dest)
            {
                instr.op = IR::Opcode::Null;
                return instr;
            }
            if (source == source2)
            {
                instr.op = IR::Opcode::MoveDoublewordReg;
                instr.set_dest(dest);
                instr.set_source(source);
                return instr;
            }

            instr.op = IR::Opcode::OrReg;
            instr.set_dest(dest);
            instr.set_source(source);
            instr.set_source2(source2);
            return instr;
        }
        case 0x26:
            // XOR
        {
            uint8_t dest = (opcode >> 11) & 0x1F;
            uint8_t source = (opcode >> 21) & 0x1F;
            uint8_t source2 = (opcode >> 16) & 0x1F;
            if (!dest)
            {
                instr.op = IR::Opcode::Null;
                return instr;
            }
            if (source == source2)
            {
                instr.op = IR::Opcode::ClearDoublewordReg;
                instr.set_dest(dest);
                return instr;
            }

            instr.op = IR::Opcode::XorReg;
            instr.set_dest(dest);
            instr.set_source(source);
            instr.set_source2(source2);
            return instr;
        }
        case 0x27:
            // NOR
        {
            uint8_t dest = (opcode >> 11) & 0x1F;
            uint8_t source = (opcode >> 21) & 0x1F;
            uint8_t source2 = (opcode >> 16) & 0x1F;
            if (!dest)
            {
                instr.op = IR::Opcode::Null;
                return instr;
            }
            if (source == source2)
            {
                // TODO: dest = (NOT)source
            }

            instr.op = IR::Opcode::NorReg;
            instr.set_dest(dest);
            instr.set_source(source);
            instr.set_source2(source2);
            return instr;
        }
        case 0x28:
            // MFSA
            instr.op = IR::Opcode::MoveDoublewordReg;
            instr.set_dest((opcode >> 11) & 0x1F);
            instr.set_source((int)EE_SpecialReg::SA);
            return instr;
        case 0x29:
            // MTSA
            instr.op = IR::Opcode::MoveDoublewordReg;
            instr.set_dest((int)EE_SpecialReg::SA);
            instr.set_source((opcode >> 21) & 0x1F);
            return instr;
        case 0x2A:
            // SLT
        {
            uint8_t dest = (opcode >> 11) & 0x1F;
            uint8_t source = (opcode >> 21) & 0x1F;
            uint8_t source2 = (opcode >> 16) & 0x1F;

            instr.op = IR::Opcode::SetOnLessThan;
            instr.set_dest(dest);
            instr.set_source(source);
            instr.set_source2(source2);
            return instr;
        }
        case 0x2B:
            // SLTU
        {

            uint8_t dest = (opcode >> 11) & 0x1F;
            uint8_t source = (opcode >> 21) & 0x1F;
            uint8_t source2 = (opcode >> 16) & 0x1F;

            instr.op = IR::Opcode::SetOnLessThanUnsigned;
            instr.set_dest(dest);
            instr.set_source(source);
            instr.set_source2(source2);
            return instr;
        }
        case 0x2C:
            // DADD
            // TODO: Overflow?
        case 0x2D:
            // DADDU
        {
            uint8_t dest = (opcode >> 11) & 0x1F;
            uint8_t source = (opcode >> 21) & 0x1F;
            uint8_t source2 = (opcode >> 16) & 0x1F;

            if (!source)
            {
                instr.op = IR::Opcode::MoveDoublewordReg;
                instr.set_dest(dest);
                instr.set_source(source2);
                return instr;
            }
            if (!source2)
            {
                instr.op = IR::Opcode::MoveDoublewordReg;
                instr.set_dest(dest);
                instr.set_source(source);
                return instr;
            }
            if (!dest)
            {
                instr.op = IR::Opcode::Null;
                return instr;
            }

            instr.op = IR::Opcode::AddDoublewordReg;
            instr.set_dest(dest);
            instr.set_source(source);
            instr.set_source2(source2);
            return instr;
        }
        case 0x2E:
            // DSUB
            // TODO: Overflow?
        case 0x2F:
            // DSUBU
        {
            uint8_t dest = (opcode >> 11) & 0x1F;
            uint8_t source = (opcode >> 21) & 0x1F;
            uint8_t source2 = (opcode >> 16) & 0x1F;
            if (!dest)
            {
                instr.op = IR::Opcode::Null;
                return instr;
            }
            if (!source)
            {
                instr.op = IR::Opcode::NegateDoublewordReg;
                instr.set_dest(dest);
                instr.set_source(source2);
            }
            if (source == source2)
            {
                instr.op = IR::Opcode::ClearDoublewordReg;
                instr.set_dest(dest);
                return instr;
            }

            instr.op = IR::Opcode::SubDoublewordReg;
            instr.set_dest(dest);
            instr.set_source(source);
            instr.set_source2(source2);
            return instr;
        }
        case 0x34:
            // TEQ
            Errors::print_warning("[EE_JIT] Unrecognized special op TEQ\n", op);
            fallback_interpreter(instr, opcode);
            return instr;
        case 0x38:
            // DSLL
        {
            uint8_t dest = (opcode >> 11) & 0x1F;
            if (!dest)
            {
                instr.op = IR::Opcode::Null;
                return instr;
            }

            instr.op = IR::Opcode::DoublewordShiftLeftLogical;
            instr.set_dest(dest);
            instr.set_source((opcode >> 16) & 0x1F);
            instr.set_source2((opcode >> 6) & 0x1F);
            return instr;
        }
        case 0x3A:
            // DSRL
        {
            uint8_t dest = (opcode >> 11) & 0x1F;
            if (!dest)
            {
                instr.op = IR::Opcode::Null;
                return instr;
            }

            instr.op = IR::Opcode::DoublewordShiftRightLogical;
            instr.set_dest(dest);
            instr.set_source((opcode >> 16) & 0x1F);
            instr.set_source2((opcode >> 6) & 0x1F);
            return instr;
        }
        case 0x3B:
            // DSRA
        {
            uint8_t dest = (opcode >> 11) & 0x1F;
            if (!dest)
            {
                instr.op = IR::Opcode::Null;
                return instr;
            }

            instr.op = IR::Opcode::DoublewordShiftRightArithmetic;
            instr.set_dest(dest);
            instr.set_source((opcode >> 16) & 0x1F);
            instr.set_source2((opcode >> 6) & 0x1F);
            return instr;
        }
        case 0x3C:
            // DSLL32
        {
            uint8_t dest = (opcode >> 11) & 0x1F;
            if (!dest)
            {
                instr.op = IR::Opcode::Null;
                return instr;
            }

            instr.op = IR::Opcode::DoublewordShiftLeftLogical;
            instr.set_dest(dest);
            instr.set_source((opcode >> 16) & 0x1F);
            instr.set_source2(((opcode >> 6) & 0x1F) + 32);
            return instr;
        }
        case 0x3E:
            // DSRL32
        {
            uint8_t dest = (opcode >> 11) & 0x1F;
            if (!dest)
            {
                instr.op = IR::Opcode::Null;
                return instr;
            }

            instr.op = IR::Opcode::DoublewordShiftRightLogical;
            instr.set_dest(dest);
            instr.set_source((opcode >> 16) & 0x1F);
            instr.set_source2(((opcode >> 6) & 0x1F) + 32);
            return instr;
        }
        case 0x3F:
            // DSRA32
        {
            uint8_t dest = (opcode >> 11) & 0x1F;
            if (!dest)
            {
                instr.op = IR::Opcode::Null;
                return instr;
            }

            instr.op = IR::Opcode::DoublewordShiftRightArithmetic;
            instr.set_dest(dest);
            instr.set_source((opcode >> 16) & 0x1F);
            instr.set_source2(((opcode >> 6) & 0x1F) + 32);
            return instr;
        }
        default:
            Errors::die("[EE_JIT] Unrecognized special op $%02X", op);
            return instr;
    }
}

IR::Instruction EE_JitTranslator::translate_op_regimm(uint32_t opcode, uint32_t PC) const
{
    uint8_t op = (opcode >> 16) & 0x1F;
    IR::Instruction instr;

    switch (op)
    {
        case 0x00:
            // BLTZ
            instr.op = IR::Opcode::BranchLessThanZero;
            instr.set_source((opcode >> 21) & 0x1F);
            instr.set_jump_dest(branch_offset_ee(opcode, PC));
            instr.set_jump_fail_dest(PC + 8);
            instr.set_is_likely(false);
            instr.set_is_link(false);
            return instr;
        case 0x01:
            // BGEZ
            instr.op = IR::Opcode::BranchGreaterThanOrEqualZero;
            instr.set_source((opcode >> 21) & 0x1F);
            instr.set_jump_dest(branch_offset_ee(opcode, PC));
            instr.set_jump_fail_dest(PC + 8);
            instr.set_is_likely(false);
            instr.set_is_link(false);
            return instr;
        case 0x02:
            // BLTZL
            instr.op = IR::Opcode::BranchLessThanZero;
            instr.set_source((opcode >> 21) & 0x1F);
            instr.set_jump_dest(branch_offset_ee(opcode, PC));
            instr.set_jump_fail_dest(PC + 8);
            instr.set_is_likely(true);
            instr.set_is_link(false);
            return instr;
        case 0x03:
            // BGEZL
            instr.op = IR::Opcode::BranchGreaterThanOrEqualZero;
            instr.set_source((opcode >> 21) & 0x1F);
            instr.set_jump_dest(branch_offset_ee(opcode, PC));
            instr.set_jump_fail_dest(PC + 8);
            instr.set_is_likely(true);
            instr.set_is_link(false);
            return instr;
        case 0x10:
            // BLTZAL
            instr.op = IR::Opcode::BranchLessThanZero;
            instr.set_source((opcode >> 21) & 0x1F);
            instr.set_jump_dest(branch_offset_ee(opcode, PC));
            instr.set_jump_fail_dest(PC + 8);
            instr.set_return_addr(PC + 8);
            instr.set_is_likely(false);
            instr.set_is_link(true);
            return instr;
        case 0x11:
            // BGEZAL
            instr.op = IR::Opcode::BranchGreaterThanOrEqualZero;
            instr.set_source((opcode >> 21) & 0x1F);
            instr.set_jump_dest(branch_offset_ee(opcode, PC));
            instr.set_jump_fail_dest(PC + 8);
            instr.set_return_addr(PC + 8);
            instr.set_is_likely(false);
            instr.set_is_link(true);
            return instr;
        case 0x12:
            // BLTZALL
            instr.op = IR::Opcode::BranchLessThanZero;
            instr.set_source((opcode >> 21) & 0x1F);
            instr.set_jump_dest(branch_offset_ee(opcode, PC));
            instr.set_jump_fail_dest(PC + 8);
            instr.set_return_addr(PC + 8);
            instr.set_is_likely(true);
            instr.set_is_link(true);
            return instr;
        case 0x13:
            // BGEZALL
            instr.op = IR::Opcode::BranchGreaterThanOrEqualZero;
            instr.set_source((opcode >> 21) & 0x1F);
            instr.set_jump_dest(branch_offset_ee(opcode, PC));
            instr.set_jump_fail_dest(PC + 8);
            instr.set_return_addr(PC + 8);
            instr.set_is_likely(true);
            instr.set_is_link(true);
            return instr;
        case 0x18:
            // MTSAB
            Errors::print_warning("[EE_JIT] Unrecognized regimm op MTSAB\n", op);
            fallback_interpreter(instr, opcode);
            return instr;
        case 0x19:
            // MTASH
            Errors::print_warning("[EE_JIT] Unrecognized regimm op MTSAH\n", op);
            fallback_interpreter(instr, opcode);
            return instr;
        default:
            Errors::die("[EE_JIT] Unrecognized regimm op $%02X", op);
            return instr;
    }
}

IR::Instruction EE_JitTranslator::translate_op_mmi(uint32_t opcode, uint32_t PC) const
{
    uint8_t op = opcode & 0x3F;
    IR::Instruction instr;

    switch (op)
    {
        case 0x00:
            // MADD
            Errors::print_warning("[EE_JIT] Unrecognized mmi op MADD\n", op);
            fallback_interpreter(instr, opcode);
            return instr;
        case 0x01:
            // MADDU
            Errors::print_warning("[EE_JIT] Unrecognized mmi op MADDU\n", op);
            fallback_interpreter(instr, opcode);
            return instr;
        case 0x04:
            // PLZCW
            Errors::print_warning("[EE_JIT] Unrecognized mmi op PLZCW\n", op);
            fallback_interpreter(instr, opcode);
            return instr;
        case 0x08:
            // MMI0 Operation
            return translate_op_mmi0(opcode, PC);
        case 0x09:
            // MMI2 Operation
            return translate_op_mmi2(opcode, PC);
        case 0x10:
            // MFHI1
            Errors::print_warning("[EE_JIT] Unrecognized mmi op MFHI1\n", op);
            fallback_interpreter(instr, opcode);
            return instr;
        case 0x11:
            // MTHI1
            Errors::print_warning("[EE_JIT] Unrecognized mmi op MTHI1\n", op);
            fallback_interpreter(instr, opcode);
            return instr;
        case 0x12:
            // MFLO1
            Errors::print_warning("[EE_JIT] Unrecognized mmi op MFLO1\n", op);
            fallback_interpreter(instr, opcode);
            return instr;
        case 0x13:
            // MTLO1
            Errors::print_warning("[EE_JIT] Unrecognized mmi op MTLO1\n", op);
            fallback_interpreter(instr, opcode);
            return instr;
        case 0x18:
            // MULT1
            Errors::print_warning("[EE_JIT] Unrecognized mmi op MULT1\n", op);
            fallback_interpreter(instr, opcode);
            return instr;
        case 0x19:
            // MULTU1
            Errors::print_warning("[EE_JIT] Unrecognized mmi op MULTU1\n", op);
            fallback_interpreter(instr, opcode);
            return instr;
        case 0x1A:
            // DIV1
            Errors::print_warning("[EE_JIT] Unrecognized mmi op DIV1\n", op);
            fallback_interpreter(instr, opcode);
            return instr;
        case 0x1B:
            // DIVU1
            Errors::print_warning("[EE_JIT] Unrecognized mmi op DIVU1\n", op);
            fallback_interpreter(instr, opcode);
            return instr;
        case 0x20:
            // MADD1
            Errors::print_warning("[EE_JIT] Unrecognized mmi op MADD1\n", op);
            fallback_interpreter(instr, opcode);
            return instr;
        case 0x21:
            // MADDU1
            Errors::print_warning("[EE_JIT] Unrecognized mmi op MADDU1\n", op);
            fallback_interpreter(instr, opcode);
            return instr;
        case 0x28:
            // MMI1 Operation
            return translate_op_mmi1(opcode, PC);
        case 0x29:
            // MMI3 Operation
            return translate_op_mmi3(opcode, PC);
        case 0x30:
            // PMFHLFMT
            Errors::print_warning("[EE_JIT] Unrecognized mmi op PMFHLFMT\n", op);
            fallback_interpreter(instr, opcode);
            return instr;
        case 0x31:
            // PMTHLLW
            Errors::print_warning("[EE_JIT] Unrecognized mmi op PMTHLLW\n", op);
            fallback_interpreter(instr, opcode);
            return instr;
        case 0x34:
            // PSLLH
            Errors::print_warning("[EE_JIT] Unrecognized mmi op PSLLH\n", op);
            fallback_interpreter(instr, opcode);
            return instr;
        case 0x36:
            // PSRLH
            Errors::print_warning("[EE_JIT] Unrecognized mmi op PSRLH\n", op);
            fallback_interpreter(instr, opcode);
            return instr;
        case 0x37:
            // PSRAH
            Errors::print_warning("[EE_JIT] Unrecognized mmi op PSRAH\n", op);
            fallback_interpreter(instr, opcode);
            return instr;
        case 0x3C:
            // PSLLW
            Errors::print_warning("[EE_JIT] Unrecognized mmi op PSLLW\n", op);
            fallback_interpreter(instr, opcode);
            return instr;
        case 0x3E:
            // PSRLW
            Errors::print_warning("[EE_JIT] Unrecognized mmi op PSRLW\n", op);
            fallback_interpreter(instr, opcode);
            return instr;
        case 0x3F:
            // PSRAW
            Errors::print_warning("[EE_JIT] Unrecognized mmi op PSRAW\n", op);
            fallback_interpreter(instr, opcode);
            return instr;
        default:
            Errors::die("[EE_JIT] Unrecognized mmi op $%02X", op);
            return instr;
    }
}

IR::Instruction EE_JitTranslator::translate_op_mmi0(uint32_t opcode, uint32_t PC) const
{
    uint8_t op = (opcode >> 6) & 0x1F;
    IR::Instruction instr;

    switch (op)
    {
        case 0x00:
            // PADDW
            Errors::print_warning("[EE_JIT] Unrecognized mmi0 op PADDW\n", op);
            fallback_interpreter(instr, opcode);
            return instr;
        case 0x01:
            // PSUBW
            Errors::print_warning("[EE_JIT] Unrecognized mmi0 op PSUBW\n", op);
            fallback_interpreter(instr, opcode);
            return instr;
        case 0x02:
            // PCGTW
            Errors::print_warning("[EE_JIT] Unrecognized mmi0 op PCTGW\n", op);
            fallback_interpreter(instr, opcode);
            return instr;
        case 0x03:
            // PMAXW
            Errors::print_warning("[EE_JIT] Unrecognized mmi0 op PMAXW\n", op);
            fallback_interpreter(instr, opcode);
            return instr;
        case 0x04:
            // PADDH
            Errors::print_warning("[EE_JIT] Unrecognized mmi0 op PADDH\n", op);
            fallback_interpreter(instr, opcode);
            return instr;
        case 0x05:
            // PSUBH
            Errors::print_warning("[EE_JIT] Unrecognized mmi0 op PSUBH\n", op);
            fallback_interpreter(instr, opcode);
            return instr;
        case 0x06:
            // PCTGH
            Errors::print_warning("[EE_JIT] Unrecognized mmi0 op PCTGH\n", op);
            fallback_interpreter(instr, opcode);
            return instr;
        case 0x07:
            // PMAXH
            Errors::print_warning("[EE_JIT] Unrecognized mmi0 op PMAXH\n", op);
            fallback_interpreter(instr, opcode);
            return instr;
        case 0x08:
            // PADDB
            Errors::print_warning("[EE_JIT] Unrecognized mmi0 op PADDB\n", op);
            fallback_interpreter(instr, opcode);
            return instr;
        case 0x09:
            // PSUBB
            Errors::print_warning("[EE_JIT] Unrecognized mmi0 op PSUBB\n", op);
            fallback_interpreter(instr, opcode);
            return instr;
        case 0x0A:
            // PCTGB
            Errors::print_warning("[EE_JIT] Unrecognized mmi0 op PCTGB\n", op);
            fallback_interpreter(instr, opcode);
            return instr;
        case 0x10:
            // PADDSW
            Errors::print_warning("[EE_JIT] Unrecognized mmi0 op PADDSW\n", op);
            fallback_interpreter(instr, opcode);
            return instr;
        case 0x11:
            // PSUBSW
            Errors::print_warning("[EE_JIT] Unrecognized mmi0 op PSUBSW\n", op);
            fallback_interpreter(instr, opcode);
            return instr;
        case 0x12:
            // PADDW
            Errors::print_warning("[EE_JIT] Unrecognized mmi0 op PADDW\n", op);
            fallback_interpreter(instr, opcode);
            return instr;
        case 0x13:
            // PPACW
            Errors::print_warning("[EE_JIT] Unrecognized mmi0 op PPACW\n", op);
            fallback_interpreter(instr, opcode);
            return instr;
        case 0x14:
            // PADDSH
            Errors::print_warning("[EE_JIT] Unrecognized mmi0 op PADDSH\n", op);
            fallback_interpreter(instr, opcode);
            return instr;
        case 0x15:
            // PSUBSH
            Errors::print_warning("[EE_JIT] Unrecognized mmi0 op PSUBSH\n", op);
            fallback_interpreter(instr, opcode);
            return instr;
        case 0x16:
            // PEXTLH
            Errors::print_warning("[EE_JIT] Unrecognized mmi0 op PEXTLH\n", op);
            fallback_interpreter(instr, opcode);
            return instr;
        case 0x17:
            // PPACH
            Errors::print_warning("[EE_JIT] Unrecognized mmi0 op PPACH\n", op);
            fallback_interpreter(instr, opcode);
            return instr;
        case 0x18:
            // PADDSB
            Errors::print_warning("[EE_JIT] Unrecognized mmi0 op PADDSB\n", op);
            fallback_interpreter(instr, opcode);
            return instr;
        case 0x19:
            // PSUBSB
            Errors::print_warning("[EE_JIT] Unrecognized mmi0 op PSUBSB\n", op);
            fallback_interpreter(instr, opcode);
            return instr;
        case 0x1A:
            // PEXTLB
            Errors::print_warning("[EE_JIT] Unrecognized mmi0 op PADDW\n", op);
            fallback_interpreter(instr, opcode);
            return instr;
        case 0x1B:
            // PPACB
            Errors::print_warning("[EE_JIT] Unrecognized mmi0 op PPACB\n", op);
            fallback_interpreter(instr, opcode);
            return instr;
        case 0x1E:
            // PEXT5
            Errors::print_warning("[EE_JIT] Unrecognized mmi0 op PEXT5\n", op);
            fallback_interpreter(instr, opcode);
            return instr;
        case 0x1F:
            // PPAC5
            Errors::print_warning("[EE_JIT] Unrecognized mmi0 op PPAC5\n", op);
            fallback_interpreter(instr, opcode);
            return instr;
        default:
            Errors::die("[EE_JIT] Unrecognized mmi0 op $%02X", op);
            return instr;
    }
}

IR::Instruction EE_JitTranslator::translate_op_mmi1(uint32_t opcode, uint32_t PC) const
{
    uint8_t op = (opcode >> 6) & 0x1F;
    IR::Instruction instr;

    switch (op)
    {
        case 0x01:
            // PABSW
            Errors::print_warning("[EE_JIT] Unrecognized mmi1 op PABSW\n", op);
            fallback_interpreter(instr, opcode);
            return instr;
        case 0x02:
            // PCEQW
            Errors::print_warning("[EE_JIT] Unrecognized mmi1 op PABSW\n", op);
            fallback_interpreter(instr, opcode);
            return instr;
        case 0x03:
            // PMINW
            Errors::print_warning("[EE_JIT] Unrecognized mmi1 op PABSW\n", op);
            fallback_interpreter(instr, opcode);
            return instr;
        case 0x04:
            // PADSBH
            Errors::print_warning("[EE_JIT] Unrecognized mmi1 op PABSW\n", op);
            fallback_interpreter(instr, opcode);
            return instr;
        case 0x05:
            // PABSH
            Errors::print_warning("[EE_JIT] Unrecognized mmi1 op PABSW\n", op);
            fallback_interpreter(instr, opcode);
            return instr;
        case 0x06:
            // PCEQH
            Errors::print_warning("[EE_JIT] Unrecognized mmi1 op PCEQH\n", op);
            fallback_interpreter(instr, opcode);
            return instr;
        case 0x07:
            // PMINH
            Errors::print_warning("[EE_JIT] Unrecognized mmi1 op PMINH\n", op);
            fallback_interpreter(instr, opcode);
            return instr;
        case 0x0A:
            // PCEQB
            Errors::print_warning("[EE_JIT] Unrecognized mmi1 op PCEQB\n", op);
            fallback_interpreter(instr, opcode);
            return instr;
        case 0x10:
            // PADDUW
            Errors::print_warning("[EE_JIT] Unrecognized mmi1 op PADDUW\n", op);
            fallback_interpreter(instr, opcode);
            return instr;
        case 0x11:
            // PSUBUW
            Errors::print_warning("[EE_JIT] Unrecognized mmi1 op PCEQH\n", op);
            fallback_interpreter(instr, opcode);
            return instr;
        case 0x12:
            // PEXTUW
            Errors::print_warning("[EE_JIT] Unrecognized mmi1 op PEXTUW\n", op);
            fallback_interpreter(instr, opcode);
            return instr;
        case 0x14:
            // PADDUH
            Errors::print_warning("[EE_JIT] Unrecognized mmi1 op PADDUH\n", op);
            fallback_interpreter(instr, opcode);
            return instr;
        case 0x15:
            // PSUBUH
            Errors::print_warning("[EE_JIT] Unrecognized mmi1 op PCEQH\n", op);
            fallback_interpreter(instr, opcode);
            return instr;
        case 0x16:
            // PEXTUH
            Errors::print_warning("[EE_JIT] Unrecognized mmi1 op PEXTUH\n", op);
            fallback_interpreter(instr, opcode);
            return instr;
        case 0x18:
            // PADDUB
            Errors::print_warning("[EE_JIT] Unrecognized mmi1 op PADDUB\n", op);
            fallback_interpreter(instr, opcode);
            return instr;
        case 0x19:
            // PSUBUB
            Errors::print_warning("[EE_JIT] Unrecognized mmi1 op PSUBUB\n", op);
            fallback_interpreter(instr, opcode);
            return instr;
        case 0x1A:
            // PEXTUB
            Errors::print_warning("[EE_JIT] Unrecognized mmi1 op PEXTUB\n", op);
            fallback_interpreter(instr, opcode);
            return instr;
        case 0x1B:
            // QFSRV
            Errors::print_warning("[EE_JIT] Unrecognized mmi1 op QFSRV\n", op);
            fallback_interpreter(instr, opcode);
            return instr;
        default:
            Errors::die("[EE_JIT] Unrecognized mmi1 op $%02X", op);
            return instr;
    }
}

IR::Instruction EE_JitTranslator::translate_op_mmi2(uint32_t opcode, uint32_t PC) const
{
    uint8_t op = (opcode >> 6) & 0x1F;
    IR::Instruction instr;

    switch (op)
    {
        case 0x00:
            // PMADDW
            Errors::print_warning("[EE_JIT] Unrecognized mmi2 op PMADDW\n", op);
            fallback_interpreter(instr, opcode);
            return instr;
        case 0x02:
            // PSLLVW
            Errors::print_warning("[EE_JIT] Unrecognized mmi2 op PSLLVW\n", op);
            fallback_interpreter(instr, opcode);
            return instr;
        case 0x03:
            // PSRLVW
            Errors::print_warning("[EE_JIT] Unrecognized mmi2 op PSRLVW\n", op);
            fallback_interpreter(instr, opcode);
            return instr;
        case 0x04:
            // PMSUBW
            Errors::print_warning("[EE_JIT] Unrecognized mmi2 op PMSUBW\n", op);
            fallback_interpreter(instr, opcode);
            return instr;
        case 0x08:
            // PMFHI
            Errors::print_warning("[EE_JIT] Unrecognized mmi2 op PMADDW\n", op);
            fallback_interpreter(instr, opcode);
            return instr;
        case 0x09:
            // PMFLO
            Errors::print_warning("[EE_JIT] Unrecognized mmi2 op PMFLO\n", op);
            fallback_interpreter(instr, opcode);
            return instr;
        case 0x0A:
            // PINTH
            Errors::print_warning("[EE_JIT] Unrecognized mmi2 op PINTH\n", op);
            fallback_interpreter(instr, opcode);
            return instr;
        case 0x0C:
            // PMULTW
            Errors::print_warning("[EE_JIT] Unrecognized mmi2 op PMULTW\n", op);
            fallback_interpreter(instr, opcode);
            return instr;
        case 0x0D:
            // PDIVW
            Errors::print_warning("[EE_JIT] Unrecognized mmi2 op PDIVW\n", op);
            fallback_interpreter(instr, opcode);
            return instr;
        case 0x0E:
            // PCPYLD
            Errors::print_warning("[EE_JIT] Unrecognized mmi2 op PCPYLD\n", op);
            fallback_interpreter(instr, opcode);
            return instr;
        case 0x10:
            // PMADDH
            Errors::print_warning("[EE_JIT] Unrecognized mmi2 op PMADDH\n", op);
            fallback_interpreter(instr, opcode);
            return instr;
        case 0x11:
            // PHMADH
            Errors::print_warning("[EE_JIT] Unrecognized mmi2 op PHMADH\n", op);
            fallback_interpreter(instr, opcode);
            return instr;
        case 0x12:
            // PAND
            Errors::print_warning("[EE_JIT] Unrecognized mmi2 op PAND\n", op);
            fallback_interpreter(instr, opcode);
            return instr;
        case 0x13:
            // PXOR
            Errors::print_warning("[EE_JIT] Unrecognized mmi2 op PXOR\n", op);
            fallback_interpreter(instr, opcode);
            return instr;
        case 0x14:
            // PMSUBH
            Errors::print_warning("[EE_JIT] Unrecognized mmi2 op PMSUBH\n", op);
            fallback_interpreter(instr, opcode);
            return instr;
        case 0x15:
            // PHMSBH
            Errors::print_warning("[EE_JIT] Unrecognized mmi2 op PMADDW\n", op);
            fallback_interpreter(instr, opcode);
            return instr;
        case 0x1A:
            // PEXEH
            Errors::print_warning("[EE_JIT] Unrecognized mmi2 op PEXEH\n", op);
            fallback_interpreter(instr, opcode);
            return instr;
        case 0x1B:
            // PREVW
            Errors::print_warning("[EE_JIT] Unrecognized mmi2 op PREVW\n", op);
            fallback_interpreter(instr, opcode);
            return instr;
        case 0x1C:
            // PMULTH
            Errors::print_warning("[EE_JIT] Unrecognized mmi2 op PMULTH\n", op);
            fallback_interpreter(instr, opcode);
            return instr;
        case 0x1D:
            // PDIVBW
            Errors::print_warning("[EE_JIT] Unrecognized mmi2 op PDIVBW\n", op);
            fallback_interpreter(instr, opcode);
            return instr;
        case 0x1E:
            // PEXEW
            Errors::print_warning("[EE_JIT] Unrecognized mmi2 op PEXEW\n", op);
            fallback_interpreter(instr, opcode);
            return instr;
        case 0x1F:
            // PROT3W
            Errors::print_warning("[EE_JIT] Unrecognized mmi2 op PROT3W\n", op);
            fallback_interpreter(instr, opcode);
            return instr;
        default:
            Errors::die("[EE_JIT] Unrecognized mmi2 op $%02X", op);
            return instr;
    }
}

IR::Instruction EE_JitTranslator::translate_op_mmi3(uint32_t opcode, uint32_t PC) const
{
    uint8_t op = (opcode >> 6) & 0x1F;
    IR::Instruction instr;

    switch (op)
    {
        case 0x00:
            // PMADDUW
            Errors::print_warning("[EE_JIT] Unrecognized mmi3 op PMADDUW\n", op);
            fallback_interpreter(instr, opcode);
            return instr;
        case 0x03:
            // PSRAVW
            Errors::print_warning("[EE_JIT] Unrecognized mmi3 op PSRAVW\n", op);
            fallback_interpreter(instr, opcode);
            return instr;
        case 0x08:
            // PMTHI
            Errors::print_warning("[EE_JIT] Unrecognized mmi3 op PMTHI\n", op);
            fallback_interpreter(instr, opcode);
            return instr;
        case 0x09:
            // PMTLO
            Errors::print_warning("[EE_JIT] Unrecognized mmi3 op PMTLO\n", op);
            fallback_interpreter(instr, opcode);
            return instr;
        case 0x0A:
            // PINTEH
            Errors::print_warning("[EE_JIT] Unrecognized mmi3 op PINTEH\n", op);
            fallback_interpreter(instr, opcode);
            return instr;
        case 0x0C:
            // PMULTUW
            Errors::print_warning("[EE_JIT] Unrecognized mmi3 op PMULTUW\n", op);
            fallback_interpreter(instr, opcode);
            return instr;
        case 0x0D:
            // PDIVUW
            Errors::print_warning("[EE_JIT] Unrecognized mmi3 op PDIVUW\n", op);
            fallback_interpreter(instr, opcode);
            return instr;
        case 0x0E:
            // PCPYUD
            Errors::print_warning("[EE_JIT] Unrecognized mmi3 op PCPYUD\n", op);
            fallback_interpreter(instr, opcode);
            return instr;
        case 0x12:
            // POR
            Errors::print_warning("[EE_JIT] Unrecognized mmi3 op POR\n", op);
            fallback_interpreter(instr, opcode);
            return instr;
        case 0x13:
            // PNOR
            Errors::print_warning("[EE_JIT] Unrecognized mmi3 op PNOR\n", op);
            fallback_interpreter(instr, opcode);
            return instr;
        case 0x1A:
            // PEXCH
            Errors::print_warning("[EE_JIT] Unrecognized mmi3 op PEXCH\n", op);
            fallback_interpreter(instr, opcode);
            return instr;
        case 0x1B:
            // PCPYH
            Errors::print_warning("[EE_JIT] Unrecognized mmi3 op PCYPH\n", op);
            fallback_interpreter(instr, opcode);
            return instr;
        case 0x1E:
            // PEXCW
            Errors::print_warning("[EE_JIT] Unrecognized mmi3 op PEXCW\n", op);
            fallback_interpreter(instr, opcode);
            return instr;
        default:
            Errors::die("[EE_JIT] Unrecognized mmi3 op $%02X", op);
            return instr;
    }
}

IR::Instruction EE_JitTranslator::translate_op_cop0(uint32_t opcode, uint32_t PC)
{
    uint8_t op = (opcode >> 21) & 0x1F;
    IR::Instruction instr;

    switch (op)
    {
        case 0x00:
            // MFC0
            Errors::print_warning("[EE_JIT] Unrecognized cop0 op MFC0\n", op);
            fallback_interpreter(instr, opcode);
            return instr;
        case 0x04:
            // MTC0
            Errors::print_warning("[EE_JIT] Unrecognized cop0 op MTC0\n", op);
            fallback_interpreter(instr, opcode);
            return instr;
        case 0x08:
            // BC0
            instr.op = IR::Opcode::BranchCop0;
            instr.set_is_likely((opcode >> 17) & 1);
            instr.set_field((opcode >> 16) & 1);
            instr.set_jump_dest(branch_offset_ee(opcode, PC));
            instr.set_jump_fail_dest(PC + 8);
            return instr;
        case 0x10:
            // Type2 op
            return translate_op_cop0_type2(opcode, PC);
        default:
            Errors::die("[EE_JIT] Unrecognized cop0 op $%02X", op);
            return instr;
    }
}

IR::Instruction EE_JitTranslator::translate_op_cop0_type2(uint32_t opcode, uint32_t PC)
{
    uint8_t op = opcode & 0x3F;
    IR::Instruction instr;

    switch (op)
    {
        case 0x02:
            // TLBWI
            Errors::print_warning("[EE_JIT] Unrecognized cop0 type2 op TLBWI\n", op);
            fallback_interpreter(instr, opcode);
            return instr;
        case 0x18:
            // ERET
            instr.op = IR::Opcode::ExceptionReturn;
            instr.set_source(opcode);
            eret_op = true;
            return instr;
        case 0x38:
            // EI
            Errors::print_warning("[EE_JIT] Unrecognized cop0 type2 op EI\n", op);
            fallback_interpreter(instr, opcode);
            return instr;
        case 0x39:
            // DI
            Errors::print_warning("[EE_JIT] Unrecognized cop0 type2 op DI\n", op);
            fallback_interpreter(instr, opcode);
            return instr;
        default:
            Errors::die("[EE_JIT] Unrecognized cop0 type2 op $%02X", op);
            return instr;
    }
}

IR::Instruction EE_JitTranslator::translate_op_cop1(uint32_t opcode, uint32_t PC) const
{
    uint8_t op = (opcode >> 21) & 0x1F;
    IR::Instruction instr;

    switch (op)
    {
        case 0x00:
            // MFC1
        {
            uint8_t dest = (opcode >> 16) & 0x1F;
            uint8_t source = (opcode >> 11) & 0x1F;
            instr.op = IR::Opcode::MoveFromCoprocessor1;
            instr.set_dest(dest);
            instr.set_source(source);
            return instr;
        }
        case 0x02:
            // CFC1
            Errors::print_warning("[EE_JIT] Unrecognized cop1 op CFC1\n", op);
            fallback_interpreter(instr, opcode);
            return instr;
        case 0x04:
            // MTC1
        {
            uint8_t dest = (opcode >> 11) & 0x1F;
            uint8_t source = (opcode >> 16) & 0x1F;
            instr.op = IR::Opcode::MoveToCoprocessor1;
            instr.set_dest(dest);
            instr.set_source(source);
            return instr;
        }
        case 0x06:
            // CTC1
            Errors::print_warning("[EE_JIT] Unrecognized cop1 op CTC1\n", op);
            fallback_interpreter(instr, opcode);
            return instr;
        case 0x08:
            // BC1
            instr.op = IR::Opcode::BranchCop1;
            instr.set_is_likely((opcode >> 17) & 1);
            instr.set_field((opcode >> 16) & 1);
            instr.set_jump_dest(branch_offset_ee(opcode, PC));
            instr.set_jump_fail_dest(PC + 8);
            return instr;
        case 0x10:
            // FPU Operation
            return translate_op_cop1_fpu(opcode, PC);
        case 0x14:
            // CVT.S.W
            Errors::print_warning("[EE_JIT] Unrecognized cop1 op CVT.S.W\n", op);
            fallback_interpreter(instr, opcode);
            return instr;
        default:
            Errors::die("[EE_JIT] Unrecognized cop1 op $%02X", op);
            return instr;
    }
}

IR::Instruction EE_JitTranslator::translate_op_cop1_fpu(uint32_t opcode, uint32_t PC) const
{
    uint8_t op = opcode & 0x3F;
    IR::Instruction instr;

    switch (op)
    {
        case 0x00:
            // ADD.S
        {
            uint8_t dest = (opcode >> 6) & 0x1F;
            uint8_t source = (opcode >> 11) & 0x1F;
            uint8_t source2 = (opcode >> 16) & 0x1F;
            instr.set_dest(dest);
            instr.set_source(source);
            instr.set_source2(source2);
            instr.op = IR::Opcode::FloatingPointAdd;
            return instr;
        }
        case 0x01:
            // SUB.S
        {
            uint8_t dest = (opcode >> 6) & 0x1F;
            uint8_t source = (opcode >> 11) & 0x1F;
            uint8_t source2 = (opcode >> 16) & 0x1F;
            instr.set_dest(dest);
            instr.set_source(source);
            instr.set_source2(source2);
            instr.op = IR::Opcode::FloatingPointSubtract;
            return instr;
        }
        case 0x02:
            // MUL.S
        {
            uint8_t dest = (opcode >> 6) & 0x1F;
            uint8_t source = (opcode >> 11) & 0x1F;
            uint8_t source2 = (opcode >> 16) & 0x1F;
            instr.set_dest(dest);
            instr.set_source(source);
            instr.set_source2(source2);
            instr.op = IR::Opcode::FloatingPointMultiply;
            return instr;
        }
        case 0x03:
            // DIV.S
            Errors::print_warning("[EE_JIT] Unrecognized fpu op DIV.S\n", op);
            fallback_interpreter(instr, opcode);
            return instr;
        case 0x04:
            // SQRT.S
        {
            uint8_t dest = (opcode >> 6) & 0x1F;
            uint8_t source = (opcode >> 16) & 0x1F;
            instr.op = IR::Opcode::FloatingPointSquareRoot;
            instr.set_dest(dest);
            instr.set_source(source);
            return instr;
        }
        case 0x05:
            // ABS.S
        {
            uint8_t dest = (opcode >> 6) & 0x1F;
            uint8_t source = (opcode >> 11) & 0x1F;
            instr.op = IR::Opcode::FloatingPointAbsoluteValue;
            instr.set_dest(dest);
            instr.set_source(source);
            return instr;
        }
        case 0x06:
            // MOV.S
        {
            uint8_t dest = (opcode >> 6) & 0x1F;
            uint8_t source = (opcode >> 11) & 0x1F;
            if (dest == source)
            {
                instr.op = IR::Opcode::Null;
                return instr;
            }
            instr.op = IR::Opcode::MoveXmmReg;
            instr.set_dest(dest);
            instr.set_source(source);
            return instr;
        }
        case 0x07:
            // NEG.S
        {
            uint8_t dest = (opcode >> 6) & 0x1F;
            uint8_t source = (opcode >> 11) & 0x1F;
            instr.op = IR::Opcode::FloatingPointNegate;
            instr.set_dest(dest);
            instr.set_source(source);
            return instr;
        }
        case 0x16:
            // RSQRT.S
            Errors::print_warning("[EE_JIT] Unrecognized fpu op RSQRT.S\n", op);
            fallback_interpreter(instr, opcode);
            return instr;
        case 0x18:
            // ADDA.S
        {
            uint8_t source = (opcode >> 11) & 0x1F;
            uint8_t source2 = (opcode >> 16) & 0x1F;
            instr.set_dest((int)FPU_SpecialReg::ACC);
            instr.set_source(source);
            instr.set_source2(source2);
            instr.op = IR::Opcode::FloatingPointAdd;
            return instr;
        }
        case 0x19:
            // SUBA.S
        {
            uint8_t source = (opcode >> 11) & 0x1F;
            uint8_t source2 = (opcode >> 16) & 0x1F;
            instr.set_dest((int)FPU_SpecialReg::ACC);
            instr.set_source(source);
            instr.set_source2(source2);
            instr.op = IR::Opcode::FloatingPointSubtract;
            return instr;
        }
        case 0x1A:
            // MULA.S
        {
            uint8_t source = (opcode >> 11) & 0x1F;
            uint8_t source2 = (opcode >> 16) & 0x1F;
            instr.set_dest((int)FPU_SpecialReg::ACC);
            instr.set_source(source);
            instr.set_source2(source2);
            instr.op = IR::Opcode::FloatingPointMultiply;
            return instr;
        }
        case 0x1C:
            // MADD.S
            Errors::print_warning("[EE_JIT] Unrecognized fpu op MADD.S\n", op);
            fallback_interpreter(instr, opcode);
            return instr;
        case 0x1D:
            // MSUB.S
            Errors::print_warning("[EE_JIT] Unrecognized fpu op MSUB.S\n", op);
            fallback_interpreter(instr, opcode);
            return instr;
        case 0x1E:
            // MADDA.S
            Errors::print_warning("[EE_JIT] Unrecognized fpu op MADDA.S\n", op);
            fallback_interpreter(instr, opcode);
            return instr;
        case 0x1F:
            // MSUBA.S
            Errors::print_warning("[EE_JIT] Unrecognized fpu op MSUBA.S\n", op);
            fallback_interpreter(instr, opcode);
            return instr;
        case 0x24:
            // CVT.W.S
            Errors::print_warning("[EE_JIT] Unrecognized fpu op CVT.W.S\n", op);
            fallback_interpreter(instr, opcode);
            return instr;
        case 0x28:
            // MAX.S
        {
            uint8_t dest = (opcode >> 6) & 0x1F;
            uint8_t source = (opcode >> 11) & 0x1F;
            uint8_t source2 = (opcode >> 16) & 0x1F;
            instr.op = IR::Opcode::FloatingPointMaximum;
            instr.set_dest(dest);
            instr.set_source(source);
            instr.set_source2(source2);
            return instr;
        }
        case 0x29:
            // MIN.S
        {
            uint8_t dest = (opcode >> 6) & 0x1F;
            uint8_t source = (opcode >> 11) & 0x1F;
            uint8_t source2 = (opcode >> 16) & 0x1F;
            instr.op = IR::Opcode::FloatingPointMinimum;
            instr.set_dest(dest);
            instr.set_source(source);
            instr.set_source2(source2);
            return instr;
        }
        case 0x30:
            // C.F.S
            Errors::print_warning("[EE_JIT] Unrecognized fpu op C.F.S\n", op);
            fallback_interpreter(instr, opcode);
            return instr;
        case 0x32:
            // C.EQ.S
            Errors::print_warning("[EE_JIT] Unrecognized fpu op C.EQ.S\n", op);
            fallback_interpreter(instr, opcode);
            return instr;
        case 0x34:
            // C.LT.S
            Errors::print_warning("[EE_JIT] Unrecognized fpu op C.LT.S\n", op);
            fallback_interpreter(instr, opcode);
            return instr;
        case 0x36:
            // C.LE.S
            Errors::print_warning("[EE_JIT] Unrecognized fpu op C.LE.S\n", op);
            fallback_interpreter(instr, opcode);
            return instr;
        default:
            Errors::die("[EE_JIT] Unrecognized fpu op $%02X", op);
            return instr;
    }
}

IR::Instruction EE_JitTranslator::translate_op_cop2(uint32_t opcode, uint32_t PC) const
{
    uint8_t op = (opcode >> 21) & 0x1F;
    IR::Instruction instr;

    switch (op)
    {
        case 0x01:
            // QMFC2
            Errors::print_warning("[EE_JIT] Unrecognized cop2 op QMFC2\n", op);
            fallback_interpreter(instr, opcode);
            return instr;
        case 0x02:
            // CFC2
            Errors::print_warning("[EE_JIT] Unrecognized cop2 op CFC2\n", op);
            fallback_interpreter(instr, opcode);
            return instr;
        case 0x05:
            // QMTC2
            Errors::print_warning("[EE_JIT] Unrecognized cop2 op QMTC2\n", op);
            fallback_interpreter(instr, opcode);
            return instr;
        case 0x06:
            // CTC2
            Errors::print_warning("[EE_JIT] Unrecognized cop2 op CTC2\n", op);
            fallback_interpreter(instr, opcode);
            return instr;
        case 0x08:
            // BC2
            instr.op = IR::Opcode::BranchCop2;
            instr.set_is_likely((opcode >> 17) & 1);
            instr.set_field((opcode >> 16) & 1);
            instr.set_jump_dest(branch_offset_ee(opcode, PC));
            instr.set_jump_fail_dest(PC + 8);
            return instr;
        case 0x10:
        case 0x11:
        case 0x12:
        case 0x13:
        case 0x14:
        case 0x15:
        case 0x16:
        case 0x17:
        case 0x18:
        case 0x19:
        case 0x1A:
        case 0x1B:
        case 0x1C:
        case 0x1D:
        case 0x1E:
        case 0x1F:
            // COP2 Special Operation
            return translate_op_cop2_special(opcode, PC);
        default:
            Errors::die("[EE_JIT] Unrecognized cop2 op $%02X", op);
            return instr;
    }
}

IR::Instruction EE_JitTranslator::translate_op_cop2_special(uint32_t opcode, uint32_t PC) const
{
    uint8_t op = opcode & 0x3F;
    IR::Instruction instr;

    switch (op)
    {
        case 0x00:
        case 0x01:
        case 0x02:
        case 0x03:
            // VADDBC
            Errors::print_warning("[EE_JIT] Unrecognized cop2 special op VADDBC\n", op);
            fallback_interpreter(instr, opcode);
            return instr;
        case 0x04:
        case 0x05:
        case 0x06:
        case 0x07:
            // VSUBBC
            Errors::print_warning("[EE_JIT] Unrecognized cop2 special op VSUBBC\n", op);
            fallback_interpreter(instr, opcode);
            return instr;
        case 0x08:
        case 0x09:
        case 0x0A:
        case 0x0B:
            // VMADDBC
            Errors::print_warning("[EE_JIT] Unrecognized cop2 special op VMADDBC\n", op);
            fallback_interpreter(instr, opcode);
            return instr;
        case 0x0C:
        case 0x0D:
        case 0x0E:
        case 0x0F:
            // VMSUBBC
            Errors::print_warning("[EE_JIT] Unrecognized cop2 special op VMSUBBC\n", op);
            fallback_interpreter(instr, opcode);
            return instr;
        case 0x10:
        case 0x11:
        case 0x12:
        case 0x13:
            // VMAXBC
            Errors::print_warning("[EE_JIT] Unrecognized cop2 special op VMAXBC\n", op);
            fallback_interpreter(instr, opcode);
            return instr;
        case 0x14:
        case 0x15:
        case 0x16:
        case 0x17:
            // VMINIBC
            Errors::print_warning("[EE_JIT] Unrecognized cop2 special op VMINIBC\n", op);
            fallback_interpreter(instr, opcode);
            return instr;
        case 0x18:
        case 0x19:
        case 0x1A:
        case 0x1B:
            // VMULBC
            Errors::print_warning("[EE_JIT] Unrecognized cop2 special op VMULBC\n", op);
            fallback_interpreter(instr, opcode);
            return instr;
        case 0x1C:
            // VMULQ
            Errors::print_warning("[EE_JIT] Unrecognized cop2 special op VMULQ\n", op);
            fallback_interpreter(instr, opcode);
            return instr;
        case 0x1D:
            // VMAXI
            Errors::print_warning("[EE_JIT] Unrecognized cop2 special op VMAXI\n", op);
            fallback_interpreter(instr, opcode);
            return instr;
        case 0x1E:
            // VMULI
            Errors::print_warning("[EE_JIT] Unrecognized cop2 special op VMAXI\n", op);
            fallback_interpreter(instr, opcode);
            return instr;
        case 0x1F:
            // VMINII
            Errors::print_warning("[EE_JIT] Unrecognized cop2 special op VMINII\n", op);
            fallback_interpreter(instr, opcode);
            return instr;
        case 0x20:
            // VADDQ
            Errors::print_warning("[EE_JIT] Unrecognized cop2 special op VADDQ\n", op);
            fallback_interpreter(instr, opcode);
            return instr;
        case 0x21:
            // VMADDQ
            Errors::print_warning("[EE_JIT] Unrecognized cop2 special op VMADDQ\n", op);
            fallback_interpreter(instr, opcode);
            return instr;
        case 0x22:
            // VADDI
            Errors::print_warning("[EE_JIT] Unrecognized cop2 special op VADDI\n", op);
            fallback_interpreter(instr, opcode);
            return instr;
        case 0x23:
            // VMADDI
            Errors::print_warning("[EE_JIT] Unrecognized cop2 special op VMADDI\n", op);
            fallback_interpreter(instr, opcode);
            return instr;
        case 0x24:
            // VSUBQ
            Errors::print_warning("[EE_JIT] Unrecognized cop2 special op VSUBQ\n", op);
            fallback_interpreter(instr, opcode);
            return instr;
        case 0x25:
            // VMSUBQ
            Errors::print_warning("[EE_JIT] Unrecognized cop2 special op VMSUBQ\n", op);
            fallback_interpreter(instr, opcode);
            return instr;
        case 0x26:
            // VSUBI
            Errors::print_warning("[EE_JIT] Unrecognized cop2 special op VSUBI\n", op);
            fallback_interpreter(instr, opcode);
            return instr;
        case 0x27:
            // VMSUBI
            Errors::print_warning("[EE_JIT] Unrecognized cop2 special op VMSUBI\n", op);
            fallback_interpreter(instr, opcode);
            return instr;
        case 0x28:
            // VADD
            Errors::print_warning("[EE_JIT] Unrecognized cop2 special op VADD\n", op);
            fallback_interpreter(instr, opcode);
            return instr;
        case 0x29:
            // VMADD
            Errors::print_warning("[EE_JIT] Unrecognized cop2 special op VMADDI\n", op);
            fallback_interpreter(instr, opcode);
            return instr;
        case 0x2A:
            // VMUL
            Errors::print_warning("[EE_JIT] Unrecognized cop2 special op VMUL\n", op);
            fallback_interpreter(instr, opcode);
            return instr;
        case 0x2B:
            // VMAX
            Errors::print_warning("[EE_JIT] Unrecognized cop2 special op VMAX\n", op);
            fallback_interpreter(instr, opcode);
            return instr;
        case 0x2C:
            // VSUB
            Errors::print_warning("[EE_JIT] Unrecognized cop2 special op VSUB\n", op);
            fallback_interpreter(instr, opcode);
            return instr;
        case 0x2D:
            // VMSUB
            Errors::print_warning("[EE_JIT] Unrecognized cop2 special op VMADDI\n", op);
            fallback_interpreter(instr, opcode);
            return instr;
        case 0x2E:
            // VOPMSUB
            Errors::print_warning("[EE_JIT] Unrecognized cop2 special op VOPMSUB\n", op);
            fallback_interpreter(instr, opcode);
            return instr;
        case 0x2F:
            // VMINI
            Errors::print_warning("[EE_JIT] Unrecognized cop2 special op VMINI\n", op);
            fallback_interpreter(instr, opcode);
            return instr;
        case 0x30:
            // VIADD
            Errors::print_warning("[EE_JIT] Unrecognized cop2 special op VIADD\n", op);
            fallback_interpreter(instr, opcode);
            return instr;
        case 0x31:
            // VISUB
            Errors::print_warning("[EE_JIT] Unrecognized cop2 special op VISUB\n", op);
            fallback_interpreter(instr, opcode);
            return instr;
        case 0x32:
            // VIADDI
            Errors::print_warning("[EE_JIT] Unrecognized cop2 special op VIADDI\n", op);
            fallback_interpreter(instr, opcode);
            return instr;
        case 0x34:
            // VIAND
            Errors::print_warning("[EE_JIT] Unrecognized cop2 special op VIAND\n", op);
            fallback_interpreter(instr, opcode);
            return instr;
        case 0x35:
            // VIOR
            Errors::print_warning("[EE_JIT] Unrecognized cop2 special op VIOR\n", op);
            fallback_interpreter(instr, opcode);
            return instr;
        case 0x38:
            // VCALLMS
        {
            instr.op = IR::Opcode::VCallMS;
            uint32_t imm = (opcode >> 6) & 0x7FFF;
            imm *= 8;
            instr.set_source(imm);
            instr.set_return_addr(PC);
            return instr;
        }
        case 0x39:
            // VCALLMSR
        {
            instr.op = IR::Opcode::VCallMSR;
            instr.set_return_addr(PC);
            return instr;
        }
        case 0x3C:
        case 0x3D:
        case 0x3E:
        case 0x3F:
            // COP2 Special2 Operation
            return translate_op_cop2_special2(opcode, PC);
        default:
            Errors::die("[EE_JIT] Unrecognized cop2 special op $%02X", op);
            return instr;
    }
}

IR::Instruction EE_JitTranslator::translate_op_cop2_special2(uint32_t opcode, uint32_t PC) const
{
    uint8_t op = opcode & 0x3F;
    IR::Instruction instr;

    switch (op)
    {
        case 0x00:
        case 0x01:
        case 0x02:
        case 0x03:
            // VADDABC
            Errors::print_warning("[EE_JIT] Unrecognized cop2 special2 op VADDABC\n", op);
            fallback_interpreter(instr, opcode);
            return instr;
        case 0x04:
        case 0x05:
        case 0x06:
        case 0x07:
            // VSUBABC
            Errors::print_warning("[EE_JIT] Unrecognized cop2 special2 op VSUBABC\n", op);
            fallback_interpreter(instr, opcode);
            return instr;
        case 0x08:
        case 0x09:
        case 0x0A:
        case 0x0B:
            // VMADDABC
            Errors::print_warning("[EE_JIT] Unrecognized cop2 special2 op VMADDABC\n", op);
            fallback_interpreter(instr, opcode);
            return instr;
        case 0x0C:
        case 0x0D:
        case 0x0E:
        case 0x0F:
            // VMSUBABC
            Errors::print_warning("[EE_JIT] Unrecognized cop2 special2 op VMSUBABC\n", op);
            fallback_interpreter(instr, opcode);
            return instr;
        case 0x10:
            // VITOF0
            Errors::print_warning("[EE_JIT] Unrecognized cop2 special2 op VITOF0\n", op);
            fallback_interpreter(instr, opcode);
            return instr;
        case 0x11:
            // VITOF4
            Errors::print_warning("[EE_JIT] Unrecognized cop2 special2 op VITOF4\n", op);
            fallback_interpreter(instr, opcode);
            return instr;
        case 0x12:
            // VITOF12
            Errors::print_warning("[EE_JIT] Unrecognized cop2 special2 op VITOF12\n", op);
            fallback_interpreter(instr, opcode);
            return instr;
        case 0x13:
            // VITOF15
            Errors::print_warning("[EE_JIT] Unrecognized cop2 special2 op VITOF15\n", op);
            fallback_interpreter(instr, opcode);
            return instr;
        case 0x14:
            // VFTOI0
            Errors::print_warning("[EE_JIT] Unrecognized cop2 special2 op VFTOI0\n", op);
            fallback_interpreter(instr, opcode);
            return instr;
        case 0x15:
            // VFTOI4
            Errors::print_warning("[EE_JIT] Unrecognized cop2 special2 op VFTOI4\n", op);
            fallback_interpreter(instr, opcode);
            return instr;
        case 0x16:
            // VFTOI12
            Errors::print_warning("[EE_JIT] Unrecognized cop2 special2 op VFTOI12\n", op);
            fallback_interpreter(instr, opcode);
            return instr;
        case 0x17:
            // VFTOI15
            Errors::print_warning("[EE_JIT] Unrecognized cop2 special2 op VFTOI15\n", op);
            fallback_interpreter(instr, opcode);
            return instr;
        case 0x18:
        case 0x19:
        case 0x1A:
        case 0x1B:
            // VMULABC
            Errors::print_warning("[EE_JIT] Unrecognized cop2 special2 op VMULABC\n", op);
            fallback_interpreter(instr, opcode);
            return instr;
        case 0x1C:
            // VMULAQ
            Errors::print_warning("[EE_JIT] Unrecognized cop2 special2 op VMULAQ\n", op);
            fallback_interpreter(instr, opcode);
            return instr;
        case 0x1D:
            // VABS
            Errors::print_warning("[EE_JIT] Unrecognized cop2 special2 op VABS\n", op);
            fallback_interpreter(instr, opcode);
            return instr;
        case 0x1E:
            // VMULAI
            Errors::print_warning("[EE_JIT] Unrecognized cop2 special2 op VMULAI\n", op);
            fallback_interpreter(instr, opcode);
            return instr;
        case 0x1F:
            // VCLIP
            Errors::print_warning("[EE_JIT] Unrecognized cop2 special2 op VCLIP\n", op);
            fallback_interpreter(instr, opcode);
            return instr;
        case 0x20:
            // VADDAQ
            Errors::print_warning("[EE_JIT] Unrecognized cop2 special2 op VADDAQ\n", op);
            fallback_interpreter(instr, opcode);
            return instr;
        case 0x21:
            // VMADDAQ
            Errors::print_warning("[EE_JIT] Unrecognized cop2 special2 op VMADDAQ\n", op);
            fallback_interpreter(instr, opcode);
            return instr;
        case 0x23:
            // VMADDAI
            Errors::print_warning("[EE_JIT] Unrecognized cop2 special2 op VMADDAI\n", op);
            fallback_interpreter(instr, opcode);
            return instr;
        case 0x25:
            // VMSUBAQ
            Errors::print_warning("[EE_JIT] Unrecognized cop2 special2 op VMSUBAQ\n", op);
            fallback_interpreter(instr, opcode);
            return instr;
        case 0x27:
            // VMSUBAI
            Errors::print_warning("[EE_JIT] Unrecognized cop2 special2 op VMSUBAI\n", op);
            fallback_interpreter(instr, opcode);
            return instr;
        case 0x28:
            // VADDA
            Errors::print_warning("[EE_JIT] Unrecognized cop2 special2 op VADDA\n", op);
            fallback_interpreter(instr, opcode);
            return instr;
        case 0x29:
            // VMADDA
            Errors::print_warning("[EE_JIT] Unrecognized cop2 special2 op VMADDA\n", op);
            fallback_interpreter(instr, opcode);
            return instr;
        case 0x2A:
            // VMULA
            Errors::print_warning("[EE_JIT] Unrecognized cop2 special2 op VMULA\n", op);
            fallback_interpreter(instr, opcode);
            return instr;
        case 0x2C:
            // VSUBA
            Errors::print_warning("[EE_JIT] Unrecognized cop2 special2 op VSUBA\n", op);
            fallback_interpreter(instr, opcode);
            return instr;
        case 0x2D:
            // VMSUBA
            Errors::print_warning("[EE_JIT] Unrecognized cop2 special2 op VMSUBA\n", op);
            fallback_interpreter(instr, opcode);
            return instr;
        case 0x2E:
            // VOPMULA
            Errors::print_warning("[EE_JIT] Unrecognized cop2 special2 op VOPMULA\n", op);
            fallback_interpreter(instr, opcode);
            return instr;
        case 0x2F:
            // VNOP ?
            Errors::print_warning("[EE_JIT] Unrecognized cop2 special2 op VNOP (?)\n", op);
            fallback_interpreter(instr, opcode);
            return instr;
        case 0x30:
            // VMOVE
            Errors::print_warning("[EE_JIT] Unrecognized cop2 special2 op VMOVE\n", op);
            fallback_interpreter(instr, opcode);
            return instr;
        case 0x31:
            // VMR32
            Errors::print_warning("[EE_JIT] Unrecognized cop2 special2 op VMR32\n", op);
            fallback_interpreter(instr, opcode);
            return instr;
        case 0x34:
            // VLQI
            Errors::print_warning("[EE_JIT] Unrecognized cop2 special2 op VLQI\n", op);
            fallback_interpreter(instr, opcode);
            return instr;
        case 0x35:
            // VSQI
            Errors::print_warning("[EE_JIT] Unrecognized cop2 special2 op VSQI\n", op);
            fallback_interpreter(instr, opcode);
            return instr;
        case 0x36:
            // VLQD
            Errors::print_warning("[EE_JIT] Unrecognized cop2 special2 op VLQD\n", op);
            fallback_interpreter(instr, opcode);
            return instr;
        case 0x37:
            // VSQD
            Errors::print_warning("[EE_JIT] Unrecognized cop2 special2 op VSQD\n", op);
            fallback_interpreter(instr, opcode);
            return instr;
        case 0x38:
            // VDIV
            Errors::print_warning("[EE_JIT] Unrecognized cop2 special2 op VDIV\n", op);
            fallback_interpreter(instr, opcode);
            return instr;
        case 0x39:
            // VSQRT
            Errors::print_warning("[EE_JIT] Unrecognized cop2 special2 op VSQRT\n", op);
            fallback_interpreter(instr, opcode);
            return instr;
        case 0x3A:
            // VRSQRT
            Errors::print_warning("[EE_JIT] Unrecognized cop2 special2 op VRSQRT\n", op);
            fallback_interpreter(instr, opcode);
            return instr;
        case 0x3B:
            // VWAITQ
            Errors::print_warning("[EE_JIT] Unrecognized cop2 special2 op VWAITQ\n", op);
            fallback_interpreter(instr, opcode);
            return instr;
        case 0x3C:
            // VMTIR
            Errors::print_warning("[EE_JIT] Unrecognized cop2 special2 op VMTIR\n", op);
            fallback_interpreter(instr, opcode);
            return instr;
        case 0x3D:
            // VMFIR
            Errors::print_warning("[EE_JIT] Unrecognized cop2 special2 op VMFIR\n", op);
            fallback_interpreter(instr, opcode);
            return instr;
        case 0x3E:
            // VILWR
            Errors::print_warning("[EE_JIT] Unrecognized cop2 special2 op VILWR\n", op);
            fallback_interpreter(instr, opcode);
            return instr;
        case 0x3F:
            // VISWR
            Errors::print_warning("[EE_JIT] Unrecognized cop2 special2 op VISWR\n", op);
            fallback_interpreter(instr, opcode);
            return instr;
        case 0x40:
            // VRNEXT
            Errors::print_warning("[EE_JIT] Unrecognized cop2 special2 op VRNEXT\n", op);
            fallback_interpreter(instr, opcode);
            return instr;
        case 0x41:
            // VRGET
            Errors::print_warning("[EE_JIT] Unrecognized cop2 special2 op VRGET\n", op);
            fallback_interpreter(instr, opcode);
            return instr;
        case 0x42:
            // VRINIT
            Errors::print_warning("[EE_JIT] Unrecognized cop2 special2 op VRINIT\n", op);
            fallback_interpreter(instr, opcode);
            return instr;
        case 0x43:
            // VRXOR
            Errors::print_warning("[EE_JIT] Unrecognized cop2 special2 op VRXOR\n", op);
            fallback_interpreter(instr, opcode);
            return instr;
        default:
            Errors::die("[EE_JIT] Unrecognized cop2 special2 op $%02X", op);
            return instr;
    }
}

// TENTATIVE
bool EE_JitTranslator::is_branch(const IR::Instruction& instr) const noexcept
{
    return instr.op == IR::Opcode::Jump ||
        instr.op == IR::Opcode::JumpIndirect ||
        instr.op == IR::Opcode::BranchCop0 ||
        instr.op == IR::Opcode::BranchCop1 ||
        instr.op == IR::Opcode::BranchCop2 ||
        instr.op == IR::Opcode::BranchEqual ||
        instr.op == IR::Opcode::BranchEqualZero ||
        instr.op == IR::Opcode::BranchGreaterThanOrEqualZero ||
        instr.op == IR::Opcode::BranchGreaterThanZero ||
        instr.op == IR::Opcode::BranchLessThanOrEqualZero ||
        instr.op == IR::Opcode::BranchLessThanZero ||
        instr.op == IR::Opcode::BranchNotEqual ||
        instr.op == IR::Opcode::BranchNotEqualZero;
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