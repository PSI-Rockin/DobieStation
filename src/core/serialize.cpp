#include <fstream>
#include <cstring>
#include "emulator.hpp"

#define VER_MAJOR 0
#define VER_MINOR 0
#define VER_REV 29

using namespace std;

bool Emulator::request_load_state(const char *file_name)
{
    ifstream state(file_name, ios::binary);
    if (!state.is_open())
        return false;
    state.close();
    save_state_path = file_name;
    load_requested = true;
    return true;
}

bool Emulator::request_save_state(const char *file_name)
{
    ofstream state(file_name, ios::binary);
    if (!state.is_open())
        return false;
    state.close();
    save_state_path = file_name;
    save_requested = true;
    return true;
}

void Emulator::load_state(const char *file_name)
{
    load_requested = false;
    printf("[Emulator] Loading state...\n");
    ifstream state(file_name, ios::binary);
    if (!state.is_open())
    {
        Errors::non_fatal("Failed to load save state");
        return;
    }

    //Perform sanity checks
    char dobie_buffer[5];
    state.read(dobie_buffer, sizeof(dobie_buffer));
    if (strncmp(dobie_buffer, "DOBIE", 5))
    {
        state.close();
        Errors::non_fatal("Save state invalid");
        return;
    }

    uint32_t major, minor, rev;
    state.read((char*)&major, sizeof(major));
    state.read((char*)&minor, sizeof(minor));
    state.read((char*)&rev, sizeof(rev));

    if (major != VER_MAJOR || minor != VER_MINOR || rev != VER_REV)
    {
        state.close();
        Errors::non_fatal("Save state doesn't match version");
        return;
    }

    reset();

    //Emulator info
    state.read((char*)&VBLANK_sent, sizeof(VBLANK_sent));
    state.read((char*)&frames, sizeof(frames));

    //RAM
    state.read((char*)RDRAM, 1024 * 1024 * 32);
    state.read((char*)IOP_RAM, 1024 * 1024 * 2);
    state.read((char*)SPU_RAM, 1024 * 1024 * 2);
    state.read((char*)scratchpad, 1024 * 16);
    state.read((char*)iop_scratchpad, 1024);
    state.read((char*)&iop_scratchpad_start, sizeof(iop_scratchpad_start));

    //CPUs
    cpu.load_state(state);
    cp0.load_state(state);
    fpu.load_state(state);
    iop.load_state(state);
    vu0.load_state(state);
    vu1.load_state(state);

    //Interrupt registers
    intc.load_state(state);
    state.read((char*)&IOP_I_CTRL, sizeof(uint32_t));
    state.read((char*)&IOP_I_MASK, sizeof(uint32_t));
    state.read((char*)&IOP_I_STAT, sizeof(uint32_t));
    state.read((char*)&iop_i_ctrl_delay, sizeof(iop_i_ctrl_delay));

    //Timers
    timers.load_state(state);
    iop_timers.load_state(state);

    //DMA
    dmac.load_state(state);
    iop_dma.load_state(state);

    //"Interfaces"
    gif.load_state(state);
    sif.load_state(state);
    vif0.load_state(state);
    vif1.load_state(state);

    //CDVD
    cdvd.load_state(state);

    //GS
    //Important note - this serialization function is located in gs.cpp as it contains a lot of thread-specific details
    gs.load_state(state);

    scheduler.load_state(state);
    pad.load_state(state);
    spu.load_state(state);
    spu2.load_state(state);

    state.close();
    printf("[Emulator] Success!\n");
}

void Emulator::save_state(const char *file_name)
{
    save_requested = false;
    printf("[Emulator] Saving state...\n");
    ofstream state(file_name, ios::binary);
    if (!state.is_open())
    {
        Errors::non_fatal("Failed to save state");
        return;
    }

    uint32_t major = VER_MAJOR;
    uint32_t minor = VER_MINOR;
    uint32_t rev = VER_REV;

    //Sanity check and version
    state << "DOBIE";
    state.write((char*)&major, sizeof(uint32_t));
    state.write((char*)&minor, sizeof(uint32_t));
    state.write((char*)&rev, sizeof(uint32_t));

    //Emulator info
    state.write((char*)&VBLANK_sent, sizeof(VBLANK_sent));
    state.write((char*)&frames, sizeof(frames));

    //RAM
    state.write((char*)RDRAM, 1024 * 1024 * 32);
    state.write((char*)IOP_RAM, 1024 * 1024 * 2);
    state.write((char*)SPU_RAM, 1024 * 1024 * 2);
    state.write((char*)scratchpad, 1024 * 16);
    state.write((char*)iop_scratchpad, 1024);
    state.write((char*)&iop_scratchpad_start, sizeof(iop_scratchpad_start));

    //CPUs
    cpu.save_state(state);
    cp0.save_state(state);
    fpu.save_state(state);
    iop.save_state(state);
    vu0.save_state(state);
    vu1.save_state(state);

    //Interrupt registers
    intc.save_state(state);
    state.write((char*)&IOP_I_CTRL, sizeof(uint32_t));
    state.write((char*)&IOP_I_MASK, sizeof(uint32_t));
    state.write((char*)&IOP_I_STAT, sizeof(uint32_t));
    state.write((char*)&iop_i_ctrl_delay, sizeof(iop_i_ctrl_delay));

    //Timers
    timers.save_state(state);
    iop_timers.save_state(state);

    //DMA
    dmac.save_state(state);
    iop_dma.save_state(state);

    //"Interfaces"
    gif.save_state(state);
    sif.save_state(state);
    vif0.save_state(state);
    vif1.save_state(state);

    //CDVD
    cdvd.save_state(state);

    //GS
    //Important note - this serialization function is located in gs.cpp as it contains a lot of thread-specific details
    gs.save_state(state);

    scheduler.save_state(state);
    pad.save_state(state);
    spu.save_state(state);
    spu2.save_state(state);

    state.close();
    printf("Success!\n");
}

void EmotionEngine::load_state(ifstream &state)
{
    state.read((char*)&cycle_count, sizeof(cycle_count));
    state.read((char*)&cycles_to_run, sizeof(cycles_to_run));
    state.read((char*)&icache, sizeof(icache));
    state.read((char*)&gpr, sizeof(gpr));
    state.read((char*)&LO, sizeof(uint64_t));
    state.read((char*)&HI, sizeof(uint64_t));
    state.read((char*)&LO.hi, sizeof(uint64_t));
    state.read((char*)&HI.hi, sizeof(uint64_t));
    state.read((char*)&PC, sizeof(uint32_t));
    state.read((char*)&new_PC, sizeof(uint32_t));
    state.read((char*)&SA, sizeof(uint64_t));

    //state.read((char*)&IRQ_raised, sizeof(IRQ_raised));
    state.read((char*)&wait_for_IRQ, sizeof(wait_for_IRQ));
    state.read((char*)&branch_on, sizeof(branch_on));
    state.read((char*)&delay_slot, sizeof(delay_slot));

    state.read((char*)&deci2size, sizeof(deci2size));
    state.read((char*)&deci2handlers, sizeof(Deci2Handler) * deci2size);
}

void EmotionEngine::save_state(ofstream &state)
{
    state.write((char*)&cycle_count, sizeof(cycle_count));
    state.write((char*)&cycles_to_run, sizeof(cycles_to_run));
    state.write((char*)&icache, sizeof(icache));
    state.write((char*)&gpr, sizeof(gpr));
    state.write((char*)&LO, sizeof(uint64_t));
    state.write((char*)&HI, sizeof(uint64_t));
    state.write((char*)&LO.lo, sizeof(uint64_t));
    state.write((char*)&HI.hi, sizeof(uint64_t));
    state.write((char*)&PC, sizeof(uint32_t));
    state.write((char*)&new_PC, sizeof(uint32_t));
    state.write((char*)&SA, sizeof(uint64_t));

    //state.write((char*)&IRQ_raised, sizeof(IRQ_raised));
    state.write((char*)&wait_for_IRQ, sizeof(wait_for_IRQ));
    state.write((char*)&branch_on, sizeof(branch_on));
    state.write((char*)&delay_slot, sizeof(delay_slot));

    state.write((char*)&deci2size, sizeof(deci2size));
    state.write((char*)&deci2handlers, sizeof(Deci2Handler) * deci2size);
}

void Cop0::load_state(ifstream &state)
{
    state.read((char*)&gpr, sizeof(uint32_t));
    state.read((char*)&status, sizeof(status));
    state.read((char*)&cause, sizeof(cause));
    state.read((char*)&EPC, sizeof(EPC));
    state.read((char*)&ErrorEPC, sizeof(ErrorEPC));
    state.read((char*)&PCCR, sizeof(PCCR));
    state.read((char*)&PCR0, sizeof(PCR0));
    state.read((char*)&PCR1, sizeof(PCR1));
    state.read((char*)&tlb, sizeof(tlb));

    //Repopulate VTLB
    for (int i = 0; i < 48; i++)
        map_tlb(&tlb[i]);
}

void Cop0::save_state(ofstream &state)
{
    state.write((char*)&gpr, sizeof(uint32_t));
    state.write((char*)&status, sizeof(status));
    state.write((char*)&cause, sizeof(cause));
    state.write((char*)&EPC, sizeof(EPC));
    state.write((char*)&ErrorEPC, sizeof(ErrorEPC));
    state.write((char*)&PCCR, sizeof(PCCR));
    state.write((char*)&PCR0, sizeof(PCR0));
    state.write((char*)&PCR1, sizeof(PCR1));
    state.write((char*)&tlb, sizeof(tlb));
}

void Cop1::load_state(ifstream &state)
{
    for (int i = 0; i < 32; i++)
        state.read((char*)&gpr[i].u, sizeof(uint32_t));
    state.read((char*)&accumulator.u, sizeof(uint32_t));
    state.read((char*)&control, sizeof(control));
}

void Cop1::save_state(ofstream &state)
{
    for (int i = 0; i < 32; i++)
        state.write((char*)&gpr[i].u, sizeof(uint32_t));
    state.write((char*)&accumulator.u, sizeof(uint32_t));
    state.write((char*)&control, sizeof(control));
}

void IOP::load_state(ifstream &state)
{
    state.read((char*)&gpr, sizeof(gpr));
    state.read((char*)&LO, sizeof(LO));
    state.read((char*)&HI, sizeof(HI));
    state.read((char*)&PC, sizeof(PC));
    state.read((char*)&new_PC, sizeof(new_PC));
    state.read((char*)&icache, sizeof(icache));

    state.read((char*)&branch_delay, sizeof(branch_delay));
    state.read((char*)&will_branch, sizeof(branch_delay));
    state.read((char*)&wait_for_IRQ, sizeof(wait_for_IRQ));

    //COP0
    state.read((char*)&cop0.status, sizeof(cop0.status));
    state.read((char*)&cop0.cause, sizeof(cop0.cause));
    state.read((char*)&cop0.EPC, sizeof(cop0.EPC));
}

void IOP::save_state(ofstream &state)
{
    state.write((char*)&gpr, sizeof(gpr));
    state.write((char*)&LO, sizeof(LO));
    state.write((char*)&HI, sizeof(HI));
    state.write((char*)&PC, sizeof(PC));
    state.write((char*)&new_PC, sizeof(new_PC));
    state.write((char*)&icache, sizeof(icache));

    state.write((char*)&branch_delay, sizeof(branch_delay));
    state.write((char*)&will_branch, sizeof(branch_delay));
    state.write((char*)&wait_for_IRQ, sizeof(wait_for_IRQ));

    //COP0
    state.write((char*)&cop0.status, sizeof(cop0.status));
    state.write((char*)&cop0.cause, sizeof(cop0.cause));
    state.write((char*)&cop0.EPC, sizeof(cop0.EPC));
}

void VectorUnit::load_state(ifstream &state)
{
    for (int i = 0; i < 32; i++)
        state.read((char*)&gpr[i].u, sizeof(uint32_t) * 4);
    state.read((char*)&int_gpr, sizeof(int_gpr));
    state.read((char*)&decoder, sizeof(decoder));

    state.read((char*)&ACC.u, sizeof(uint32_t) * 4);
    state.read((char*)&R.u, sizeof(R.u));
    state.read((char*)&I.u, sizeof(I.u));
    state.read((char*)&Q.u, sizeof(Q.u));
    state.read((char*)&P.u, sizeof(P.u));
    state.read((char*)&CMSAR0, sizeof(CMSAR0));

    //Pipelines
    state.read((char*)&new_MAC_flags, sizeof(new_MAC_flags));
    state.read((char*)&MAC_pipeline, sizeof(MAC_pipeline));
    state.read((char*)&cycle_count, sizeof(cycle_count));
    state.read((char*)&finish_DIV_event, sizeof(finish_DIV_event));
    state.read((char*)&new_Q_instance.u, sizeof(new_Q_instance.u));
    state.read((char*)&DIV_event_started, sizeof(DIV_event_started));
    state.read((char*)&finish_EFU_event, sizeof(finish_EFU_event));
    state.read((char*)&new_P_instance.u, sizeof(new_P_instance.u));
    state.read((char*)&EFU_event_started, sizeof(EFU_event_started));

    state.read((char*)&int_branch_delay, sizeof(int_branch_delay));
    state.read((char*)&int_backup_reg, sizeof(int_backup_reg));
    state.read((char*)&int_backup_id, sizeof(int_backup_id));

    state.read((char*)&status, sizeof(status));
    state.read((char*)&status_value, sizeof(status_value));
    state.read((char*)&status_pipe, sizeof(status_pipe));
    state.read((char*)&int_branch_pipeline, sizeof(int_branch_pipeline));
    state.read((char*)&ILW_pipeline, sizeof(ILW_pipeline));

    state.read((char*)&pipeline_state, sizeof(pipeline_state));

    //XGKICK
    state.read((char*)&GIF_addr, sizeof(GIF_addr));
    state.read((char*)&transferring_GIF, sizeof(transferring_GIF));
    state.read((char*)&XGKICK_stall, sizeof(XGKICK_stall));
    state.read((char*)&stalled_GIF_addr, sizeof(stalled_GIF_addr));

    //Memory
    if (id == 0)
    {
        state.read((char*)&instr_mem, 1024 * 4);
        state.read((char*)&data_mem, 1024 * 4);
    }
    else
    {
        state.read((char*)&instr_mem, 1024 * 16);
        state.read((char*)&data_mem, 1024 * 16);
    }

    state.read((char*)&running, sizeof(running));
    state.read((char*)&PC, sizeof(PC));
    state.read((char*)&new_PC, sizeof(new_PC));
    state.read((char*)&secondbranch_PC, sizeof(secondbranch_PC));
    state.read((char*)&second_branch_pending, sizeof(second_branch_pending));
    state.read((char*)&branch_on, sizeof(branch_on));
    state.read((char*)&branch_on_delay, sizeof(branch_on_delay));
    state.read((char*)&finish_on, sizeof(finish_on));
    state.read((char*)&branch_delay_slot, sizeof(branch_delay_slot));
    state.read((char*)&ebit_delay_slot, sizeof(ebit_delay_slot));
}

void VectorUnit::save_state(ofstream &state)
{
    for (int i = 0; i < 32; i++)
        state.write((char*)&gpr[i].u, sizeof(uint32_t) * 4);
    state.write((char*)&int_gpr, sizeof(int_gpr));
    state.write((char*)&decoder, sizeof(decoder));

    state.write((char*)&ACC.u, sizeof(uint32_t) * 4);
    state.write((char*)&R.u, sizeof(R.u));
    state.write((char*)&I.u, sizeof(I.u));
    state.write((char*)&Q.u, sizeof(Q.u));
    state.write((char*)&P.u, sizeof(P.u));
    state.write((char*)&CMSAR0, sizeof(CMSAR0));

    //Pipelines
    state.write((char*)&new_MAC_flags, sizeof(new_MAC_flags));
    state.write((char*)&MAC_pipeline, sizeof(MAC_pipeline));
    state.write((char*)&cycle_count, sizeof(cycle_count));
    state.write((char*)&finish_DIV_event, sizeof(finish_DIV_event));
    state.write((char*)&new_Q_instance.u, sizeof(new_Q_instance.u));
    state.write((char*)&DIV_event_started, sizeof(DIV_event_started));
    state.write((char*)&finish_EFU_event, sizeof(finish_EFU_event));
    state.write((char*)&new_P_instance.u, sizeof(new_P_instance.u));
    state.write((char*)&EFU_event_started, sizeof(EFU_event_started));

    state.write((char*)&int_branch_delay, sizeof(int_branch_delay));
    state.write((char*)&int_backup_reg, sizeof(int_backup_reg));
    state.write((char*)&int_backup_id, sizeof(int_backup_id));
    state.write((char*)&status, sizeof(status));
    state.write((char*)&status_value, sizeof(status_value));
    state.write((char*)&status_pipe, sizeof(status_pipe));
    state.write((char*)&int_branch_pipeline, sizeof(int_branch_pipeline));
    state.write((char*)&ILW_pipeline, sizeof(ILW_pipeline));

    state.write((char*)&pipeline_state, sizeof(pipeline_state));

    //XGKICK
    state.write((char*)&GIF_addr, sizeof(GIF_addr));
    state.write((char*)&transferring_GIF, sizeof(transferring_GIF));
    state.write((char*)&XGKICK_stall, sizeof(XGKICK_stall));
    state.write((char*)&stalled_GIF_addr, sizeof(stalled_GIF_addr));

    //Memory
    if (id == 0)
    {
        state.write((char*)&instr_mem, 1024 * 4);
        state.write((char*)&data_mem, 1024 * 4);
    }
    else
    {
        state.write((char*)&instr_mem, 1024 * 16);
        state.write((char*)&data_mem, 1024 * 16);
    }

    state.write((char*)&running, sizeof(running));
    state.write((char*)&PC, sizeof(PC));
    state.write((char*)&new_PC, sizeof(new_PC));
    state.write((char*)&secondbranch_PC, sizeof(secondbranch_PC));
    state.write((char*)&second_branch_pending, sizeof(second_branch_pending));
    state.write((char*)&branch_on, sizeof(branch_on));
    state.write((char*)&branch_on_delay, sizeof(branch_on_delay));
    state.write((char*)&finish_on, sizeof(finish_on));
    state.write((char*)&branch_delay_slot, sizeof(branch_delay_slot));
    state.write((char*)&ebit_delay_slot, sizeof(ebit_delay_slot));
}

void INTC::load_state(ifstream &state)
{
    state.read((char*)&INTC_MASK, sizeof(INTC_MASK));
    state.read((char*)&INTC_STAT, sizeof(INTC_STAT));
    state.read((char*)&stat_speedhack_active, sizeof(stat_speedhack_active));
    state.read((char*)&read_stat_count, sizeof(read_stat_count));
}

void INTC::save_state(ofstream &state)
{
    state.write((char*)&INTC_MASK, sizeof(INTC_MASK));
    state.write((char*)&INTC_STAT, sizeof(INTC_STAT));
    state.write((char*)&stat_speedhack_active, sizeof(stat_speedhack_active));
    state.write((char*)&read_stat_count, sizeof(read_stat_count));
}

void EmotionTiming::load_state(ifstream &state)
{
    state.read((char*)&timers, sizeof(timers));
    state.read((char*)&cycle_count, sizeof(cycle_count));
    state.read((char*)&next_event, sizeof(next_event));
}

void EmotionTiming::save_state(ofstream &state)
{
    state.write((char*)&timers, sizeof(timers));
    state.write((char*)&cycle_count, sizeof(cycle_count));
    state.write((char*)&next_event, sizeof(next_event));
}

void IOPTiming::load_state(ifstream &state)
{
    state.read((char*)&timers, sizeof(timers));
    state.read((char*)&cycle_count, sizeof(cycle_count));
    state.read((char*)&next_event, sizeof(next_event));
}

void IOPTiming::save_state(ofstream &state)
{
    state.write((char*)&timers, sizeof(timers));
    state.write((char*)&cycle_count, sizeof(cycle_count));
    state.write((char*)&next_event, sizeof(next_event));
}

void DMAC::load_state(ifstream &state)
{
    state.read((char*)&channels, sizeof(channels));

    apply_dma_funcs();

    state.read((char*)&control, sizeof(control));
    state.read((char*)&interrupt_stat, sizeof(interrupt_stat));
    state.read((char*)&PCR, sizeof(PCR));
    state.read((char*)&RBOR, sizeof(RBOR));
    state.read((char*)&RBSR, sizeof(RBSR));
    state.read((char*)&SQWC, sizeof(SQWC));
    state.read((char*)&STADR, sizeof(STADR));
    state.read((char*)&mfifo_empty_triggered, sizeof(mfifo_empty_triggered));
    state.read((char*)&cycles_to_run, sizeof(cycles_to_run));
    state.read((char*)&master_disable, sizeof(master_disable));

    int index;
    state.read((char*)&index, sizeof(index));
    if (index >= 0)
        active_channel = &channels[index];
    else
        active_channel = nullptr;

    int queued_size;
    state.read((char*)&queued_size, sizeof(queued_size));
    if (queued_size > 0)
    {
        for (int i = 0; i < queued_size; i++)
        {
            state.read((char*)&index, sizeof(index));
            queued_channels.push_back(&channels[index]);
        }
    }
}

void DMAC::save_state(ofstream &state)
{
    state.write((char*)&channels, sizeof(channels));

    state.write((char*)&control, sizeof(control));
    state.write((char*)&interrupt_stat, sizeof(interrupt_stat));
    state.write((char*)&PCR, sizeof(PCR));
    state.write((char*)&RBOR, sizeof(RBOR));
    state.write((char*)&RBSR, sizeof(RBSR));
    state.write((char*)&SQWC, sizeof(SQWC));
    state.write((char*)&STADR, sizeof(STADR));
    state.write((char*)&mfifo_empty_triggered, sizeof(mfifo_empty_triggered));
    state.write((char*)&cycles_to_run, sizeof(cycles_to_run));
    state.write((char*)&master_disable, sizeof(master_disable));

    int index;
    if (active_channel)
        index = active_channel->index;
    else
        index = -1;

    state.write((char*)&index, sizeof(index));
    int size = queued_channels.size();
    state.write((char*)&size, sizeof(size));
    if (size > 0)
    {
        for (auto it = queued_channels.begin(); it != queued_channels.end(); )
        {
            index = (*it)->index;
            state.write((char*)&index, sizeof(index));
        }
    }
}

void IOP_DMA::load_state(ifstream &state)
{
    state.read((char*)&channels, sizeof(channels));

    int active_index;
    state.read((char*)&active_index, sizeof(active_index));
    if (active_index)
        active_channel = &channels[active_index - 1];
    else
        active_channel = nullptr;

    int queued_size = 0;
    state.read((char*)&queued_size, sizeof(queued_size));
    for (int i = 0; i < queued_size; i++)
    {
        int index;
        state.read((char*)&index, sizeof(index));
        queued_channels.push_back(&channels[index]);
    }

    state.read((char*)&DPCR, sizeof(DPCR));
    state.read((char*)&DICR, sizeof(DICR));

    //We have to reapply the function pointers as there's no guarantee they will remain in memory
    //the next time Dobie is loaded
    apply_dma_functions();
}

void IOP_DMA::save_state(ofstream &state)
{
    state.write((char*)&channels, sizeof(channels));

    int active_index = 0;
    if (active_channel)
        active_index = active_channel->index + 1;
    state.write((char*)&active_index, sizeof(active_index));

    int queued_size = queued_channels.size();
    state.write((char*)&queued_size, sizeof(queued_size));
    for (auto it = queued_channels.begin(); it != queued_channels.end(); it++)
    {
        IOP_DMA_Channel* chan = *it;
        state.write((char*)&chan->index, sizeof(chan->index));
    }

    state.write((char*)&DPCR, sizeof(DPCR));
    state.write((char*)&DICR, sizeof(DICR));
}

void GraphicsInterface::load_state(ifstream &state)
{
    int size;
    uint128_t FIFO_buffer[16];
    state.read((char*)&size, sizeof(size));
    state.read((char*)&FIFO_buffer, sizeof(uint128_t) * size);
    for (int i = 0; i < size; i++)
        FIFO.push(FIFO_buffer[i]);

    state.read((char*)&path, sizeof(path));
    state.read((char*)&active_path, sizeof(active_path));
    state.read((char*)&path_queue, sizeof(path_queue));
    state.read((char*)&path3_vif_masked, sizeof(path3_vif_masked));
    state.read((char*)&internal_Q, sizeof(internal_Q));
    state.read((char*)&path3_dma_waiting, sizeof(path3_dma_waiting));
}

void GraphicsInterface::save_state(ofstream &state)
{
    int size = FIFO.size();
    uint128_t FIFO_buffer[16];
    for (int i = 0; i < size; i++)
    {
        FIFO_buffer[i] = FIFO.front();
        FIFO.pop();
    }
    state.write((char*)&size, sizeof(size));
    state.write((char*)&FIFO_buffer, sizeof(uint128_t) * size);
    for (int i = 0; i < size; i++)
        FIFO.push(FIFO_buffer[i]);

    state.write((char*)&path, sizeof(path));
    state.write((char*)&active_path, sizeof(active_path));
    state.write((char*)&path_queue, sizeof(path_queue));
    state.write((char*)&path3_vif_masked, sizeof(path3_vif_masked));
    state.write((char*)&internal_Q, sizeof(internal_Q));
    state.write((char*)&path3_dma_waiting, sizeof(path3_dma_waiting));
}

void SubsystemInterface::load_state(ifstream &state)
{
    state.read((char*)&mscom, sizeof(mscom));
    state.read((char*)&smcom, sizeof(smcom));
    state.read((char*)&msflag, sizeof(msflag));
    state.read((char*)&smflag, sizeof(smflag));
    state.read((char*)&control, sizeof(control));

    int size;
    uint32_t buffer[32];
    state.read((char*)&size, sizeof(int));
    state.read((char*)&buffer, sizeof(uint32_t) * size);

    //FIFOs are already cleared by the reset call, so no need to pop them
    for (int i = 0; i < size; i++)
        SIF0_FIFO.push(buffer[i]);

    state.read((char*)&size, sizeof(int));
    state.read((char*)&buffer, sizeof(uint32_t) * size);

    for (int i = 0; i < size; i++)
        SIF1_FIFO.push(buffer[i]);
}

void SubsystemInterface::save_state(ofstream &state)
{
    state.write((char*)&mscom, sizeof(mscom));
    state.write((char*)&smcom, sizeof(smcom));
    state.write((char*)&msflag, sizeof(msflag));
    state.write((char*)&smflag, sizeof(smflag));
    state.write((char*)&control, sizeof(control));

    int size = SIF0_FIFO.size();
    uint32_t buffer[32];
    for (int i = 0; i < size; i++)
    {
        buffer[i] = SIF0_FIFO.front();
        SIF0_FIFO.pop();
    }
    state.write((char*)&size, sizeof(int));
    state.write((char*)&buffer, sizeof(uint32_t) * size);
    for (int i = 0; i < size; i++)
        SIF0_FIFO.push(buffer[i]);

    size = SIF1_FIFO.size();
    for (int i = 0; i < size; i++)
    {
        buffer[i] = SIF1_FIFO.front();
        SIF1_FIFO.pop();
    }
    state.write((char*)&size, sizeof(int));
    state.write((char*)&buffer, sizeof(uint32_t) * size);
    for (int i = 0; i < size; i++)
        SIF1_FIFO.push(buffer[i]);
}

void VectorInterface::load_state(ifstream &state)
{
    int size;
    uint32_t FIFO_buffer[64];
    state.read((char*)&size, sizeof(size));
    state.read((char*)&FIFO_buffer, sizeof(uint32_t) * size);
    for (int i = 0; i < size; i++)
        FIFO.push(FIFO_buffer[i]);

    state.read((char*)&imm, sizeof(imm));
    state.read((char*)&command, sizeof(command));
    state.read((char*)&mpg, sizeof(mpg));
    state.read((char*)&unpack, sizeof(unpack));
    state.read((char*)&wait_for_VU, sizeof(wait_for_VU));
    state.read((char*)&flush_stall, sizeof(flush_stall));
    state.read((char*)&wait_cmd_value, sizeof(wait_cmd_value));

    state.read((char*)&buffer_size, sizeof(buffer_size));
    state.read((char*)&buffer, sizeof(buffer));

    state.read((char*)&DBF, sizeof(DBF));
    state.read((char*)&CYCLE, sizeof(CYCLE));
    state.read((char*)&OFST, sizeof(OFST));
    state.read((char*)&BASE, sizeof(BASE));
    state.read((char*)&TOP, sizeof(TOP));
    state.read((char*)&TOPS, sizeof(TOPS));
    state.read((char*)&ITOP, sizeof(ITOP));
    state.read((char*)&ITOPS, sizeof(ITOPS));
    state.read((char*)&MODE, sizeof(MODE));
    state.read((char*)&MASK, sizeof(MASK));
    state.read((char*)&ROW, sizeof(ROW));
    state.read((char*)&COL, sizeof(COL));
    state.read((char*)&CODE, sizeof(CODE));
    state.read((char*)&command_len, sizeof(command_len));

    state.read((char*)&vif_ibit_detected, sizeof(vif_ibit_detected));
    state.read((char*)&vif_interrupt, sizeof(vif_interrupt));
    state.read((char*)&vif_stalled, sizeof(vif_stalled));
    state.read((char*)&vif_stop, sizeof(vif_stop));

    state.read((char*)&mark_detected, sizeof(mark_detected));
    state.read((char*)&VIF_ERR, sizeof(VIF_ERR));
}

void VectorInterface::save_state(ofstream &state)
{
    int size = FIFO.size();
    uint32_t FIFO_buffer[64];
    for (int i = 0; i < size; i++)
    {
        FIFO_buffer[i] = FIFO.front();
        FIFO.pop();
    }
    state.write((char*)&size, sizeof(size));
    state.write((char*)&FIFO_buffer, sizeof(uint32_t) * size);
    for (int i = 0; i < size; i++)
        FIFO.push(FIFO_buffer[i]);

    state.write((char*)&imm, sizeof(imm));
    state.write((char*)&command, sizeof(command));
    state.write((char*)&mpg, sizeof(mpg));
    state.write((char*)&unpack, sizeof(unpack));
    state.write((char*)&wait_for_VU, sizeof(wait_for_VU));
    state.write((char*)&flush_stall, sizeof(flush_stall));
    state.write((char*)&wait_cmd_value, sizeof(wait_cmd_value));

    state.write((char*)&buffer_size, sizeof(buffer_size));
    state.write((char*)&buffer, sizeof(buffer));

    state.write((char*)&DBF, sizeof(DBF));
    state.write((char*)&CYCLE, sizeof(CYCLE));
    state.write((char*)&OFST, sizeof(OFST));
    state.write((char*)&BASE, sizeof(BASE));
    state.write((char*)&TOP, sizeof(TOP));
    state.write((char*)&TOPS, sizeof(TOPS));
    state.write((char*)&ITOP, sizeof(ITOP));
    state.write((char*)&ITOPS, sizeof(ITOPS));
    state.write((char*)&MODE, sizeof(MODE));
    state.write((char*)&MASK, sizeof(MASK));
    state.write((char*)&ROW, sizeof(ROW));
    state.write((char*)&COL, sizeof(COL));
    state.write((char*)&CODE, sizeof(CODE));
    state.write((char*)&command_len, sizeof(command_len));

    state.write((char*)&vif_ibit_detected, sizeof(vif_ibit_detected));
    state.write((char*)&vif_interrupt, sizeof(vif_interrupt));
    state.write((char*)&vif_stalled, sizeof(vif_stalled));
    state.write((char*)&vif_stop, sizeof(vif_stop));

    state.write((char*)&mark_detected, sizeof(mark_detected));
    state.write((char*)&VIF_ERR, sizeof(VIF_ERR));
}

void CDVD_Drive::load_state(ifstream &state)
{
    state.read((char*)&file_size, sizeof(file_size));
    state.read((char*)&read_bytes_left, sizeof(read_bytes_left));
    state.read((char*)&speed, sizeof(speed));
    state.read((char*)&current_sector, sizeof(current_sector));
    state.read((char*)&sector_pos, sizeof(sector_pos));
    state.read((char*)&sectors_left, sizeof(sectors_left));
    state.read((char*)&block_size, sizeof(block_size));
    state.read((char*)&read_buffer, sizeof(read_buffer));
    state.read((char*)&ISTAT, sizeof(ISTAT));
    state.read((char*)&drive_status, sizeof(drive_status));
    state.read((char*)&is_spinning, sizeof(is_spinning));

    state.read((char*)&active_N_command, sizeof(active_N_command));
    state.read((char*)&N_command, sizeof(N_command));
    state.read((char*)&N_command_params, sizeof(N_command_params));
    state.read((char*)&N_params, sizeof(N_params));
    state.read((char*)&N_status, sizeof(N_status));

    state.read((char*)&S_command, sizeof(S_command));
    state.read((char*)&S_command_params, sizeof(S_command_params));
    state.read((char*)&S_outdata, sizeof(S_outdata));
    state.read((char*)&S_params, sizeof(S_params));
    state.read((char*)&S_out_params, sizeof(S_out_params));
    state.read((char*)&S_status, sizeof(S_status));
    state.read((char*)&rtc, sizeof(rtc));
}

void CDVD_Drive::save_state(ofstream &state)
{
    state.write((char*)&file_size, sizeof(file_size));
    state.write((char*)&read_bytes_left, sizeof(read_bytes_left));
    state.write((char*)&speed, sizeof(speed));
    state.write((char*)&current_sector, sizeof(current_sector));
    state.write((char*)&sector_pos, sizeof(sector_pos));
    state.write((char*)&sectors_left, sizeof(sectors_left));
    state.write((char*)&block_size, sizeof(block_size));
    state.write((char*)&read_buffer, sizeof(read_buffer));
    state.write((char*)&ISTAT, sizeof(ISTAT));
    state.write((char*)&drive_status, sizeof(drive_status));
    state.write((char*)&is_spinning, sizeof(is_spinning));

    state.write((char*)&active_N_command, sizeof(active_N_command));
    state.write((char*)&N_command, sizeof(N_command));
    state.write((char*)&N_command_params, sizeof(N_command_params));
    state.write((char*)&N_params, sizeof(N_params));
    state.write((char*)&N_status, sizeof(N_status));

    state.write((char*)&S_command, sizeof(S_command));
    state.write((char*)&S_command_params, sizeof(S_command_params));
    state.write((char*)&S_outdata, sizeof(S_outdata));
    state.write((char*)&S_params, sizeof(S_params));
    state.write((char*)&S_out_params, sizeof(S_out_params));
    state.write((char*)&S_status, sizeof(S_status));
    state.write((char*)&rtc, sizeof(rtc));
}

void Scheduler::load_state(ifstream &state)
{
    state.read((char*)&ee_cycles, sizeof(ee_cycles));
    state.read((char*)&bus_cycles, sizeof(bus_cycles));
    state.read((char*)&iop_cycles, sizeof(iop_cycles));
    state.read((char*)&run_cycles, sizeof(run_cycles));
    state.read((char*)&closest_event_time, sizeof(closest_event_time));

    int event_size = 0;
    state.read((char*)&event_size, sizeof(event_size));

    for (int i = 0; i < event_size; i++)
    {
        SchedulerEvent event;
        state.read((char*)&event.id, sizeof(event.id));
        state.read((char*)&event.time_to_run, sizeof(event.time_to_run));

        switch (event.id)
        {
            case EVENT_ID::SPU_SAMPLE:
                event.func = &Emulator::gen_sound_sample;
                break;
            case EVENT_ID::EE_IRQ_CHECK:
                event.func = &Emulator::ee_irq_check;
                break;
            case EVENT_ID::CDVD_EVENT:
                event.func = &Emulator::cdvd_event;
                break;
            default:
                Errors::die("Event id %d not recognized!", event.id);
        }

        events.push_back(event);
    }
}

void Scheduler::save_state(ofstream &state)
{
    state.write((char*)&ee_cycles, sizeof(ee_cycles));
    state.write((char*)&bus_cycles, sizeof(bus_cycles));
    state.write((char*)&iop_cycles, sizeof(iop_cycles));
    state.write((char*)&run_cycles, sizeof(run_cycles));
    state.write((char*)&closest_event_time, sizeof(closest_event_time));

    int event_size = events.size();
    state.write((char*)&event_size, sizeof(event_size));

    for (auto it = events.begin(); it != events.end(); it++)
    {
        SchedulerEvent event = *it;
        state.write((char*)&event.id, sizeof(event.id));
        state.write((char*)&event.time_to_run, sizeof(event.time_to_run));
    }
}

void Gamepad::load_state(ifstream &state)
{
    state.read((char*)&command_buffer, sizeof(command_buffer));
    state.read((char*)&rumble_values, sizeof(rumble_values));
    state.read((char*)&mode_lock, sizeof(mode_lock));
    state.read((char*)&command, sizeof(command));
    state.read((char*)&command_length, sizeof(command_length));
    state.read((char*)&data_count, sizeof(data_count));
    state.read((char*)&pad_mode, sizeof(pad_mode));
    state.read((char*)&config_mode, sizeof(config_mode));
}

void Gamepad::save_state(ofstream &state)
{
    state.write((char*)&command_buffer, sizeof(command_buffer));
    state.write((char*)&rumble_values, sizeof(rumble_values));
    state.write((char*)&mode_lock, sizeof(mode_lock));
    state.write((char*)&command, sizeof(command));
    state.write((char*)&command_length, sizeof(command_length));
    state.write((char*)&data_count, sizeof(data_count));
    state.write((char*)&pad_mode, sizeof(pad_mode));
    state.write((char*)&config_mode, sizeof(config_mode));
}

void SPU::load_state(ifstream &state)
{
    state.read((char*)&voices, sizeof(voices));
    state.read((char*)&core_att, sizeof(core_att));
    state.read((char*)&status, sizeof(status));
    state.read((char*)&spdif_irq, sizeof(spdif_irq));
    state.read((char*)&transfer_addr, sizeof(transfer_addr));
    state.read((char*)&current_addr, sizeof(current_addr));
    state.read((char*)&autodma_ctrl, sizeof(autodma_ctrl));
    state.read((char*)&ADMA_left, sizeof(ADMA_left));
    state.read((char*)&input_pos, sizeof(input_pos));
    state.read((char*)&IRQA, sizeof(IRQA));
    state.read((char*)&ENDX, sizeof(ENDX));
    state.read((char*)&key_off, sizeof(key_off));
    state.read((char*)&key_on, sizeof(key_on));
}

void SPU::save_state(ofstream &state)
{
    state.write((char*)&voices, sizeof(voices));
    state.write((char*)&core_att, sizeof(core_att));
    state.write((char*)&status, sizeof(status));
    state.write((char*)&spdif_irq, sizeof(spdif_irq));
    state.write((char*)&transfer_addr, sizeof(transfer_addr));
    state.write((char*)&current_addr, sizeof(current_addr));
    state.write((char*)&autodma_ctrl, sizeof(autodma_ctrl));
    state.write((char*)&ADMA_left, sizeof(ADMA_left));
    state.write((char*)&input_pos, sizeof(input_pos));
    state.write((char*)&IRQA, sizeof(IRQA));
    state.write((char*)&ENDX, sizeof(ENDX));
    state.write((char*)&key_off, sizeof(key_off));
    state.write((char*)&key_on, sizeof(key_on));
}
