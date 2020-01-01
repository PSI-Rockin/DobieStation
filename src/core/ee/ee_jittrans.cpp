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

    di_delay = 0;

    branch_op = false;
    branch_delayslot = false;
    eret_op = false;
    cycle_count = 0;
    int ops_translated = 0;

    while (!branch_delayslot && !eret_op)
    {
        uint32_t opcode = ee.read32(pc);
        translate_op(opcode, pc, instrs);

        ops_translated++;

        if (branch_op)
            branch_delayslot = true;

        //The EE kernel has a bug where it tries to disable interrupts, but due to pipelining this doesn't happen.
        //The code in the kernel looks like this:
        //* di
        //* mfc0 k0, Status
        //* ori k0, 0x13
        //* mtc0 k0, Status
        //* sync
        //* eret
        //DI is non-atomic, so MFC0 will read the *old* value of the Status register.
        //The DI completes before MTC0 executes, so MTC0 will overwrite the old value of EIE.
        //Some games (Namco 50th Anniversary, Jak X) check if interrupts are enabled during bootup and error
        //out if they are not, so emulating this is required.
        if (di_delay)
        {
            di_delay--;
            if (!di_delay)
            {
                IR::Instruction instr;
                uint32_t opcode = 0x42000039;
                fallback_interpreter(instr, opcode);
                instrs.push_back(instr);
            }
        }

        if (instrs.size())
        {
            branch_op = instrs.back().is_jump();

            //The EE has a bug in its pipelining logic that causes branches to be skipped in certain conditions.
            //They are as follows:
            // * The branch points to the start of a loop - forward branches are not affected
            // * The branch is conditional - unconditional branches, jumps, and calls are not affected
            // * No other branch/jump instructions are present in the loop
            // * Less than six instructions are present in the loop, including delay slot
            // * On EE revision #2.9 and later, the delay slot is not NOP
            // When all conditions are met, the branch is treated as if it were a NOP.

            //Note that the branch delay slot has not been translated yet, so we use 5 instead of 6
            if (branch_op && ops_translated < 5)
            {
                if (instrs.back().op != IR::Opcode::Jump && instrs.back().op != IR::Opcode::JumpIndirect)
                {
                    if (instrs.back().get_jump_dest() == ee.get_PC() && ee.read32(pc + 4) != 0)
                    {
                        //Set branch dest to instruction after delay slot
                        instrs.back().set_jump_dest(pc + 8);
                    }
                }
            }
        }

        pc += 4;
        cycle_count++;
    }

    for (auto instr : instrs)
        if (instr.op != IR::Opcode::Null)
            block.add_instr(instr);

    block.set_cycle_count(cycle_count);

    return block;
}

void EE_JitTranslator::translate_op(uint32_t opcode, uint32_t PC, std::vector<IR::Instruction>& instrs)
{
    uint8_t op = opcode >> 26;
    IR::Instruction instr;
    instr.set_opcode(opcode);

    switch (op)
    {
        case 0x00:
            // Special Operation
            translate_op_special(opcode, PC, instrs);
            break;
        case 0x01:
            // Regimm Operation
            translate_op_regimm(opcode, PC, instrs);
            break;
        case 0x02:
            // J
            instr.op = IR::Opcode::Jump;
            instr.set_jump_dest(jump_offset_ee(opcode, PC));
            instr.set_is_link(false);
            instrs.push_back(instr);
            break;
        case 0x03:
            // JAL
            instr.op = IR::Opcode::Jump;
            instr.set_jump_dest(jump_offset_ee(opcode, PC));
            instr.set_return_addr(PC + 8);
            instr.set_is_link(true);
            instrs.push_back(instr);
            break;
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
                instr.set_is_link(false);
                instrs.push_back(instr);
                break;
            }
            if (!source)
            {
                // BEQZ
                instr.op = IR::Opcode::BranchEqualZero;
                instr.set_source(source2);
                instr.set_jump_dest(branch_offset_ee(opcode, PC));
                instr.set_jump_fail_dest(PC + 8);
                instr.set_is_likely(false);
                instrs.push_back(instr);
                break;
            }
            if (!source2)
            {
                // BEQZ
                instr.op = IR::Opcode::BranchEqualZero;
                instr.set_source(source);
                instr.set_jump_dest(branch_offset_ee(opcode, PC));
                instr.set_jump_fail_dest(PC + 8);
                instr.set_is_likely(false);
                instrs.push_back(instr);
                break;
            }

            instr.op = IR::Opcode::BranchEqual;
            instr.set_source((opcode >> 21) & 0x1F);
            instr.set_source2((opcode >> 16) & 0x1F);
            instr.set_jump_dest(branch_offset_ee(opcode, PC));
            instr.set_jump_fail_dest(PC + 8);
            instr.set_is_likely(false);
            instrs.push_back(instr);
            break;
        }
        case 0x05:
            // BNE
        {
            uint8_t source = (opcode >> 21) & 0x1F;
            uint8_t source2 = (opcode >> 16) & 0x1F;
            if (source == source2)
            {
                // NOP
                break;
            }
            if (!source)
            {
                // BNEZ
                instr.op = IR::Opcode::BranchNotEqualZero;
                instr.set_source(source2);
                instr.set_jump_dest(branch_offset_ee(opcode, PC));
                instr.set_jump_fail_dest(PC + 8);
                instr.set_is_likely(false);
                instrs.push_back(instr);
                break;
            }
            if (!source2)
            {
                // BNEZ
                instr.op = IR::Opcode::BranchNotEqualZero;
                instr.set_source(source);
                instr.set_jump_dest(branch_offset_ee(opcode, PC));
                instr.set_jump_fail_dest(PC + 8);
                instr.set_is_likely(false);
                instrs.push_back(instr);
                break;
            }

            instr.op = IR::Opcode::BranchNotEqual;
            instr.set_source((opcode >> 21) & 0x1F);
            instr.set_source2((opcode >> 16) & 0x1F);
            instr.set_jump_dest(branch_offset_ee(opcode, PC));
            instr.set_jump_fail_dest(PC + 8);
            instr.set_is_likely(false);
            instrs.push_back(instr);
            break;
        }
        case 0x06:
            // BLEZ
            instr.op = IR::Opcode::BranchLessThanOrEqualZero;
            instr.set_source((opcode >> 21) & 0x1F);
            instr.set_jump_dest(branch_offset_ee(opcode, PC));
            instr.set_jump_fail_dest(PC + 8);
            instr.set_is_likely(false);
            instrs.push_back(instr);
            break;
        case 0x07:
            // BGTZ
            instr.op = IR::Opcode::BranchGreaterThanZero;
            instr.set_source((opcode >> 21) & 0x1F);
            instr.set_jump_dest(branch_offset_ee(opcode, PC));
            instr.set_jump_fail_dest(PC + 8);
            instr.set_is_likely(false);
            instrs.push_back(instr);
            break;
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
                // NOP
                break;
            }
            if (!source)
            {
                instr.op = IR::Opcode::LoadConst;
                instr.set_dest(dest);
                instr.set_source(immediate);
                instrs.push_back(instr);
                break;
            }
            instr.op = IR::Opcode::AddWordImm;
            instr.set_dest(dest);
            instr.set_source(source);
            instr.set_source2((int64_t)immediate);
            instrs.push_back(instr);
            break;
        }
        case 0x0A:
            // SLTI
        {
            uint8_t dest = (opcode >> 16) & 0x1F;
            if (!dest)
            {
                // NOP
                break;
            }
            int64_t imm = (int16_t)(opcode & 0xFFFF);
            instr.op = IR::Opcode::SetOnLessThanImmediate;
            instr.set_dest(dest);
            instr.set_source((opcode >> 21) & 0x1F);
            instr.set_source2(imm);
            instrs.push_back(instr);
            break;
        }
        case 0x0B:
            // SLTIU
        {
            uint8_t dest = (opcode >> 16) & 0x1F;
            if (!dest)
            {
                // NOP
                break;
            }
            int64_t imm = (int16_t)(opcode & 0xFFFF);
            instr.op = IR::Opcode::SetOnLessThanImmediateUnsigned;
            instr.set_dest(dest);
            instr.set_source((opcode >> 21) & 0x1F);
            instr.set_source2(imm);
            instrs.push_back(instr);
            break;
        }
        case 0x0C:
            // ANDI
        {
            uint8_t dest = (opcode >> 16) & 0x1F;
            if (!dest)
            {
                // NOP
                break;
            }
            instr.op = IR::Opcode::AndImm;
            instr.set_dest(dest);
            instr.set_source((opcode >> 21) & 0x1F);
            instr.set_source2(opcode & 0xFFFF);
            instrs.push_back(instr);
            break;
        }
        case 0x0D:
            // ORI
        {
            uint8_t dest = (opcode >> 16) & 0x1F;
            if (!dest)
            {
                // NOP
                break;
            }
            instr.op = IR::Opcode::OrImm;
            instr.set_dest((opcode >> 16) & 0x1F);
            instr.set_source((opcode >> 21) & 0x1F);
            instr.set_source2(opcode & 0xFFFF);
            instrs.push_back(instr);
            break;
        }
        case 0x0E:
            // XORI
        {
            uint8_t dest = (opcode >> 16) & 0x1F;
            if (!dest)
            {
                // NOP
                break;
            }
            instr.op = IR::Opcode::XorImm;
            instr.set_dest(dest);
            instr.set_source((opcode >> 21) & 0x1F);
            instr.set_source2(opcode & 0xFFFF);
            instrs.push_back(instr);
            break;
        }
        case 0x0F:
            // LUI
        {
            uint8_t dest = (opcode >> 16) & 0x1F;
            if (!dest)
            {
                // NOP
                break;
            }
            instr.op = IR::Opcode::LoadConst;
            instr.set_dest(dest);
            instr.set_source((int64_t)(int32_t)((opcode & 0xFFFF) << 16));
            instrs.push_back(instr);
            break;
        }
        case 0x10:
            // COP0 (System Coprocessor)
            translate_op_cop0(opcode, PC, instrs);
            break;
        case 0x11:
            // COP1 (Floating Point Unit)
            translate_op_cop1(opcode, PC, instrs);
            break;
        case 0x12:
            // COP2 (Vector Unit 0)
            translate_op_cop2(opcode, PC, instrs);
            break;
        case 0x13:
            // COP3 (unimplemented)
            Errors::die("[EE_JIT] Unrecognized cop3 opcode $%08X", opcode);
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
                instrs.push_back(instr);
                break;
            }
            if (!source2)
            {
                // BEQZL
                instr.op = IR::Opcode::BranchEqualZero;
                instr.set_source(source);
                instr.set_jump_dest(branch_offset_ee(opcode, PC));
                instr.set_jump_fail_dest(PC + 8);
                instr.set_is_likely(true);
                instrs.push_back(instr);
                break;
            }
            instr.op = IR::Opcode::BranchEqual;
            instr.set_source((opcode >> 21) & 0x1F);
            instr.set_source2((opcode >> 16) & 0x1F);
            instr.set_jump_dest(branch_offset_ee(opcode, PC));
            instr.set_jump_fail_dest(PC + 8);
            instr.set_is_likely(true);
            instrs.push_back(instr);
            break;
        }
        case 0x15:
            // BNEL
        {
            uint8_t source = (opcode >> 21) & 0x1F;
            uint8_t source2 = (opcode >> 16) & 0x1F;
            if (source == source2)
            {
                // NOP
                break;
            }
            if (!source)
            {
                // BNEZL
                instr.op = IR::Opcode::BranchNotEqualZero;
                instr.set_source(source2);
                instr.set_jump_dest(branch_offset_ee(opcode, PC));
                instr.set_jump_fail_dest(PC + 8);
                instr.set_is_likely(true);
                instrs.push_back(instr);
                break;
            }
            if (!source2)
            {
                // BNEZL
                instr.op = IR::Opcode::BranchNotEqualZero;
                instr.set_source(source);
                instr.set_jump_dest(branch_offset_ee(opcode, PC));
                instr.set_jump_fail_dest(PC + 8);
                instr.set_is_likely(true);
                instrs.push_back(instr);
                break;
            }
            instr.op = IR::Opcode::BranchNotEqual;
            instr.set_source((opcode >> 21) & 0x1F);
            instr.set_source2((opcode >> 16) & 0x1F);
            instr.set_jump_dest(branch_offset_ee(opcode, PC));
            instr.set_jump_fail_dest(PC + 8);
            instr.set_is_likely(true);
            instrs.push_back(instr);
            break;
        }
        case 0x16:
            // BLEZL
            instr.op = IR::Opcode::BranchLessThanOrEqualZero;
            instr.set_source((opcode >> 21) & 0x1F);
            instr.set_jump_dest(branch_offset_ee(opcode, PC));
            instr.set_jump_fail_dest(PC + 8);
            instr.set_is_likely(true);
            instrs.push_back(instr);
            break;
        case 0x17:
            // BGTZL
            instr.op = IR::Opcode::BranchGreaterThanZero;
            instr.set_source((opcode >> 21) & 0x1F);
            instr.set_jump_dest(branch_offset_ee(opcode, PC));
            instr.set_jump_fail_dest(PC + 8);
            instr.set_is_likely(true);
            instrs.push_back(instr);
            break;
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
                // NOP
                break;
            }
            if (!source)
            {
                instr.op = IR::Opcode::LoadConst;
                instr.set_dest(dest);
                instr.set_source((int64_t)immediate);
                instrs.push_back(instr);
                break;
            }
            instr.op = IR::Opcode::AddDoublewordImm;
            instr.set_dest(dest);
            instr.set_source(source);
            instr.set_source2((int64_t)immediate);
            instrs.push_back(instr);
            break;
        }
        case 0x1A:
            // LDL
        {
            uint8_t dest = (opcode >> 16) & 0x1F;
            if (!dest)
            {
                // NOP
                break;
            }
            instr.op = IR::Opcode::LoadDoublewordLeft;
            instr.set_dest(dest);
            instr.set_source((opcode >> 21) & 0x1F);
            instr.set_source2((int64_t)(int16_t)(opcode & 0xFFFF));
            instrs.push_back(instr);
            break;
        }
        case 0x1B:
            // LDR
        {
            uint8_t dest = (opcode >> 16) & 0x1F;
            if (!dest)
            {
                // NOP
                break;
            }
            instr.op = IR::Opcode::LoadDoublewordRight;
            instr.set_dest(dest);
            instr.set_source((opcode >> 21) & 0x1F);
            instr.set_source2((int64_t)(int16_t)(opcode & 0xFFFF));
            instrs.push_back(instr);
            break;
        }
        case 0x1C:
            // MMI Operation
            translate_op_mmi(opcode, PC, instrs);
            break;
        case 0x1E:
            // LQ
        {
            uint8_t dest = (opcode >> 16) & 0x1F;
            if (!dest)
            {
                // NOP
                break;
            }
            instr.op = IR::Opcode::LoadQuadword;
            instr.set_dest(dest);
            instr.set_source((opcode >> 21) & 0x1F);
            instr.set_source2((int64_t)(int16_t)(opcode & 0xFFFF));
            instrs.push_back(instr);
            break;
        }
        case 0x1F:
            // SQ
        {
            instr.op = IR::Opcode::StoreQuadword;
            instr.set_dest((opcode >> 21) & 0x1F);
            instr.set_source((opcode >> 16) & 0x1F);
            instr.set_source2((int64_t)(int16_t)(opcode & 0xFFFF));
            instrs.push_back(instr);
            break;
        }
        case 0x20:
            // LB
        {
            uint8_t dest = (opcode >> 16) & 0x1F;
            if (!dest)
            {
                // NOP
                break;
            }
            instr.op = IR::Opcode::LoadByte;
            instr.set_dest(dest);
            instr.set_source((opcode >> 21) & 0x1F);
            instr.set_source2((int64_t)(int16_t)(opcode & 0xFFFF));
            instrs.push_back(instr);
            break;
        }
        case 0x21:
            // LH
        {
            uint8_t dest = (opcode >> 16) & 0x1F;
            if (!dest)
            {
                // NOP
                break;
            }
            instr.op = IR::Opcode::LoadHalfword;
            instr.set_dest(dest);
            instr.set_source((opcode >> 21) & 0x1F);
            instr.set_source2((int64_t)(int16_t)(opcode & 0xFFFF));
            instrs.push_back(instr);
            break;
        }
        case 0x22:
            // LWL
        {
            uint8_t dest = (opcode >> 16) & 0x1F;
            if (!dest)
            {
                // NOP
                break;
            }
            instr.op = IR::Opcode::LoadWordLeft;
            instr.set_dest(dest);
            instr.set_source((opcode >> 21) & 0x1F);
            instr.set_source2((int64_t)(int16_t)(opcode & 0xFFFF));
            instrs.push_back(instr);
            break;
        }
        case 0x23:
            // LW
        {
            uint8_t dest = (opcode >> 16) & 0x1F;
            if (!dest)
            {
                // NOP
                break;
            }
            instr.op = IR::Opcode::LoadWord;
            instr.set_dest(dest);
            instr.set_source((opcode >> 21) & 0x1F);
            instr.set_source2((int64_t)(int16_t)(opcode & 0xFFFF));
            instrs.push_back(instr);
            break;
        }
        case 0x24:
            // LBU
        {
            uint8_t dest = (opcode >> 16) & 0x1F;
            if (!dest)
            {
                // NOP
                break;
            }
            instr.op = IR::Opcode::LoadByteUnsigned;
            instr.set_dest(dest);
            instr.set_source((opcode >> 21) & 0x1F);
            instr.set_source2((int64_t)(int16_t)(opcode & 0xFFFF));
            instrs.push_back(instr);
            break;
        }
        case 0x25:
            // LHU
        {
            uint8_t dest = (opcode >> 16) & 0x1F;
            if (!dest)
            {
                // NOP
                break;
            }
            instr.op = IR::Opcode::LoadHalfwordUnsigned;
            instr.set_dest(dest);
            instr.set_source((opcode >> 21) & 0x1F);
            instr.set_source2((int64_t)(int16_t)(opcode & 0xFFFF));
            instrs.push_back(instr);
            break;
        }
        case 0x26:
            // LWR
        {
            uint8_t dest = (opcode >> 16) & 0x1F;
            if (!dest)
            {
                // NOP
                break;
            }
            instr.op = IR::Opcode::LoadWordRight;
            instr.set_dest(dest);
            instr.set_source((opcode >> 21) & 0x1F);
            instr.set_source2((int64_t)(int16_t)(opcode & 0xFFFF));
            instrs.push_back(instr);
            break;
        }
        case 0x27:
            // LWU
        {
            uint8_t dest = (opcode >> 16) & 0x1F;
            if (!dest)
            {
                // NOP
                break;
            }
            instr.op = IR::Opcode::LoadWordUnsigned;
            instr.set_dest(dest);
            instr.set_source((opcode >> 21) & 0x1F);
            instr.set_source2((int64_t)(int16_t)(opcode & 0xFFFF));
            instrs.push_back(instr);
            break;
        }
        case 0x28:
            // SB
        {
            instr.op = IR::Opcode::StoreByte;
            instr.set_dest((opcode >> 21) & 0x1F);
            instr.set_source((opcode >> 16) & 0x1F);
            instr.set_source2((int64_t)(int16_t)(opcode & 0xFFFF));
            instrs.push_back(instr);
            break;
        }
        case 0x29:
            // SH
        {
            instr.op = IR::Opcode::StoreHalfword;
            instr.set_dest((opcode >> 21) & 0x1F);
            instr.set_source((opcode >> 16) & 0x1F);
            instr.set_source2((int64_t)(int16_t)(opcode & 0xFFFF));
            instrs.push_back(instr);
            break;
        }
        case 0x2A:
            // SWL
        {
            instr.op = IR::Opcode::StoreWordLeft;
            instr.set_dest((opcode >> 21) & 0x1F);
            instr.set_source((opcode >> 16) & 0x1F);
            instr.set_source2((int64_t)(int16_t)(opcode & 0xFFFF));
            instrs.push_back(instr);
            break;
        }
        case 0x2B:
            // SW
        {
            instr.op = IR::Opcode::StoreWord;
            instr.set_dest((opcode >> 21) & 0x1F);
            instr.set_source((opcode >> 16) & 0x1F);
            instr.set_source2((int64_t)(int16_t)(opcode & 0xFFFF));
            instrs.push_back(instr);
            break;
        }
        case 0x2C:
            // SDL
        {
            instr.op = IR::Opcode::StoreDoublewordLeft;
            instr.set_dest((opcode >> 21) & 0x1F);
            instr.set_source((opcode >> 16) & 0x1F);
            instr.set_source2((int64_t)(int16_t)(opcode & 0xFFFF));
            instrs.push_back(instr);
            break;
        }
        case 0x2D:
            // SDR
        {
            instr.op = IR::Opcode::StoreDoublewordRight;
            instr.set_dest((opcode >> 21) & 0x1F);
            instr.set_source((opcode >> 16) & 0x1F);
            instr.set_source2((int64_t)(int16_t)(opcode & 0xFFFF));
            instrs.push_back(instr);
            break;
        }
        case 0x2E:
            // SWR
        {
            instr.op = IR::Opcode::StoreWordRight;
            instr.set_dest((opcode >> 21) & 0x1F);
            instr.set_source((opcode >> 16) & 0x1F);
            instr.set_source2((int64_t)(int16_t)(opcode & 0xFFFF));
            instrs.push_back(instr);
            break;
        }
        case 0x2F:
            // CACHE
            fallback_interpreter(instr, opcode);
            instrs.push_back(instr);
            break;
        case 0x31:
            // LWC1
        {
            instr.op = IR::Opcode::LoadWordCoprocessor1;
            instr.set_dest((opcode >> 16) & 0x1F);
            instr.set_source((opcode >> 21) & 0x1F);
            instr.set_source2((int64_t)(int16_t)(opcode & 0xFFFF));
            instrs.push_back(instr);
            break;
        }
        case 0x33:
            // PREFETCH
            fallback_interpreter(instr, opcode);
            instrs.push_back(instr);
            break;
        case 0x36:
            // LQC2
        {
            uint8_t dest = (opcode >> 16) & 0x1F;
            if (!dest)
            {
                // NOP
                break;
            }
            instr.op = IR::Opcode::LoadQuadwordCoprocessor2;
            instr.set_dest(dest);
            instr.set_source((opcode >> 21) & 0x1F);
            instr.set_source2((int64_t)(int16_t)(opcode & 0xFFFF));
            instrs.push_back(instr);
            break;
        }
        case 0x37:
            // LD
        {
            uint8_t dest = (opcode >> 16) & 0x1F;
            if (!dest)
            {
                // NOP
                break;
            }
            instr.op = IR::Opcode::LoadDoubleword;
            instr.set_dest(dest);
            instr.set_source((opcode >> 21) & 0x1F);
            instr.set_source2((int64_t)(int16_t)(opcode & 0xFFFF));
            instrs.push_back(instr);
            break;
        }
        case 0x39:
            // SWC1
        {
            instr.op = IR::Opcode::StoreWordCoprocessor1;
            instr.set_dest((opcode >> 21) & 0x1F);
            instr.set_source((opcode >> 16) & 0x1F);
            instr.set_source2((int64_t)(int16_t)(opcode & 0xFFFF));
            instrs.push_back(instr);
            break;
        }
        case 0x3E:
            // SQC2
        {
            instr.op = IR::Opcode::StoreQuadwordCoprocessor2;
            instr.set_dest((opcode >> 21) & 0x1F);
            instr.set_source((opcode >> 16) & 0x1F);
            instr.set_source2((int64_t)(int16_t)(opcode & 0xFFFF));
            instrs.push_back(instr);
            break;
        }
        case 0x3F:
            // SD
        {
            instr.op = IR::Opcode::StoreDoubleword;
            instr.set_dest((opcode >> 21) & 0x1F);
            instr.set_source((opcode >> 16) & 0x1F);
            instr.set_source2((int64_t)(int16_t)(opcode & 0xFFFF));
            instrs.push_back(instr);
            break;
        }
        default:
            Errors::die("[EE_JIT] Unrecognized op $%02X", op);
    }
}

void EE_JitTranslator::translate_op_special(uint32_t opcode, uint32_t PC, std::vector<IR::Instruction>& instrs)
{
    uint8_t op = opcode & 0x3F;
    IR::Instruction instr;
    instr.set_opcode(opcode);

    switch (op)
    {
        case 0x00:
            // SLL
        {
            uint8_t dest = (opcode >> 11) & 0x1F;
            if (!dest)
            {
                // NOP
                break;
            }
            instr.op = IR::Opcode::ShiftLeftLogical;
            instr.set_dest(dest);
            instr.set_source((opcode >> 16) & 0x1F);
            instr.set_source2((opcode >> 6) & 0x1F);
            instrs.push_back(instr);
            break;
        }
        case 0x02:
            // SRL
        {
            uint8_t dest = (opcode >> 11) & 0x1F;
            if (!dest)
            {
                // NOP
                break;
            }
            instr.op = IR::Opcode::ShiftRightLogical;
            instr.set_dest(dest);
            instr.set_source((opcode >> 16) & 0x1F);
            instr.set_source2((opcode >> 6) & 0x1F);
            instrs.push_back(instr);
            break;
        }
        case 0x03:
            // SRA
        {
            uint8_t dest = (opcode >> 11) & 0x1F;
            if (!dest)
            {
                // NOP
                break;
            }
            instr.op = IR::Opcode::ShiftRightArithmetic;
            instr.set_dest(dest);
            instr.set_source((opcode >> 16) & 0x1F);
            instr.set_source2((opcode >> 6) & 0x1F);
            instrs.push_back(instr);
            break;
        }
        case 0x04:
            // SLLV
        {
            uint8_t dest = (opcode >> 11) & 0x1F;
            if (!dest)
            {
                // NOP
                break;
            }
            instr.op = IR::Opcode::ShiftLeftLogicalVariable;
            instr.set_dest(dest);
            instr.set_source((opcode >> 16) & 0x1F);
            instr.set_source2((opcode >> 21) & 0x1F);
            instrs.push_back(instr);
            break;
        }
        case 0x06:
            // SRLV
        {
            uint8_t dest = (opcode >> 11) & 0x1F;
            if (!dest)
            {
                // NOP
                break;
            }
            instr.op = IR::Opcode::ShiftRightLogicalVariable;
            instr.set_dest(dest);
            instr.set_source((opcode >> 16) & 0x1F);
            instr.set_source2((opcode >> 21) & 0x1F);
            instrs.push_back(instr);
            break;
        }
        case 0x07:
            // SRAV
        {
            uint8_t dest = (opcode >> 11) & 0x1F;
            if (!dest)
            {
                // NOP
                break;
            }
            instr.op = IR::Opcode::ShiftRightArithmeticVariable;
            instr.set_dest(dest);
            instr.set_source((opcode >> 16) & 0x1F);
            instr.set_source2((opcode >> 21) & 0x1F);
            instrs.push_back(instr);
            break;
        }
        case 0x08:
            // JR
            instr.op = IR::Opcode::JumpIndirect;
            instr.set_source((opcode >> 21) & 0x1F);
            instr.set_is_link(false);
            instrs.push_back(instr);
            break;
        case 0x09:
            // JALR
            instr.op = IR::Opcode::JumpIndirect;
            instr.set_source((opcode >> 21) & 0x1F);
            instr.set_return_addr(PC + 8);
            instr.set_dest((opcode >> 11) & 0x1F);
            instr.set_is_link(true);
            instrs.push_back(instr);
            break;
        case 0x0A:
            // MOVZ
        {
            uint8_t dest = (opcode >> 11) & 0x1F;
            uint8_t source = (opcode >> 16) & 0x1F;
            uint8_t source2 = (opcode >> 21) & 0x1F;
            if (!dest)
            {
                // NOP
                break;
            }
            if (!source)
            {
                instr.op = IR::Opcode::MoveDoublewordReg;
                instr.set_dest(dest);
                instr.set_source(source2);
                instrs.push_back(instr);
                break;
            }
            instr.op = IR::Opcode::MoveConditionalOnZero;
            instr.set_dest(dest);
            instr.set_source(source);
            instr.set_source2(source2);
            instrs.push_back(instr);
            break;
        }
        case 0x0B:
            // MOVN
        {
            uint8_t dest = (opcode >> 11) & 0x1F;
            uint8_t source = (opcode >> 16) & 0x1F;
            uint8_t source2 = (opcode >> 21) & 0x1F;
            if (!dest || !source)
            {
                // NOP
                break;
            }
            instr.op = IR::Opcode::MoveConditionalOnNotZero;
            instr.set_dest(dest);
            instr.set_source(source);
            instr.set_source2(source2);
            instrs.push_back(instr);
            break;
        }
        case 0x0C:
            // SYSCALL
            instr.op = IR::Opcode::SystemCall;
            instr.set_return_addr(PC);
            eret_op = true;
            instrs.push_back(instr);
            break;
        case 0x0D:
            // BREAK
            fallback_interpreter(instr, opcode);
            instrs.push_back(instr);
            break;
        case 0x0F:
            // SYNC
            fallback_interpreter(instr, opcode);
            instrs.push_back(instr);
            break;
        case 0x10:
            // MFHI
        {
            uint8_t dest = (opcode >> 11) & 0x1F;
            instr.op = IR::Opcode::MoveDoublewordReg;
            instr.set_source((int)EE_SpecialReg::HI);
            instr.set_dest(dest);
            instrs.push_back(instr);
            break;
        }
        case 0x11:
            // MTHI
        {
            uint8_t source = (opcode >> 21) & 0x1F;
            instr.op = IR::Opcode::MoveDoublewordReg;
            instr.set_source(source);
            instr.set_dest((int)EE_SpecialReg::HI);
            instrs.push_back(instr);
            break;
        }
        case 0x12:
            // MFLO
        {
            uint8_t dest = (opcode >> 11) & 0x1F;
            instr.op = IR::Opcode::MoveDoublewordReg;
            instr.set_source((int)EE_SpecialReg::LO);
            instr.set_dest(dest);
            instrs.push_back(instr);
            break;
        }
        case 0x13:
            // MTLO
        {
            uint8_t source = (opcode >> 21) & 0x1F;
            instr.op = IR::Opcode::MoveDoublewordReg;
            instr.set_source(source);
            instr.set_dest((int)EE_SpecialReg::LO);
            instrs.push_back(instr);
            break;
        }
        case 0x14:
            // DSLLV
        {
            uint8_t dest = (opcode >> 11) & 0x1F;
            if (!dest)
            {
                // NOP
                break;
            }
            instr.op = IR::Opcode::DoublewordShiftLeftLogicalVariable;
            instr.set_dest(dest);
            instr.set_source((opcode >> 16) & 0x1F);
            instr.set_source2((opcode >> 21) & 0x1F);
            instrs.push_back(instr);
            break;
        }
        case 0x16:
            // DSRLV
        {
            uint8_t dest = (opcode >> 11) & 0x1F;
            if (!dest)
            {
                // NOP
                break;
            }
            instr.op = IR::Opcode::DoublewordShiftRightLogicalVariable;
            instr.set_dest(dest);
            instr.set_source((opcode >> 16) & 0x1F);
            instr.set_source2((opcode >> 21) & 0x1F);
            instrs.push_back(instr);
            break;
        }
        case 0x17:
            // DSRAV
        {
            uint8_t dest = (opcode >> 11) & 0x1F;
            if (!dest)
            {
                // NOP
                break;
            }
            instr.op = IR::Opcode::DoublewordShiftRightArithmeticVariable;
            instr.set_dest(dest);
            instr.set_source((opcode >> 16) & 0x1F);
            instr.set_source2((opcode >> 21) & 0x1F);
            instrs.push_back(instr);
            break;
        }
        case 0x18:
            // MULT
        {
            instr.op = IR::Opcode::MultiplyWord;
            instr.set_dest((opcode >> 11) & 0x1F);
            instr.set_source((opcode >> 21) & 0x1F);
            instr.set_source2((opcode >> 16) & 0x1F);
            instrs.push_back(instr);
            break;
        }
        case 0x19:
            // MULTU
        {
            instr.op = IR::Opcode::MultiplyUnsignedWord;
            instr.set_dest((opcode >> 11) & 0x1F);
            instr.set_source((opcode >> 21) & 0x1F);
            instr.set_source2((opcode >> 16) & 0x1F);
            instrs.push_back(instr);
            break;
        }
        case 0x1A:
            // DIV
        {
            instr.op = IR::Opcode::DivideWord;
            instr.set_source((opcode >> 21) & 0x1F);
            instr.set_source2((opcode >> 16) & 0x1F);
            instrs.push_back(instr);
            break;
        }
        case 0x1B:
            // DIVU
        {
            instr.op = IR::Opcode::DivideUnsignedWord;
            instr.set_source((opcode >> 21) & 0x1F);
            instr.set_source2((opcode >> 16) & 0x1F);
            instrs.push_back(instr);
            break;
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
                instr.op = IR::Opcode::MoveDoublewordReg;
                instr.set_dest(dest);
                instr.set_source(source2);
                instrs.push_back(instr);
                break;
            }
            if (!source2)
            {
                instr.op = IR::Opcode::MoveDoublewordReg;
                instr.set_dest(dest);
                instr.set_source(source);
                instrs.push_back(instr);
                break;
            }
            if (!dest)
            {
                // NOP
                break;
            }
            instr.op = IR::Opcode::AddWordReg;
            instr.set_dest(dest);
            instr.set_source(source);
            instr.set_source2(source2);
            instrs.push_back(instr);
            break;
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
                // NOP
                break;
            }
            if (!source)
            {
                // TODO: Seems to be broken (causes cursor to cycle in RD's title menu by itself)
                /*
                instr.op = IR::Opcode::NegateWordReg;
                instr.set_dest(dest);
                instr.set_source(source2);
                instrs.push_back(instr);
                break;
                */
            }
            if (source == source2)
            {
                instr.op = IR::Opcode::ClearWordReg;
                instr.set_dest(dest);
                instrs.push_back(instr);
                break;
            }
            instr.op = IR::Opcode::SubWordReg;
            instr.set_dest(dest);
            instr.set_source(source);
            instr.set_source2(source2);
            instrs.push_back(instr);
            break;
        }
        case 0x24:
            // AND
        {
            uint8_t dest = (opcode >> 11) & 0x1F;
            uint8_t source = (opcode >> 21) & 0x1F;
            uint8_t source2 = (opcode >> 16) & 0x1F;
            if (!dest)
            {
                // NOP
                break;
            }
            if (source == source2)
            {
                instr.op = IR::Opcode::MoveDoublewordReg;
                instr.set_dest(dest);
                instr.set_source(source);
                instrs.push_back(instr);
                break;
            }
            instr.op = IR::Opcode::AndReg;
            instr.set_dest(dest);
            instr.set_source(source);
            instr.set_source2(source2);
            instrs.push_back(instr);
            break;
        }
        case 0x25:
            // OR
        {
            uint8_t dest = (opcode >> 11) & 0x1F;
            uint8_t source = (opcode >> 21) & 0x1F;
            uint8_t source2 = (opcode >> 16) & 0x1F;
            if (!dest)
            {
                // NOP
                break;
            }
            if (source == source2)
            {
                instr.op = IR::Opcode::MoveDoublewordReg;
                instr.set_dest(dest);
                instr.set_source(source);
                instrs.push_back(instr);
                break;
            }
            instr.op = IR::Opcode::OrReg;
            instr.set_dest(dest);
            instr.set_source(source);
            instr.set_source2(source2);
            instrs.push_back(instr);
            break;
        }
        case 0x26:
            // XOR
        {
            uint8_t dest = (opcode >> 11) & 0x1F;
            uint8_t source = (opcode >> 21) & 0x1F;
            uint8_t source2 = (opcode >> 16) & 0x1F;
            if (!dest)
            {
                // NOP
                break;
            }
            if (source == source2)
            {
                instr.op = IR::Opcode::ClearDoublewordReg;
                instr.set_dest(dest);
                instrs.push_back(instr);
                break;
            }
            instr.op = IR::Opcode::XorReg;
            instr.set_dest(dest);
            instr.set_source(source);
            instr.set_source2(source2);
            instrs.push_back(instr);
            break;
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
                instrs.push_back(instr);
                break;
            }
            if (source == source2)
            {
                // TODO: dest = (NOT)source
            }
            instr.op = IR::Opcode::NorReg;
            instr.set_dest(dest);
            instr.set_source(source);
            instr.set_source2(source2);
            instrs.push_back(instr);
            break;
        }
        case 0x28:
            // MFSA
            instr.op = IR::Opcode::MoveDoublewordReg;
            instr.set_dest((opcode >> 11) & 0x1F);
            instr.set_source((int)EE_SpecialReg::SA);
            instrs.push_back(instr);
            break;
        case 0x29:
            // MTSA
            instr.op = IR::Opcode::MoveDoublewordReg;
            instr.set_dest((int)EE_SpecialReg::SA);
            instr.set_source((opcode >> 21) & 0x1F);
            instrs.push_back(instr);
            break;
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
            instrs.push_back(instr);
            break;
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
            instrs.push_back(instr);
            break;
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
                instrs.push_back(instr);
                break;
            }
            if (!source2)
            {
                instr.op = IR::Opcode::MoveDoublewordReg;
                instr.set_dest(dest);
                instr.set_source(source);
                instrs.push_back(instr);
                break;
            }
            if (!dest)
            {
                // NOP
                break;
            }
            instr.op = IR::Opcode::AddDoublewordReg;
            instr.set_dest(dest);
            instr.set_source(source);
            instr.set_source2(source2);
            instrs.push_back(instr);
            break;
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
                // NOP
                break;
            }
            if (!source)
            {
                /*
                instr.op = IR::Opcode::NegateDoublewordReg;
                instr.set_dest(dest);
                instr.set_source(source2);
                instrs.push_back(instr);
                break;
                */
            }
            if (source == source2)
            {
                instr.op = IR::Opcode::ClearDoublewordReg;
                instr.set_dest(dest);
                instrs.push_back(instr);
                break;
            }
            instr.op = IR::Opcode::SubDoublewordReg;
            instr.set_dest(dest);
            instr.set_source(source);
            instr.set_source2(source2);
            instrs.push_back(instr);
            break;
        }
        case 0x34:
            // TEQ
            Errors::print_warning("[EE_JIT] Unrecognized special op TEQ\n", op);
            fallback_interpreter(instr, opcode);
            instrs.push_back(instr);
            break;
        case 0x38:
            // DSLL
        {
            uint8_t dest = (opcode >> 11) & 0x1F;
            if (!dest)
            {
                // NOP
                break;
            }
            instr.op = IR::Opcode::DoublewordShiftLeftLogical;
            instr.set_dest(dest);
            instr.set_source((opcode >> 16) & 0x1F);
            instr.set_source2((opcode >> 6) & 0x1F);
            instrs.push_back(instr);
            break;
        }
        case 0x3A:
            // DSRL
        {
            uint8_t dest = (opcode >> 11) & 0x1F;
            if (!dest)
            {
                // NOP
                break;
            }
            instr.op = IR::Opcode::DoublewordShiftRightLogical;
            instr.set_dest(dest);
            instr.set_source((opcode >> 16) & 0x1F);
            instr.set_source2((opcode >> 6) & 0x1F);
            instrs.push_back(instr);
            break;
        }
        case 0x3B:
            // DSRA
        {
            uint8_t dest = (opcode >> 11) & 0x1F;
            if (!dest)
            {
                // NOP
                break;
            }
            instr.op = IR::Opcode::DoublewordShiftRightArithmetic;
            instr.set_dest(dest);
            instr.set_source((opcode >> 16) & 0x1F);
            instr.set_source2((opcode >> 6) & 0x1F);
            instrs.push_back(instr);
            break;
        }
        case 0x3C:
            // DSLL32
        {
            uint8_t dest = (opcode >> 11) & 0x1F;
            if (!dest)
            {
                // NOP
                break;
            }
            instr.op = IR::Opcode::DoublewordShiftLeftLogical;
            instr.set_dest(dest);
            instr.set_source((opcode >> 16) & 0x1F);
            instr.set_source2(((opcode >> 6) & 0x1F) + 32);
            instrs.push_back(instr);
            break;
        }
        case 0x3E:
            // DSRL32
        {
            uint8_t dest = (opcode >> 11) & 0x1F;
            if (!dest)
            {
                // NOP
                break;
            }
            instr.op = IR::Opcode::DoublewordShiftRightLogical;
            instr.set_dest(dest);
            instr.set_source((opcode >> 16) & 0x1F);
            instr.set_source2(((opcode >> 6) & 0x1F) + 32);
            instrs.push_back(instr);
            break;
        }
        case 0x3F:
            // DSRA32
        {
            uint8_t dest = (opcode >> 11) & 0x1F;
            if (!dest)
            {
                // NOP
                break;
            }
            instr.op = IR::Opcode::DoublewordShiftRightArithmetic;
            instr.set_dest(dest);
            instr.set_source((opcode >> 16) & 0x1F);
            instr.set_source2(((opcode >> 6) & 0x1F) + 32);
            instrs.push_back(instr);
            break;
        }
        default:
            Errors::die("[EE_JIT] Unrecognized special op $%02X", op);
    }
}

void EE_JitTranslator::translate_op_regimm(uint32_t opcode, uint32_t PC, std::vector<IR::Instruction>& instrs) const
{
    uint8_t op = (opcode >> 16) & 0x1F;
    IR::Instruction instr;
    instr.set_opcode(opcode);

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
            instrs.push_back(instr);
            break;
        case 0x01:
            // BGEZ
            instr.op = IR::Opcode::BranchGreaterThanOrEqualZero;
            instr.set_source((opcode >> 21) & 0x1F);
            instr.set_jump_dest(branch_offset_ee(opcode, PC));
            instr.set_jump_fail_dest(PC + 8);
            instr.set_is_likely(false);
            instr.set_is_link(false);
            instrs.push_back(instr);
            break;
        case 0x02:
            // BLTZL
            instr.op = IR::Opcode::BranchLessThanZero;
            instr.set_source((opcode >> 21) & 0x1F);
            instr.set_jump_dest(branch_offset_ee(opcode, PC));
            instr.set_jump_fail_dest(PC + 8);
            instr.set_is_likely(true);
            instr.set_is_link(false);
            instrs.push_back(instr);
            break;
        case 0x03:
            // BGEZL
            instr.op = IR::Opcode::BranchGreaterThanOrEqualZero;
            instr.set_source((opcode >> 21) & 0x1F);
            instr.set_jump_dest(branch_offset_ee(opcode, PC));
            instr.set_jump_fail_dest(PC + 8);
            instr.set_is_likely(true);
            instr.set_is_link(false);
            instrs.push_back(instr);
            break;
        case 0x10:
            // BLTZAL
            instr.op = IR::Opcode::BranchLessThanZero;
            instr.set_source((opcode >> 21) & 0x1F);
            instr.set_jump_dest(branch_offset_ee(opcode, PC));
            instr.set_jump_fail_dest(PC + 8);
            instr.set_return_addr(PC + 8);
            instr.set_is_likely(false);
            instr.set_is_link(true);
            instrs.push_back(instr);
            break;
        case 0x11:
            // BGEZAL
            instr.op = IR::Opcode::BranchGreaterThanOrEqualZero;
            instr.set_source((opcode >> 21) & 0x1F);
            instr.set_jump_dest(branch_offset_ee(opcode, PC));
            instr.set_jump_fail_dest(PC + 8);
            instr.set_return_addr(PC + 8);
            instr.set_is_likely(false);
            instr.set_is_link(true);
            instrs.push_back(instr);
            break;
        case 0x12:
            // BLTZALL
            instr.op = IR::Opcode::BranchLessThanZero;
            instr.set_source((opcode >> 21) & 0x1F);
            instr.set_jump_dest(branch_offset_ee(opcode, PC));
            instr.set_jump_fail_dest(PC + 8);
            instr.set_return_addr(PC + 8);
            instr.set_is_likely(true);
            instr.set_is_link(true);
            instrs.push_back(instr);
            break;
        case 0x13:
            // BGEZALL
            instr.op = IR::Opcode::BranchGreaterThanOrEqualZero;
            instr.set_source((opcode >> 21) & 0x1F);
            instr.set_jump_dest(branch_offset_ee(opcode, PC));
            instr.set_jump_fail_dest(PC + 8);
            instr.set_return_addr(PC + 8);
            instr.set_is_likely(true);
            instr.set_is_link(true);
            instrs.push_back(instr);
            break;
        case 0x18:
            // MTSAB
            instr.op = IR::Opcode::MoveDoublewordReg;
            instr.set_dest((int)EE_SpecialReg::SA);
            instr.set_source((opcode >> 21) & 0x1F);
            instrs.push_back(instr);

            instr.set_opcode(0);
            instr.op = IR::Opcode::XorImm;
            instr.set_dest((int)EE_SpecialReg::SA);
            instr.set_source((int)EE_SpecialReg::SA);
            instr.set_source2(opcode & 0xF);
            instrs.push_back(instr);

            instr.op = IR::Opcode::AndImm;
            instr.set_dest((int)EE_SpecialReg::SA);
            instr.set_source((int)EE_SpecialReg::SA);
            instr.set_source2(0xF);
            instrs.push_back(instr);

            break;
        case 0x19:
            // MTSAH
            instr.op = IR::Opcode::MoveDoublewordReg;
            instr.set_dest((int)EE_SpecialReg::SA);
            instr.set_source((opcode >> 21) & 0x1F);
            instrs.push_back(instr);

            instr.set_opcode(0);
            instr.op = IR::Opcode::XorImm;
            instr.set_dest((int)EE_SpecialReg::SA);
            instr.set_source((int)EE_SpecialReg::SA);
            instr.set_source2(opcode & 0x7);
            instrs.push_back(instr);

            instr.op = IR::Opcode::AndImm;
            instr.set_dest((int)EE_SpecialReg::SA);
            instr.set_source((int)EE_SpecialReg::SA);
            instr.set_source2(0x7);
            instrs.push_back(instr);

            instr.op = IR::Opcode::DoublewordShiftLeftLogical;
            instr.set_dest((int)EE_SpecialReg::SA);
            instr.set_source((int)EE_SpecialReg::SA);
            instr.set_source2(0x1);
            instrs.push_back(instr);
            break;
        default:
            Errors::die("[EE_JIT] Unrecognized regimm op $%02X", op);
    }
}

void EE_JitTranslator::translate_op_mmi(uint32_t opcode, uint32_t PC, std::vector<IR::Instruction>& instrs) const
{
    uint8_t op = opcode & 0x3F;
    IR::Instruction instr;
    instr.set_opcode(opcode);

    switch (op)
    {
        case 0x00:
            // MADD
            instr.op = IR::Opcode::MultiplyAddWord;
            instr.set_dest((opcode >> 11) & 0x1F);
            instr.set_source((opcode >> 21) & 0x1F);
            instr.set_source2((opcode >> 16) & 0x1F);
            instrs.push_back(instr);
            break;
        case 0x01:
            // MADDU
            instr.op = IR::Opcode::MultiplyAddUnsignedWord;
            instr.set_dest((opcode >> 11) & 0x1F);
            instr.set_source((opcode >> 21) & 0x1F);
            instr.set_source2((opcode >> 16) & 0x1F);
            instrs.push_back(instr);
            break;
        case 0x04:
            // PLZCW
            Errors::print_warning("[EE_JIT] Unrecognized mmi op PLZCW\n", op);
            fallback_interpreter(instr, opcode);
            instrs.push_back(instr);
            break;
        case 0x08:
            // MMI0 Operation
            translate_op_mmi0(opcode, PC, instrs);
            break;
        case 0x09:
            // MMI2 Operation
            translate_op_mmi2(opcode, PC, instrs);
            break;
        case 0x10:
            // MFHI1
        {
            uint8_t dest = (opcode >> 11) & 0x1F;
            instr.op = IR::Opcode::MoveDoublewordReg;
            instr.set_source((int)EE_SpecialReg::HI1);
            instr.set_dest(dest);
            instrs.push_back(instr);
            break;
        }
        case 0x11:
            // MTHI1
        {
            uint8_t source = (opcode >> 21) & 0x1F;
            instr.op = IR::Opcode::MoveDoublewordReg;
            instr.set_source(source);
            instr.set_dest((int)EE_SpecialReg::HI1);
            instrs.push_back(instr);
            break;
        }
        case 0x12:
            // MFLO1
        {
            uint8_t dest = (opcode >> 11) & 0x1F;
            instr.op = IR::Opcode::MoveDoublewordReg;
            instr.set_source((int)EE_SpecialReg::LO1);
            instr.set_dest(dest);
            instrs.push_back(instr);
            break;
        }
        case 0x13:
            // MTLO1
        {
            uint8_t source = (opcode >> 21) & 0x1F;
            instr.op = IR::Opcode::MoveDoublewordReg;
            instr.set_source(source);
            instr.set_dest((int)EE_SpecialReg::LO1);
            instrs.push_back(instr);
            break;
        }
        case 0x18:
            // MULT1
        {
            instr.op = IR::Opcode::MultiplyWord1;
            instr.set_dest((opcode >> 11) & 0x1F);
            instr.set_source((opcode >> 21) & 0x1F);
            instr.set_source2((opcode >> 16) & 0x1F);
            instrs.push_back(instr);
            break;
        }
        case 0x19:
            // MULTU1
        {
            instr.op = IR::Opcode::MultiplyUnsignedWord1;
            instr.set_dest((opcode >> 11) & 0x1F);
            instr.set_source((opcode >> 21) & 0x1F);
            instr.set_source2((opcode >> 16) & 0x1F);
            instrs.push_back(instr);
            break;
        }
        case 0x1A:
            // DIV1
        {
            instr.op = IR::Opcode::DivideWord1;
            instr.set_source((opcode >> 21) & 0x1F);
            instr.set_source2((opcode >> 16) & 0x1F);
            instrs.push_back(instr);
            break;
        }
        case 0x1B:
            // DIVU1
        {
            instr.op = IR::Opcode::DivideUnsignedWord1;
            instr.set_source((opcode >> 21) & 0x1F);
            instr.set_source2((opcode >> 16) & 0x1F);
            instrs.push_back(instr);
            break;
        }
        case 0x20:
            // MADD1
            instr.op = IR::Opcode::MultiplyAddWord1;
            instr.set_dest((opcode >> 11) & 0x1F);
            instr.set_source((opcode >> 21) & 0x1F);
            instr.set_source2((opcode >> 16) & 0x1F);
            instrs.push_back(instr);
            break;
        case 0x21:
            // MADDU1
            instr.op = IR::Opcode::MultiplyAddUnsignedWord1;
            instr.set_dest((opcode >> 11) & 0x1F);
            instr.set_source((opcode >> 21) & 0x1F);
            instr.set_source2((opcode >> 16) & 0x1F);
            instrs.push_back(instr);
            break;
        case 0x28:
            // MMI1 Operation
            translate_op_mmi1(opcode, PC, instrs);
            break;
        case 0x29:
            // MMI3 Operation
            translate_op_mmi3(opcode, PC, instrs);
            break;
        case 0x30:
            // PMFHLFMT
            Errors::print_warning("[EE_JIT] Unrecognized mmi op PMFHLFMT\n", op);
            fallback_interpreter(instr, opcode);
            instrs.push_back(instr);
            break;
        case 0x31:
            // PMTHLLW
            Errors::print_warning("[EE_JIT] Unrecognized mmi op PMTHLLW\n", op);
            fallback_interpreter(instr, opcode);
            instrs.push_back(instr);
            break;
        case 0x34:
            // PSLLH
        {
            uint8_t dest = (opcode >> 11) & 0x1F;
            uint8_t source = (opcode >> 16) & 0x1F;
            uint8_t sa = (opcode >> 6) & 0xF;
            instr.op = IR::Opcode::ParallelShiftLeftLogicalHalfword;
            instr.set_dest(dest);
            instr.set_source(source);
            instr.set_source2(sa);
            instrs.push_back(instr);
            break;
        }
        case 0x36:
            // PSRLH
        {
            uint8_t dest = (opcode >> 11) & 0x1F;
            uint8_t source = (opcode >> 16) & 0x1F;
            uint8_t sa = (opcode >> 6) & 0xF;
            instr.op = IR::Opcode::ParallelShiftRightLogicalHalfword;
            instr.set_dest(dest);
            instr.set_source(source);
            instr.set_source2(sa);
            instrs.push_back(instr);
            break;
        }
        case 0x37:
            // PSRAH
        {
            uint8_t dest = (opcode >> 11) & 0x1F;
            uint8_t source = (opcode >> 16) & 0x1F;
            uint8_t sa = (opcode >> 6) & 0xF;
            instr.op = IR::Opcode::ParallelShiftRightArithmeticHalfword;
            instr.set_dest(dest);
            instr.set_source(source);
            instr.set_source2(sa);
            instrs.push_back(instr);
            break;
        }
        case 0x3C:
            // PSLLW
        {
            uint8_t dest = (opcode >> 11) & 0x1F;
            uint8_t source = (opcode >> 16) & 0x1F;
            uint8_t sa = (opcode >> 6) & 0x1F;
            instr.op = IR::Opcode::ParallelShiftLeftLogicalWord;
            instr.set_dest(dest);
            instr.set_source(source);
            instr.set_source2(sa);
            instrs.push_back(instr);
            break;
        }
        case 0x3E:
            // PSRLW
        {
            uint8_t dest = (opcode >> 11) & 0x1F;
            uint8_t source = (opcode >> 16) & 0x1F;
            uint8_t sa = (opcode >> 6) & 0x1F;
            instr.op = IR::Opcode::ParallelShiftRightLogicalWord;
            instr.set_dest(dest);
            instr.set_source(source);
            instr.set_source2(sa);
            instrs.push_back(instr);
            break;
        }
        case 0x3F:
            // PSRAW
        {
            uint8_t dest = (opcode >> 11) & 0x1F;
            uint8_t source = (opcode >> 16) & 0x1F;
            uint8_t sa = (opcode >> 6) & 0x1F;
            instr.op = IR::Opcode::ParallelShiftRightArithmeticWord;
            instr.set_dest(dest);
            instr.set_source(source);
            instr.set_source2(sa);
            instrs.push_back(instr);
            break;
        }
        default:
            Errors::die("[EE_JIT] Unrecognized mmi op $%02X", op);
    }
}
void EE_JitTranslator::translate_op_mmi0(uint32_t opcode, uint32_t PC, std::vector<IR::Instruction>& instrs) const
{
    uint8_t op = (opcode >> 6) & 0x1F;
    IR::Instruction instr;
    instr.set_opcode(opcode);

    switch (op)
    {
        case 0x00:
            // PADDW
        {
            uint8_t dest = (opcode >> 11) & 0x1F;
            uint8_t source = (opcode >> 21) & 0x1F;
            uint8_t source2 = (opcode >> 16) & 0x1F;
            if (!dest)
            {
                // NOP
                break;
            }
            instr.set_dest(dest);
            instr.set_source(source);
            instr.set_source2(source2);
            instr.op = IR::Opcode::ParallelAddWord;
            instrs.push_back(instr);
            break;
        }
        case 0x01:
            // PSUBW
        {
            uint8_t dest = (opcode >> 11) & 0x1F;
            uint8_t source = (opcode >> 21) & 0x1F;
            uint8_t source2 = (opcode >> 16) & 0x1F;
            if (!dest)
            {
                // NOP
                break;
            }
            instr.set_dest(dest);
            instr.set_source(source);
            instr.set_source2(source2);
            instr.op = IR::Opcode::ParallelSubtractWord;
            instrs.push_back(instr);
            break;
        }
        case 0x02:
            // PCGTW
        {
            uint8_t dest = (opcode >> 11) & 0x1F;
            uint8_t source = (opcode >> 21) & 0x1F;
            uint8_t source2 = (opcode >> 16) & 0x1F;
            if (!dest)
            {
                // NOP
                break;
            }
            instr.set_dest(dest);
            instr.set_source(source);
            instr.set_source2(source2);
            instr.op = IR::Opcode::ParallelCompareGreaterThanWord;
            instrs.push_back(instr);
            break;
        }
        case 0x03:
            // PMAXW
        {
            uint8_t dest = (opcode >> 11) & 0x1F;
            uint8_t source = (opcode >> 21) & 0x1F;
            uint8_t source2 = (opcode >> 16) & 0x1F;
            if (!dest)
            {
                // NOP
                break;
            }
            instr.set_dest(dest);
            instr.set_source(source);
            instr.set_source2(source2);
            instr.op = IR::Opcode::ParallelMaximizeWord;
            instrs.push_back(instr);
            break;
        }
        case 0x04:
            // PADDH
        {
            uint8_t dest = (opcode >> 11) & 0x1F;
            uint8_t source = (opcode >> 21) & 0x1F;
            uint8_t source2 = (opcode >> 16) & 0x1F;
            if (!dest)
            {
                // NOP
                break;
            }
            instr.set_dest(dest);
            instr.set_source(source);
            instr.set_source2(source2);
            instr.op = IR::Opcode::ParallelAddHalfword;
            instrs.push_back(instr);
            break;
        }
        case 0x05:
            // PSUBH
        {
            uint8_t dest = (opcode >> 11) & 0x1F;
            uint8_t source = (opcode >> 21) & 0x1F;
            uint8_t source2 = (opcode >> 16) & 0x1F;
            if (!dest)
            {
                // NOP
                break;
            }
            instr.set_dest(dest);
            instr.set_source(source);
            instr.set_source2(source2);
            instr.op = IR::Opcode::ParallelSubtractHalfword;
            instrs.push_back(instr);
            break;
        }
        case 0x06:
            // PCGTH
        {
            uint8_t dest = (opcode >> 11) & 0x1F;
            uint8_t source = (opcode >> 21) & 0x1F;
            uint8_t source2 = (opcode >> 16) & 0x1F;
            if (!dest)
            {
                // NOP
                break;
            }
            instr.set_dest(dest);
            instr.set_source(source);
            instr.set_source2(source2);
            instr.op = IR::Opcode::ParallelCompareGreaterThanHalfword;
            instrs.push_back(instr);
            break;
        }
        case 0x07:
            // PMAXH
        {
            uint8_t dest = (opcode >> 11) & 0x1F;
            uint8_t source = (opcode >> 21) & 0x1F;
            uint8_t source2 = (opcode >> 16) & 0x1F;
            if (!dest)
            {
                // NOP
                break;
            }
            instr.set_dest(dest);
            instr.set_source(source);
            instr.set_source2(source2);
            instr.op = IR::Opcode::ParallelMaximizeHalfword;
            instrs.push_back(instr);
            break;
        }
        case 0x08:
            // PADDB
        {
            uint8_t dest = (opcode >> 11) & 0x1F;
            uint8_t source = (opcode >> 21) & 0x1F;
            uint8_t source2 = (opcode >> 16) & 0x1F;
            if (!dest)
            {
                // NOP
                break;
            }
            instr.set_dest(dest);
            instr.set_source(source);
            instr.set_source2(source2);
            instr.op = IR::Opcode::ParallelAddByte;
            instrs.push_back(instr);
            break;
        }
        case 0x09:
            // PSUBB
        {
            uint8_t dest = (opcode >> 11) & 0x1F;
            uint8_t source = (opcode >> 21) & 0x1F;
            uint8_t source2 = (opcode >> 16) & 0x1F;
            if (!dest)
            {
                // NOP
                break;
            }
            instr.set_dest(dest);
            instr.set_source(source);
            instr.set_source2(source2);
            instr.op = IR::Opcode::ParallelSubtractByte;
            instrs.push_back(instr);
            break;
        }
        case 0x0A:
            // PCGTB
        {
            uint8_t dest = (opcode >> 11) & 0x1F;
            uint8_t source = (opcode >> 21) & 0x1F;
            uint8_t source2 = (opcode >> 16) & 0x1F;
            if (!dest)
            {
                // NOP
                break;
            }
            instr.set_dest(dest);
            instr.set_source(source);
            instr.set_source2(source2);
            instr.op = IR::Opcode::ParallelCompareGreaterThanByte;
            instrs.push_back(instr);
            break;
        }
        case 0x10:
            // PADDSW
        {
            uint8_t dest = (opcode >> 11) & 0x1F;
            uint8_t source = (opcode >> 21) & 0x1F;
            uint8_t source2 = (opcode >> 16) & 0x1F;
            if (!dest)
            {
                // NOP
                break;
            }
            instr.set_dest(dest);
            instr.set_source(source);
            instr.set_source2(source2);
            instr.op = IR::Opcode::ParallelAddWithSignedSaturationWord;
            instrs.push_back(instr);
            break;
        }
        case 0x11:
            // PSUBSW
        {
            uint8_t dest = (opcode >> 11) & 0x1F;
            uint8_t source = (opcode >> 21) & 0x1F;
            uint8_t source2 = (opcode >> 16) & 0x1F;
            if (!dest)
            {
                // NOP
                break;
            }
            instr.set_dest(dest);
            instr.set_source(source);
            instr.set_source2(source2);
            instr.op = IR::Opcode::ParallelSubtractWithSignedSaturationWord;
            instrs.push_back(instr);
            break;
        }
        case 0x12:
            // PEXTLW
            Errors::print_warning("[EE_JIT] Unrecognized mmi0 op PEXTLW\n", op);
            fallback_interpreter(instr, opcode);
            instrs.push_back(instr);
            break;
        case 0x13:
            // PPACW
        {
            uint8_t dest = (opcode >> 11) & 0x1F;
            uint8_t source = (opcode >> 21) & 0x1F;
            uint8_t source2 = (opcode >> 16) & 0x1F;
            if (!dest)
            {
                // NOP
                break;
            }
            instr.set_dest(dest);
            instr.set_source(source);
            instr.set_source2(source2);
            instr.op = IR::Opcode::ParallelPackToWord;
            instrs.push_back(instr);
            break;
        }
        case 0x14:
            // PADDSH
        {
            uint8_t dest = (opcode >> 11) & 0x1F;
            uint8_t source = (opcode >> 21) & 0x1F;
            uint8_t source2 = (opcode >> 16) & 0x1F;
            if (!dest)
            {
                // NOP
                break;
            }
            instr.set_dest(dest);
            instr.set_source(source);
            instr.set_source2(source2);
            instr.op = IR::Opcode::ParallelAddWithSignedSaturationHalfword;
            instrs.push_back(instr);
            break;
        }
        case 0x15:
            // PSUBSH
        {
            uint8_t dest = (opcode >> 11) & 0x1F;
            uint8_t source = (opcode >> 21) & 0x1F;
            uint8_t source2 = (opcode >> 16) & 0x1F;
            if (!dest)
            {
                // NOP
                break;
            }
            instr.set_dest(dest);
            instr.set_source(source);
            instr.set_source2(source2);
            instr.op = IR::Opcode::ParallelSubtractWithSignedSaturationHalfword;
            instrs.push_back(instr);
            break;
        }
        case 0x16:
            // PEXTLH
            Errors::print_warning("[EE_JIT] Unrecognized mmi0 op PEXTLH\n", op);
            fallback_interpreter(instr, opcode);
            instrs.push_back(instr);
            break;
        case 0x17:
            // PPACH
        {
            uint8_t dest = (opcode >> 11) & 0x1F;
            uint8_t source = (opcode >> 21) & 0x1F;
            uint8_t source2 = (opcode >> 16) & 0x1F;
            if (!dest)
            {
                // NOP
                break;
            }
            instr.set_dest(dest);
            instr.set_source(source);
            instr.set_source2(source2);
            instr.op = IR::Opcode::ParallelPackToHalfword;
            instrs.push_back(instr);
            break;
        }
        case 0x18:
            // PADDSB
        {
            uint8_t dest = (opcode >> 11) & 0x1F;
            uint8_t source = (opcode >> 21) & 0x1F;
            uint8_t source2 = (opcode >> 16) & 0x1F;
            if (!dest)
            {
                // NOP
                break;
            }
            instr.set_dest(dest);
            instr.set_source(source);
            instr.set_source2(source2);
            instr.op = IR::Opcode::ParallelAddWithSignedSaturationByte;
            instrs.push_back(instr);
            break;
        }
        case 0x19:
            // PSUBSB
        {
            uint8_t dest = (opcode >> 11) & 0x1F;
            uint8_t source = (opcode >> 21) & 0x1F;
            uint8_t source2 = (opcode >> 16) & 0x1F;
            if (!dest)
            {
                // NOP
                break;
            }
            instr.set_dest(dest);
            instr.set_source(source);
            instr.set_source2(source2);
            instr.op = IR::Opcode::ParallelSubtractWithSignedSaturationByte;
            instrs.push_back(instr);
            break;
        }
        case 0x1A:
            // PEXTLB
            Errors::print_warning("[EE_JIT] Unrecognized mmi0 op PEXTLB\n", op);
            fallback_interpreter(instr, opcode);
            instrs.push_back(instr);
            break;
        case 0x1B:
            // PPACB
        {
            uint8_t dest = (opcode >> 11) & 0x1F;
            uint8_t source = (opcode >> 21) & 0x1F;
            uint8_t source2 = (opcode >> 16) & 0x1F;
            if (!dest)
            {
                // NOP
                break;
            }
            instr.set_dest(dest);
            instr.set_source(source);
            instr.set_source2(source2);
            instr.op = IR::Opcode::ParallelPackToByte;
            instrs.push_back(instr);
            break;
        }
        case 0x1E:
            // PEXT5
            Errors::print_warning("[EE_JIT] Unrecognized mmi0 op PEXT5\n", op);
            fallback_interpreter(instr, opcode);
            instrs.push_back(instr);
            break;
        case 0x1F:
            // PPAC5
            Errors::print_warning("[EE_JIT] Unrecognized mmi0 op PPAC5\n", op);
            fallback_interpreter(instr, opcode);
            instrs.push_back(instr);
            break;
        default:
            Errors::die("[EE_JIT] Unrecognized mmi0 op $%02X", op);
    }
}

void EE_JitTranslator::translate_op_mmi1(uint32_t opcode, uint32_t PC, std::vector<IR::Instruction>& instrs) const
{
    uint8_t op = (opcode >> 6) & 0x1F;
    IR::Instruction instr;
    instr.set_opcode(opcode);

    switch (op)
    {
        case 0x01:
            // PABSW
        {
            uint8_t dest = (opcode >> 11) & 0x1F;
            uint8_t source = (opcode >> 16) & 0x1F;
            if (!dest)
            {
                // NOP
                break;
            }
            instr.set_dest(dest);
            instr.set_source(source);
            instr.op = IR::ParallelAbsoluteWord;
            instrs.push_back(instr);
            break;
        }
        case 0x02:
            // PCEQW
        {
            uint8_t dest = (opcode >> 11) & 0x1F;
            uint8_t source = (opcode >> 21) & 0x1F;
            uint8_t source2 = (opcode >> 16) & 0x1F;
            if (!dest)
            {
                // NOP
                break;
            }
            instr.set_dest(dest);
            instr.set_source(source);
            instr.set_source2(source2);
            instr.op = IR::Opcode::ParallelCompareEqualWord;
            instrs.push_back(instr);
            break;
        }
        case 0x03:
            // PMINW
        {
            uint8_t dest = (opcode >> 11) & 0x1F;
            uint8_t source = (opcode >> 21) & 0x1F;
            uint8_t source2 = (opcode >> 16) & 0x1F;
            if (!dest)
            {
                // NOP
                break;
            }
            instr.set_dest(dest);
            instr.set_source(source);
            instr.set_source2(source2);
            instr.op = IR::Opcode::ParallelMinimizeWord;
            instrs.push_back(instr);
            break;
        }
        case 0x04:
            // PADSBH
            Errors::print_warning("[EE_JIT] Unrecognized mmi1 op PABSW\n", op);
            fallback_interpreter(instr, opcode);
            instrs.push_back(instr);
            break;
        case 0x05:
            // PABSH
        {
            uint8_t dest = (opcode >> 11) & 0x1F;
            uint8_t source = (opcode >> 16) & 0x1F;
            if (!dest)
            {
                // NOP
                break;
            }
            instr.set_dest(dest);
            instr.set_source(source);
            instr.op = IR::ParallelAbsoluteHalfword;
            instrs.push_back(instr);
            break;
        }
        case 0x06:
            // PCEQH
        {
            uint8_t dest = (opcode >> 11) & 0x1F;
            uint8_t source = (opcode >> 21) & 0x1F;
            uint8_t source2 = (opcode >> 16) & 0x1F;
            if (!dest)
            {
                // NOP
                break;
            }
            instr.set_dest(dest);
            instr.set_source(source);
            instr.set_source2(source2);
            instr.op = IR::Opcode::ParallelCompareEqualHalfword;
            instrs.push_back(instr);
            break;
        }
        case 0x07:
            // PMINH
        {
            uint8_t dest = (opcode >> 11) & 0x1F;
            uint8_t source = (opcode >> 21) & 0x1F;
            uint8_t source2 = (opcode >> 16) & 0x1F;
            if (!dest)
            {
                // NOP
                break;
            }
            instr.set_dest(dest);
            instr.set_source(source);
            instr.set_source2(source2);
            instr.op = IR::Opcode::ParallelMinimizeHalfword;
            instrs.push_back(instr);
            break;
        }
        case 0x0A:
            // PCEQB
        {
            uint8_t dest = (opcode >> 11) & 0x1F;
            uint8_t source = (opcode >> 21) & 0x1F;
            uint8_t source2 = (opcode >> 16) & 0x1F;
            if (!dest)
            {
                // NOP
                break;
            }
            instr.set_dest(dest);
            instr.set_source(source);
            instr.set_source2(source2);
            instr.op = IR::Opcode::ParallelCompareEqualByte;
            instrs.push_back(instr);
            break;
        }
        case 0x10:
            // PADDUW
        {
            uint8_t dest = (opcode >> 11) & 0x1F;
            uint8_t source = (opcode >> 21) & 0x1F;
            uint8_t source2 = (opcode >> 16) & 0x1F;
            if (!dest)
            {
                // NOP
                break;
            }
            instr.set_dest(dest);
            instr.set_source(source);
            instr.set_source2(source2);
            instr.op = IR::Opcode::ParallelAddWithUnsignedSaturationWord;
            instrs.push_back(instr);
            break;
        }
        case 0x11:
            // PSUBUW
        {
            uint8_t dest = (opcode >> 11) & 0x1F;
            uint8_t source = (opcode >> 21) & 0x1F;
            uint8_t source2 = (opcode >> 16) & 0x1F;
            if (!dest)
            {
                // NOP
                break;
            }
            instr.set_dest(dest);
            instr.set_source(source);
            instr.set_source2(source2);
            instr.op = IR::Opcode::ParallelSubtractWithUnsignedSaturationWord;
            instrs.push_back(instr);
            break;
        }
        case 0x12:
            // PEXTUW
            Errors::print_warning("[EE_JIT] Unrecognized mmi1 op PEXTUW\n", op);
            fallback_interpreter(instr, opcode);
            instrs.push_back(instr);
            break;
        case 0x14:
            // PADDUH
        {
            uint8_t dest = (opcode >> 11) & 0x1F;
            uint8_t source = (opcode >> 21) & 0x1F;
            uint8_t source2 = (opcode >> 16) & 0x1F;
            if (!dest)
            {
                // NOP
                break;
            }
            instr.set_dest(dest);
            instr.set_source(source);
            instr.set_source2(source2);
            instr.op = IR::Opcode::ParallelAddWithUnsignedSaturationHalfword;
            instrs.push_back(instr);
            break;
        }
        case 0x15:
            // PSUBUH
        {
            uint8_t dest = (opcode >> 11) & 0x1F;
            uint8_t source = (opcode >> 21) & 0x1F;
            uint8_t source2 = (opcode >> 16) & 0x1F;
            if (!dest)
            {
                // NOP
                break;
            }
            instr.set_dest(dest);
            instr.set_source(source);
            instr.set_source2(source2);
            instr.op = IR::Opcode::ParallelSubtractWithUnsignedSaturationHalfword;
            instrs.push_back(instr);
            break;
        }
        case 0x16:
            // PEXTUH
            Errors::print_warning("[EE_JIT] Unrecognized mmi1 op PEXTUH\n", op);
            fallback_interpreter(instr, opcode);
            instrs.push_back(instr);
            break;
        case 0x18:
            // PADDUB
        {
            uint8_t dest = (opcode >> 11) & 0x1F;
            uint8_t source = (opcode >> 21) & 0x1F;
            uint8_t source2 = (opcode >> 16) & 0x1F;
            if (!dest)
            {
                // NOP
                break;
            }
            instr.set_dest(dest);
            instr.set_source(source);
            instr.set_source2(source2);
            instr.op = IR::Opcode::ParallelAddWithUnsignedSaturationByte;
            instrs.push_back(instr);
            break;
        }
        case 0x19:
            // PSUBUB
        {
            uint8_t dest = (opcode >> 11) & 0x1F;
            uint8_t source = (opcode >> 21) & 0x1F;
            uint8_t source2 = (opcode >> 16) & 0x1F;
            if (!dest)
            {
                // NOP
                break;
            }
            instr.set_dest(dest);
            instr.set_source(source);
            instr.set_source2(source2);
            instr.op = IR::Opcode::ParallelSubtractWithUnsignedSaturationByte;
            instrs.push_back(instr);
            break;
        }
        case 0x1A:
            // PEXTUB
            Errors::print_warning("[EE_JIT] Unrecognized mmi1 op PEXTUB\n", op);
            fallback_interpreter(instr, opcode);
            instrs.push_back(instr);
            break;
        case 0x1B:
            // QFSRV
            Errors::print_warning("[EE_JIT] Unrecognized mmi1 op QFSRV\n", op);
            fallback_interpreter(instr, opcode);
            instrs.push_back(instr);
            break;
        default:
            Errors::die("[EE_JIT] Unrecognized mmi1 op $%02X", op);
    }
}

void EE_JitTranslator::translate_op_mmi2(uint32_t opcode, uint32_t PC, std::vector<IR::Instruction>& instrs) const
{
    uint8_t op = (opcode >> 6) & 0x1F;
    IR::Instruction instr;
    instr.set_opcode(opcode);

    switch (op)
    {
        case 0x00:
            // PMADDW
            Errors::print_warning("[EE_JIT] Unrecognized mmi2 op PMADDW\n", op);
            fallback_interpreter(instr, opcode);
            instrs.push_back(instr);
            break;
        case 0x02:
            // PSLLVW
            Errors::print_warning("[EE_JIT] Unrecognized mmi2 op PSLLVW\n", op);
            fallback_interpreter(instr, opcode);
            instrs.push_back(instr);
            break;
        case 0x03:
            // PSRLVW
            Errors::print_warning("[EE_JIT] Unrecognized mmi2 op PSRLVW\n", op);
            fallback_interpreter(instr, opcode);
            instrs.push_back(instr);
            break;
        case 0x04:
            // PMSUBW
            Errors::print_warning("[EE_JIT] Unrecognized mmi2 op PMSUBW\n", op);
            fallback_interpreter(instr, opcode);
            instrs.push_back(instr);
            break;
        case 0x08:
            // PMFHI
        {
            uint8_t dest = (opcode >> 11) & 0x1F;
            instr.op = IR::Opcode::MoveQuadwordReg;
            instr.set_source((int)EE_SpecialReg::HI);
            instr.set_dest(dest);
            instrs.push_back(instr);
            break;
        }
        case 0x09:
            // PMFLO
        {
            uint8_t dest = (opcode >> 11) & 0x1F;
            instr.op = IR::Opcode::MoveQuadwordReg;
            instr.set_source((int)EE_SpecialReg::LO);
            instr.set_dest(dest);
            instrs.push_back(instr);
            break;
        }
        case 0x0A:
            // PINTH
            Errors::print_warning("[EE_JIT] Unrecognized mmi2 op PINTH\n", op);
            fallback_interpreter(instr, opcode);
            instrs.push_back(instr);
            break;
        case 0x0C:
            // PMULTW
            Errors::print_warning("[EE_JIT] Unrecognized mmi2 op PMULTW\n", op);
            fallback_interpreter(instr, opcode);
            instrs.push_back(instr);
            break;
        case 0x0D:
            // PDIVW
        {
            uint8_t source = (opcode >> 21) & 0x1F;
            uint8_t source2 = (opcode >> 16) & 0x1F;
            instr.set_source(source);
            instr.set_source2(source2);
            instr.op = IR::Opcode::ParallelDivideWord;
            instrs.push_back(instr);
            break;
        }
        case 0x0E:
            // PCPYLD
            Errors::print_warning("[EE_JIT] Unrecognized mmi2 op PCPYLD\n", op);
            fallback_interpreter(instr, opcode);
            instrs.push_back(instr);
            break;
        case 0x10:
            // PMADDH
            Errors::print_warning("[EE_JIT] Unrecognized mmi2 op PMADDH\n", op);
            fallback_interpreter(instr, opcode);
            instrs.push_back(instr);
            break;
        case 0x11:
            // PHMADH
            Errors::print_warning("[EE_JIT] Unrecognized mmi2 op PHMADH\n", op);
            fallback_interpreter(instr, opcode);
            instrs.push_back(instr);
            break;
        case 0x12:
            // PAND
        {
            uint8_t dest = (opcode >> 11) & 0x1F;
            uint8_t source = (opcode >> 21) & 0x1F;
            uint8_t source2 = (opcode >> 16) & 0x1F;
            if (!dest)
            {
                // NOP
                break;
            }
            instr.set_dest(dest);
            instr.set_source(source);
            instr.set_source2(source2);
            instr.op = IR::Opcode::ParallelAnd;
            instrs.push_back(instr);
            break;
        }
        case 0x13:
            // PXOR
        {
            uint8_t dest = (opcode >> 11) & 0x1F;
            uint8_t source = (opcode >> 21) & 0x1F;
            uint8_t source2 = (opcode >> 16) & 0x1F;
            if (!dest)
            {
                // NOP
                break;
            }
            instr.set_dest(dest);
            instr.set_source(source);
            instr.set_source2(source2);
            instr.op = IR::Opcode::ParallelXor;
            instrs.push_back(instr);
            break;
        }
        case 0x14:
            // PMSUBH
            Errors::print_warning("[EE_JIT] Unrecognized mmi2 op PMSUBH\n", op);
            fallback_interpreter(instr, opcode);
            instrs.push_back(instr);
            break;
        case 0x15:
            // PHMSBH
            Errors::print_warning("[EE_JIT] Unrecognized mmi2 op PMADDW\n", op);
            fallback_interpreter(instr, opcode);
            instrs.push_back(instr);
            break;
        case 0x1A:
            // PEXEH
        {
            uint8_t dest = (opcode >> 11) & 0x1F;
            uint8_t source = (opcode >> 16) & 0x1F;
            if (!dest)
            {
                // NOP
                break;
            }
            instr.set_dest(dest);
            instr.set_source(source);
            instr.op = IR::Opcode::ParallelExchangeEvenHalfword;
            instrs.push_back(instr);
            break;
        }
        case 0x1B:
            // PREVH
        {
            uint8_t dest = (opcode >> 11) & 0x1F;
            uint8_t source = (opcode >> 16) & 0x1F;
            if (!dest)
            {
                // NOP
                break;
            }
            instr.set_dest(dest);
            instr.set_source(source);
            instr.op = IR::Opcode::ParallelReverseHalfword;
            instrs.push_back(instr);
            break;
        }
        case 0x1C:
            // PMULTH
            Errors::print_warning("[EE_JIT] Unrecognized mmi2 op PMULTH\n", op);
            fallback_interpreter(instr, opcode);
            instrs.push_back(instr);
            break;
        case 0x1D:
            // PDIVBW
            Errors::print_warning("[EE_JIT] Unrecognized mmi2 op PDIVBW\n", op);
            fallback_interpreter(instr, opcode);
            instrs.push_back(instr);
            break;
        case 0x1E:
            // PEXEW
        {
            uint8_t dest = (opcode >> 11) & 0x1F;
            uint8_t source = (opcode >> 16) & 0x1F;
            if (!dest)
            {
                // NOP
                break;
            }
            instr.set_dest(dest);
            instr.set_source(source);
            instr.op = IR::Opcode::ParallelExchangeEvenWord;
            instrs.push_back(instr);
            break;
        }
        case 0x1F:
            // PROT3W
        {
            uint8_t dest = (opcode >> 11) & 0x1F;
            uint8_t source = (opcode >> 16) & 0x1F;
            if (!dest)
            {
                // NOP
                break;
            }
            instr.set_dest(dest);
            instr.set_source(source);
            instr.op = IR::Opcode::ParallelRotate3WordsLeft;
            instrs.push_back(instr);
            break;
        }
        default:
            Errors::die("[EE_JIT] Unrecognized mmi2 op $%02X", op);
    }
}

void EE_JitTranslator::translate_op_mmi3(uint32_t opcode, uint32_t PC, std::vector<IR::Instruction>& instrs) const
{
    uint8_t op = (opcode >> 6) & 0x1F;
    IR::Instruction instr;
    instr.set_opcode(opcode);

    switch (op)
    {
        case 0x00:
            // PMADDUW
            Errors::print_warning("[EE_JIT] Unrecognized mmi3 op PMADDUW\n", op);
            fallback_interpreter(instr, opcode);
            instrs.push_back(instr);
            break;
        case 0x03:
            // PSRAVW
            Errors::print_warning("[EE_JIT] Unrecognized mmi3 op PSRAVW\n", op);
            fallback_interpreter(instr, opcode);
            instrs.push_back(instr);
            break;
        case 0x08:
            // PMTHI
        {
            uint8_t source = (opcode >> 21) & 0x1F;
            instr.op = IR::Opcode::MoveQuadwordReg;
            instr.set_source(source);
            instr.set_dest((int)EE_SpecialReg::HI);
            instrs.push_back(instr);
            break;
        }
        case 0x09:
            // PMTLO
        {
            uint8_t source = (opcode >> 21) & 0x1F;
            instr.op = IR::Opcode::MoveQuadwordReg;
            instr.set_source(source);
            instr.set_dest((int)EE_SpecialReg::LO);
            instrs.push_back(instr);
            break;
        }
        case 0x0A:
            // PINTEH
            Errors::print_warning("[EE_JIT] Unrecognized mmi3 op PINTEH\n", op);
            fallback_interpreter(instr, opcode);
            instrs.push_back(instr);
            break;
        case 0x0C:
            // PMULTUW
            Errors::print_warning("[EE_JIT] Unrecognized mmi3 op PMULTUW\n", op);
            fallback_interpreter(instr, opcode);
            instrs.push_back(instr);
            break;
        case 0x0D:
            // PDIVUW
            Errors::print_warning("[EE_JIT] Unrecognized mmi3 op PDIVUW\n", op);
            fallback_interpreter(instr, opcode);
            instrs.push_back(instr);
            break;
        case 0x0E:
            // PCPYUD
            Errors::print_warning("[EE_JIT] Unrecognized mmi3 op PCPYUD\n", op);
            fallback_interpreter(instr, opcode);
            instrs.push_back(instr);
            break;
        case 0x12:
            // POR
        {
            uint8_t dest = (opcode >> 11) & 0x1F;
            uint8_t source = (opcode >> 21) & 0x1F;
            uint8_t source2 = (opcode >> 16) & 0x1F;
            if (!dest)
            {
                // NOP
                break;
            }
            instr.set_dest(dest);
            instr.set_source(source);
            instr.set_source2(source2);
            instr.op = IR::Opcode::ParallelOr;
            instrs.push_back(instr);
            break;
        }
        case 0x13:
            // PNOR
        {
            uint8_t dest = (opcode >> 11) & 0x1F;
            uint8_t source = (opcode >> 21) & 0x1F;
            uint8_t source2 = (opcode >> 16) & 0x1F;
            if (!dest)
            {
                // NOP
                break;
            }
            instr.set_dest(dest);
            instr.set_source(source);
            instr.set_source2(source2);
            instr.op = IR::Opcode::ParallelNor;
            instrs.push_back(instr);
            break;
        }
        case 0x1A:
            // PEXCH
        {
            uint8_t dest = (opcode >> 11) & 0x1F;
            uint8_t source = (opcode >> 16) & 0x1F;
            if (!dest)
            {
                // NOP
                break;
            }
            instr.set_dest(dest);
            instr.set_source(source);
            instr.op = IR::Opcode::ParallelExchangeCenterHalfword;
            instrs.push_back(instr);
            break;
        }
        case 0x1B:
            // PCPYH
            Errors::print_warning("[EE_JIT] Unrecognized mmi3 op PCYPH\n", op);
            fallback_interpreter(instr, opcode);
            instrs.push_back(instr);
            break;
        case 0x1E:
            // PEXCW
        {
            uint8_t dest = (opcode >> 11) & 0x1F;
            uint8_t source = (opcode >> 16) & 0x1F;
            if (!dest)
            {
                // NOP
                break;
            }
            instr.set_dest(dest);
            instr.set_source(source);
            instr.op = IR::Opcode::ParallelExchangeCenterWord;
            instrs.push_back(instr);
            break;
        }
        default:
            Errors::die("[EE_JIT] Unrecognized mmi3 op $%02X", op);
    }
}

void EE_JitTranslator::translate_op_cop0(uint32_t opcode, uint32_t PC, std::vector<IR::Instruction>& instrs)
{
    uint8_t op = (opcode >> 21) & 0x1F;
    IR::Instruction instr;
    instr.set_opcode(opcode);

    switch (op)
    {
        case 0x00:
            // MFC0
            Errors::print_warning("[EE_JIT] Unrecognized cop0 op MFC0\n", op);
            fallback_interpreter(instr, opcode);
            instrs.push_back(instr);
            break;
        case 0x04:
            // MTC0
            Errors::print_warning("[EE_JIT] Unrecognized cop0 op MTC0\n", op);
            fallback_interpreter(instr, opcode);
            instrs.push_back(instr);
            break;
        case 0x08:
            // BC0
            instr.op = IR::Opcode::BranchCop0;
            instr.set_is_likely((opcode >> 17) & 1);
            instr.set_field((opcode >> 16) & 1);
            instr.set_jump_dest(branch_offset_ee(opcode, PC));
            instr.set_jump_fail_dest(PC + 8);
            instrs.push_back(instr);
            break;
        case 0x10:
            // Type2 op
            translate_op_cop0_type2(opcode, PC, instrs);
            break;
        default:
            Errors::die("[EE_JIT] Unrecognized cop0 op $%02X", op);
    }
}

void EE_JitTranslator::translate_op_cop0_type2(uint32_t opcode, uint32_t PC, std::vector<IR::Instruction>& instrs)
{
    uint8_t op = opcode & 0x3F;
    IR::Instruction instr;
    instr.set_opcode(opcode);

    switch (op)
    {
        case 0x02:
            // TLBWI
            Errors::print_warning("[EE_JIT] Unrecognized cop0 type2 op TLBWI\n", op);
            fallback_interpreter(instr, opcode);
            instrs.push_back(instr);
            break;
        case 0x18:
            // ERET
            instr.op = IR::Opcode::ExceptionReturn;
            instr.set_source(opcode);
            eret_op = true;
            instrs.push_back(instr);
            break;
        case 0x38:
            // EI
            Errors::print_warning("[EE_JIT] Unrecognized cop0 type2 op EI\n", op);
            fallback_interpreter(instr, opcode);
            instrs.push_back(instr);
            break;
        case 0x39:
            // DI
            if (!branch_op)
                di_delay = 2;
            else
            {
                Errors::print_warning("[EE_JIT] Unrecognized cop0 type2 op DI\n", op);
                fallback_interpreter(instr, opcode);
                instrs.push_back(instr);
            }
            break;
        default:
            Errors::die("[EE_JIT] Unrecognized cop0 type2 op $%02X", op);
    }
}

void EE_JitTranslator::translate_op_cop1(uint32_t opcode, uint32_t PC, std::vector<IR::Instruction>& instrs) const
{
    uint8_t op = (opcode >> 21) & 0x1F;
    IR::Instruction instr;
    instr.set_opcode(opcode);

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
            instrs.push_back(instr);
            break;
        }
        case 0x02:
            // CFC1
        {
            uint8_t dest = (opcode >> 16) & 0x1F;
            uint8_t source = (opcode >> 11) & 0x1F;
            instr.op = IR::Opcode::MoveControlWordFromFloatingPoint;
            instr.set_dest(dest);
            instr.set_source(source);

            // undefined if source is not 31 or 0
            if (!(source == 0x1F || source == 0x0))
                Errors::die("ee_jittrans.cpp: CFC1 has invalid source register %d", source);

            instrs.push_back(instr);
            break;
        }
        case 0x04:
            // MTC1
        {
            uint8_t dest = (opcode >> 11) & 0x1F;
            uint8_t source = (opcode >> 16) & 0x1F;
            instr.op = IR::Opcode::MoveToCoprocessor1;
            instr.set_dest(dest);
            instr.set_source(source);
            instrs.push_back(instr);
            break;
        }
        case 0x06:
            // CTC1
        {
            uint8_t dest = (opcode >> 11) & 0x1F;
            uint8_t source = (opcode >> 16) & 0x1F;
            instr.op = IR::Opcode::MoveControlWordToFloatingPoint;
            instr.set_dest(dest);
            instr.set_source(source);

            // undefined
            if (dest != 0x1F)
                break;

            instrs.push_back(instr);
            break;
        }
        case 0x08:
            // BC1
            instr.op = IR::Opcode::BranchCop1;
            instr.set_is_likely((opcode >> 17) & 1);
            instr.set_field((opcode >> 16) & 1);
            instr.set_jump_dest(branch_offset_ee(opcode, PC));
            instr.set_jump_fail_dest(PC + 8);
            instrs.push_back(instr);
            break;
        case 0x10:
            // FPU Operation
            translate_op_cop1_fpu(opcode, PC, instrs);
            break;
        case 0x14:
            // CVT.S.W
        {
            uint8_t dest = (opcode >> 6) & 0x1F;
            uint8_t source = (opcode >> 11) & 0x1F;
            instr.set_dest(dest);
            instr.set_source(source);
            instr.op = IR::Opcode::FixedPointConvertToFloatingPoint;
            instrs.push_back(instr);
            break;
        }
        default:
            Errors::die("[EE_JIT] Unrecognized cop1 op $%02X", op);
    }
}

void EE_JitTranslator::translate_op_cop1_fpu(uint32_t opcode, uint32_t PC, std::vector<IR::Instruction>& instrs) const
{
    uint8_t op = opcode & 0x3F;
    IR::Instruction instr;
    instr.set_opcode(opcode);

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
            instrs.push_back(instr);
            break;
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
            instrs.push_back(instr);
            break;
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
            instrs.push_back(instr);
            break;
        }
        case 0x03:
            // DIV.S
        {
            uint8_t dest = (opcode >> 6) & 0x1F;
            uint8_t source = (opcode >> 11) & 0x1F;
            uint8_t source2 = (opcode >> 16) & 0x1F;
            instr.set_dest(dest);
            instr.set_source(source);
            instr.set_source2(source2);
            instr.op = IR::Opcode::FloatingPointDivide;
            instrs.push_back(instr);
            break;
        }
        case 0x04:
            // SQRT.S
        {
            uint8_t dest = (opcode >> 6) & 0x1F;
            uint8_t source = (opcode >> 16) & 0x1F;
            instr.op = IR::Opcode::FloatingPointSquareRoot;
            instr.set_dest(dest);
            instr.set_source(source);
            instrs.push_back(instr);
            break;
        }
        case 0x05:
            // ABS.S
        {
            uint8_t dest = (opcode >> 6) & 0x1F;
            uint8_t source = (opcode >> 11) & 0x1F;
            instr.op = IR::Opcode::FloatingPointAbsoluteValue;
            instr.set_dest(dest);
            instr.set_source(source);
            instrs.push_back(instr);
            break;
        }
        case 0x06:
            // MOV.S
        {
            uint8_t dest = (opcode >> 6) & 0x1F;
            uint8_t source = (opcode >> 11) & 0x1F;
            if (dest == source)
            {
                // NOP
                break;
            }
            instr.op = IR::Opcode::MoveXmmReg;
            instr.set_dest(dest);
            instr.set_source(source);
            instrs.push_back(instr);
            break;
        }
        case 0x07:
            // NEG.S
        {
            uint8_t dest = (opcode >> 6) & 0x1F;
            uint8_t source = (opcode >> 11) & 0x1F;
            instr.op = IR::Opcode::FloatingPointNegate;
            instr.set_dest(dest);
            instr.set_source(source);
            instrs.push_back(instr);
            break;
        }
        case 0x16:
            // RSQRT.S
        {
            uint8_t dest = (opcode >> 6) & 0x1F;
            uint8_t source = (opcode >> 11) & 0x1F;
            uint8_t source2 = (opcode >> 16) & 0x1F;
            instr.set_dest(dest);
            instr.set_source(source);
            instr.set_source2(source2);
            instr.op = IR::Opcode::FloatingPointReciprocalSquareRoot;
            instrs.push_back(instr);
            break;
        }
        case 0x18:
            // ADDA.S
        {
            uint8_t source = (opcode >> 11) & 0x1F;
            uint8_t source2 = (opcode >> 16) & 0x1F;
            instr.set_dest((int)FPU_SpecialReg::ACC);
            instr.set_source(source);
            instr.set_source2(source2);
            instr.op = IR::Opcode::FloatingPointAdd;
            instrs.push_back(instr);
            break;
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
            instrs.push_back(instr);
            break;
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
            instrs.push_back(instr);
            break;
        }
        case 0x1C:
            // MADD.S
        {
            uint8_t dest = (opcode >> 6) & 0x1F;
            uint8_t source = (opcode >> 11) & 0x1F;
            uint8_t source2 = (opcode >> 16) & 0x1F;
            instr.set_dest(dest);
            instr.set_source(source);
            instr.set_source2(source2);
            instr.op = IR::Opcode::FloatingPointMultiplyAdd;
            instrs.push_back(instr);
            break;
        }
        case 0x1D:
            // MSUB.S
        {
            uint8_t dest = (opcode >> 6) & 0x1F;
            uint8_t source = (opcode >> 11) & 0x1F;
            uint8_t source2 = (opcode >> 16) & 0x1F;
            instr.set_dest(dest);
            instr.set_source(source);
            instr.set_source2(source2);
            instr.op = IR::Opcode::FloatingPointMultiplySubtract;
            instrs.push_back(instr);
            break;
        }
        case 0x1E:
            // MADDA.S
        {
            uint8_t source = (opcode >> 11) & 0x1F;
            uint8_t source2 = (opcode >> 16) & 0x1F;
            instr.set_dest((int)FPU_SpecialReg::ACC);
            instr.set_source(source);
            instr.set_source2(source2);
            instr.op = IR::Opcode::FloatingPointMultiplyAdd;
            instrs.push_back(instr);
            break;
        }
        case 0x1F:
            // MSUBA.S
        {
            uint8_t source = (opcode >> 11) & 0x1F;
            uint8_t source2 = (opcode >> 16) & 0x1F;
            instr.set_dest((int)FPU_SpecialReg::ACC);
            instr.set_source(source);
            instr.set_source2(source2);
            instr.op = IR::Opcode::FloatingPointMultiplySubtract;
            instrs.push_back(instr);
            break;
        }
        case 0x24:
            // CVT.W.S
        {
            uint8_t dest = (opcode >> 6) & 0x1F;
            uint8_t source = (opcode >> 11) & 0x1F;
            instr.set_dest(dest);
            instr.set_source(source);
            instr.op = IR::Opcode::FloatingPointConvertToFixedPoint;
            instrs.push_back(instr);
            break;
        }
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
            instrs.push_back(instr);
            break;
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
            instrs.push_back(instr);
            break;
        }
        case 0x30:
            // C.F.S
        {
            instr.op = IR::Opcode::FloatingPointClearControl;
            instrs.push_back(instr);
            break;
        }
        case 0x32:
            // C.EQ.S
        {
            uint8_t source = (opcode >> 11) & 0x1F;
            uint8_t source2 = (opcode >> 16) & 0x1F;
            instr.op = IR::Opcode::FloatingPointCompareEqual;
            instr.set_source(source);
            instr.set_source2(source2);
            instrs.push_back(instr);
            break;
        }
        case 0x34:
            // C.LT.S
        {
            uint8_t source = (opcode >> 11) & 0x1F;
            uint8_t source2 = (opcode >> 16) & 0x1F;
            instr.op = IR::Opcode::FloatingPointCompareLessThan;
            instr.set_source(source);
            instr.set_source2(source2);
            instrs.push_back(instr);
            break;
        }
        case 0x36:
            // C.LE.S
        {
            uint8_t source = (opcode >> 11) & 0x1F;
            uint8_t source2 = (opcode >> 16) & 0x1F;
            instr.op = IR::Opcode::FloatingPointCompareLessThanOrEqual;
            instr.set_source(source);
            instr.set_source2(source2);
            instrs.push_back(instr);
            break;
        }
        default:
            Errors::die("[EE_JIT] Unrecognized fpu op $%02X", op);
    }
}

void EE_JitTranslator::translate_op_cop2(uint32_t opcode, uint32_t PC, std::vector<IR::Instruction>& instrs)
{
    uint8_t op = (opcode >> 21) & 0x1F;
    IR::Instruction instr;
    // run branch operation again if COP2 in block is in the delay slot
    if (branch_op)
        instr.set_return_addr(PC - 4);
    else
        instr.set_return_addr(PC);
    instr.set_cycle_count(cycle_count);
    instr.op = IR::Opcode::WaitVU0;
    instr.set_opcode(0);
    instrs.push_back(instr);
    instr.set_opcode(opcode);

    switch (op)
    {
        case 0x01:
            // QMFC2
            Errors::print_warning("[EE_JIT] Unrecognized cop2 op QMFC2\n", op);
            fallback_interpreter(instr, opcode);
            instrs.push_back(instr);
            break;
        case 0x02:
            // CFC2
            Errors::print_warning("[EE_JIT] Unrecognized cop2 op CFC2\n", op);
            fallback_interpreter(instr, opcode);
            instrs.push_back(instr);
            break;
        case 0x05:
            // QMTC2
            Errors::print_warning("[EE_JIT] Unrecognized cop2 op QMTC2\n", op);
            fallback_interpreter(instr, opcode);
            instrs.push_back(instr);
            break;
        case 0x06:
            // CTC2
            Errors::print_warning("[EE_JIT] Unrecognized cop2 op CTC2\n", op);
            fallback_interpreter(instr, opcode);
            instrs.push_back(instr);
            break;
        case 0x08:
            // BC2
            instr.op = IR::Opcode::BranchCop2;
            instr.set_is_likely((opcode >> 17) & 1);
            instr.set_field((opcode >> 16) & 1);
            instr.set_jump_dest(branch_offset_ee(opcode, PC));
            instr.set_jump_fail_dest(PC + 8);
            instrs.push_back(instr);
            break;
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
            translate_op_cop2_special(opcode, PC, instrs);
            break;
        default:
            Errors::die("[EE_JIT] Unrecognized cop2 op $%02X", op);
    }
}

void EE_JitTranslator::translate_op_cop2_special(uint32_t opcode, uint32_t PC, std::vector<IR::Instruction>& instrs)
{
    uint8_t op = opcode & 0x3F;
    IR::Instruction instr;
    instr.set_opcode(opcode);

    switch (op)
    {
        case 0x00:
        case 0x01:
        case 0x02:
        case 0x03:
            // VADDBC
            Errors::print_warning("[EE_JIT] Unrecognized cop2 special op VADDBC\n", op);
            fallback_interpreter(instr, opcode);
            instrs.push_back(instr);
            break;
        case 0x04:
        case 0x05:
        case 0x06:
        case 0x07:
            // VSUBBC
            Errors::print_warning("[EE_JIT] Unrecognized cop2 special op VSUBBC\n", op);
            fallback_interpreter(instr, opcode);
            instrs.push_back(instr);
            break;
        case 0x08:
        case 0x09:
        case 0x0A:
        case 0x0B:
            // VMADDBC
            Errors::print_warning("[EE_JIT] Unrecognized cop2 special op VMADDBC\n", op);
            fallback_interpreter(instr, opcode);
            instrs.push_back(instr);
            break;
        case 0x0C:
        case 0x0D:
        case 0x0E:
        case 0x0F:
            // VMSUBBC
            Errors::print_warning("[EE_JIT] Unrecognized cop2 special op VMSUBBC\n", op);
            fallback_interpreter(instr, opcode);
            instrs.push_back(instr);
            break;
        case 0x10:
        case 0x11:
        case 0x12:
        case 0x13:
            // VMAXBC
            Errors::print_warning("[EE_JIT] Unrecognized cop2 special op VMAXBC\n", op);
            fallback_interpreter(instr, opcode);
            instrs.push_back(instr);
            break;
        case 0x14:
        case 0x15:
        case 0x16:
        case 0x17:
            // VMINIBC
            Errors::print_warning("[EE_JIT] Unrecognized cop2 special op VMINIBC\n", op);
            fallback_interpreter(instr, opcode);
            instrs.push_back(instr);
            break;
        case 0x18:
        case 0x19:
        case 0x1A:
        case 0x1B:
            // VMULBC
            Errors::print_warning("[EE_JIT] Unrecognized cop2 special op VMULBC\n", op);
            fallback_interpreter(instr, opcode);
            instrs.push_back(instr);
            break;
        case 0x1C:
            // VMULQ
            Errors::print_warning("[EE_JIT] Unrecognized cop2 special op VMULQ\n", op);
            fallback_interpreter(instr, opcode);
            instrs.push_back(instr);
            break;
        case 0x1D:
            // VMAXI
            Errors::print_warning("[EE_JIT] Unrecognized cop2 special op VMAXI\n", op);
            fallback_interpreter(instr, opcode);
            instrs.push_back(instr);
            break;
        case 0x1E:
            // VMULI
            Errors::print_warning("[EE_JIT] Unrecognized cop2 special op VMAXI\n", op);
            fallback_interpreter(instr, opcode);
            instrs.push_back(instr);
            break;
        case 0x1F:
            // VMINII
            Errors::print_warning("[EE_JIT] Unrecognized cop2 special op VMINII\n", op);
            fallback_interpreter(instr, opcode);
            instrs.push_back(instr);
            break;
        case 0x20:
            // VADDQ
            Errors::print_warning("[EE_JIT] Unrecognized cop2 special op VADDQ\n", op);
            fallback_interpreter(instr, opcode);
            instrs.push_back(instr);
            break;
        case 0x21:
            // VMADDQ
            Errors::print_warning("[EE_JIT] Unrecognized cop2 special op VMADDQ\n", op);
            fallback_interpreter(instr, opcode);
            instrs.push_back(instr);
            break;
        case 0x22:
            // VADDI
            Errors::print_warning("[EE_JIT] Unrecognized cop2 special op VADDI\n", op);
            fallback_interpreter(instr, opcode);
            instrs.push_back(instr);
            break;
        case 0x23:
            // VMADDI
            Errors::print_warning("[EE_JIT] Unrecognized cop2 special op VMADDI\n", op);
            fallback_interpreter(instr, opcode);
            instrs.push_back(instr);
            break;
        case 0x24:
            // VSUBQ
            Errors::print_warning("[EE_JIT] Unrecognized cop2 special op VSUBQ\n", op);
            fallback_interpreter(instr, opcode);
            instrs.push_back(instr);
            break;
        case 0x25:
            // VMSUBQ
            Errors::print_warning("[EE_JIT] Unrecognized cop2 special op VMSUBQ\n", op);
            fallback_interpreter(instr, opcode);
            instrs.push_back(instr);
            break;
        case 0x26:
            // VSUBI
            Errors::print_warning("[EE_JIT] Unrecognized cop2 special op VSUBI\n", op);
            fallback_interpreter(instr, opcode);
            instrs.push_back(instr);
            break;
        case 0x27:
            // VMSUBI
            Errors::print_warning("[EE_JIT] Unrecognized cop2 special op VMSUBI\n", op);
            fallback_interpreter(instr, opcode);
            instrs.push_back(instr);
            break;
        case 0x28:
            // VADD
        {
            uint8_t dest = (opcode >> 6) & 0x1F;
            uint8_t source = (opcode >> 11) & 0x1F;
            uint8_t source2 = (opcode >> 16) & 0x1F;
            uint8_t field = (opcode >> 21) & 0xF;
            instr.set_dest(dest);
            instr.set_source(source);
            instr.set_source2(source2);
            instr.set_field(field);
            instr.op = IR::Opcode::VAddVectors;
            instrs.push_back(instr);
            break;
        }
        case 0x29:
            // VMADD
            Errors::print_warning("[EE_JIT] Unrecognized cop2 special op VMADDI\n", op);
            fallback_interpreter(instr, opcode);
            instrs.push_back(instr);
            break;
        case 0x2A:
            // VMUL
        {
            uint8_t dest = (opcode >> 6) & 0x1F;
            uint8_t source = (opcode >> 11) & 0x1F;
            uint8_t source2 = (opcode >> 16) & 0x1F;
            uint8_t field = (opcode >> 21) & 0xF;
            instr.set_dest(dest);
            instr.set_source(source);
            instr.set_source2(source2);
            instr.set_field(field);
            instr.op = IR::Opcode::VMulVectors;
            instrs.push_back(instr);
            break;
        }
        case 0x2B:
            // VMAX
            Errors::print_warning("[EE_JIT] Unrecognized cop2 special op VMAX\n", op);
            fallback_interpreter(instr, opcode);
            instrs.push_back(instr);
            break;
        case 0x2C:
            // VSUB
        {
            uint8_t dest = (opcode >> 6) & 0x1F;
            uint8_t source = (opcode >> 11) & 0x1F;
            uint8_t source2 = (opcode >> 16) & 0x1F;
            uint8_t field = (opcode >> 21) & 0xF;
            instr.set_dest(dest);
            instr.set_source(source);
            instr.set_source2(source2);
            instr.set_field(field);
            instr.op = IR::Opcode::VSubVectors;
            instrs.push_back(instr);
            break;
        }
        case 0x2D:
            // VMSUB
            Errors::print_warning("[EE_JIT] Unrecognized cop2 special op VMSUB\n", op);
            fallback_interpreter(instr, opcode);
            instrs.push_back(instr);
            break;
        case 0x2E:
            // VOPMSUB
            Errors::print_warning("[EE_JIT] Unrecognized cop2 special op VOPMSUB\n", op);
            fallback_interpreter(instr, opcode);
            instrs.push_back(instr);
            break;
        case 0x2F:
            // VMINI
            Errors::print_warning("[EE_JIT] Unrecognized cop2 special op VMINI\n", op);
            fallback_interpreter(instr, opcode);
            instrs.push_back(instr);
            break;
        case 0x30:
            // VIADD
            Errors::print_warning("[EE_JIT] Unrecognized cop2 special op VIADD\n", op);
            fallback_interpreter(instr, opcode);
            instrs.push_back(instr);
            break;
        case 0x31:
            // VISUB
            Errors::print_warning("[EE_JIT] Unrecognized cop2 special op VISUB\n", op);
            fallback_interpreter(instr, opcode);
            instrs.push_back(instr);
            break;
        case 0x32:
            // VIADDI
            Errors::print_warning("[EE_JIT] Unrecognized cop2 special op VIADDI\n", op);
            fallback_interpreter(instr, opcode);
            instrs.push_back(instr);
            break;
        case 0x34:
            // VIAND
            Errors::print_warning("[EE_JIT] Unrecognized cop2 special op VIAND\n", op);
            fallback_interpreter(instr, opcode);
            instrs.push_back(instr);
            break;
        case 0x35:
            // VIOR
            Errors::print_warning("[EE_JIT] Unrecognized cop2 special op VIOR\n", op);
            fallback_interpreter(instr, opcode);
            instrs.push_back(instr);
            break;
        case 0x38:
            // VCALLMS
        {
            instr.op = IR::Opcode::VCallMS;
            uint32_t imm = (opcode >> 6) & 0x7FFF;
            imm *= 8;
            instr.set_source(imm);
            instrs.push_back(instr);
            break;
        }
        case 0x39:
            // VCALLMSR
        {
            instr.op = IR::Opcode::VCallMSR;
            instrs.push_back(instr);
            break;
        }
        case 0x3C:
        case 0x3D:
        case 0x3E:
        case 0x3F:
            // COP2 Special2 Operation
            translate_op_cop2_special2(opcode, PC, instrs);
            break;
        default:
            Errors::die("[EE_JIT] Unrecognized cop2 special op $%02X", op);
    }
}

void EE_JitTranslator::translate_op_cop2_special2(uint32_t opcode, uint32_t PC, std::vector<IR::Instruction>& instrs) const
{
    uint8_t op = opcode & 0x3F;
    IR::Instruction instr;
    instr.set_opcode(opcode);

    switch (op)
    {
        case 0x00:
        case 0x01:
        case 0x02:
        case 0x03:
            // VADDABC
            Errors::print_warning("[EE_JIT] Unrecognized cop2 special2 op VADDABC\n", op);
            fallback_interpreter(instr, opcode);
            instrs.push_back(instr);
            break;
        case 0x04:
        case 0x05:
        case 0x06:
        case 0x07:
            // VSUBABC
            Errors::print_warning("[EE_JIT] Unrecognized cop2 special2 op VSUBABC\n", op);
            fallback_interpreter(instr, opcode);
            instrs.push_back(instr);
            break;
        case 0x08:
        case 0x09:
        case 0x0A:
        case 0x0B:
            // VMADDABC
            Errors::print_warning("[EE_JIT] Unrecognized cop2 special2 op VMADDABC\n", op);
            fallback_interpreter(instr, opcode);
            instrs.push_back(instr);
            break;
        case 0x0C:
        case 0x0D:
        case 0x0E:
        case 0x0F:
            // VMSUBABC
            Errors::print_warning("[EE_JIT] Unrecognized cop2 special2 op VMSUBABC\n", op);
            fallback_interpreter(instr, opcode);
            instrs.push_back(instr);
            break;
        case 0x10:
            // VITOF0
            Errors::print_warning("[EE_JIT] Unrecognized cop2 special2 op VITOF0\n", op);
            fallback_interpreter(instr, opcode);
            instrs.push_back(instr);
            break;
        case 0x11:
            // VITOF4
            Errors::print_warning("[EE_JIT] Unrecognized cop2 special2 op VITOF4\n", op);
            fallback_interpreter(instr, opcode);
            instrs.push_back(instr);
            break;
        case 0x12:
            // VITOF12
            Errors::print_warning("[EE_JIT] Unrecognized cop2 special2 op VITOF12\n", op);
            fallback_interpreter(instr, opcode);
            instrs.push_back(instr);
            break;
        case 0x13:
            // VITOF15
            Errors::print_warning("[EE_JIT] Unrecognized cop2 special2 op VITOF15\n", op);
            fallback_interpreter(instr, opcode);
            instrs.push_back(instr);
            break;
        case 0x14:
            // VFTOI0
            Errors::print_warning("[EE_JIT] Unrecognized cop2 special2 op VFTOI0\n", op);
            fallback_interpreter(instr, opcode);
            instrs.push_back(instr);
            break;
        case 0x15:
            // VFTOI4
            Errors::print_warning("[EE_JIT] Unrecognized cop2 special2 op VFTOI4\n", op);
            fallback_interpreter(instr, opcode);
            instrs.push_back(instr);
            break;
        case 0x16:
            // VFTOI12
            Errors::print_warning("[EE_JIT] Unrecognized cop2 special2 op VFTOI12\n", op);
            fallback_interpreter(instr, opcode);
            instrs.push_back(instr);
            break;
        case 0x17:
            // VFTOI15
            Errors::print_warning("[EE_JIT] Unrecognized cop2 special2 op VFTOI15\n", op);
            fallback_interpreter(instr, opcode);
            instrs.push_back(instr);
            break;
        case 0x18:
        case 0x19:
        case 0x1A:
        case 0x1B:
            // VMULABC
            Errors::print_warning("[EE_JIT] Unrecognized cop2 special2 op VMULABC\n", op);
            fallback_interpreter(instr, opcode);
            instrs.push_back(instr);
            break;
        case 0x1C:
            // VMULAQ
            Errors::print_warning("[EE_JIT] Unrecognized cop2 special2 op VMULAQ\n", op);
            fallback_interpreter(instr, opcode);
            instrs.push_back(instr);
            break;
        case 0x1D:
            // VABS
        {
            uint8_t source = (opcode >> 11) & 0x1F;
            uint8_t dest = (opcode >> 16) & 0x1F;
            uint8_t field = (opcode >> 21) & 0xF;

            if (!dest)
            {
                // NOP
                break;
            }

            instr.set_dest(dest);
            instr.set_source(source);
            instr.set_field(field);
            instr.set_return_addr(PC);
            instr.op = IR::Opcode::VAbs;
            instrs.push_back(instr);
            break;
        }
        case 0x1E:
            // VMULAI
            Errors::print_warning("[EE_JIT] Unrecognized cop2 special2 op VMULAI\n", op);
            fallback_interpreter(instr, opcode);
            instrs.push_back(instr);
            break;
        case 0x1F:
            // VCLIP
            Errors::print_warning("[EE_JIT] Unrecognized cop2 special2 op VCLIP\n", op);
            fallback_interpreter(instr, opcode);
            instrs.push_back(instr);
            break;
        case 0x20:
            // VADDAQ
            Errors::print_warning("[EE_JIT] Unrecognized cop2 special2 op VADDAQ\n", op);
            fallback_interpreter(instr, opcode);
            instrs.push_back(instr);
            break;
        case 0x21:
            // VMADDAQ
            Errors::print_warning("[EE_JIT] Unrecognized cop2 special2 op VMADDAQ\n", op);
            fallback_interpreter(instr, opcode);
            instrs.push_back(instr);
            break;
        case 0x23:
            // VMADDAI
            Errors::print_warning("[EE_JIT] Unrecognized cop2 special2 op VMADDAI\n", op);
            fallback_interpreter(instr, opcode);
            instrs.push_back(instr);
            break;
        case 0x25:
            // VMSUBAQ
            Errors::print_warning("[EE_JIT] Unrecognized cop2 special2 op VMSUBAQ\n", op);
            fallback_interpreter(instr, opcode);
            instrs.push_back(instr);
            break;
        case 0x27:
            // VMSUBAI
            Errors::print_warning("[EE_JIT] Unrecognized cop2 special2 op VMSUBAI\n", op);
            fallback_interpreter(instr, opcode);
            instrs.push_back(instr);
            break;
        case 0x28:
            // VADDA
            Errors::print_warning("[EE_JIT] Unrecognized cop2 special2 op VADDA\n", op);
            fallback_interpreter(instr, opcode);
            instrs.push_back(instr);
            break;
        case 0x29:
            // VMADDA
            Errors::print_warning("[EE_JIT] Unrecognized cop2 special2 op VMADDA\n", op);
            fallback_interpreter(instr, opcode);
            instrs.push_back(instr);
            break;
        case 0x2A:
            // VMULA
            Errors::print_warning("[EE_JIT] Unrecognized cop2 special2 op VMULA\n", op);
            fallback_interpreter(instr, opcode);
            instrs.push_back(instr);
            break;
        case 0x2C:
            // VSUBA
            Errors::print_warning("[EE_JIT] Unrecognized cop2 special2 op VSUBA\n", op);
            fallback_interpreter(instr, opcode);
            instrs.push_back(instr);
            break;
        case 0x2D:
            // VMSUBA
            Errors::print_warning("[EE_JIT] Unrecognized cop2 special2 op VMSUBA\n", op);
            fallback_interpreter(instr, opcode);
            instrs.push_back(instr);
            break;
        case 0x2E:
            // VOPMULA
            Errors::print_warning("[EE_JIT] Unrecognized cop2 special2 op VOPMULA\n", op);
            fallback_interpreter(instr, opcode);
            instrs.push_back(instr);
            break;
        case 0x2F:
            // VNOP ?
            Errors::print_warning("[EE_JIT] Unrecognized cop2 special2 op VNOP (?)\n", op);
            fallback_interpreter(instr, opcode);
            instrs.push_back(instr);
            break;
        case 0x30:
            // VMOVE
            Errors::print_warning("[EE_JIT] Unrecognized cop2 special2 op VMOVE\n", op);
            fallback_interpreter(instr, opcode);
            instrs.push_back(instr);
            break;
        case 0x31:
            // VMR32
            Errors::print_warning("[EE_JIT] Unrecognized cop2 special2 op VMR32\n", op);
            fallback_interpreter(instr, opcode);
            instrs.push_back(instr);
            break;
        case 0x34:
            // VLQI
            Errors::print_warning("[EE_JIT] Unrecognized cop2 special2 op VLQI\n", op);
            fallback_interpreter(instr, opcode);
            instrs.push_back(instr);
            break;
        case 0x35:
            // VSQI
            Errors::print_warning("[EE_JIT] Unrecognized cop2 special2 op VSQI\n", op);
            fallback_interpreter(instr, opcode);
            instrs.push_back(instr);
            break;
        case 0x36:
            // VLQD
            Errors::print_warning("[EE_JIT] Unrecognized cop2 special2 op VLQD\n", op);
            fallback_interpreter(instr, opcode);
            instrs.push_back(instr);
            break;
        case 0x37:
            // VSQD
            Errors::print_warning("[EE_JIT] Unrecognized cop2 special2 op VSQD\n", op);
            fallback_interpreter(instr, opcode);
            instrs.push_back(instr);
            break;
        case 0x38:
            // VDIV
            Errors::print_warning("[EE_JIT] Unrecognized cop2 special2 op VDIV\n", op);
            fallback_interpreter(instr, opcode);
            instrs.push_back(instr);
            break;
        case 0x39:
            // VSQRT
            Errors::print_warning("[EE_JIT] Unrecognized cop2 special2 op VSQRT\n", op);
            fallback_interpreter(instr, opcode);
            instrs.push_back(instr);
            break;
        case 0x3A:
            // VRSQRT
            Errors::print_warning("[EE_JIT] Unrecognized cop2 special2 op VRSQRT\n", op);
            fallback_interpreter(instr, opcode);
            instrs.push_back(instr);
            break;
        case 0x3B:
            // VWAITQ
            Errors::print_warning("[EE_JIT] Unrecognized cop2 special2 op VWAITQ\n", op);
            fallback_interpreter(instr, opcode);
            instrs.push_back(instr);
            break;
        case 0x3C:
            // VMTIR
            Errors::print_warning("[EE_JIT] Unrecognized cop2 special2 op VMTIR\n", op);
            fallback_interpreter(instr, opcode);
            instrs.push_back(instr);
            break;
        case 0x3D:
            // VMFIR
            Errors::print_warning("[EE_JIT] Unrecognized cop2 special2 op VMFIR\n", op);
            fallback_interpreter(instr, opcode);
            instrs.push_back(instr);
            break;
        case 0x3E:
            // VILWR
            Errors::print_warning("[EE_JIT] Unrecognized cop2 special2 op VILWR\n", op);
            fallback_interpreter(instr, opcode);
            instrs.push_back(instr);
            break;
        case 0x3F:
            // VISWR
            Errors::print_warning("[EE_JIT] Unrecognized cop2 special2 op VISWR\n", op);
            fallback_interpreter(instr, opcode);
            instrs.push_back(instr);
            break;
        case 0x40:
            // VRNEXT
            Errors::print_warning("[EE_JIT] Unrecognized cop2 special2 op VRNEXT\n", op);
            fallback_interpreter(instr, opcode);
            instrs.push_back(instr);
            break;
        case 0x41:
            // VRGET
            Errors::print_warning("[EE_JIT] Unrecognized cop2 special2 op VRGET\n", op);
            fallback_interpreter(instr, opcode);
            instrs.push_back(instr);
            break;
        case 0x42:
            // VRINIT
            Errors::print_warning("[EE_JIT] Unrecognized cop2 special2 op VRINIT\n", op);
            fallback_interpreter(instr, opcode);
            instrs.push_back(instr);
            break;
        case 0x43:
            // VRXOR
            Errors::print_warning("[EE_JIT] Unrecognized cop2 special2 op VRXOR\n", op);
            fallback_interpreter(instr, opcode);
            instrs.push_back(instr);
            break;
        default:
            Errors::die("[EE_JIT] Unrecognized cop2 special2 op $%02X", op);
    }
}

void EE_JitTranslator::fallback_interpreter(IR::Instruction& instr, uint32_t instr_word) const noexcept
{
    instr.op = IR::Opcode::FallbackInterpreter;
    instr.set_opcode(instr_word);
}

/**
 * Determine base operation information and which instructions need to be swapped
 */
void EE_JitTranslator::interpreter_pass(EmotionEngine &ee, uint32_t pc)
{
    //TODO
}

void EE_JitTranslator::op_vector_by_scalar(IR::Instruction &instr, uint32_t upper, VU_SpecialReg scalar) const
{
    instr.set_dest((upper >> 6) & 0x1F);
    instr.set_source((upper >> 11) & 0x1F);
    instr.set_field((upper >> 21) & 0xF);

    if (scalar == VU_Regular)
    {
        instr.set_source2((upper >> 16) & 0x1F);
        instr.set_bc(upper & 0x3);
    }
    else
    {
        instr.set_source2(scalar);
        instr.set_bc(0);
    }
}
