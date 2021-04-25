#include "serialize.hpp"
#include "emulator.hpp"
#include <cstring>
#include <fstream>

static constexpr uint32_t STATE_VERSION = 51;

using namespace std;

bool Emulator::request_load_state(const char* file_name)
{
    ifstream state(file_name, ios::binary);
    if (!state.is_open())
        return false;
    state.close();
    save_state_path = file_name;
    load_requested = true;
    return true;
}

bool Emulator::request_save_state(const char* file_name)
{
    ofstream state(file_name, ios::binary);
    if (!state.is_open())
        return false;
    state.close();
    save_state_path = file_name;
    save_requested = true;
    return true;
}

void Emulator::load_state(const char* file_name)
{
    load_requested = false;
    printf("[Emulator] Loading state...\n");
    fstream state(file_name, ios::binary | fstream::in);
    if (!state.is_open())
    {
        Errors::non_fatal("Failed to load save state");
        return;
    }

    StateSerializer ss(state, StateSerializer::Mode::Read);
    if (ss.DoMarker("DOBIE") == false)
    {
        state.close();
        Errors::non_fatal("Save state invalid");
        return;
    }

    uint32_t rev = 0;
    ss.Do(&rev);

    if (rev != STATE_VERSION)
    {
        state.close();
        Errors::non_fatal("Save state doesn't match version");
        return;
    }

    reset();

    do_state(ss);

    state.close();
    printf("[Emulator] Success!\n");
}

void Emulator::save_state(const char* file_name)
{
    save_requested = false;

    printf("[Emulator] Saving state...\n");
    fstream state(file_name, ios::binary | fstream::out | fstream::trunc);
    if (!state.is_open())
    {
        Errors::non_fatal("Failed to save state");
        return;
    }

    StateSerializer ss(state, StateSerializer::Mode::Write);
    ss.DoMarker("DOBIE");
    ss.Do(&STATE_VERSION);

    do_state(ss);

    state.close();
    printf("Success!\n");
}

void Emulator::do_state(StateSerializer& state)
{
    //Emulator info
    state.Do(&VBLANK_sent);
    state.Do(&frames);

    //RAM
    state.DoBytes(RDRAM, 1024 * 1024 * 32);
    state.DoBytes(IOP_RAM, 1024 * 1024 * 2);
    state.DoBytes(SPU_RAM, 1024 * 1024 * 2);
    state.DoBytes(scratchpad, 1024 * 16);
    state.DoBytes(iop_scratchpad, 1024);
    state.Do(&iop_scratchpad_start);

    //CPUs
    cpu.do_state(state);
    cp0.do_state(state);
    fpu.do_state(state);
    iop.do_state(state);
    vu0.do_state(state);
    vu1.do_state(state);

    //Interrupt registers
    intc.do_state(state);
    iop_intc.do_state(state);

    ////Timers
    timers.do_state(state);
    iop_timers.do_state(state);

    ////DMA
    dmac.do_state(state);
    iop_dma.do_state(state);

    ////"Interfaces"
    gif.do_state(state);
    sif.do_state(state);
    vif0.do_state(state);
    vif1.do_state(state);

    ////CDVD
    cdvd.do_state(state);

    ////GS
    ////Important note - this serialization function is located in gs.cpp as it contains a lot of thread-specific details
    gs.do_state(state);

    scheduler.do_state(state);
    pad.do_state(state);
    spu.do_state(state);
    spu2.do_state(state);
}

void EmotionEngine::do_state(StateSerializer& state)
{
    state.Do(&cycle_count);
    state.Do(&cycles_to_run);
    state.DoArray(icache, 128);
    state.DoArray(gpr, 512);
    state.Do(&LO);
    state.Do(&HI);
    state.Do(&LO.hi);
    state.Do(&HI.hi);
    state.Do(&PC);
    state.Do(&new_PC);
    state.Do(&SA);

    state.Do(&wait_for_IRQ);
    state.Do(&branch_on);
    state.Do(&delay_slot);

    state.Do(&deci2size);
    //state.DoBytes(&deci2handlers, sizeof(Deci2Handler) * deci2size);
    state.DoArray(deci2handlers, 128);
}

void Cop0::do_state(StateSerializer& state)
{
    state.DoArray(gpr, 32);
    state.Do(&status);
    state.Do(&cause);
    state.Do(&EPC);
    state.Do(&ErrorEPC);
    state.Do(&PCCR);
    state.Do(&PCR0);
    state.Do(&PCR1);
    state.DoArray(tlb, 48);

    if (state.GetMode() == StateSerializer::Mode::Read)
    {
        //Repopulate VTLB
        for (int i = 0; i < 48; i++)
            map_tlb(&tlb[i]);
    }
}

void Cop1::do_state(StateSerializer& state)
{
    //for (int i = 0; i < 32; i++)
    //    state.read((char*)&gpr[i].u, sizeof(uint32_t));
    state.DoArray(gpr, 32);
    state.Do(&accumulator);
    state.Do(&control);
}

void IOP::do_state(StateSerializer& state)
{
    state.DoArray(gpr, 32);
    state.Do(&LO);
    state.Do(&HI);
    state.Do(&PC);
    state.Do(&new_PC);
    state.DoArray(icache, 256);

    state.Do(&branch_delay);
    state.Do(&will_branch);
    state.Do(&wait_for_IRQ);

    //COP0
    state.Do(&cop0.status);
    state.Do(&cop0.cause);
    state.Do(&cop0.EPC);
}

void VectorUnit::do_state(StateSerializer& state)
{
    //for (int i = 0; i < 32; i++)
    //    state.read((char*)&gpr[i].u, sizeof(uint32_t) * 4);
    state.DoArray(gpr, 32);
    state.DoArray(int_gpr, 16);
    state.Do(&decoder);

    state.Do(&ACC);
    state.Do(&R);
    state.Do(&I);
    state.Do(&Q);
    state.Do(&P);
    state.Do(&CMSAR0);

    //Pipelines
    state.Do(&new_MAC_flags);
    state.DoArray(MAC_pipeline, 4);
    state.Do(&cycle_count);
    state.Do(&finish_DIV_event);
    state.Do(&new_Q_instance);
    state.Do(&DIV_event_started);
    state.Do(&finish_EFU_event);
    state.Do(&new_P_instance);
    state.Do(&EFU_event_started);

    state.Do(&int_branch_delay);
    state.Do(&int_backup_reg);
    state.Do(&int_backup_id);

    state.Do(&status);
    state.Do(&status_value);
    state.Do(&status_pipe);
    state.Do(&int_branch_pipeline);
    state.DoArray(ILW_pipeline, 4);

    state.DoArray(pipeline_state, 2);

    //XGKICK
    state.Do(&GIF_addr);
    state.Do(&transferring_GIF);
    state.Do(&XGKICK_stall);
    state.Do(&stalled_GIF_addr);

    //Memory
    if (id == 0)
    {
        state.DoBytes(&instr_mem, 1024 * 4);
        state.DoBytes(&data_mem, 1024 * 4);
    }
    else
    {
        state.DoBytes(&instr_mem, 1024 * 16);
        state.DoBytes(&data_mem, 1024 * 16);
    }

    state.Do(&running);
    state.Do(&PC);
    state.Do(&new_PC);
    state.Do(&secondbranch_PC);
    state.Do(&second_branch_pending);
    state.Do(&branch_on);
    state.Do(&branch_on_delay);
    state.Do(&finish_on);
    state.Do(&branch_delay_slot);
    state.Do(&ebit_delay_slot);
}

void INTC::do_state(StateSerializer& state)
{
    state.Do(&INTC_MASK);
    state.Do(&INTC_STAT);
    state.Do(&stat_speedhack_active);
    state.Do(&read_stat_count);
}

void IOP_INTC::do_state(StateSerializer& state)
{
    state.Do(&I_CTRL);
    state.Do(&I_STAT);
    state.Do(&I_MASK);
}

void EmotionTiming::do_state(StateSerializer& state)
{
    state.DoArray(timers, 4);
    state.DoArray(events, 4);
}

void IOPTiming::do_state(StateSerializer& state)
{
    state.DoArray(timers, 6);
}

void DMAC::do_state(StateSerializer& state)
{
    state.DoArray(channels, 15);

    if (state.GetMode() == StateSerializer::Mode::Read)
        apply_dma_funcs();

    state.Do(&control);
    state.Do(&interrupt_stat);
    state.Do(&PCR);
    state.Do(&RBOR);
    state.Do(&RBSR);
    state.Do(&SQWC);
    state.Do(&STADR);
    state.Do(&mfifo_empty_triggered);
    state.Do(&cycles_to_run);
    state.Do(&master_disable);

    int index = -1;

    if (state.GetMode() == StateSerializer::Mode::Read)
    {
        state.Do(&index);
        if (index >= 0)
            active_channel = &channels[index];
        else
            active_channel = nullptr;
    }
    else
    {
        if (active_channel)
            index = active_channel->index;
        else
            index = -1;

        state.Do(&index);
    }

    if (state.GetMode() == StateSerializer::Mode::Read)
    {
        int queued_size;
        state.Do(&queued_size);
        if (queued_size > 0)
        {
            for (int i = 0; i < queued_size; i++)
            {
                state.Do(&index);
                queued_channels.push_back(&channels[index]);
            }
        }
    }
    else
    {
        int size = queued_channels.size();
        state.Do(&size);
        if (size > 0)
        {
            for (auto it = queued_channels.begin(); it != queued_channels.end(); it++)
            {
                index = (*it)->index;
                state.Do((&index));
            }
        }
    }
}

void IOP_DMA::do_state(StateSerializer& state)
{
    state.Do(&channels);

    if (state.GetMode() == StateSerializer::Mode::Read)
    {
        int active_index;
        state.Do(&active_index);
        if (active_index)
            active_channel = &channels[active_index - 1];
        else
            active_channel = nullptr;
    }
    else
    {
        int active_index = 0;
        if (active_channel)
            active_index = active_channel->index + 1;
        state.Do(&active_index);
    }

    if (state.GetMode() == StateSerializer::Mode::Read)
    {
        int queued_size = 0;
        state.Do(&queued_size);
        for (int i = 0; i < queued_size; i++)
        {
            int index;
            state.Do(&index);
            queued_channels.push_back(&channels[index]);
        }
    }
    else
    {
        int queued_size = static_cast<int>(queued_channels.size());
        state.Do(&queued_size);
        for (auto it = queued_channels.begin(); it != queued_channels.end(); it++)
        {
            IOP_DMA_Channel* chan = *it;
            state.Do(&chan->index);
        }
    }

    state.Do(&DPCR);
    state.Do(&DICR);

    //We have to reapply the function pointers as there's no guarantee they will remain in memory
    //the next time Dobie is loaded
    if (state.GetMode() == StateSerializer::Mode::Read)
        apply_dma_functions();
}

void GraphicsInterface::do_state(StateSerializer& state)
{
    state.Do(&FIFO);

    state.Do(&path);
    state.Do(&active_path);
    state.Do(&path_queue);
    state.Do(&path3_vif_masked);
    state.Do(&internal_Q);
    state.Do(&path3_dma_running);
    state.Do(&intermittent_mode);
    state.Do(&outputting_path);
    state.Do(&path3_mode_masked);
    state.Do(&gif_temporary_stop);
}

void SubsystemInterface::do_state(StateSerializer& state)
{
    state.Do(&mscom);
    state.Do(&smcom);
    state.Do(&msflag);
    state.Do(&smflag);
    state.Do(&control);
    state.Do(&SIF0_FIFO);
    state.Do(&SIF1_FIFO);
}

void VectorInterface::do_state(StateSerializer& state)
{
    state.Do(&FIFO);
    state.Do(&internal_FIFO);

    state.Do(&imm);
    state.Do(&command);
    state.Do(&mpg);
    state.Do(&unpack);
    state.Do(&wait_for_VU);
    state.Do(&flush_stall);
    state.Do(&wait_cmd_value);
    state.Do(&buffer_size);
    state.DoArray(buffer, 4);

    state.Do(&DBF);
    state.Do(&CYCLE);
    state.Do(&OFST);
    state.Do(&BASE);
    state.Do(&TOP);
    state.Do(&TOPS);
    state.Do(&ITOP);
    state.Do(&ITOPS);
    state.Do(&MODE);
    state.Do(&MASK);
    state.DoArray(ROW, 4);
    state.DoArray(COL, 4);
    state.Do(&CODE);
    state.Do(&command_len);

    state.Do(&vif_ibit_detected);
    state.Do(&vif_interrupt);
    state.Do(&vif_stalled);
    state.Do(&vif_stop);
    state.Do(&vif_forcebreak);
    state.Do(&vif_cmd_status);
    state.Do(&internal_WL);

    state.Do(&mark_detected);
    state.Do(&VIF_ERR);
}

void CDVD_Drive::do_state(StateSerializer& state)
{
    state.Do(&file_size);
    state.Do(&read_bytes_left);
    state.Do(&disc_type);
    state.Do(&speed);
    state.Do(&current_sector);
    state.Do(&sector_pos);
    state.Do(&sectors_left);
    state.Do(&block_size);
    state.DoBytes(read_buffer, 4096);
    state.Do(&ISTAT);
    state.Do(&drive_status);
    state.Do(&is_spinning);

    state.Do(&active_N_command);
    state.Do(&N_command);
    state.DoArray(N_command_params, 11);
    state.Do(&N_params);
    state.Do(&N_status);

    state.Do(&S_command);
    state.DoArray(S_command_params, 16);
    state.DoArray(S_outdata, 16);
    state.Do(&S_params);
    state.Do(&S_out_params);
    state.Do(&S_status);
    state.Do(&rtc);
}

void Scheduler::do_state(StateSerializer& state)
{
    state.Do(&ee_cycles);
    state.Do(&bus_cycles);
    state.Do(&iop_cycles);
    state.Do(&run_cycles);
    state.Do(&closest_event_time);

    state.Do(&events);
    state.Do(&timers);
}

void Gamepad::do_state(StateSerializer& state)
{
    state.DoArray(command_buffer, 25);
    state.DoArray(rumble_values, 8);
    state.Do(&mode_lock);
    state.Do(&command);
    state.Do(&command_length);
    state.Do(&data_count);
    state.Do(&pad_mode);
    state.Do(&config_mode);
}

void SPU::do_state(StateSerializer& state)
{
    state.DoArray(voices, 24);
    state.DoArray(core_att, 2);
    state.Do(&status);
    state.Do(&spdif_irq);
    state.Do(&transfer_addr);
    state.Do(&current_addr);
    state.Do(&autodma_ctrl);
    state.Do(&buffer_pos);
    state.DoArray(IRQA, 2);
    state.Do(&ENDX);
    state.Do(&key_off);
    state.Do(&key_on);
    state.Do(&noise);
    state.Do(&output_enable);
    state.Do(&reverb);
    state.Do(&effect_enable);
    state.Do(&effect_volume_l);
    state.Do(&effect_volume_r);
    state.Do(&current_buffer);
    state.Do(&ADMA_progress);
    state.Do(&data_input_volume_l);
    state.Do(&data_input_volume_r);
    state.Do(&core_volume_l);
    state.Do(&core_volume_r);
    state.Do(&MVOLL);
    state.Do(&MVOLR);
    state.Do(&mix_state);
    state.Do(&voice_mixdry_left);
    state.Do(&voice_mixdry_right);
    state.Do(&voice_mixwet_left);
    state.Do(&voice_mixwet_right);
    state.Do(&voice_pitch_mod);
    state.Do(&voice_noise_gen);
}
