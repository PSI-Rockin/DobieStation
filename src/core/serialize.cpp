#include <fstream>
#include <cstring>
#include "emulator.hpp"

#define VER_MAJOR 0
#define VER_MINOR 0
#define VER_REV 2

using namespace std;

bool Emulator::request_load_state(const char *file_name)
{
    ifstream state(file_name, ios::binary);
    if (!state.is_open())
        return false;
    state.close();
    savestate_path = file_name;
    load_requested = true;
    return true;
}

bool Emulator::request_save_state(const char *file_name)
{
    ofstream state(file_name, ios::binary);
    if (!state.is_open())
        return false;
    state.close();
    savestate_path = file_name;
    save_requested = true;
    return true;
}

void Emulator::load_state(const char *file_name)
{
    printf("[Emulator] Loading state...\n");
    ifstream state(file_name, ios::binary);
    if (!state.is_open())
        Errors::die("Failed to load save state");

    //Perform sanity checks
    char dobie_buffer[5];
    state.read(dobie_buffer, sizeof(dobie_buffer));
    if (strncmp(dobie_buffer, "DOBIE", 5))
    {
        state.close();
        Errors::die("Save state invalid");
    }

    uint32_t major, minor, rev;
    state.read((char*)&major, sizeof(major));
    state.read((char*)&minor, sizeof(minor));
    state.read((char*)&rev, sizeof(rev));

    if (major != VER_MAJOR || minor != VER_MINOR || rev != VER_REV)
    {
        state.close();
        Errors::die("Save state doesn't match version");
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

    state.close();
    printf("[Emulator] Success!\n");
    load_requested = false;
}

void Emulator::save_state(const char *file_name)
{
    printf("[Emulator] Saving state...\n");
    ofstream state(file_name, ios::binary);
    if (!state.is_open())
        Errors::die("Failed to save state");

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

    state.close();
    printf("Success!\n");
    save_requested = false;
}

void EmotionEngine::load_state(ifstream &state)
{
    state.read((char*)&gpr, sizeof(gpr));
    state.read((char*)&LO, sizeof(uint64_t));
    state.read((char*)&HI, sizeof(uint64_t));
    state.read((char*)&LO1, sizeof(uint64_t));
    state.read((char*)&HI1, sizeof(uint64_t));
    state.read((char*)&PC, sizeof(uint32_t));
    state.read((char*)&new_PC, sizeof(uint32_t));
    state.read((char*)&SA, sizeof(uint64_t));

    state.read((char*)&increment_PC, sizeof(bool));
    state.read((char*)&branch_on, sizeof(bool));
    state.read((char*)&delay_slot, sizeof(int));

    state.read((char*)&deci2size, sizeof(int));
    state.read((char*)&deci2handlers, sizeof(Deci2Handler) * deci2size);
}

void EmotionEngine::save_state(ofstream &state)
{
    state.write((char*)&gpr, sizeof(gpr));
    state.write((char*)&LO, sizeof(uint64_t));
    state.write((char*)&HI, sizeof(uint64_t));
    state.write((char*)&LO1, sizeof(uint64_t));
    state.write((char*)&HI1, sizeof(uint64_t));
    state.write((char*)&PC, sizeof(uint32_t));
    state.write((char*)&new_PC, sizeof(uint32_t));
    state.write((char*)&SA, sizeof(uint64_t));

    state.write((char*)&increment_PC, sizeof(bool));
    state.write((char*)&branch_on, sizeof(bool));
    state.write((char*)&delay_slot, sizeof(int));

    state.write((char*)&deci2size, sizeof(int));
    state.write((char*)&deci2handlers, sizeof(Deci2Handler) * deci2size);
}

void Cop0::load_state(ifstream &state)
{
    state.read((char*)&gpr, sizeof(uint32_t));
    state.read((char*)&status, sizeof(status));
    state.read((char*)&cause, sizeof(cause));
    state.read((char*)&EPC, sizeof(EPC));
    state.read((char*)&ErrorEPC, sizeof(ErrorEPC));
}

void Cop0::save_state(ofstream &state)
{
    state.write((char*)&gpr, sizeof(uint32_t));
    state.write((char*)&status, sizeof(status));
    state.write((char*)&cause, sizeof(cause));
    state.write((char*)&EPC, sizeof(EPC));
    state.write((char*)&ErrorEPC, sizeof(ErrorEPC));
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

    state.read((char*)&branch_delay, sizeof(branch_delay));
    state.read((char*)&will_branch, sizeof(branch_delay));
    state.read((char*)&inc_PC, sizeof(inc_PC));

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

    state.write((char*)&branch_delay, sizeof(branch_delay));
    state.write((char*)&will_branch, sizeof(branch_delay));
    state.write((char*)&inc_PC, sizeof(inc_PC));

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

    state.read((char*)&ACC.u, sizeof(uint32_t) * 4);
    state.read((char*)&R.u, sizeof(R.u));
    state.read((char*)&I.u, sizeof(I.u));
    state.read((char*)&Q.u, sizeof(Q.u));
    state.read((char*)&P.u, sizeof(P.u));
    state.read((char*)&CMSAR0, sizeof(CMSAR0));

    //Pipelines
    state.read((char*)&new_MAC_flags, sizeof(new_MAC_flags));
    state.read((char*)&MAC_pipeline, sizeof(MAC_pipeline));
    state.read((char*)&Q_Pipeline, sizeof(Q_Pipeline));
    state.read((char*)&new_Q_instance.u, sizeof(new_Q_instance.u));

    //XGKICK
    state.read((char*)&XGKICK_cycles, sizeof(XGKICK_cycles));
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
    state.read((char*)&finish_on, sizeof(finish_on));
    state.read((char*)&delay_slot, sizeof(delay_slot));
}

void VectorUnit::save_state(ofstream &state)
{
    for (int i = 0; i < 32; i++)
        state.write((char*)&gpr[i].u, sizeof(uint32_t) * 4);
    state.write((char*)&int_gpr, sizeof(int_gpr));

    state.write((char*)&ACC.u, sizeof(uint32_t) * 4);
    state.write((char*)&R.u, sizeof(R.u));
    state.write((char*)&I.u, sizeof(I.u));
    state.write((char*)&Q.u, sizeof(Q.u));
    state.write((char*)&P.u, sizeof(P.u));
    state.write((char*)&CMSAR0, sizeof(CMSAR0));

    //Pipelines
    state.write((char*)&new_MAC_flags, sizeof(new_MAC_flags));
    state.write((char*)&MAC_pipeline, sizeof(MAC_pipeline));
    state.write((char*)&Q_Pipeline, sizeof(Q_Pipeline));
    state.write((char*)&new_Q_instance.u, sizeof(new_Q_instance.u));

    //XGKICK
    state.write((char*)&XGKICK_cycles, sizeof(XGKICK_cycles));
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
    state.write((char*)&finish_on, sizeof(finish_on));
    state.write((char*)&delay_slot, sizeof(delay_slot));
}

void INTC::load_state(ifstream &state)
{
    state.read((char*)&INTC_MASK, sizeof(INTC_MASK));
    state.read((char*)&INTC_STAT, sizeof(INTC_STAT));
}

void INTC::save_state(ofstream &state)
{
    state.write((char*)&INTC_MASK, sizeof(INTC_MASK));
    state.write((char*)&INTC_STAT, sizeof(INTC_STAT));
}

void EmotionTiming::load_state(ifstream &state)
{
    state.read((char*)&timers, sizeof(timers));
}

void EmotionTiming::save_state(ofstream &state)
{
    state.write((char*)&timers, sizeof(timers));
}

void IOPTiming::load_state(ifstream &state)
{
    state.read((char*)&timers, sizeof(timers));
}

void IOPTiming::save_state(ofstream &state)
{
    state.write((char*)&timers, sizeof(timers));
}

void DMAC::load_state(ifstream &state)
{
    state.read((char*)&channels, sizeof(channels));

    state.read((char*)&control, sizeof(control));
    state.read((char*)&interrupt_stat, sizeof(interrupt_stat));
    state.read((char*)&PCR, sizeof(PCR));
    state.read((char*)&RBOR, sizeof(RBOR));
    state.read((char*)&RBSR, sizeof(RBSR));
    state.read((char*)&mfifo_empty_triggered, sizeof(mfifo_empty_triggered));
    state.read((char*)&master_disable, sizeof(master_disable));
}

void DMAC::save_state(ofstream &state)
{
    state.write((char*)&channels, sizeof(channels));

    state.write((char*)&control, sizeof(control));
    state.write((char*)&interrupt_stat, sizeof(interrupt_stat));
    state.write((char*)&PCR, sizeof(PCR));
    state.write((char*)&RBOR, sizeof(RBOR));
    state.write((char*)&RBSR, sizeof(RBSR));
    state.write((char*)&mfifo_empty_triggered, sizeof(mfifo_empty_triggered));
    state.write((char*)&master_disable, sizeof(master_disable));
}

void IOP_DMA::load_state(ifstream &state)
{
    state.read((char*)&channels, sizeof(channels));

    state.read((char*)&DPCR, sizeof(DPCR));
    state.read((char*)&DICR, sizeof(DICR));
}

void IOP_DMA::save_state(ofstream &state)
{
    state.write((char*)&channels, sizeof(channels));

    state.write((char*)&DPCR, sizeof(DPCR));
    state.write((char*)&DICR, sizeof(DICR));
}

void GraphicsInterface::load_state(ifstream &state)
{
    state.read((char*)&current_tag, sizeof(current_tag));
    state.read((char*)&active_path, sizeof(active_path));
    state.read((char*)&path_queue, sizeof(path_queue));
    state.read((char*)&path3_vif_masked, sizeof(path3_vif_masked));
}

void GraphicsInterface::save_state(ofstream &state)
{
    state.write((char*)&current_tag, sizeof(current_tag));
    state.write((char*)&active_path, sizeof(active_path));
    state.write((char*)&path_queue, sizeof(path_queue));
    state.write((char*)&path3_vif_masked, sizeof(path3_vif_masked));
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
    state.read((char*)&N_cycles_left, sizeof(N_cycles_left));

    state.read((char*)&S_command, sizeof(S_command));
    state.read((char*)&S_command_params, sizeof(S_command_params));
    state.read((char*)&S_outdata, sizeof(S_outdata));
    state.read((char*)&S_params, sizeof(S_params));
    state.read((char*)&S_out_params, sizeof(S_out_params));
    state.read((char*)&S_status, sizeof(S_status));
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
    state.write((char*)&N_cycles_left, sizeof(N_cycles_left));

    state.write((char*)&S_command, sizeof(S_command));
    state.write((char*)&S_command_params, sizeof(S_command_params));
    state.write((char*)&S_outdata, sizeof(S_outdata));
    state.write((char*)&S_params, sizeof(S_params));
    state.write((char*)&S_out_params, sizeof(S_out_params));
    state.write((char*)&S_status, sizeof(S_status));
}
