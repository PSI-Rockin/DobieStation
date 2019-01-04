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
            IR::Instruction loi(IR::Opcode::LoadConst);
            loi.set_dest(VU_SpecialReg::I);
            loi.set_source_u32(lower);
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
            if (delay_slot)
                Errors::die("[VU JIT] End microprogram in delay slot");
            delay_slot = true;
        }

        PC += 8;
        block.add_cycle();
    }

    return block;
}

void translate_upper(std::vector<IR::Instruction>& instrs, uint32_t upper)
{
    uint8_t op = upper & 0x3F;
    switch (op)
    {
        case 0x3C:
        case 0x3D:
        case 0x3E:
        case 0x3F:
            upper_special(instrs, upper);
            break;
        default:
            Errors::die("[VU_JIT] Unrecognized upper op $%02X", op);
    }
}

void upper_special(std::vector<IR::Instruction> &instrs, uint32_t upper)
{
    uint8_t op = (upper & 0x3) | ((upper >> 4) & 0x7C);
    switch (op)
    {
        case 0x2F:
        case 0x30:
            //NOP - no need to add an instruction
            break;
        default:
            Errors::die("[VU_JIT] Unrecognized upper special op $%02X", op);
    }
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
    switch (op)
    {
        default:
            Errors::die("[VU_JIT] Unrecognized lower1 op $%02X", op);
    }
}

void lower2(std::vector<IR::Instruction> &instrs, uint32_t lower, uint32_t PC)
{
    uint8_t op = (lower >> 25) & 0x7F;
    IR::Instruction instr;
    switch (op)
    {
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
            instr.set_link_register((lower >> 16) & 0x1F);
            break;
        default:
            Errors::die("[VU_JIT] Unrecognized lower2 op $%02X", op);
    }
    instrs.push_back(instr);
}

};
