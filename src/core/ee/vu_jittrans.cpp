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

void VU_JitTranslator::reset_instr_info()
{
    memset(instr_info, 0, sizeof(VU_InstrInfo));
}

IR::Block VU_JitTranslator::translate(VectorUnit &vu, uint8_t* instr_mem, uint32_t prev_pc)
{
    IR::Block block;

    bool block_end = false;
    bool ebit = false;

    trans_delay_slot = false;
    has_q_stalled = false;
    q_pipeline_delay = 0;
    cycles_this_block = 0;
    cycles_since_xgkick_update = 0;

    interpreter_pass(vu, instr_mem, prev_pc);
    flag_pass(vu);

    if(prev_pc != 0xFFFFFFFF)
        q_pipeline_delay = instr_info[prev_pc].q_pipe_delay_trans_pass;

    cur_PC = vu.get_PC();
    while (!block_end)
    {
        std::vector<IR::Instruction> upper_instrs;
        std::vector<IR::Instruction> lower_instrs;
        if (trans_delay_slot)
            block_end = true;

        uint32_t upper = *(uint32_t*)&instr_mem[cur_PC + 4];
        uint32_t lower = *(uint32_t*)&instr_mem[cur_PC];
        
        if (q_pipeline_delay)
        {
            q_pipeline_delay -= instr_info[cur_PC].stall_amount+1;
            if (q_pipeline_delay <= 0)
            {
                IR::Instruction stall(IR::Opcode::UpdateQ);
                block.add_instr(stall);
                q_pipeline_delay = 0;
            }
        }

        cycles_this_block += instr_info[cur_PC].stall_amount + 1;
        cycles_since_xgkick_update += instr_info[cur_PC].stall_amount + 1;

        translate_upper(upper_instrs, upper);

        if (instr_info[cur_PC].advance_mac_pipeline)
        {
            IR::Instruction mac_update(IR::Opcode::UpdateMacPipeline);
            mac_update.set_source((uint64_t)(instr_info[cur_PC].stall_amount + 1));
            block.add_instr(mac_update);
        }

        if (instr_info[cur_PC].update_mac_pipeline)
        {
            IR::Instruction update(IR::Opcode::UpdateMacFlags);
            block.add_instr(update);
        }

        if (instr_info[cur_PC].update_q_pipeline)
        {
            IR::Instruction stall(IR::Opcode::UpdateQ);
            block.add_instr(stall);
            q_pipeline_delay = 0;
        }

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
            translate_lower(lower_instrs, lower, cur_PC);

            if (instr_info[cur_PC].swap_ops)
            {
                //Lower op first
                for (unsigned int i = 0; i < lower_instrs.size(); i++)
                {
                    block.add_instr(lower_instrs[i]);

                    if (lower_instrs[i].is_jump())
                    {
                        if (trans_delay_slot)
                            Errors::die("[VU JIT] Branch in delay slot");
                        trans_delay_slot = true;
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
                        if (trans_delay_slot)
                            Errors::die("[VU JIT] Branch in delay slot");
                        trans_delay_slot = true;
                    }
                }
            }
        }

        //End of microprogram delay slot
        if (upper & (1 << 30))
        {
            ebit = true;
            if (trans_delay_slot)
                Errors::die("[VU JIT] End microprogram in delay slot");
            trans_delay_slot = true;
        }
        else if (ebit)
        {
            IR::Instruction instr(IR::Opcode::Stop);
            instr.set_jump_dest(cur_PC + 8);
            block.add_instr(instr);
        }
        else if (block_end)
        {
            if (q_pipeline_delay)
            {
                if (!has_q_stalled)
                {
                    IR::Instruction q;
                    q.op = IR::Opcode::UpdateQPipeline;
                    q.set_source(cycles_this_block);
                    block.add_instr(q);
                    cycles_this_block = 0;
                }
            }
            //Save end PC for block, helps us remember where the next block jumped from
            IR::Instruction savepc;
            savepc.op = IR::Opcode::SavePC;
            savepc.set_jump_dest(cur_PC);
            block.add_instr(savepc);
        }

        cur_PC += 8;
    }
    instr_info[cur_PC - 8].q_pipe_delay_trans_pass = q_pipeline_delay;

    IR::Instruction update;
    update.op = IR::Opcode::UpdateXgkick;
    update.set_source(cycles_since_xgkick_update);
    block.add_instr(update);
    block.set_cycle_count(cycles_this_block);

    return block;
}

int VU_JitTranslator::fdiv_pipe_cycles(uint32_t lower_instr)
{
    if (((lower_instr >> 31) & 0x1))
    {
        uint32_t op = ((lower_instr >> 4) & 0x7C) | (lower_instr & 0x3);
        switch (op)
        {
            case 0x38:
            case 0x39:
                return 7;
            case 0x3A:
                return 13;
            default:
                return 0;

        }
    }
    return 0;
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

int VU_JitTranslator::is_flag_instruction(uint32_t lower_instr)
{
    if (!(lower_instr & (1 << 31)))
    {
        uint8_t op = (lower_instr >> 25) & 0x7F;

        switch (op)
        {
            case 0x11:
            case 0x12:
            case 0x13:
            case 0x1C:
                return FlagInstr_Clip;
            case 0x16:
            case 0x18:
            case 0x1A:
            case 0x1B:
                return FlagInstr_Mac;
            default:
                return FlagInstr_None;
        }
    }
    return FlagInstr_None;
}

void VU_JitTranslator::update_pipeline(VectorUnit &vu, int cycles)
{
    for (int i = 0; i < cycles; i++)
    {
        stall_pipe[3] = stall_pipe[2];
        stall_pipe[2] = stall_pipe[1];
        stall_pipe[1] = stall_pipe[0];

        stall_pipe[0] = ((uint32_t)vu.decoder.vf_write[0] << 16) | ((uint32_t)vu.decoder.vf_write[1] << 24);
        stall_pipe[0] |= ((uint64_t)vu.decoder.vf_write_field[0] << 32UL) | ((uint64_t)vu.decoder.vf_write_field[1] << 36UL);
    }
}

/**
 * Determine FMAC stalls
 */
void VU_JitTranslator::analyze_FMAC_stalls(VectorUnit &vu, uint16_t PC)
{
    if (vu.decoder.vf_read0[0] == 0 && vu.decoder.vf_read1[0] == 0
        && vu.decoder.vf_read0[1] == 0 && vu.decoder.vf_read1[1] == 0)
    {
        return;
    }

    for (int i = 0; i < 3; i++)
    {
        uint8_t write0 = (stall_pipe[i] >> 16) & 0xFF;
        uint8_t write1 = (stall_pipe[i] >> 24) & 0xFF;

        if (write0 == 0 && write1 == 0)
            continue;

        uint8_t write0_field = (stall_pipe[i] >> 32) & 0xF;
        uint8_t write1_field = (stall_pipe[i] >> 36) & 0xF;
        bool stall_found = false;
        for (int j = 0; j < 2; j++)
        {
            if (vu.decoder.vf_read0[j])
            {
                if (vu.decoder.vf_read0[j] == write0)
                {
                    if (vu.decoder.vf_read0_field[j] & write0_field)
                    {
                        stall_found = true;
                        break;
                    }
                }
                else if (vu.decoder.vf_read0[j] == write1)
                {
                    if (vu.decoder.vf_read0_field[j] & write1_field)
                    {
                        stall_found = true;
                        break;
                    }
                }
            }
            if (vu.decoder.vf_read1[j])
            {
                if (vu.decoder.vf_read1[j] == write0)
                {
                    if (vu.decoder.vf_read1_field[j] & write0_field)
                    {
                        stall_found = true;
                        break;
                    }
                }
                else if (vu.decoder.vf_read1[j] == write1)
                {
                    if (vu.decoder.vf_read1_field[j] & write1_field)
                    {
                        stall_found = true;
                        break;
                    }
                }
            }
        }
        if (stall_found)
        {
           // printf("[VU_JIT]FMAC stall at $%08X for %d cycles!\n", PC, 3 - i);
            instr_info[PC].stall_amount = 3 - i;
            break;
        }
    }
}

/**
 * Determine base operation information and which instructions need to be swapped
 */
void VU_JitTranslator::interpreter_pass(VectorUnit &vu, uint8_t *instr_mem, uint32_t prev_pc)
{
    bool block_end = false;
    bool delay_slot = false;
    int q_pipe_delay = 0;
    
    //Restore state from end of the block we jumped from
    if (prev_pc != 0xFFFFFFFF)
    {        
        printf("[VU_JIT64] Restoring state from %x\n", prev_pc);
        stall_pipe[0] = instr_info[prev_pc].stall_state[0];
        stall_pipe[1] = instr_info[prev_pc].stall_state[1];
        stall_pipe[2] = instr_info[prev_pc].stall_state[2];
        stall_pipe[3] = instr_info[prev_pc].stall_state[3];

        vu.decoder.vf_write[0] = instr_info[prev_pc].decoder_vf_write[0];
        vu.decoder.vf_write[1] = instr_info[prev_pc].decoder_vf_write[1];
        vu.decoder.vf_write_field[0] = instr_info[prev_pc].decoder_vf_write_field[0];
        vu.decoder.vf_write_field[1] = instr_info[prev_pc].decoder_vf_write_field[1];

        q_pipe_delay = instr_info[prev_pc].q_pipe_delay_int_pass;
    }
    else //Else it's a new block so there is no previous state
    {
        stall_pipe[0] = 0;
        stall_pipe[1] = 0;
        stall_pipe[2] = 0;
        stall_pipe[3] = 0;

        vu.decoder.vf_write[0] = 0;
        vu.decoder.vf_write[1] = 0;
        vu.decoder.vf_write_field[0] = 0;
        vu.decoder.vf_write_field[1] = 0;
    }

    uint16_t PC = vu.get_PC();
    while (!block_end)
    {
        instr_info[PC].stall_amount = 0;
        instr_info[PC].swap_ops = false;
        instr_info[PC].update_q_pipeline = false;
        instr_info[PC].update_mac_pipeline = false;
        instr_info[PC].advance_mac_pipeline = false;
        instr_info[PC].flag_instruction = FlagInstr_None;

        uint32_t upper = *(uint32_t*)&instr_mem[PC + 4];
        uint32_t lower = *(uint32_t*)&instr_mem[PC];

        if (delay_slot)
            block_end = true;

        update_pipeline(vu, 1);

        vu.decoder.reset();

        instr_info[PC].has_mac_result = updates_mac_flags(upper);

        if ((upper & 0x7FF) == 0x1FF)
            instr_info[PC].has_clip_result = true;
        else
            instr_info[PC].has_clip_result = false;

        VU_Interpreter::upper(vu, upper);

        //Handle lower op if there is one
        if (!(upper & (1 << 31)))
        {
            VU_Interpreter::lower(vu, lower);

            instr_info[PC].flag_instruction = is_flag_instruction(lower);
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
            if (!(upper & (1 << 31)))
            {
                if (write && ((write == read0 || write == read1) || (write == write1)))
                    instr_info[PC].swap_ops = true;
            }
        }

        analyze_FMAC_stalls(vu, PC);

        //WaitQ, DIV, RSQRT, SQRT
        if (((lower & 0x800007FC) == 0x800003BC))
        {
            if (q_pipe_delay > 0)
            {
                //printf("[VU_JIT] Q pipe delay of %d stall amount of %d\n", q_pipe_delay, instr_info[PC].stall_amount);
                if (instr_info[PC].stall_amount < q_pipe_delay)
                    instr_info[PC].stall_amount = q_pipe_delay;

                instr_info[PC].update_q_pipeline = true;
            }

            q_pipe_delay = fdiv_pipe_cycles(lower);
            if(!q_pipe_delay)
                instr_info[PC].update_q_pipeline = true;
        }
        else if (q_pipe_delay > 0)
        {
            q_pipe_delay -= instr_info[PC].stall_amount + 1;
        }

        if(instr_info[PC].stall_amount)
            update_pipeline(vu, instr_info[PC].stall_amount);

        if (upper & (1 << 30))
        {
            if (delay_slot)
                Errors::die("[VU_IR] End block in delay slot");
            delay_slot = true;
        }

        PC += 8;
    }

    end_PC = PC - 8;

    //Save the state at the end of the block
    instr_info[end_PC].stall_state[0] = stall_pipe[0];
    instr_info[end_PC].stall_state[1] = stall_pipe[1];
    instr_info[end_PC].stall_state[2] = stall_pipe[2];
    instr_info[end_PC].stall_state[3] = stall_pipe[3];

    instr_info[end_PC].decoder_vf_write[0] = vu.decoder.vf_write[0];
    instr_info[end_PC].decoder_vf_write[1] = vu.decoder.vf_write[1];
    instr_info[end_PC].decoder_vf_write_field [0] = vu.decoder.vf_write_field[0];
    instr_info[end_PC].decoder_vf_write_field[1] = vu.decoder.vf_write_field[1];

    instr_info[end_PC].q_pipe_delay_int_pass = q_pipe_delay;
}

/**
 * Determine when MAC, clip, and status flags need to be updated.
 */
void VU_JitTranslator::flag_pass(VectorUnit &vu)
{
    uint16_t start_pc = vu.get_PC();
    bool clip_instruction_found = false;
    bool mac_instruction_found = false;
    bool final_mac_instance_found = false;
    bool needs_update = false;
    int mac_cycles = 0;
    int clip_cycles = 0;

    //Start from the end of the block
    uint16_t i = end_PC;
    while (i >= start_pc && i <= end_PC)
    {
        //Update any instructions succeeded by a flag instruction to get the correct instance
        if (instr_info[i].flag_instruction)
        {
            if (instr_info[i].flag_instruction == FlagInstr_Clip)
            {
                clip_instruction_found = true;
                clip_cycles = 5;
            }
            if (instr_info[i].flag_instruction == FlagInstr_Mac)
            {
                mac_instruction_found = true;
                mac_cycles = 5;
            }
        }

        //Update the flags at the end of the block ready for the next block in case of flag read instruction
        if (i >= (end_PC - 32) || final_mac_instance_found == false)
        {
            needs_update = true;
        }

        //Update the flags at the beginning of a block also, I'm scared of subroutines checking flags
        if (i <= (start_pc + 32))
        {
            needs_update = true;
        }

        //Always update the pipe for flag instances
        if (mac_instruction_found)
        {
            needs_update = true;
            mac_cycles -= instr_info[i].stall_amount;
            if (mac_cycles <= 0)
                mac_instruction_found = false;
        }

        if (clip_instruction_found)
        {
            needs_update = true;
            clip_cycles -= instr_info[i].stall_amount;
            if (clip_cycles <= 0)
                clip_instruction_found = false;
        }

        //Always update for clip instructions also, probably not needed but just in case
        if (instr_info[i].has_clip_result)
        {
            needs_update = true;
        }

        if (needs_update)
        {
            instr_info[i].advance_mac_pipeline = true;
            if (instr_info[i].has_mac_result)
            {
                final_mac_instance_found = true;
                instr_info[i].update_mac_pipeline = true;
            }
            needs_update = false;
        }

        i -= 8;
    }
}

void VU_JitTranslator::fallback_interpreter(IR::Instruction &instr, uint32_t instr_word, bool is_upper)
{
    instr.op = IR::Opcode::FallbackInterpreter;
    instr.set_source(instr_word);
    instr.set_field(is_upper);
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
        case 0x2D:
            //MSUB
            instr.op = IR::Opcode::VMsubVectors;
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
            //Errors::die("[VU_JIT] Unrecognized upper op $%02X", op);
            fallback_interpreter(instr, upper, true);
            Errors::print_warning("[VU_JIT] Unrecognized upper op $%02X\n", op);
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
        case 0x1C:
            //MULAq
            check_q_stall(instrs);
            instr.op = IR::Opcode::VMulVectorByScalar;
            op_acc_by_scalar(instr, upper, VU_SpecialReg::Q);
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
            fallback_interpreter(instr, upper, true);
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
            //Errors::die("[VU_JIT] Unrecognized upper special op $%02X", op);
            fallback_interpreter(instr, upper, true);
            Errors::print_warning("[VU_JIT] Unrecognized upper special op $%02X\n", op);
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
            //Errors::die("[VU_JIT] Unrecognized lower1 op $%02X", op);
            fallback_interpreter(instr, lower, false);
            Errors::print_warning("[VU_JIT] Unrecognized lower1 op $%02X\n", op);
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
        case 0x39:
            //SQRT
            start_q_event(instrs, 7);
            fallback_interpreter(instr, lower, false);
            Errors::print_warning("[VU_JIT] Unrecognized lower1 special op SQRT\n");
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
        case 0x42:
            //RINIT
            instr.op = IR::Opcode::VRInit;
            instr.set_source((lower >> 11) & 0x1F);
            instr.set_field((lower >> 21) & 0x3);
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
            instr.set_source(cur_PC);
            instr.set_dest(trans_delay_slot);
            cycles_since_xgkick_update = 0;
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
            //Errors::die("[VU_JIT] Unrecognized lower1 special op $%02X", op);
            fallback_interpreter(instr, lower, false);
            Errors::print_warning("[VU_JIT] Unrecognized lower1 special op $%02X\n", op);
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
            //Errors::die("[VU_JIT] Unrecognized lower2 op $%02X", op);
            fallback_interpreter(instr, lower, false);
            Errors::print_warning("[VU_JIT] Unrecognized lower2 op $%02X\n", op);
    }
    instrs.push_back(instr);
}
