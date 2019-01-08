#include "vu_jittrans.hpp"
#include "../errors.hpp"

namespace VU_JitTranslator
{

uint32_t branch_offset(uint32_t instr, uint32_t PC)
{
    int16_t imm = instr & 0x7FF;
    imm = ((int16_t)(imm << 5)) >> 5;
    imm *= 8;
    return PC + imm + 8;
}

IR::Block translate(uint32_t PC, uint8_t* instr_mem)
{
    IR::Block block;

    bool block_end = false;
    bool delay_slot = false;
    bool ebit = false;
    while (!block_end)
    {
        std::vector<IR::Instruction> upper_instrs;
        std::vector<IR::Instruction> lower_instrs;
        if (delay_slot)
            block_end = true;

        uint32_t upper = *(uint32_t*)&instr_mem[PC + 4];
        uint32_t lower = *(uint32_t*)&instr_mem[PC];

        translate_upper(upper_instrs, upper);

        //LOI - upper goes first, lower gets loaded into I register
        if (upper & (1 << 31))
        {
            IR::Instruction loi(IR::Opcode::LoadFloatConst);
            loi.set_dest(VU_SpecialReg::I);
            loi.set_source(lower);
            for (unsigned int i = 0; i < upper_instrs.size(); i++)
                block.add_instr(upper_instrs[i]);
            block.add_instr(loi);
        }
        else
        {
            translate_lower(lower_instrs, lower, PC);

            //TODO: Check for upper/lower op swapping
            for (unsigned int i = 0; i < upper_instrs.size(); i++)
                block.add_instr(upper_instrs[i]);

            if (lower_instrs.size())
            {
                for (unsigned int i = 0; i < lower_instrs.size(); i++)
                    block.add_instr(lower_instrs[i]);
                if (lower_instrs[0].is_jump())
                {
                    if (delay_slot)
                        Errors::die("[VU JIT] Branch in delay slot");
                    delay_slot = true;
                }
            }
        }

        //End of microprogram delay slot
        if (upper & (1 << 30))
        {
            ebit = true;
            if (delay_slot)
                Errors::die("[VU JIT] End microprogram in delay slot");
            delay_slot = true;
        }
        else if (ebit)
        {
            IR::Instruction instr(IR::Opcode::Stop);
            instr.set_jump_dest(PC + 8);
            block.add_instr(instr);
        }

        PC += 8;
        block.add_cycle();
    }

    return block;
}

void translate_upper(std::vector<IR::Instruction>& instrs, uint32_t upper)
{
    uint8_t op = upper & 0x3F;
    IR::Instruction instr;
    switch (op)
    {
        case 0x08:
        case 0x09:
        case 0x0A:
        case 0x0B:
            //MADDbc
            instr.op = IR::Opcode::VMaddVectorByScalar;
            instr.set_dest((upper >> 6) & 0x1F);
            instr.set_source((upper >> 11) & 0x1F);
            instr.set_source2((upper >> 16) & 0x1F);
            instr.set_bc(upper & 0x3);
            instr.set_dest_field((upper >> 21) & 0xF);
            break;
        case 0x18:
        case 0x19:
        case 0x1A:
        case 0x1B:
            //MULbc
            instr.op = IR::Opcode::VMulVectorByScalar;
            instr.set_dest((upper >> 6) & 0x1F);
            instr.set_source((upper >> 11) & 0x1F);
            instr.set_source2((upper >> 16) & 0x1F);
            instr.set_bc(upper & 0x3);
            instr.set_dest_field((upper >> 21) & 0xF);
            break;
        case 0x3C:
        case 0x3D:
        case 0x3E:
        case 0x3F:
            upper_special(instrs, upper);
            return;
        default:
            Errors::die("[VU_JIT] Unrecognized upper op $%02X", op);
    }
    instrs.push_back(instr);
}

void upper_special(std::vector<IR::Instruction> &instrs, uint32_t upper)
{
    uint8_t op = (upper & 0x3) | ((upper >> 4) & 0x7C);
    IR::Instruction instr;
    switch (op)
    {
        case 0x08:
        case 0x09:
        case 0x0A:
        case 0x0B:
            //MADDAbc
            instr.op = IR::Opcode::VMaddVectorByScalar;
            instr.set_dest(VU_SpecialReg::ACC);
            instr.set_source((upper >> 11) & 0x1F);
            instr.set_source2((upper >> 16) & 0x1F);
            instr.set_bc(upper & 0x3);
            instr.set_dest_field((upper >> 21) & 0xF);
            break;
        case 0x18:
        case 0x19:
        case 0x1A:
        case 0x1B:
            //MULAbc
            instr.op = IR::Opcode::VMulVectorByScalar;
            instr.set_dest(VU_SpecialReg::ACC);
            instr.set_source((upper >> 11) & 0x1F);
            instr.set_source2((upper >> 16) & 0x1F);
            instr.set_bc(upper & 0x3);
            instr.set_dest_field((upper >> 21) & 0xF);
            break;
        case 0x2F:
        case 0x30:
            //NOP - no need to add an instruction
            return;
        default:
            Errors::die("[VU_JIT] Unrecognized upper special op $%02X", op);
    }
    instrs.push_back(instr);
}

void translate_lower(std::vector<IR::Instruction>& instrs, uint32_t lower, uint32_t PC)
{
    if (lower & (1 << 31))
    {
        //0x8000033C encodes "move vf0, vf0", effectively a nop. We can safely discard it if it occurs.
        if (lower != 0x8000033C)
            lower1(instrs, lower);
    }
    else
        lower2(instrs, lower, PC);
}

void lower1(std::vector<IR::Instruction> &instrs, uint32_t lower)
{
    uint8_t op = lower & 0x3F;
    IR::Instruction instr;
    switch (op)
    {
        case 0x3C:
        case 0x3D:
        case 0x3E:
        case 0x3F:
            lower1_special(instrs, lower);
            return;
        default:
            Errors::die("[VU_JIT] Unrecognized lower1 op $%02X", op);
    }
    instrs.push_back(instr);
}

void lower1_special(std::vector<IR::Instruction> &instrs, uint32_t lower)
{
    uint8_t op = (lower & 0x3) | ((lower >> 4) & 0x7C);
    IR::Instruction instr;
    switch (op)
    {
        case 0x34:
            //LQI
            instr.op = IR::Opcode::LoadQuadInc;
            instr.set_dest_field((lower >> 21) & 0xF);
            instr.set_dest((lower >> 16) & 0x1F);
            instr.set_base((lower >> 11) & 0x1F);
            break;
        case 0x35:
            //SQI
            instr.op = IR::Opcode::StoreQuadInc;
            instr.set_dest_field((lower >> 21) & 0xF);
            instr.set_base((lower >> 16) & 0x1F);
            instr.set_source((lower >> 11) & 0x1F);
            break;
        default:
            Errors::die("[VU_JIT] Unrecognized lower1 special op $%02X", op);
    }
    instrs.push_back(instr);
}

void lower2(std::vector<IR::Instruction> &instrs, uint32_t lower, uint32_t PC)
{
    uint8_t op = (lower >> 25) & 0x7F;
    IR::Instruction instr;
    switch (op)
    {
        case 0x08:
            //IADDIU
            instr.op = IR::Opcode::AddUnsignedImm;
            instr.set_dest((lower >> 16) & 0xF);
            instr.set_source((lower >> 11) & 0xF);
        {
            uint16_t imm = lower & 0x7FF;
            imm |= ((lower >> 21) & 0xF) << 11;
            instr.set_source2(imm);
            if (instr.get_source() != instr.get_dest())
            {
                if (!instr.get_source())
                {
                    instr.op = IR::Opcode::LoadConst;
                    instr.set_source(imm);
                }
            }
            else
                instr.op = IR::Opcode::MoveIntReg;
        }
            break;
        case 0x20:
            //B
            instr.op = IR::Opcode::Jump;
            instr.set_jump_dest(branch_offset(lower, PC));
            break;
        case 0x21:
            //BAL
            instr.op = IR::Opcode::JumpAndLink;
            instr.set_jump_dest(branch_offset(lower, PC));
            instr.set_return_addr((PC + 16) / 8);
            instr.set_dest((lower >> 16) & 0xF);
            break;
        case 0x24:
            //JR
            instr.op = IR::Opcode::JumpIndirect;
            instr.set_source((lower >> 11) & 0xF);
            break;
        default:
            Errors::die("[VU_JIT] Unrecognized lower2 op $%02X", op);
    }
    instrs.push_back(instr);
}

};
