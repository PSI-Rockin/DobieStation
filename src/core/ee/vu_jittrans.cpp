#include "vu_jittrans.hpp"
#include "vu_interpreter.hpp"
#include "../errors.hpp"

uint32_t branch_offset(uint32_t instr, uint32_t PC)
{
    int16_t imm = instr & 0x7FF;
    imm = ((int16_t)(imm << 5)) >> 5;
    imm *= 8;
    return PC + imm + 8;
}

IR::Block VU_JitTranslator::translate(VectorUnit &vu, uint8_t* instr_mem)
{
    IR::Block block;

    bool block_end = false;
    bool delay_slot = false;
    bool ebit = false;

    has_q_stalled = false;
    q_pipeline_delay = 0;
    cycles_this_block = 0;
    cycles_since_xgkick_update = 0;

    interpreter_pass(vu, instr_mem);

    uint16_t PC = vu.get_PC();
    while (!block_end)
    {
        std::vector<IR::Instruction> upper_instrs;
        std::vector<IR::Instruction> lower_instrs;
        if (delay_slot)
            block_end = true;

        uint32_t upper = *(uint32_t*)&instr_mem[PC + 4];
        uint32_t lower = *(uint32_t*)&instr_mem[PC];

        if (q_pipeline_delay)
        {
            q_pipeline_delay--;
            if (!q_pipeline_delay)
            {
                IR::Instruction stall(IR::Opcode::UpdateQ);
                block.add_instr(stall);
            }
        }

        translate_upper(upper_instrs, upper);
        IR::Instruction mac_update(IR::Opcode::UpdateMacPipeline);
        block.add_instr(mac_update);

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
            if (instr_info[PC].update_q_pipeline)
            {
                IR::Instruction stall(IR::Opcode::UpdateQ);
                block.add_instr(stall);
                q_pipeline_delay = 0;
            }

            translate_lower(lower_instrs, lower, PC);

            if (instr_info[PC].swap_ops)
            {
                //Lower op first
                for (unsigned int i = 0; i < lower_instrs.size(); i++)
                {
                    block.add_instr(lower_instrs[i]);
                    if (lower_instrs[i].is_jump())
                    {
                        if (delay_slot)
                            Errors::die("[VU JIT] Branch in delay slot");
                        delay_slot = true;
                    }
                }

                for (unsigned int i = 0; i < upper_instrs.size(); i++)
                {
                    block.add_instr(upper_instrs[i]);
                }
            }
            else
            {
                //Upper op first
                for (unsigned int i = 0; i < upper_instrs.size(); i++)
                {
                    block.add_instr(upper_instrs[i]);
                }

                for (unsigned int i = 0; i < lower_instrs.size(); i++)
                {
                    block.add_instr(lower_instrs[i]);
                    if (lower_instrs[i].is_jump())
                    {
                        if (delay_slot)
                            Errors::die("[VU JIT] Branch in delay slot");
                        delay_slot = true;
                    }
                }
            }
        }

        if (instr_info[PC].updates_mac_pipeline)
        {
            IR::Instruction update(IR::Opcode::UpdateMacFlags);
            block.add_instr(update);
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
        cycles_this_block++;
        cycles_since_xgkick_update++;
    }

    IR::Instruction update;
    update.op = IR::Opcode::UpdateXgkick;
    update.set_source(cycles_since_xgkick_update);
    block.add_instr(update);
    block.set_cycle_count(cycles_this_block);

    return block;
}

bool VU_JitTranslator::updates_mac_flags(uint32_t upper_instr)
{
    switch (upper_instr & 0x3F)
    {
        //MAXbc/MINIbc
        case 0x10:
        case 0x11:
        case 0x12:
        case 0x13:
        case 0x14:
        case 0x15:
        case 0x16:
        case 0x17:
            return false;
        //MAXi/MINIi
        case 0x1D:
        case 0x1F:
            return false;
        //MAX/MINI
        case 0x2B:
        case 0x2F:
            return false;
        case 0x3C:
        case 0x3D:
        case 0x3E:
        case 0x3F:
            return updates_mac_flags_special(upper_instr);
        default:
            return true;
    }
}

bool VU_JitTranslator::updates_mac_flags_special(uint32_t upper_instr)
{
    switch ((upper_instr & 0x3) | ((upper_instr >> 4) & 0x7C))
    {
        //ITOF/FTOI
        case 0x10:
        case 0x11:
        case 0x12:
        case 0x13:
        case 0x14:
        case 0x15:
        case 0x16:
        case 0x17:
            return false;
        //ABS
        case 0x1D:
            return false;
        //CLIP
        case 0x1F:
            return false;
        //NOP
        case 0x2F:
        case 0x30:
            return false;
        default:
            return true;
    }
}

/**
 * Determine which operations affect the pipelines, cause FMAC stalls, or need to be swapped
 */
void VU_JitTranslator::interpreter_pass(VectorUnit &vu, uint8_t *instr_mem)
{
    bool block_end = false;
    bool delay_slot = false;

    uint16_t PC = vu.get_PC();
    while (!block_end)
    {
        instr_info[PC].stall_amount = 0;
        instr_info[PC].swap_ops = false;
        instr_info[PC].update_q_pipeline = false;

        uint32_t upper = *(uint32_t*)&instr_mem[PC + 4];
        uint32_t lower = *(uint32_t*)&instr_mem[PC];

        if (delay_slot)
            block_end = true;

        vu.decoder.reset();

        instr_info[PC].updates_mac_pipeline = updates_mac_flags(upper);

        if ((upper & 0x7FF) == 0x1FF)
            instr_info[PC].updates_clip_pipeline = true;
        else
            instr_info[PC].updates_clip_pipeline = false;

        VU_Interpreter::upper(vu, upper);

        if (!(upper & (1 << 31)))
        {
            VU_Interpreter::lower(vu, lower);

            //WaitQ, DIV, RSQRT, SQRT
            if (((lower & 0x800007FC) == 0x800003BC))
                instr_info[PC].update_q_pipeline = true;

            //Branch instructions
            if ((lower & 0xC0000000) == 0x40000000)
            {
                if (delay_slot)
                    Errors::die("[VU_IR] End block in delay slot");
                delay_slot = true;
            }

            int write = vu.decoder.vf_write[0];
            int write1 = vu.decoder.vf_write[1];
            int read0 = vu.decoder.vf_read0[1];
            int read1 = vu.decoder.vf_read1[1];

            //If an upper op writes to a register a lower op reads from, the lower op executes first
            //Additionally, if an upper op and a lower op write to the same register, the upper op
            //has priority.
            if (write && ((write == read0 || write == read1) || (write == write1)))
                instr_info[PC].swap_ops = true;
        }
        else
            VU_Interpreter::upper(vu, upper);

        if (upper & (1 << 30))
        {
            if (delay_slot)
                Errors::die("[VU_IR] End block in delay slot");
            delay_slot = true;
        }

        PC += 8;
    }

    end_PC = PC - 8;
}

/**
 * Determine when MAC, clip, and status flags need to be updated.
 */
void VU_JitTranslator::flag_pass(VectorUnit &vu, uint8_t *instr_mem)
{
    uint16_t start_pc = vu.get_PC();

    //First we need to "look ahead" the end of a block

    uint16_t i = end_PC;
    while (i >= start_pc)
    {

        i -= 8;
    }
}

void VU_JitTranslator::update_xgkick(std::vector<IR::Instruction> &instrs)
{
    IR::Instruction update;
    update.op = IR::Opcode::UpdateXgkick;
    update.set_source(cycles_since_xgkick_update);
    cycles_since_xgkick_update = 0;
    instrs.push_back(update);
}

void VU_JitTranslator::check_q_stall(std::vector<IR::Instruction> &instrs)
{
    if (!has_q_stalled)
    {
        IR::Instruction q;
        q.op = IR::Opcode::UpdateQPipeline;
        q.set_source(cycles_this_block);
        instrs.push_back(q);
        cycles_this_block = 0;
    }
}

void VU_JitTranslator::start_q_event(std::vector<IR::Instruction> &instrs, int latency)
{
    IR::Instruction q;
    q.op = IR::Opcode::StartQEvent;
    q.set_source(latency);
    q.set_source2(cycles_this_block);
    instrs.push_back(q);
    q_pipeline_delay = latency;
    cycles_this_block = 0;
}

void VU_JitTranslator::op_vectors(IR::Instruction &instr, uint32_t upper)
{
    instr.set_dest((upper >> 6) & 0x1F);
    instr.set_source((upper >> 11) & 0x1F);
    instr.set_source2((upper >> 16) & 0x1F);
    instr.set_field((upper >> 21) & 0xF);
}

void VU_JitTranslator::op_acc_and_vectors(IR::Instruction &instr, uint32_t upper)
{
    op_vectors(instr, upper);
    instr.set_dest(VU_SpecialReg::ACC);
}

void VU_JitTranslator::op_vector_by_scalar(IR::Instruction &instr, uint32_t upper, VU_SpecialReg scalar)
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

void VU_JitTranslator::op_acc_by_scalar(IR::Instruction &instr, uint32_t upper, VU_SpecialReg scalar)
{
    op_vector_by_scalar(instr, upper, scalar);
    instr.set_dest(VU_SpecialReg::ACC);
}

void VU_JitTranslator::op_conversion(IR::Instruction &instr, uint32_t upper)
{
    instr.set_source((upper >> 11) & 0x1F);
    instr.set_dest((upper >> 16) & 0x1F);
    instr.set_field((upper >> 21) & 0xF);
}

void VU_JitTranslator::translate_upper(std::vector<IR::Instruction>& instrs, uint32_t upper)
{
    uint8_t op = upper & 0x3F;
    IR::Instruction instr;
    switch (op)
    {
        case 0x00:
        case 0x01:
        case 0x02:
        case 0x03:
            //ADDbc
            instr.op = IR::Opcode::VAddVectorByScalar;
            op_vector_by_scalar(instr, upper);
            break;
        case 0x04:
        case 0x05:
        case 0x06:
        case 0x07:
            //SUBbc
            instr.op = IR::Opcode::VSubVectorByScalar;
            op_vector_by_scalar(instr, upper);
            break;
        case 0x08:
        case 0x09:
        case 0x0A:
        case 0x0B:
            //MADDbc
            instr.op = IR::Opcode::VMaddVectorByScalar;
            op_vector_by_scalar(instr, upper);
            break;
        case 0x0C:
        case 0x0D:
        case 0x0E:
        case 0x0F:
            //MSUBbc
            instr.op = IR::Opcode::VMsubVectorByScalar;
            op_vector_by_scalar(instr, upper);
            break;
        case 0x10:
        case 0x11:
        case 0x12:
        case 0x13:
            //MAXbc
            instr.op = IR::Opcode::VMaxVectorByScalar;
            op_vector_by_scalar(instr, upper);
            break;
        case 0x14:
        case 0x15:
        case 0x16:
        case 0x17:
            //MINIbc
            instr.op = IR::Opcode::VMinVectorByScalar;
            op_vector_by_scalar(instr, upper);
            break;
        case 0x18:
        case 0x19:
        case 0x1A:
        case 0x1B:
            //MULbc
            instr.op = IR::Opcode::VMulVectorByScalar;
            op_vector_by_scalar(instr, upper);
            break;
        case 0x1C:
            //MULq
            check_q_stall(instrs);
            instr.op = IR::Opcode::VMulVectorByScalar;
            op_vector_by_scalar(instr, upper, VU_SpecialReg::Q);
            break;
        case 0x1D:
            //MAXi
            instr.op = IR::Opcode::VMaxVectorByScalar;
            op_vector_by_scalar(instr, upper, VU_SpecialReg::I);
            break;
        case 0x1E:
            //MULi
            instr.op = IR::Opcode::VMulVectorByScalar;
            op_vector_by_scalar(instr, upper, VU_SpecialReg::I);
            break;
        case 0x1F:
            //MINIi
            instr.op = IR::Opcode::VMinVectorByScalar;
            op_vector_by_scalar(instr, upper, VU_SpecialReg::I);
            break;
        case 0x20:
            //ADDq
            check_q_stall(instrs);
            instr.op = IR::Opcode::VAddVectorByScalar;
            op_vector_by_scalar(instr, upper, VU_SpecialReg::Q);
            break;
        case 0x21:
            //MADDq
            check_q_stall(instrs);
            instr.op = IR::Opcode::VMaddVectorByScalar;
            op_vector_by_scalar(instr, upper, VU_SpecialReg::Q);
            break;
        case 0x22:
            //ADDi
            instr.op = IR::Opcode::VAddVectorByScalar;
            op_vector_by_scalar(instr, upper, VU_SpecialReg::I);
            break;
        case 0x23:
            //MADDi
            instr.op = IR::Opcode::VMaddVectorByScalar;
            op_vector_by_scalar(instr, upper, VU_SpecialReg::I);
            break;
        case 0x24:
            //SUBq
            check_q_stall(instrs);
            instr.op = IR::Opcode::VSubVectorByScalar;
            op_vector_by_scalar(instr, upper, VU_SpecialReg::Q);
            break;
        case 0x25:
            //MSUBq
            check_q_stall(instrs);
            instr.op = IR::Opcode::VMsubVectorByScalar;
            op_vector_by_scalar(instr, upper, VU_SpecialReg::Q);
            break;
        case 0x26:
            //SUBi
            instr.op = IR::Opcode::VSubVectorByScalar;
            op_vector_by_scalar(instr, upper, VU_SpecialReg::I);
            break;
        case 0x27:
            //MSUBi
            instr.op = IR::Opcode::VMsubVectorByScalar;
            op_vector_by_scalar(instr, upper, VU_SpecialReg::I);
            break;
        case 0x28:
            //ADD
            instr.op = IR::Opcode::VAddVectors;
            op_vectors(instr, upper);
            break;
        case 0x29:
            //MADD
            instr.op = IR::Opcode::VMaddVectors;
            op_vectors(instr, upper);
            break;
        case 0x2A:
            //MUL
            instr.op = IR::Opcode::VMulVectors;
            op_vectors(instr, upper);
            break;
        case 0x2B:
            //MAX
            instr.op = IR::Opcode::VMaxVectors;
            op_vectors(instr, upper);
            break;
        case 0x2C:
            //SUB
            instr.op = IR::Opcode::VSubVectors;
            op_vectors(instr, upper);
            break;
        case 0x2E:
            //OPMSUB
            instr.op = IR::Opcode::VOpMsub;
            instr.set_dest((upper >> 6) & 0x1F);
            instr.set_source((upper >> 11) & 0x1F);
            instr.set_source2((upper >> 16) & 0x1F);
            break;
        case 0x2F:
            //MINI
            instr.op = IR::Opcode::VMinVectors;
            op_vectors(instr, upper);
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

void VU_JitTranslator::upper_special(std::vector<IR::Instruction> &instrs, uint32_t upper)
{
    uint8_t op = (upper & 0x3) | ((upper >> 4) & 0x7C);
    IR::Instruction instr;
    switch (op)
    {
        case 0x00:
        case 0x01:
        case 0x02:
        case 0x03:
            //ADDAbc
            instr.op = IR::Opcode::VAddVectorByScalar;
            op_acc_by_scalar(instr, upper);
            break;
        case 0x04:
        case 0x05:
        case 0x06:
        case 0x07:
            //SUBAbc
            instr.op = IR::Opcode::VSubVectorByScalar;
            op_acc_by_scalar(instr, upper);
            break;
        case 0x08:
        case 0x09:
        case 0x0A:
        case 0x0B:
            //MADDAbc
            instr.op = IR::Opcode::VMaddAccByScalar;
            op_acc_by_scalar(instr, upper);
            break;
        case 0x0C:
        case 0x0D:
        case 0x0E:
        case 0x0F:
            //MSUBAbc
            instr.op = IR::Opcode::VMsubAccByScalar;
            op_acc_by_scalar(instr, upper);
            break;
        case 0x10:
            //ITOF0
            instr.op = IR::Opcode::VFixedToFloat0;
            op_conversion(instr, upper);
            break;
        case 0x11:
            //ITOF4
            instr.op = IR::Opcode::VFixedToFloat4;
            op_conversion(instr, upper);
            break;
        case 0x12:
            //ITOF12
            instr.op = IR::Opcode::VFixedToFloat12;
            op_conversion(instr, upper);
            break;
        case 0x13:
            //ITOF15
            instr.op = IR::Opcode::VFixedToFloat15;
            op_conversion(instr, upper);
            break;
        case 0x14:
            //FTOI0
            instr.op = IR::Opcode::VFloatToFixed0;
            op_conversion(instr, upper);
            break;
        case 0x15:
            //FTOI4
            instr.op = IR::Opcode::VFloatToFixed4;
            op_conversion(instr, upper);
            break;
        case 0x16:
            //FTOI12
            instr.op = IR::Opcode::VFloatToFixed12;
            op_conversion(instr, upper);
            break;
        case 0x17:
            //FTOI15
            instr.op = IR::Opcode::VFloatToFixed15;
            op_conversion(instr, upper);
            break;
        case 0x18:
        case 0x19:
        case 0x1A:
        case 0x1B:
            //MULAbc
            instr.op = IR::Opcode::VMulVectorByScalar;
            op_acc_by_scalar(instr, upper);
            break;
        case 0x1D:
            //ABS
            instr.op = IR::Opcode::VAbs;
            instr.set_source((upper >> 11) & 0x1F);
            instr.set_dest((upper >> 16) & 0x1F);
            instr.set_field((upper >> 21) & 0xF);
            break;
        case 0x1E:
            //MULAi
            instr.op = IR::Opcode::VMulVectorByScalar;
            op_acc_by_scalar(instr, upper, VU_SpecialReg::I);
            break;
        case 0x1F:
            //CLIP
            instr.op = IR::Opcode::VClip;
            instr.set_source((upper >> 11) & 0x1F);
            instr.set_source2((upper >> 16) & 0x1F);
            break;
        case 0x20:
            //ADDAq
            check_q_stall(instrs);
            instr.op = IR::Opcode::VAddVectorByScalar;
            op_acc_by_scalar(instr, upper, VU_SpecialReg::Q);
            break;
        case 0x21:
            //MADDAq
            check_q_stall(instrs);
            instr.op = IR::Opcode::VMaddAccByScalar;
            op_acc_by_scalar(instr, upper, VU_SpecialReg::Q);
            break;
        case 0x22:
            //ADDAi
            instr.op = IR::Opcode::VAddVectorByScalar;
            op_acc_by_scalar(instr, upper, VU_SpecialReg::I);
            break;
        case 0x23:
            //MADDAi
            instr.op = IR::Opcode::VMaddAccByScalar;
            op_acc_by_scalar(instr, upper, VU_SpecialReg::I);
            break;
        case 0x24:
            //SUBAq
            check_q_stall(instrs);
            instr.op = IR::Opcode::VSubVectorByScalar;
            op_acc_by_scalar(instr, upper, VU_SpecialReg::Q);
            break;
        case 0x25:
            //MSUBAq
            check_q_stall(instrs);
            instr.op = IR::Opcode::VMsubAccByScalar;
            op_acc_by_scalar(instr, upper, VU_SpecialReg::Q);
            break;
        case 0x26:
            //SUBAi
            instr.op = IR::Opcode::VSubVectorByScalar;
            op_acc_by_scalar(instr, upper, VU_SpecialReg::I);
            break;
        case 0x27:
            //MSUBAi
            instr.op = IR::Opcode::VMsubAccByScalar;
            op_acc_by_scalar(instr, upper, VU_SpecialReg::I);
            break;
        case 0x28:
            //ADDA
            instr.op = IR::Opcode::VAddVectors;
            op_acc_and_vectors(instr, upper);
            break;
        case 0x29:
            //MADDA
            instr.op = IR::Opcode::VMaddAccAndVectors;
            op_acc_and_vectors(instr, upper);
            break;
        case 0x2A:
            //MULA
            instr.op = IR::Opcode::VMulVectors;
            op_acc_and_vectors(instr, upper);
            break;
        case 0x2C:
            //SUBA
            instr.op = IR::Opcode::VSubVectors;
            op_acc_and_vectors(instr, upper);
            break;
        case 0x2E:
            //OPMULA
            instr.op = IR::Opcode::VOpMula;
            instr.set_source((upper >> 11) & 0x1F);
            instr.set_source2((upper >> 16) & 0x1F);
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

void VU_JitTranslator::translate_lower(std::vector<IR::Instruction>& instrs, uint32_t lower, uint32_t PC)
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

void VU_JitTranslator::lower1(std::vector<IR::Instruction> &instrs, uint32_t lower)
{
    uint8_t op = lower & 0x3F;
    IR::Instruction instr;
    switch (op)
    {
        case 0x30:
            //IADD
            instr.op = IR::Opcode::AddIntReg;
            instr.set_dest((lower >> 6) & 0xF);
            instr.set_source((lower >> 11) & 0xF);
            instr.set_source2((lower >> 16) & 0xF);
            break;
        case 0x31:
            //ISUB
            instr.op = IR::Opcode::SubIntReg;
            instr.set_dest((lower >> 6) & 0xF);
            instr.set_source((lower >> 11) & 0xF);
            instr.set_source2((lower >> 16) & 0xF);
            break;
        case 0x32:
            //IADDI
        {
            int16_t imm = ((lower >> 6) & 0x1f);
            imm = ((imm & 0x10 ? 0xfff0 : 0) | (imm & 0xf));
            instr.op = IR::Opcode::AddUnsignedImm;
            instr.set_source((lower >> 11) & 0xF);
            instr.set_dest((lower >> 16) & 0xF);
            instr.set_source2((int64_t)imm);
            if (!instr.get_source())
            {
                instr.op = IR::Opcode::LoadConst;
                instr.set_source((int64_t)imm);
            }
        }
            break;
        case 0x34:
            //IAND
            instr.op = IR::Opcode::AndInt;
            instr.set_dest((lower >> 6) & 0xF);
            instr.set_source((lower >> 11) & 0xF);
            instr.set_source2((lower >> 16) & 0xF);
            //Don't add the instruction if the destination is 0 or if RD == RS == RT
            if (instr.get_dest() == 0)
                return;
            if (instr.get_dest() == instr.get_source() && instr.get_dest() == instr.get_source2())
                return;
            break;
        case 0x35:
            //IOR
            instr.op = IR::Opcode::OrInt;
            instr.set_dest((lower >> 6) & 0xF);
            instr.set_source((lower >> 11) & 0xF);
            instr.set_source2((lower >> 16) & 0xF);

            //Don't add the instruction if the destination is 0 or if RD == RS == RT
            if (instr.get_dest() == 0)
                return;
            if (instr.get_dest() == instr.get_source() && instr.get_dest() == instr.get_source2())
                return;
            break;
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

void VU_JitTranslator::lower1_special(std::vector<IR::Instruction> &instrs, uint32_t lower)
{
    uint8_t op = (lower & 0x3) | ((lower >> 4) & 0x7C);
    IR::Instruction instr;
    switch (op)
    {
        case 0x30:
            //MOVE
            instr.op = IR::Opcode::VMoveFloat;
            instr.set_source((lower >> 11) & 0x1F);
            instr.set_dest((lower >> 16) & 0x1F);
            instr.set_field((lower >> 21) & 0xF);

            //Handle NOPs
            if (!instr.get_dest() && !instr.get_source())
                return;
            break;
        case 0x31:
            //MR32
            instr.op = IR::Opcode::VMoveRotatedFloat;
            instr.set_source((lower >> 11) & 0x1F);
            instr.set_dest((lower >> 16) & 0x1F);
            instr.set_field((lower >> 21) & 0xF);
            break;
        case 0x34:
            //LQI
            instr.op = IR::Opcode::LoadQuadInc;
            instr.set_field((lower >> 21) & 0xF);
            instr.set_dest((lower >> 16) & 0x1F);
            instr.set_base((lower >> 11) & 0xF);
            break;
        case 0x35:
            //SQI
            update_xgkick(instrs);
            instr.op = IR::Opcode::StoreQuadInc;
            instr.set_field((lower >> 21) & 0xF);
            instr.set_base((lower >> 16) & 0xF);
            instr.set_source((lower >> 11) & 0x1F);
            break;
        case 0x36:
            //LQD
            instr.op = IR::Opcode::LoadQuadDec;
            instr.set_field((lower >> 21) & 0xF);
            instr.set_dest((lower >> 16) & 0x1F);
            instr.set_base((lower >> 11) & 0xF);
            break;
        case 0x37:
            //SQD
            update_xgkick(instrs);
            instr.op = IR::Opcode::StoreQuadDec;
            instr.set_field((lower >> 21) & 0xF);
            instr.set_base((lower >> 16) & 0xF);
            instr.set_source((lower >> 11) & 0x1F);
            break;
        case 0x38:
            //DIV
            start_q_event(instrs, 7);
            instr.op = IR::Opcode::VDiv;
            instr.set_source((lower >> 11) & 0x1F);
            instr.set_source2((lower >> 16) & 0x1F);
            instr.set_field((lower >> 21) & 0x3);
            instr.set_field2((lower >> 23) & 0x3);
            has_q_stalled = true;
            break;
        case 0x3A:
            //RSQRT
            start_q_event(instrs, 13);
            instr.op = IR::Opcode::VRsqrt;
            instr.set_source((lower >> 11) & 0x1F);
            instr.set_source2((lower >> 16) & 0x1F);
            instr.set_field((lower >> 21) & 0x3);
            instr.set_field2((lower >> 23) & 0x3);
            has_q_stalled = true;
            break;
        case 0x3B:
            //WAITq - the stall is handled by the interpreter pass, so we can just return
            has_q_stalled = true;
            return;
        case 0x3C:
            //MTIR
            instr.op = IR::Opcode::VMoveToInt;
            instr.set_source((lower >> 11) & 0x1F);
            instr.set_dest((lower >> 16) & 0xF);
            instr.set_field((lower >> 21) & 0x3);
            break;
        case 0x3D:
            //MFIR
            instr.op = IR::Opcode::VMoveFromInt;
            instr.set_source((lower >> 11) & 0xF);
            instr.set_dest((lower >> 16) & 0x1F);
            instr.set_field((lower >> 21) & 0xF);
            break;
        case 0x3E:
            //ILWR
            instr.op = IR::Opcode::LoadInt;
            instr.set_base((lower >> 11) & 0xF);
            instr.set_dest((lower >> 16) & 0xF);
            instr.set_field((lower >> 21) & 0xF);
            instr.set_source(0);
            break;
        case 0x3F:
            //ISWR
            update_xgkick(instrs);
            instr.op = IR::Opcode::StoreInt;
            instr.set_field((lower >> 21) & 0xF);
            instr.set_source((lower >> 16) & 0xF);
            instr.set_base((lower >> 11) & 0xF);
            instr.set_source2(0);
            break;
        case 0x64:
            //MFP
            instr.op = IR::Opcode::VMoveFromP;
            instr.set_dest((lower >> 16) & 0x1F);
            instr.set_field((lower >> 21) & 0xF);
            break;
        case 0x68:
            //XTOP
            instr.op = IR::Opcode::MoveXTOP;
            instr.set_dest((lower >> 16) & 0xF);
            break;
        case 0x69:
            //XITOP
            instr.op = IR::Opcode::MoveXITOP;
            instr.set_dest((lower >> 16) & 0xF);
            break;
        case 0x6C:
            //XGKICK
            instr.op = IR::Opcode::Xgkick;
            instr.set_base((lower >> 11) & 0xF);
            break;
        case 0x72:
            //ELENG
            instr.op = IR::Opcode::VEleng;
            instr.set_source((lower >> 11) & 0x1F);
            break;
        case 0x73:
            //ERLENG
            instr.op = IR::Opcode::VErleng;
            instr.set_source((lower >> 11) & 0x1F);
            break;
        case 0x78:
            //ESQRT
            instr.op = IR::Opcode::VESqrt;
            instr.set_source((lower >> 11) & 0x1F);
            instr.set_field((lower >> 21) & 0x3);
            break;
        case 0x79:
            //ERSQRT
            instr.op = IR::Opcode::VERsqrt;
            instr.set_source((lower >> 11) & 0x1F);
            instr.set_field((lower >> 21) & 0x3);
            break;
        case 0x7B:
            //WAITp - TODO
            return;
        default:
            Errors::die("[VU_JIT] Unrecognized lower1 special op $%02X", op);
    }
    instrs.push_back(instr);
}

void VU_JitTranslator::lower2(std::vector<IR::Instruction> &instrs, uint32_t lower, uint32_t PC)
{
    uint8_t op = (lower >> 25) & 0x7F;
    IR::Instruction instr;
    switch (op)
    {
        case 0x00:
            //LQ
        {
            int16_t imm = (int16_t)((lower & 0x400) ? (lower & 0x3ff) | 0xfc00 : (lower & 0x3ff));
            imm *= 16;
            instr.op = IR::Opcode::LoadQuad;
            instr.set_field((lower >> 21) & 0xF);
            instr.set_dest((lower >> 16) & 0x1F);
            instr.set_base((lower >> 11) & 0xF);
            instr.set_source((int64_t)imm);
        }
            break;
        case 0x01:
            //SQ
        {
            update_xgkick(instrs);
            int16_t imm = (int16_t)((lower & 0x400) ? (lower & 0x3ff) | 0xfc00 : (lower & 0x3ff));
            imm *= 16;
            instr.op = IR::Opcode::StoreQuad;
            instr.set_field((lower >> 21) & 0xF);
            instr.set_source((lower >> 11) & 0x1F);
            instr.set_base((lower >> 16) & 0xF);
            instr.set_source2((int64_t)imm);
        }
            break;
        case 0x04:
            //ILW
        {
            int16_t imm = lower & 0x7FF;
            imm = ((int16_t)(imm << 5)) >> 5;
            imm *= 16;
            instr.op = IR::Opcode::LoadInt;
            instr.set_field((lower >> 21) & 0xF);
            instr.set_dest((lower >> 16) & 0xF);
            instr.set_base((lower >> 11) & 0xF);
            instr.set_source((int64_t)imm);
        }
            break;
        case 0x05:
            //ISW
        {
            update_xgkick(instrs);
            int16_t imm = lower & 0x7FF;
            imm = ((int16_t)(imm << 5)) >> 5;
            imm *= 16;
            instr.op = IR::Opcode::StoreInt;
            instr.set_field((lower >> 21) & 0xF);
            instr.set_source((lower >> 16) & 0xF);
            instr.set_base((lower >> 11) & 0xF);
            instr.set_source2((int64_t)imm);
        }
            break;
        case 0x08:
        case 0x09:
            //IADDIU/ISUBIU
        {
            if (op == 0x08)
                instr.op = IR::Opcode::AddUnsignedImm;
            else
                instr.op = IR::Opcode::SubUnsignedImm;
            instr.set_dest((lower >> 16) & 0xF);
            instr.set_source((lower >> 11) & 0xF);
            uint16_t imm = lower & 0x7FF;
            imm |= ((lower >> 21) & 0xF) << 11;
            instr.set_source2(imm);
            if (!instr.get_source() && op == 0x08)
            {
                instr.op = IR::Opcode::LoadConst;
                instr.set_source(imm);
            }
        }
            break;
        case 0x11:
            //FCSET
            instr.op = IR::Opcode::SetClipFlags;
            instr.set_source(lower & 0xFFFFFF);
            break;
        case 0x12:
            //FCAND
            instr.op = IR::Opcode::AndClipFlags;
            instr.set_source(lower & 0xFFFFFF);
            break;
        case 0x13:
            //FCOR
            instr.op = IR::Opcode::OrClipFlags;
            instr.set_source(lower & 0xFFFFFF);
            break;
        case 0x16:
            //FSAND
        {
            uint16_t imm = (((lower >> 21 ) & 0x1) << 11) | (lower & 0x7FF);
            instr.op = IR::Opcode::AndStatFlags;
            instr.set_dest((lower >> 16) & 0xF);
            instr.set_source(imm);
        }
            break;
        case 0x18:
            //FMEQ
            instr.op = IR::Opcode::VMacEq;
            instr.set_source((lower >> 11) & 0xF);
            instr.set_dest((lower >> 16) & 0xF);
            break;
        case 0x1A:
            //FMAND
            instr.op = IR::Opcode::VMacAnd;
            instr.set_source((lower >> 11) & 0xF);
            instr.set_dest((lower >> 16) & 0xF);
            break;
        case 0x1C:
            //FCGET
            instr.op = IR::Opcode::GetClipFlags;
            instr.set_dest((lower >> 16) & 0xF);
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
        case 0x25:
            //JALR
            instr.op = IR::Opcode::JumpAndLinkIndirect;
            instr.set_source((lower >> 11) & 0xF);
            instr.set_return_addr((PC + 16) / 8);
            instr.set_dest((lower >> 16) & 0xF);
            break;
        case 0x28:
            //IBEQ
            instr.op = IR::Opcode::BranchEqual;
            instr.set_source((lower >> 11) & 0xF);
            instr.set_source2((lower >> 16) & 0xF);
            instr.set_jump_dest(branch_offset(lower, PC));
            instr.set_jump_fail_dest(PC + 16);
            break;
        case 0x29:
            //IBNE
            instr.op = IR::Opcode::BranchNotEqual;
            instr.set_source((lower >> 11) & 0xF);
            instr.set_source2((lower >> 16) & 0xF);
            instr.set_jump_dest(branch_offset(lower, PC));
            instr.set_jump_fail_dest(PC + 16);
            break;
        case 0x2C:
            //IBLTZ
            instr.op = IR::Opcode::BranchLessThanZero;
            instr.set_source((lower >> 11) & 0xF);
            instr.set_jump_dest(branch_offset(lower, PC));
            instr.set_jump_fail_dest(PC + 16);
            break;
        case 0x2D:
            //IBGTZ
            instr.op = IR::Opcode::BranchGreaterThanZero;
            instr.set_source((lower >> 11) & 0xF);
            instr.set_jump_dest(branch_offset(lower, PC));
            instr.set_jump_fail_dest(PC + 16);
            break;
        case 0x2E:
            //IBLEZ
            instr.op = IR::Opcode::BranchLessOrEqualThanZero;
            instr.set_source((lower >> 11) & 0xF);
            instr.set_jump_dest(branch_offset(lower, PC));
            instr.set_jump_fail_dest(PC + 16);
            break;
        case 0x2F:
            //IBGEZ
            instr.op = IR::Opcode::BranchGreaterOrEqualThanZero;
            instr.set_source((lower >> 11) & 0xF);
            instr.set_jump_dest(branch_offset(lower, PC));
            instr.set_jump_fail_dest(PC + 16);
            break;
        default:
            Errors::die("[VU_JIT] Unrecognized lower2 op $%02X", op);
    }
    instrs.push_back(instr);
}
