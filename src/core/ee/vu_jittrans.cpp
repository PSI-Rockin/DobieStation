#include <cstring>
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
    memset(instr_info, 0, sizeof(instr_info));
}

IR::Block VU_JitTranslator::translate(VectorUnit &vu, uint8_t* instr_mem, uint32_t prev_pc)
{
    IR::Block block;

    bool block_end = false;

    trans_branch_delay_slot = false;
    trans_ebit_delay_slot = false;
    cycles_this_block = 0;
    cycles_since_xgkick_update = 0;

    interpreter_pass(vu, instr_mem, prev_pc);
    flag_pass(vu);
       
    cur_PC = vu.get_PC();

    if (prev_pc != 0xFFFFFFFF)
    {
        trans_branch_delay_slot = (vu.pipeline_state[1] >> 55) & 0x1;
        trans_ebit_delay_slot = (vu.pipeline_state[1] >> 56) & 0x1;
    }

    IR::Instruction clear_delay(IR::Opcode::ClearIntDelay);
    block.add_instr(clear_delay);

    while (!block_end)
    {
        std::vector<IR::Instruction> upper_instrs;
        std::vector<IR::Instruction> lower_instrs;

        if (instr_info[cur_PC].branch_delay_slot || instr_info[cur_PC].ebit_delay_slot || instr_info[cur_PC].tbit_end)
        {
            block_end = true;

            if (trans_ebit_delay_slot)
                trans_ebit_delay_slot = true;

            if (instr_info[cur_PC].branch_delay_slot)
                trans_branch_delay_slot = true;
        }

        uint32_t upper = *(uint32_t*)&instr_mem[cur_PC + 4];
        uint32_t lower = *(uint32_t*)&instr_mem[cur_PC];
        
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
        }

        if (instr_info[cur_PC].update_p_pipeline)
        {
            IR::Instruction stall(IR::Opcode::UpdateP);
            block.add_instr(stall);
        }

        if (instr_info[cur_PC].backup_vi)
        {
            IR::Instruction backup(IR::Opcode::BackupVI);
            backup.set_source(instr_info[cur_PC].backup_vi);
            block.add_instr(backup);
        }

        if (instr_info[cur_PC].branch_delay_slot)
        {
            IR::Instruction branch(IR::Opcode::MoveDelayedBranch);
            block.add_instr(branch);
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
                int vf_reg = instr_info[cur_PC].decoder_vf_write[0];

                IR::Instruction backup_old(IR::Opcode::BackupVF);
                backup_old.set_source(vf_reg);
                backup_old.set_dest(0); //0 = OldVF
                block.add_instr(backup_old);

                for (unsigned int i = 0; i < upper_instrs.size(); i++)
                {
                    block.add_instr(upper_instrs[i]);
                }

                IR::Instruction backup_new(IR::Opcode::BackupVF);
                backup_new.set_source(vf_reg);
                backup_new.set_dest(1); //1 = NewVF
                block.add_instr(backup_new);

                IR::Instruction restore_old(IR::Opcode::RestoreVF);
                restore_old.set_source(vf_reg);
                restore_old.set_dest(0); //0 = OldVF
                block.add_instr(restore_old);
               
                for (unsigned int i = 0; i < lower_instrs.size(); i++)
                {
                    block.add_instr(lower_instrs[i]);
                }

                IR::Instruction restore_new(IR::Opcode::RestoreVF);
                restore_new.set_source(vf_reg);
                restore_new.set_dest(1); //1 = NewVF
                block.add_instr(restore_new);
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
                }
            }
        }

        //End of microprogram delay slot
        if (upper & (1 << 30))
        {
            if (instr_info[cur_PC].is_branch)
                Errors::print_warning("[VU_JIT] Warning! E-Bit On Branch!\n");
        }
        
        if (instr_info[cur_PC].ebit_delay_slot || instr_info[cur_PC].tbit_end)
        {
            if (instr_info[cur_PC].tbit_end)
            {
                IR::Instruction instr(IR::Opcode::StopTBit);
                instr.set_jump_dest(cur_PC + 8);
                block.add_instr(instr);
                Errors::print_warning("[VU_JIT] Stopped by T-Bit\n");
            }
            else
            {
                IR::Instruction instr(IR::Opcode::Stop);
                instr.set_jump_dest(cur_PC + 8);
                block.add_instr(instr);
            }

            //Clear the pipeline state, stalls won't happen in a new microprogram
            IR::Instruction pipeline;
            pipeline.op = IR::Opcode::SavePipelineState;
            pipeline.set_source(0);
            pipeline.set_source2(0);
            block.add_instr(pipeline);

            //Flush P & Q Pipelines on E-Bit.  Not perfect emulation but close enough
            IR::Instruction q_stall(IR::Opcode::UpdateQ);
            block.add_instr(q_stall);

            IR::Instruction p_stall(IR::Opcode::UpdateP);
            block.add_instr(p_stall);

            IR::Instruction clear_delay(IR::Opcode::ClearIntDelay);
            block.add_instr(clear_delay);
            //The branch will be taken instantly as we finish, I think
            //Only when in the delay slot of the ebit
            //if the branch was the ebit, this has already been done
            if (instr_info[cur_PC].is_branch)
            {
                IR::Instruction branch(IR::Opcode::MoveDelayedBranch);
                block.add_instr(branch);
                Errors::print_warning("[VU_JIT] Warning! Branch in E-Bit Delay Slot!\n");
            }

            IR::Instruction mac_update(IR::Opcode::UpdateMacPipeline);
            mac_update.set_source(4);
            block.add_instr(mac_update);
        }
        else if(block_end)
        {
            //Save end PC for block, helps us remember where the next block jumped from
            IR::Instruction savepc;
            savepc.op = IR::Opcode::SavePC;
            savepc.set_jump_dest(cur_PC);
            block.add_instr(savepc);

            //Save the pipeline state, helps choose the correct block to load/compile next
            IR::Instruction pipeline;
            pipeline.op = IR::Opcode::SavePipelineState;
            pipeline.set_source(instr_info[cur_PC].pipeline_state[0]);
            pipeline.set_source2(instr_info[cur_PC].pipeline_state[1]);
            block.add_instr(pipeline);
        }

        cur_PC += 8;
    }

    IR::Instruction update;
    update.op = IR::Opcode::UpdateXgkick;
    update.set_source(cycles_since_xgkick_update);
    block.add_instr(update);
    block.set_cycle_count(cycles_this_block);

    return block;
}

int VU_JitTranslator::fdiv_pipe_cycles(uint32_t lower_instr)
{
    if (lower_instr & (1 << 31))
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

int VU_JitTranslator::efu_pipe_cycles(uint32_t lower_instr)
{
    if (lower_instr & (1 << 31))
    {
        uint32_t op = ((lower_instr >> 4) & 0x7C) | (lower_instr & 0x3);
        switch (op)
        {
            case 0x70:
                return 11;
            case 0x71:
            case 0x72:
            case 0x79:
                return 18;
            case 0x73:
                return 24;
            case 0x74:
            case 0x75:
                return 54;
            case 0x76:
            case 0x78:
            case 0x7A:
                return 12;
            case 0x7C:
                return 29;
            case 0x7D:
                return 54;
            case 0x7E:
                return 44;
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
            case 0x10:
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

        stall_pipe[0] = ((uint64_t)(vu.decoder.vf_write[0] & 0x1F)) | ((uint64_t)(vu.decoder.vf_write[1] & 0x1F) << 5);
        stall_pipe[0] |= ((uint64_t)(vu.decoder.vf_write_field[0] & 0xF) << 10) | ((uint64_t)(vu.decoder.vf_write_field[1] & 0xF) << 14);
        stall_pipe[0] |= (uint64_t)(vu.decoder.vi_write_from_load & 0xF) << 18;
    }
}

/**
 * Determine FMAC stalls
 */
void VU_JitTranslator::analyze_FMAC_stalls(VectorUnit &vu, uint16_t PC)
{
    for (int i = 0; i < 3; i++)
    {
        uint8_t write0 = stall_pipe[i] & 0x1F;
        uint8_t write1 = (stall_pipe[i] >> 5) & 0x1F;
        uint8_t viwrite = (stall_pipe[i] >> 18) & 0xF;

        bool stall_found = false;
        if (write0 != 0 || write1 != 0)
        {
            uint8_t write0_field = (stall_pipe[i] >> 10) & 0xF;
            uint8_t write1_field = (stall_pipe[i] >> 14) & 0xF;
            
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
        }
        //Integer Load Delays
        if (viwrite)
        {
            if (viwrite == vu.decoder.vi_read0 || viwrite == vu.decoder.vi_read1)
            {
                stall_found = true;
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

void VU_JitTranslator::handle_vu_stalls(VectorUnit &vu, uint16_t PC, uint32_t lower, int &q_pipe_delay, int &p_pipe_delay)
{
    analyze_FMAC_stalls(vu, PC);

    //WaitQ, DIV, RSQRT, SQRT
    if (((lower & 0x800007FC) == 0x800003BC))
    {
        instr_info[PC].q_pipeline_instr = true;
        if (q_pipe_delay > 0)
        {
            //printf("[VU_JIT] Q pipe delay of %d stall amount of %d\n", q_pipe_delay, instr_info[PC].stall_amount);
            if (instr_info[PC].stall_amount < q_pipe_delay)
                instr_info[PC].stall_amount = q_pipe_delay;

            instr_info[PC].update_q_pipeline = true;
        }

        q_pipe_delay = fdiv_pipe_cycles(lower);
    }

    //WaitP, EATAN(xy/xz), EEXP, ELENG, ERCPR, ERLENG, ERSADD, ERSQRT, ESADD, ESIN, ESQRT, ESUM
    if ((lower & (1 << 31)) && ((lower >> 2) & 0x1CF) == 0x1CF)
    {
        instr_info[PC].p_pipeline_instr = true;
        if (p_pipe_delay > 0)
        {
            //printf("[VU_JIT] P pipe delay of %d stall amount of %d\n", q_pipe_delay, instr_info[PC].stall_amount);
            if (instr_info[PC].stall_amount < (p_pipe_delay - 1))
                instr_info[PC].stall_amount = (p_pipe_delay - 1);

            instr_info[PC].update_p_pipeline = true;
        }
        p_pipe_delay = efu_pipe_cycles(lower);
    }

    //Branch instructions
    if ((lower & 0xC0000000) == 0x40000000)
    {
        //Conditional branches only
        //Also if there is a stall on the branch instruction, I "Think" this gives it time to write the value so this no longer applies
        if (((lower >> 25) & 0xF) >= 0x4 && instr_info[PC].stall_amount == 0)
        {
            instr_info[PC].use_backup_vi = false;
            if (vu.int_branch_delay)
            {
                if ((vu.int_backup_id == vu.decoder.vi_read0 || vu.int_backup_id == vu.decoder.vi_read1))
                {
                    instr_info[PC].use_backup_vi = true;
                    vu.int_backup_id_rec = vu.int_backup_id;

                    printf("[VU_JIT] Using backed up VI%d at PC %x\n", vu.int_backup_id_rec, PC);
                }
            }
            else
            {
                //Reg used in branch was modified on the previous instruction
                if (instr_info[PC - 8].decoder_vi_write != 0)
                {
                    if (instr_info[PC - 8].decoder_vi_write == vu.decoder.vi_read0)
                    {
                        vu.int_backup_id_rec = vu.decoder.vi_read0;
                        instr_info[PC].use_backup_vi = true;
                    }

                    if (instr_info[PC - 8].decoder_vi_write == vu.decoder.vi_read1)
                    {
                        vu.int_backup_id_rec = vu.decoder.vi_read1;
                        instr_info[PC].use_backup_vi = true;
                    }

                    if (instr_info[PC].use_backup_vi)
                    {
                        int backup_pc = ((PC - 32) < vu.get_PC()) ? vu.get_PC() : (PC - 32);

                        int stalls = 0;
                        for (int i = PC - 8; i >= backup_pc; i -= 8)
                        {
                            //Stalls cause the chain to break
                            if (instr_info[i].stall_amount)
                            {
                                backup_pc += i - backup_pc;
                                break;
                            }
                        }
                        printf("[VU_JIT] Backing up VI%d at %x for PC %x\n", vu.int_backup_id_rec, backup_pc, PC);
                        instr_info[backup_pc].backup_vi = vu.int_backup_id_rec;
                    }
                }
            }
        }
    }
}

void VU_JitTranslator::populate_vu_state(VectorUnit &vu, int q_pipe_delay, int p_pipe_delay, uint16_t PC)
{
    //Save the state at the end of the block
    instr_info[PC].pipeline_state[0] = stall_pipe[0] & 0x7FFFFF;
    instr_info[PC].pipeline_state[0] |= (stall_pipe[1] & 0x7FFFFF) << 23;
    instr_info[PC].pipeline_state[0] |= (stall_pipe[2] & 0x7FFFFF) << 46;
    instr_info[PC].pipeline_state[1] = stall_pipe[3] & 0x7FFFFF;

    instr_info[PC].pipeline_state[1] |= (q_pipe_delay & 0xF) << 23;
    instr_info[PC].pipeline_state[1] |= (uint64_t)(p_pipe_delay & 0x3F) << 27;

    instr_info[PC].pipeline_state[1] |= (uint64_t)(vu.decoder.vf_write[0] & 0x1F) << 33UL;
    instr_info[PC].pipeline_state[1] |= (uint64_t)(vu.decoder.vf_write[1] & 0x1F) << 38UL;
    instr_info[PC].pipeline_state[1] |= (uint64_t)(vu.decoder.vf_write_field[0] & 0xF) << 43UL;
    instr_info[PC].pipeline_state[1] |= (uint64_t)(vu.decoder.vf_write_field[1] & 0xF) << 47UL;
    instr_info[PC].pipeline_state[1] |= (uint64_t)(vu.decoder.vi_write_from_load & 0xF) << 51UL;
    instr_info[PC].pipeline_state[1] |= (uint64_t)(instr_info[PC].is_branch & 0x1) << 55UL;
    instr_info[PC].pipeline_state[1] |= (uint64_t)(instr_info[PC].is_ebit & 0x1) << 56UL;
}
/**
 * Determine base operation information and which instructions need to be swapped
 */
void VU_JitTranslator::interpreter_pass(VectorUnit &vu, uint8_t *instr_mem, uint32_t prev_pc)
{
    bool block_end = false;
    bool branch_delay_slot = false;
    bool ebit_delay_slot = false;
    int q_pipe_delay = 0;
    int p_pipe_delay = 0;
    
    //Restore state from end of the block we jumped from
    if (prev_pc != 0xFFFFFFFF)
    {        
        printf("[VU_JIT64] Restoring state from %x\n", prev_pc);
        stall_pipe[0] = vu.pipeline_state[0] & 0x7FFFFF;
        stall_pipe[1] = (vu.pipeline_state[0] >> 23) & 0x7FFFFF;
        stall_pipe[2] = (vu.pipeline_state[0] >> 46) & 0x7FFFFF;
        stall_pipe[3] = vu.pipeline_state[1] & 0x7FFFFF;

        vu.decoder.vf_write[0] = (vu.pipeline_state[1] >> 33) & 0x1F;
        vu.decoder.vf_write[1] = (vu.pipeline_state[1] >> 38) & 0x1F;
        vu.decoder.vf_write_field[0] = (vu.pipeline_state[1] >> 43) & 0xF;
        vu.decoder.vf_write_field[1] = (vu.pipeline_state[1] >> 47) & 0xF;
        vu.decoder.vi_write_from_load = (vu.pipeline_state[1] >> 51) & 0xF;

        q_pipe_delay = (vu.pipeline_state[1] >> 23) & 0xF;
        p_pipe_delay = (vu.pipeline_state[1] >> 27) & 0x3F;

        branch_delay_slot = (vu.pipeline_state[1] >> 55) & 0x1;
        ebit_delay_slot = (vu.pipeline_state[1] >> 56) & 0x1;
    }
    else //Else it's a new block so there is no previous state
    {
        stall_pipe[0] = 0;
        stall_pipe[1] = 0;
        stall_pipe[2] = 0;
        stall_pipe[3] = 0;

        vu.decoder.reset();
    }

    vu.int_backup_id_rec = 0;

    uint16_t PC = vu.get_PC();
    while (!block_end)
    {
        instr_info[PC].backup_vi = 0;
        instr_info[PC].use_backup_vi = false;
        instr_info[PC].stall_amount = 0;
        instr_info[PC].swap_ops = false;
        instr_info[PC].update_q_pipeline = false;
        instr_info[PC].update_p_pipeline = false;
        instr_info[PC].update_mac_pipeline = false;
        instr_info[PC].advance_mac_pipeline = false;
        instr_info[PC].flag_instruction = FlagInstr_None;
        instr_info[PC].branch_delay_slot = false;
        instr_info[PC].ebit_delay_slot = false;
        instr_info[PC].is_branch = false;
        instr_info[PC].is_ebit = false;
        instr_info[PC].q_pipeline_instr = false;
        instr_info[PC].p_pipeline_instr = false;
        instr_info[PC].tbit_end = false;

        uint32_t upper = *(uint32_t*)&instr_mem[PC + 4];
        uint32_t lower = *(uint32_t*)&instr_mem[PC];

        if (branch_delay_slot || ebit_delay_slot)
        {
            block_end = true;
            //In case of branch on XGKick stall
            instr_info[PC].branch_delay_slot = branch_delay_slot;
            instr_info[PC].ebit_delay_slot = ebit_delay_slot;
        }

        ebit_delay_slot = false;
        branch_delay_slot = false;

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

            int write = vu.decoder.vf_write[0];
            int write1 = vu.decoder.vf_write[1];
            int read0 = vu.decoder.vf_read0[1];
            int read1 = vu.decoder.vf_read1[1];

            instr_info[PC].decoder_vi_write = vu.decoder.vi_write;

            //If an upper op writes to a register a lower op reads from, the lower op executes first
            //Additionally, if an upper op and a lower op write to the same register, the upper op
            //has priority.
            if (write && ((write == read0 || write == read1) || (write == write1)))
            {
                instr_info[PC].swap_ops = true;
                instr_info[PC].decoder_vf_write[0] = write;
            }

            handle_vu_stalls(vu, PC, lower, q_pipe_delay, p_pipe_delay);

            //Branch instructions
            if ((lower & 0xC0000000) == 0x40000000)
            {
                if (block_end)
                    Errors::print_warning("[VU_IR] Branch in delay slot\n");
                branch_delay_slot = true;
                instr_info[PC].is_branch = true;
            }
        }
        else
        {
            instr_info[PC].decoder_vi_read0 = 0;
            instr_info[PC].decoder_vi_read1 = 0;
            instr_info[PC].decoder_vi_write = 0;
            analyze_FMAC_stalls(vu, PC);
        }

        //Handle the pipes separately from the instructions in case an EFU instruction happens after a FDIV instruction and stalls
        if (q_pipe_delay > 0 && !instr_info[PC].q_pipeline_instr)
        {
            q_pipe_delay -= instr_info[PC].stall_amount + 1;

            if(q_pipe_delay <= 0)
            {
                instr_info[PC].update_q_pipeline = true;
                q_pipe_delay = 0;
            }
        }

        if (p_pipe_delay > 0 && !instr_info[PC].p_pipeline_instr)
        {
            p_pipe_delay -= instr_info[PC].stall_amount + 1;

            if (p_pipe_delay <= 0)
            {
                instr_info[PC].update_p_pipeline = true;
                p_pipe_delay = 0;
            }
        }

        if(instr_info[PC].stall_amount)
            update_pipeline(vu, instr_info[PC].stall_amount);

        if (upper & (1 << 30))
        {
            ebit_delay_slot = true;
            instr_info[PC].is_ebit = true;
        }

        if (upper & (1 << 27))
        {
            if (vu.read_fbrst() & (1 << (3 + (vu.get_id() * 8))))
            {
                block_end = true;
                instr_info[PC].tbit_end = true;
            }
        }

        //XGKick we need to save the VU state if it stalls
        if (!(upper & (1 << 31)) && (lower & (1 << 31)) && (lower & 0x7FF) == 0x6FC)
        {
            populate_vu_state(vu, q_pipe_delay, p_pipe_delay, PC);
        }

        if (instr_info[PC].decoder_vi_write != vu.int_backup_id)
            vu.int_branch_delay = 0;

        PC += 8;
    }

    end_PC = PC - 8;

    if (!instr_info[end_PC].ebit_delay_slot)
    {
        if (instr_info[end_PC].decoder_vi_write && !vu.decoder.vi_write_from_load)
        {
            instr_info[end_PC].backup_vi = instr_info[end_PC].decoder_vi_write;
        }
    }

    populate_vu_state(vu, q_pipe_delay, p_pipe_delay, end_PC);
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

            if (!instr.get_dest())
                return;
            break;
        case 0x14:
        case 0x15:
        case 0x16:
        case 0x17:
            //MINIbc
            instr.op = IR::Opcode::VMinVectorByScalar;
            op_vector_by_scalar(instr, upper);

            if (!instr.get_dest())
                return;
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
            instr.op = IR::Opcode::VMulVectorByScalar;
            op_vector_by_scalar(instr, upper, VU_SpecialReg::Q);
            break;
        case 0x1D:
            //MAXi
            instr.op = IR::Opcode::VMaxVectorByScalar;
            op_vector_by_scalar(instr, upper, VU_SpecialReg::I);

            if (!instr.get_dest())
                return;
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

            if (!instr.get_dest())
                return;
            break;
        case 0x20:
            //ADDq
            instr.op = IR::Opcode::VAddVectorByScalar;
            op_vector_by_scalar(instr, upper, VU_SpecialReg::Q);
            break;
        case 0x21:
            //MADDq
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
            instr.op = IR::Opcode::VSubVectorByScalar;
            op_vector_by_scalar(instr, upper, VU_SpecialReg::Q);
            break;
        case 0x25:
            //MSUBq
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

            if (!instr.get_dest())
                return;
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

            if (!instr.get_dest())
                return;
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

            if (!instr.get_dest())
                return;
            break;
        case 0x11:
            //ITOF4
            instr.op = IR::Opcode::VFixedToFloat4;
            op_conversion(instr, upper);

            if (!instr.get_dest())
                return;
            break;
        case 0x12:
            //ITOF12
            instr.op = IR::Opcode::VFixedToFloat12;
            op_conversion(instr, upper);

            if (!instr.get_dest())
                return;
            break;
        case 0x13:
            //ITOF15
            instr.op = IR::Opcode::VFixedToFloat15;
            op_conversion(instr, upper);

            if (!instr.get_dest())
                return;
            break;
        case 0x14:
            //FTOI0
            instr.op = IR::Opcode::VFloatToFixed0;
            op_conversion(instr, upper);

            if (!instr.get_dest())
                return;
            break;
        case 0x15:
            //FTOI4
            instr.op = IR::Opcode::VFloatToFixed4;
            op_conversion(instr, upper);

            if (!instr.get_dest())
                return;
            break;
        case 0x16:
            //FTOI12
            instr.op = IR::Opcode::VFloatToFixed12;
            op_conversion(instr, upper);

            if (!instr.get_dest())
                return;
            break;
        case 0x17:
            //FTOI15
            instr.op = IR::Opcode::VFloatToFixed15;
            op_conversion(instr, upper);

            if (!instr.get_dest())
                return;
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
            instr.op = IR::Opcode::VAddVectorByScalar;
            op_acc_by_scalar(instr, upper, VU_SpecialReg::Q);
            break;
        case 0x21:
            //MADDAq
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
            instr.op = IR::Opcode::VSubVectorByScalar;
            op_acc_by_scalar(instr, upper, VU_SpecialReg::Q);
            break;
        case 0x25:
            //MSUBAq
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

    //If it's writing to vi0, ignore it
    if (!instr.get_dest())
        return;

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

            if (!instr.get_dest())
                return;
            break;
        case 0x34:
            //LQI
            instr.op = IR::Opcode::LoadQuadInc;
            instr.set_field((lower >> 21) & 0xF);
            instr.set_dest((lower >> 16) & 0x1F);
            instr.set_base((lower >> 11) & 0xF);

            if (!instr.get_dest())
                return;
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

            if (!instr.get_dest())
                return;
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
            instr.op = IR::Opcode::VDiv;
            instr.set_source((lower >> 11) & 0x1F);
            instr.set_source2((lower >> 16) & 0x1F);
            instr.set_field((lower >> 21) & 0x3);
            instr.set_field2((lower >> 23) & 0x3);
            break;
        case 0x39:
            //SQRT
            fallback_interpreter(instr, lower, false);
            Errors::print_warning("[VU_JIT] Unrecognized lower1 special op SQRT\n");
            break;
        case 0x3A:
            //RSQRT
            instr.op = IR::Opcode::VRsqrt;
            instr.set_source((lower >> 11) & 0x1F);
            instr.set_source2((lower >> 16) & 0x1F);
            instr.set_field((lower >> 21) & 0x3);
            instr.set_field2((lower >> 23) & 0x3);
            break;
        case 0x3B:
            //WAITq - the stall is handled by the interpreter pass, so we can just return
            return;
        case 0x3C:
            //MTIR
            instr.op = IR::Opcode::VMoveToInt;
            instr.set_source((lower >> 11) & 0x1F);
            instr.set_dest((lower >> 16) & 0xF);
            instr.set_field((lower >> 21) & 0x3);

            if (!instr.get_dest())
                return;
            break;
        case 0x3D:
            //MFIR
            instr.op = IR::Opcode::VMoveFromInt;
            instr.set_source((lower >> 11) & 0xF);
            instr.set_dest((lower >> 16) & 0x1F);
            instr.set_field((lower >> 21) & 0xF);

            if (!instr.get_dest())
                return;
            break;
        case 0x3E:
            //ILWR
            instr.op = IR::Opcode::LoadInt;
            instr.set_base((lower >> 11) & 0xF);
            instr.set_dest((lower >> 16) & 0xF);
            instr.set_field((lower >> 21) & 0xF);
            instr.set_source(0);

            if (!instr.get_dest())
                return;
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

            if (!instr.get_dest())
                return;
            break;
        case 0x68:
            //XTOP
            instr.op = IR::Opcode::MoveXTOP;
            instr.set_dest((lower >> 16) & 0xF);

            if (!instr.get_dest())
                return;
            break;
        case 0x69:
            //XITOP
            instr.op = IR::Opcode::MoveXITOP;
            instr.set_dest((lower >> 16) & 0xF);

            if (!instr.get_dest())
                return;
            break;
        case 0x6C:
            //XGKICK
            update_xgkick(instrs);
            instr.op = IR::Opcode::Xgkick;
            instr.set_base((lower >> 11) & 0xF);
            instr.set_return_addr(cur_PC);
            instr.set_source(instr_info[cur_PC].pipeline_state[0]);
            instr.set_source2(instr_info[cur_PC].pipeline_state[1]);
            instr.set_dest(trans_branch_delay_slot);
            instr.set_jump_dest(trans_ebit_delay_slot);
            break;
        case 0x70:
            //ESADD
            fallback_interpreter(instr, lower, false);
            Errors::print_warning("[VU_JIT] Unrecognized lower1 special op ESADD\n", op);
            break;
        case 0x71:
            //ERSADD
            fallback_interpreter(instr, lower, false);
            Errors::print_warning("[VU_JIT] Unrecognized lower1 special op ERSADD\n", op);
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
        case 0x74:
            //EATANxy
            fallback_interpreter(instr, lower, false);
            Errors::print_warning("[VU_JIT] Unrecognized lower1 special op EATANxy\n", op);
            break;
        case 0x75:
            //EATANxz
            fallback_interpreter(instr, lower, false);
            Errors::print_warning("[VU_JIT] Unrecognized lower1 special op EATANxz\n", op);
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
        case 0x7A:
            //ERCPR
            fallback_interpreter(instr, lower, false);
            Errors::print_warning("[VU_JIT] Unrecognized lower1 special op ERCPR\n", op);
            break;
        case 0x7B:
            //WAITp - the stall is handled by the interpreter pass, so we can just return
            return;
        case 0x7C:
            //ESIN
            fallback_interpreter(instr, lower, false);
            Errors::print_warning("[VU_JIT] Unrecognized lower1 special op ESIN\n", op);
            break;
        case 0x7D:
            //EATAN
            fallback_interpreter(instr, lower, false);
            Errors::print_warning("[VU_JIT] Unrecognized lower1 special op EATAN\n", op);
            break;
        case 0x7E:
            //EEXP
            fallback_interpreter(instr, lower, false);
            Errors::print_warning("[VU_JIT] Unrecognized lower1 special op EEXP\n", op);
            break;
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

            if (!instr.get_dest())
                return;
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

            if (!instr.get_dest())
                return;
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

            if (!instr.get_dest())
                return;
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

            if (!instr.get_dest())
                return;
        }
            break;
        case 0x18:
            //FMEQ
            instr.op = IR::Opcode::VMacEq;
            instr.set_source((lower >> 11) & 0xF);
            instr.set_dest((lower >> 16) & 0xF);

            if (!instr.get_dest())
                return;
            break;
        case 0x1A:
            //FMAND
            instr.op = IR::Opcode::VMacAnd;
            instr.set_source((lower >> 11) & 0xF);
            instr.set_dest((lower >> 16) & 0xF);

            if (!instr.get_dest())
                return;
            break;
        case 0x1C:
            //FCGET
            instr.op = IR::Opcode::GetClipFlags;
            instr.set_dest((lower >> 16) & 0xF);

            if (!instr.get_dest())
                return;
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
            instr.set_bc(trans_branch_delay_slot);
            if (!instr.get_dest())
                instr.op = IR::Opcode::Jump;
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
            instr.set_bc(trans_branch_delay_slot);
            if (!instr.get_dest())
                instr.op = IR::Opcode::JumpIndirect;
            break;
        case 0x28:
            //IBEQ
            instr.op = IR::Opcode::BranchEqual;
            instr.set_source((lower >> 11) & 0xF);
            instr.set_source2((lower >> 16) & 0xF);
            instr.set_jump_dest(branch_offset(lower, PC));
            instr.set_jump_fail_dest(PC + 16);
            instr.set_bc(trans_branch_delay_slot);
            instr.set_field(instr_info[PC].use_backup_vi);
            break;
        case 0x29:
            //IBNE
            instr.op = IR::Opcode::BranchNotEqual;
            instr.set_source((lower >> 11) & 0xF);
            instr.set_source2((lower >> 16) & 0xF);
            instr.set_jump_dest(branch_offset(lower, PC));
            instr.set_jump_fail_dest(PC + 16);
            instr.set_bc(trans_branch_delay_slot);
            instr.set_field(instr_info[PC].use_backup_vi);
            break;
        case 0x2C:
            //IBLTZ
            instr.op = IR::Opcode::BranchLessThanZero;
            instr.set_source((lower >> 11) & 0xF);
            instr.set_jump_dest(branch_offset(lower, PC));
            instr.set_jump_fail_dest(PC + 16);
            instr.set_bc(trans_branch_delay_slot);
            instr.set_field(instr_info[PC].use_backup_vi);
            break;
        case 0x2D:
            //IBGTZ
            instr.op = IR::Opcode::BranchGreaterThanZero;
            instr.set_source((lower >> 11) & 0xF);
            instr.set_jump_dest(branch_offset(lower, PC));
            instr.set_jump_fail_dest(PC + 16);
            instr.set_bc(trans_branch_delay_slot);
            instr.set_field(instr_info[PC].use_backup_vi);
            break;
        case 0x2E:
            //IBLEZ
            instr.op = IR::Opcode::BranchLessOrEqualThanZero;
            instr.set_source((lower >> 11) & 0xF);
            instr.set_jump_dest(branch_offset(lower, PC));
            instr.set_jump_fail_dest(PC + 16);
            instr.set_bc(trans_branch_delay_slot);
            instr.set_field(instr_info[PC].use_backup_vi);
            break;
        case 0x2F:
            //IBGEZ
            instr.op = IR::Opcode::BranchGreaterOrEqualThanZero;
            instr.set_source((lower >> 11) & 0xF);
            instr.set_jump_dest(branch_offset(lower, PC));
            instr.set_jump_fail_dest(PC + 16);
            instr.set_bc(trans_branch_delay_slot);
            instr.set_field(instr_info[PC].use_backup_vi);
            break;
        default:
            //Errors::die("[VU_JIT] Unrecognized lower2 op $%02X", op);
            fallback_interpreter(instr, lower, false);
            Errors::print_warning("[VU_JIT] Unrecognized lower2 op $%02X\n", op);
    }
    instrs.push_back(instr);
}
