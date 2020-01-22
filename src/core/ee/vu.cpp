#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <algorithm>
#include "vu.hpp"
#include "vu_interpreter.hpp"
#include "vu_jit.hpp"
#include "vu_disasm.hpp"

#include "../emulator.hpp"
#include "../errors.hpp"
#include "../gif.hpp"

#define _x(f) f&8
#define _y(f) f&4
#define _z(f) f&2
#define _w(f) f&1

#define _ft_ ((instr >> 16) & 0x1F)
#define _fs_ ((instr >> 11) & 0x1F)
#define _fd_ ((instr >> 6) & 0x1F)

#define _field ((instr >> 21) & 0xF)

#define _it_ (_ft_ & 0xF)
#define _is_ (_fs_ & 0xF)
#define _id_ (_fd_ & 0xF)

#define _fsf_ ((instr >> 21) & 0x3)
#define _ftf_ ((instr >> 23) & 0x3)

#define _bc_ (instr & 0x3)

#define _Imm11_		(int32_t)(instr & 0x400 ? 0xfffffc00 | (instr & 0x3ff) : instr & 0x3ff)
#define _UImm11_	(int32_t)(instr & 0x7ff)

uint32_t VectorUnit::FBRST = 0;

void VuIntBranchPipelineEntry::clear()
{
    write_reg = 0;
    old_value = {0};
    read_and_write = false;
}

void VuIntBranchPipeline::reset()
{
    next.clear();
    flush();
}

VU_I VuIntBranchPipeline::get_branch_condition_reg(uint8_t reg, VU_I current_value, uint8_t vu_id, uint16_t PC)
{
    // first check to see if there's a conflict
    if (reg && reg == pipeline[0].write_reg)
    {
        current_value = pipeline[0].old_value;
        // now we want the OLDEST write in the pipeline:
        if (pipeline[0].read_and_write)
        {
            for (int i = 0; i < length; i++)
            {
                if (reg == pipeline[i].write_reg)
                {
                    current_value = pipeline[i].old_value;
                    printf("[VU%d] Integer branch using register from %d instructions ago (PC = 0x%x), vi%d now 0x%x!\n", vu_id, i+1, PC, reg, current_value.s);
                }

            }
        }
    }
    return current_value;
}

// inform pipeline of a register write
void VuIntBranchPipeline::write_reg(uint8_t reg, VU_I old_value, bool also_read)
{
    next.old_value = old_value;
    next.write_reg = reg;
    next.read_and_write = also_read;
}

void VuIntBranchPipeline::update()
{
    pipeline[0] = next;
    for(int i = 0; i < length - 1; i++)
        pipeline[i+1] = pipeline[i];

    next.clear();
}

// flushes the old stuff out, doesn't affect the current instruction
void VuIntBranchPipeline::flush()
{
    for(auto& p : pipeline)
        p.clear();
}

/**
 * The VU max and min instructions support denormals.
 * Because of this, we must compare registers as signed integers instead of floats.
 */
int32_t vu_max(int32_t a, int32_t b)
{
    if (a < 0 && b < 0)
        return std::min(a, b);
    return std::max(a, b);
}

int32_t vu_min(int32_t a, int32_t b)
{
    if (a < 0 && b < 0)
        return std::max(a, b);
    return std::min(a, b);
}

void DecodedRegs::reset()
{
    vf_read0[0] = 0; vf_read0[1] = 0;
    vf_read0_field[0] = 0; vf_read0_field[1] = 0;

    vf_read1[0] = 0; vf_read1[1] = 0;
    vf_read1_field[0] = 0; vf_read1_field[1] = 0;

    vf_write[0] = 0; vf_write[1] = 0;
    vf_write_field[0] = 0; vf_write_field[1] = 0;

    vi_read0 = 0; vi_read1 = 0;
    vi_write = 0;
    vi_write_from_load = 0;
}

VectorUnit::VectorUnit(int id, Emulator* e, INTC* intc, EmotionEngine* cpu, VectorUnit* other_vu) :
    id(id), e(e), intc(intc), eecpu(cpu), gif(nullptr), other_vu(other_vu)
{
    gpr[0].f[0] = 0.0;
    gpr[0].f[1] = 0.0;
    gpr[0].f[2] = 0.0;
    gpr[0].f[3] = 1.0;
    int_gpr[0].u = 0;

    if (id == 0)
        mem_mask = 0xFFF;
    else
        mem_mask = 0x3FFF;

    VIF_TOP = nullptr;
    VIF_ITOP = nullptr;

    MAC_flags = &MAC_pipeline[3];
    CLIP_flags = &CLIP_pipeline[3];
}

void VectorUnit::reset()
{
    status = 0;
    status_pipe = 0;
    clip_flags = 0;
    PC = 0;
    //cycle_count = 1; //Set to 1 to prevent spurious events from occurring during execution
    eecpu->set_cop2_last_cycle(eecpu->get_cycle_count());
    cycle_count = eecpu->get_cop2_last_cycle() >> 1;
    run_event = 0;
    running = false;
    tbit_stop = false;
    vumem_is_dirty = true; //assume we don't know the contents on reset
    finish_on = false;
    branch_on = false;
    second_branch_pending = false;
    secondbranch_PC = 0;
    branch_delay_slot = 0;
    ebit_delay_slot = 0;
    transferring_GIF = false;
    XGKICK_stall = false;
    new_MAC_flags = 0;
    new_Q_instance.u = 0;
    finish_DIV_event = 0;
    pipeline_state[0] = 0;
    pipeline_state[1] = 0;
    XGKICK_cycles = 0;

    for(int i = 0; i < 4; i++) {
        MAC_pipeline[i] = 0;
        CLIP_pipeline[i] = 0;
        ILW_pipeline[i] = 0;
    }

    flush_pipes();
    int_branch_pipeline.reset();

    int_backup_id = 0;
    int_backup_reg = 0;
    int_branch_delay = 0;

    for (int i = 1; i < 32; i++)
    {
        gpr[i].f[0] = 0.0;
        gpr[i].f[1] = 0.0;
        gpr[i].f[2] = 0.0;
        gpr[i].f[3] = 0.0;        
    }

    for (int i = 0; i < 16; i++)
    {
        int_gpr[i].u = 0;
    }
}

void VectorUnit::clear_interlock()
{
    e->clear_cop2_interlock();
}

bool VectorUnit::check_interlock()
{
    return e->interlock_cop2_check(false);
}

bool VectorUnit::is_interlocked()
{
    return e->check_cop2_interlock();
}
void VectorUnit::set_TOP_regs(uint16_t *TOP, uint16_t *ITOP)
{
    VIF_TOP = TOP;
    VIF_ITOP = ITOP;
}

void VectorUnit::set_GIF(GraphicsInterface *gif)
{
    this->gif = gif;
}

void VectorUnit::cop2_updatepipes(int cycles)
{
    //TODO: Affect EE cycles when it's a 1 cycle stall caused by QMTC2, LQC2, CTC2
    if (cycles > 0)
    {
        for (int i = 0; i < cycles; i++)
        {
            //This could run for a long time if many cycles have passed, there's no reason
            if (i > 4 && DIV_event_started == false && EFU_event_started == false)
            {
                cycle_count += cycles - i;
                flush_pipes();
                break;
            }
            cycle_count++;

            if (i >= 1)
            {
                decoder.reset();
            }
            update_mac_pipeline();
            update_DIV_EFU_pipes();
            int_branch_pipeline.update();
        }
    }
}

void VectorUnit::handle_cop2_stalls()
{
    check_for_COP2_FMAC_stall();
}

//Propogate all pipeline updates instantly
void VectorUnit::flush_pipes()
{
    for (int i = 0; i < 4; i++)
        update_mac_pipeline();

    int_branch_pipeline.flush();
    finish_DIV_event = cycle_count;
    finish_EFU_event = cycle_count;
    DIV_event_started = false;
    EFU_event_started = false;
    Q.u = new_Q_instance.u;
    P.u = new_P_instance.u;
}

void VectorUnit::run(int cycles)
{
    int cycles_to_run;
    uint32_t cycles_this_op = 0;

    if (running && !id)
    {
        cycles_to_run = (eecpu->get_cycle_count() - eecpu->get_cop2_last_cycle());
        if (cycles_to_run > 0)
            eecpu->set_cop2_last_cycle(eecpu->get_cop2_last_cycle() + cycles_to_run);
        else
            return;

        clear_interlock();
    }
    else
    {
        cycles_to_run = cycles;
    }

    while (running && cycles_to_run > 0)
    {
        cycles_this_op = cycle_count;
        cycle_count++;
        update_mac_pipeline();
        update_DIV_EFU_pipes();
        int_branch_pipeline.update();

        if (XGKICK_stall)
            break;

        uint32_t upper_instr = *(uint32_t*)&instr_mem.m[PC + 4];
        uint32_t lower_instr = *(uint32_t*)&instr_mem.m[PC];
        //printf("[$%08X] $%08X:$%08X\n", PC, upper_instr, lower_instr);
        VU_Interpreter::interpret(*this, upper_instr, lower_instr);

        PC += 8;

        if (branch_on)
        {
            if (!branch_delay_slot)
            {
                PC = new_PC;
                if (second_branch_pending)
                {
                    new_PC = secondbranch_PC;
                    second_branch_pending = false;
                }
                else
                    branch_on = false;
            }
            else
                branch_delay_slot--;
        }
        if (finish_on)
        {
            if (!ebit_delay_slot)
            {
                //printf("[VU%d] Ended execution at PC %x!\n", id, PC);
                running = false;
                finish_on = false;
                flush_pipes();
                cycle_count = eecpu->get_cop2_last_cycle() >> 1;
            }
            else
                ebit_delay_slot--;
        }
        if (upper_instr & (1 << 27))
        {
            if (read_fbrst() & (1 << (3 + (get_id() * 8))))
            {
                if (!get_id())
                {
                    intc->assert_IRQ((int)Interrupt::VU0);
                }
                else
                {
                    intc->assert_IRQ((int)Interrupt::VU1);
                }
                tbit_stop = true;
                running = false;
                finish_on = false;
                flush_pipes();
                //Errors::die("VU%d Using T-Bit\n", get_id());
            }
        }

        cycles_this_op = cycle_count - cycles_this_op;
        XGKICK_cycles += cycles_this_op;
        cycles_to_run-= cycles_this_op;

        if (get_id() == 0)
        {
            if (is_interlocked())
            {
                //Errors::die("VU%d Using M-Bit\n", vu.get_id());
                //Break out from the VU0 loop to give COP2 time to catch the interlock
                if(check_interlock())
                    break;
            }
        }
        else if (transferring_GIF)
        {
            while (XGKICK_cycles >= 2)
            {
                if (gif->path_active(1, true))
                {
                    handle_XGKICK();
                    XGKICK_cycles -= 2;
                }
                else
                {
                    XGKICK_cycles = 0;
                    break;
                }
            }
        }
    }

    if (cycles_to_run < 0 && !id)
    {
        eecpu->set_cop2_last_cycle(eecpu->get_cop2_last_cycle() + std::abs((int)cycles_to_run));
    }

    if (transferring_GIF)
    {
        if (gif->path_active(1, true))
        {
            while (cycles_to_run >= 2 && transferring_GIF)
            {
                cycles_to_run -= 2;
                handle_XGKICK();
            }
        }
    }
}

void VectorUnit::correct_jit_pipeline(int cycles)
{
    uint64_t stall_pipe[4];
    int q_pipe_delay = (pipeline_state[1] >> 23) & 0xF;
    int p_pipe_delay = (pipeline_state[1] >> 27) & 0x3F;

    bool branch_delay_slot = (pipeline_state[1] >> 55) & 0x1;
    bool ebit_delay_slot = (pipeline_state[1] >> 56) & 0x1;

    stall_pipe[0] = pipeline_state[0] & 0x7FFFFF;
    stall_pipe[1] = (pipeline_state[0] >> 23) & 0x7FFFFF;
    stall_pipe[2] = (pipeline_state[0] >> 46) & 0x7FFFFF;
    stall_pipe[3] = pipeline_state[1] & 0x7FFFFF;

    decoder.vf_write[0] = (pipeline_state[1] >> 33) & 0x1F;
    decoder.vf_write[1] = (pipeline_state[1] >> 38) & 0x1F;
    decoder.vf_write_field[0] = (pipeline_state[1] >> 43) & 0xF;
    decoder.vf_write_field[1] = (pipeline_state[1] >> 47) & 0xF;
    decoder.vi_write_from_load = (pipeline_state[1] >> 51) & 0xF;

    for (int i = 0; i < cycles; i++)
    {
        update_mac_pipeline();
        decoder.reset();

        if (q_pipe_delay > 0)
        {
            if (--q_pipe_delay == 0)
            {
                Q.u = new_Q_instance.u;
            }
        }
        if (p_pipe_delay > 0)
        {
            if (--p_pipe_delay == 0)
            {
                P.u = new_P_instance.u;
            }
        }

        stall_pipe[0] = stall_pipe[1];
        stall_pipe[1] = stall_pipe[2];
        stall_pipe[3] = stall_pipe[3];
        stall_pipe[3] = 0;
    }

    pipeline_state[0] = stall_pipe[0] & 0x7FFFFF;
    pipeline_state[0] |= (stall_pipe[1] & 0x7FFFFF) << 23;
    pipeline_state[0] |= (stall_pipe[2] & 0x7FFFFF) << 46;
    pipeline_state[1] = stall_pipe[3] & 0x7FFFFF;

    pipeline_state[1] |= (q_pipe_delay & 0xF) << 23;
    pipeline_state[1] |= (uint64_t)(p_pipe_delay & 0x3F) << 27;

    pipeline_state[1] |= (uint64_t)(decoder.vf_write[0] & 0x1F) << 33UL;
    pipeline_state[1] |= (uint64_t)(decoder.vf_write[1] & 0x1F) << 38UL;
    pipeline_state[1] |= (uint64_t)(decoder.vf_write_field[0] & 0xF) << 43UL;
    pipeline_state[1] |= (uint64_t)(decoder.vf_write_field[1] & 0xF) << 47UL;
    pipeline_state[1] |= (uint64_t)(decoder.vi_write_from_load & 0xF) << 51UL;
    pipeline_state[1] |= (uint64_t)(branch_delay_slot & 0x1) << 55UL;
    pipeline_state[1] |= (uint64_t)(ebit_delay_slot & 0x1) << 56UL;
}

void VectorUnit::run_jit(int cycles)
{
    int stalled_cycles = 0;
    if (cycles > 0)
    {
        cycle_count += cycles;
        if ((!running || XGKICK_stall) && transferring_GIF)
        {
            gif->request_PATH(1, true);
            while (cycles >= 2 && gif->path_active(1, true))
            {
                if (XGKICK_stall)
                    stalled_cycles+=2;
                cycles -= 2; //BUS Speed
                handle_XGKICK();
            }
        }
        //Get the pipeline in the right state after an XGKick stall
        if (stalled_cycles > 0)
        {
            correct_jit_pipeline(stalled_cycles);
            run_event += stalled_cycles;
        }

        while (running && !XGKICK_stall && run_event < cycle_count)
        {
            run_event += VU_JIT::run(this);
        }
    }
}

void VectorUnit::handle_XGKICK()
{
    uint128_t quad = read_mem<uint128_t>(GIF_addr);
    GIF_addr += 16;
    if (gif->send_PATH1(quad))
    {
        //printf("[VU1] XGKICK transfer ended!\n");
        if (XGKICK_stall)
        {
            //printf("[VU1] Activating stalled XGKICK transfer\n");
            XGKICK_stall = false;
            GIF_addr = stalled_GIF_addr;
            run_event = cycle_count;
        }
        else
        {
            gif->deactivate_PATH(1);
            transferring_GIF = false;
        }
    }
}

//VU0 can access VU1 registers through the addresses (anded with 0x7FFF) 0x4000-0x4400
uint32_t VectorUnit::read_reg(uint32_t addr)
{
    addr &= 0x3FF;
    if (addr < 0x0200)
        return get_gpr_u(addr / 0x10, (addr & 0xC) / 4);
    else if (addr < 0x0300)
    {
        if ((addr & 0xF) < 4)
            return get_int((addr - 0x0200) / 0x10);
        else
            return 0;
    }
    else
    {
        switch (addr)
        {
            case 0x300:
                return status;
            case 0x310:
                return *MAC_flags;
            case 0x320:
                return *CLIP_flags;
            case 0x340:
                return R.u;
            case 0x350:
                return I.u;
            case 0x360:
                return Q.u;
            case 0x370:
                return P.u;
            case 0x3A0:
                return PC;
            default:
                return 0;
        }
    }
}

void VectorUnit::write_reg(uint32_t addr, uint32_t data)
{
    addr &= 0x3FF;
    if (addr < 0x0200)
        set_gpr_u(addr / 0x10, (addr & 0xC) / 4, data);
    else if (addr < 0x0300)
    {
        if ((addr & 0xF) < 4)
            set_int((addr - 0x0200) / 0x10, data);
    }
    else
        printf("[VU0] Unrecognized write to VU1 register $%04X: $%08X\n", addr, data);
}

/**
 * The FMAC pipeline has four stages, from the perspective of emulation.
 * When an instruction reads a register that is being written to, an FMAC stall occurs.
 * If a read occurs 1 cycle after a write, the stall lasts for 3 cycles.
 * Stalls do not occur on the same doubleword.
 */
void VectorUnit::check_for_FMAC_stall()
{
    for (int i = 0; i < 3; i++)
    {
        uint8_t write0 = (MAC_pipeline[i] >> 16) & 0xFF;
        uint8_t write1 = (MAC_pipeline[i] >> 24) & 0xFF;

        bool stall_found = false;

        if (write0 != 0 || write1 != 0)
        {
            uint8_t write0_field = (MAC_pipeline[i] >> 32) & 0xF;
            uint8_t write1_field = (MAC_pipeline[i] >> 36) & 0xF;

            for (int j = 0; j < 2; j++)
            {
                if (decoder.vf_read0[j])
                {
                    if (decoder.vf_read0[j] == write0)
                    {
                        if (decoder.vf_read0_field[j] & write0_field)
                        {
                            stall_found = true;
                            break;
                        }
                    }
                    else if (decoder.vf_read0[j] == write1)
                    {
                        if (decoder.vf_read0_field[j] & write1_field)
                        {
                            stall_found = true;
                            break;
                        }
                    }
                }
                if (decoder.vf_read1[j])
                {
                    if (decoder.vf_read1[j] == write0)
                    {
                        if (decoder.vf_read1_field[j] & write0_field)
                        {
                            stall_found = true;
                            break;
                        }
                    }
                    else if (decoder.vf_read1[j] == write1)
                    {
                        if (decoder.vf_read1_field[j] & write1_field)
                        {
                            stall_found = true;
                            break;
                        }
                    }
                }
            }
        }

        if (ILW_pipeline[i])
        {
            if (ILW_pipeline[i] == decoder.vi_read0 || ILW_pipeline[i] == decoder.vi_read1)
            {
                //printf("[VU%d] Load hazard stall on vi%d at PC = 0x%x\n", id, ILW_pipeline[i], PC);
                stall_found = true;
            }
        }


        if (stall_found)
        {
            //printf("FMAC stall at $%08X for %d cycles!\n", PC, 3 - i);
            int delay = 3 - i;
            for (int j = 0; j < delay; j++)
            {
                update_mac_pipeline();
                int_branch_pipeline.flush();
            }

            cycle_count += delay;
            update_DIV_EFU_pipes();
            break;
        }
    }
}

void VectorUnit::check_for_COP2_FMAC_stall()
{
    for (int i = 0; i < 3; i++)
    {
        uint8_t write0 = (MAC_pipeline[i] >> 16) & 0xFF;
        uint8_t write1 = (MAC_pipeline[i] >> 24) & 0xFF;

        bool stall_found = false;

        if (write0 != 0 || write1 != 0)
        {
            for (int j = 0; j < 2; j++)
            {
                if (decoder.vf_read0[j])
                {
                    if (decoder.vf_read0[j] == write0)
                    {
                        stall_found = true;
                        break;
                    }
                    else if (decoder.vf_read0[j] == write1)
                    {
                        stall_found = true;
                        break;
                    }
                }
                if (decoder.vf_read1[j])
                {
                    if (decoder.vf_read1[j] == write0)
                    {
                        stall_found = true;
                        break;
                    }
                    else if (decoder.vf_read1[j] == write1)
                    {
                        stall_found = true;
                        break;
                    }
                }
            }
        }

        if (stall_found)
        {
            //printf("COP2 FMAC stall at $%08X for %d cycles!\n", PC, 3 - i);
            int delay = 3 - i;
            for (int j = 0; j < delay; j++)
            {
                update_mac_pipeline();
                int_branch_pipeline.flush();
            }
            //TODO: Affect EE cycles also
            cycle_count += delay;
            update_DIV_EFU_pipes();
            break;
        }
    }
}

void VectorUnit::backup_vf(bool newvf, int index)
{
    if (newvf)
        backup_newgpr = gpr[index];
    else
        backup_oldgpr = gpr[index];
}

void VectorUnit::restore_vf(bool newvf, int index)
{
    if (newvf)
        gpr[index] = backup_newgpr;
    else
        gpr[index] = backup_oldgpr;
}

uint32_t VectorUnit::read_fbrst()
{
    return FBRST;
}

uint32_t VectorUnit::read_CMSAR0()
{
    return CMSAR0;
}

void VectorUnit::disasm_micromem()
{
    //If the memory hasn't changed since the last CRC/Disasm, don't bother checking it
    if (!is_dirty())
        return;
    //Check for branch targets and also see if the microprogram is the same as the one previously disassembled
    uint32_t crc = crc_microprogram();

    //Set the current program crc to the VU JIT
    if (get_id() == 1)
    {
        VU_JIT::set_current_program(crc);
    }

    clear_dirty();

    if (seen_microprogram_crcs.find(crc) != seen_microprogram_crcs.end())
        return;

    seen_microprogram_crcs.insert(crc);

    int size = (get_id()) ? 0x4000 : 0x1000;
    bool is_branch_target[0x4000 / 8];
    memset(is_branch_target, 0, size / 8);
    for (int i = 0; i < size; i += 8)
    {
        uint32_t lower = read_instr<uint32_t>(i);

        //If the lower instruction is a branch, set branch target to true for the location it points to
        if (VU_Disasm::is_branch(lower))
        {
            int32_t imm = lower & 0x7FF;
            imm = ((int16_t)(imm << 5)) >> 5;
            imm *= 8;

            uint32_t addr = i + imm + 8;
            if (addr < size)
                is_branch_target[addr / 8] = true;
        }
    }


    using namespace std;

    ofstream file;
    string name = "microvu" + to_string(get_id()) + "_" + to_string(crc) + ".txt";

    file.open(name);

    if (!file.is_open())
    {
        Errors::die("Failed to open\n");
    }

    for (int i = 0; i < size; i += 8)
    {
        if (is_branch_target[i / 8])
        {
            file << endl;
            file << "Branch target $" << setfill('0') << setw(4) << right << hex << i << ":";
            file << endl;
        }
        //PC
        file << "[$" << setfill('0') << setw(4) << right << hex << i << "] ";

        //Raw instructions
        uint32_t lower = read_instr<uint32_t>(i);
        uint32_t upper = read_instr<uint32_t>(i + 4);

        file << setw(8) << hex << upper << ":" << setw(8) << lower << " ";
        file << setfill(' ') << setw(30) << left << VU_Disasm::upper(i, upper);

        if (upper & (1 << 31))
            file << VU_Disasm::loi(lower);
        else
            file << VU_Disasm::lower(i, lower);
        file << endl;
    }
    file.close();
}

#define POLY 0x82f63b78

uint32_t VectorUnit::crc_microprogram()
{
    uint32_t crc = 0;
    int len = (get_id()) ? 0x4000 : 0x1000;
    int addr = 0x0000;

    crc = ~crc;
    while (len--) {
        crc ^= read_instr<uint8_t>(addr++);
        for (int k = 0; k < 8; k++)
            crc = crc & 1 ? (crc >> 1) ^ POLY : crc >> 1;
    }
    return ~crc;
}

void VectorUnit::start_program(uint32_t addr)
{
    uint32_t new_addr = addr & mem_mask;
    //printf("[VU%d] CallMS Starting execution at $%08X! Cur PC %x\n", get_id(), new_addr, PC);

    //Enable this if disabling micromem disasm
    if (get_id() == 1 && is_dirty())
    {
        VU_JIT::set_current_program(crc_microprogram());
        clear_dirty();
    }
    
    if (running == false)
    {
        running = true;
        tbit_stop = false;
        PC = new_addr;
        //printf("[VU%d] Starting execution at PC %x!\n", id, PC);
        //Try to keep VU0 in sync with the EE
        //TODO: Account for VIF0 MSCAL timing
        if (!id)
        {
            eecpu->set_cop2_last_cycle(eecpu->get_cycle_count());
        }
        cycle_count = eecpu->get_cycle_count();
        flush_pipes();
    }
    //disasm_micromem();
    run_event = cycle_count;
}

void VectorUnit::end_execution()
{
    ebit_delay_slot = 1;
    finish_on = true;
}

void VectorUnit::stop()
{
    running = false;
    flush_pipes();
}

void VectorUnit::stop_by_tbit()
{
    tbit_stop = true;
    running = false;
    flush_pipes();

    if (!get_id())
    {
        intc->assert_IRQ((int)Interrupt::VU0);
    }
    else
    {
        intc->assert_IRQ((int)Interrupt::VU1);
    }
}

float VectorUnit::update_mac_flags(float value, int index)
{
    uint32_t value_u = *(uint32_t*)&value;
    int flag_id = 3 - index;

    //Sign flag
    if (value_u & 0x80000000)
        new_MAC_flags |= 0x10 << flag_id;
    else
        new_MAC_flags &= ~(0x10 << flag_id);

    //Zero flag, clear under/overflow
    if ((value_u & 0x7FFFFFFF) == 0)
    {
        new_MAC_flags |= 1 << flag_id;
        new_MAC_flags &= ~(0x1100 << flag_id);
        return value;
    }

    switch ((value_u >> 23) & 0xFF)
    {
        //Underflow, set zero
        case 0:
            new_MAC_flags |= 0x101 << flag_id;
            new_MAC_flags &= ~(0x1000 << flag_id);
            value_u = value_u & 0x80000000;
            break;
        //Overflow
        case 255:
            new_MAC_flags |= 0x1000 << flag_id;
            new_MAC_flags &= ~(0x101 << flag_id);
            value_u = (value_u & 0x80000000) | 0x7F7FFFFF;
            break;
        //Clear all but sign
        default:
            new_MAC_flags &= ~(0x1101 << flag_id);
            break;
    }

    return *(float*)&value_u;
}

void VectorUnit::clear_mac_flags(int index)
{
    new_MAC_flags &= ~(0x1111 << (3 - index));
}

void VectorUnit::update_status()
{
    status &= ~0x3F;

    status |= (*MAC_flags & 0x000F) ? 1 : 0;
    status |= (*MAC_flags & 0x00F0) ? 2 : 0;
    status |= (*MAC_flags & 0x0F00) ? 4 : 0;
    status |= (*MAC_flags & 0xF000) ? 8 : 0;

    status |= (status & 0x3F) << 6;
}

void VectorUnit::update_mac_pipeline()
{
    bool updatestatus = false;

    //Pipeline has updated, so the status needs to update
    if ((MAC_pipeline[3] & 0xFFFF) != (MAC_pipeline[2] & 0xFFFF))
        updatestatus = true;

    MAC_pipeline[3] = MAC_pipeline[2];
    MAC_pipeline[2] = MAC_pipeline[1];
    MAC_pipeline[1] = MAC_pipeline[0];
    MAC_pipeline[0] = new_MAC_flags;

    MAC_pipeline[0] |= ((uint32_t)decoder.vf_write[0] << 16) | ((uint32_t)decoder.vf_write[1] << 24);
    MAC_pipeline[0] |= ((uint64_t)decoder.vf_write_field[0] << 32UL) | ((uint64_t)decoder.vf_write_field[1] << 36UL);

    CLIP_pipeline[3] = CLIP_pipeline[2];
    CLIP_pipeline[2] = CLIP_pipeline[1];
    CLIP_pipeline[1] = CLIP_pipeline[0];
    CLIP_pipeline[0] = clip_flags;

    ILW_pipeline[3] = ILW_pipeline[2];
    ILW_pipeline[2] = ILW_pipeline[1];
    ILW_pipeline[1] = ILW_pipeline[0];
    ILW_pipeline[0] = decoder.vi_write_from_load;

    if(updatestatus)
        update_status();

    if (status_pipe > 0)
    {
        status_pipe--;
        if (!status_pipe)
            status = (status & 0x3F) | (status_value & 0xFFF);
    }
}

void VectorUnit::update_DIV_EFU_pipes()
{
    if (DIV_event_started)
    {
        if (cycle_count >= finish_DIV_event)
        {
            DIV_event_started = false;
            Q.u = new_Q_instance.u;
        }
    }

    if (EFU_event_started)
    {
        if (cycle_count >= finish_EFU_event)
        {
            EFU_event_started = false;
            P.u = new_P_instance.u;
        }
    }
}

void VectorUnit::start_DIV_unit(int latency)
{
    finish_DIV_event = cycle_count + latency;
    DIV_event_started = true;
}

void VectorUnit::start_EFU_unit(int latency)
{
    finish_EFU_event = cycle_count + latency;
    EFU_event_started = true;
}

void VectorUnit::write_int(uint8_t reg, uint8_t read0, uint8_t read1)
{
    if(reg)
    {
        int_branch_pipeline.write_reg(reg, int_gpr[reg], (reg == read0) || (reg == read1));
    }
}

VU_I VectorUnit::read_int_for_branch_condition(uint8_t reg)
{
    VU_I value = int_gpr[reg];
    if(reg)
    {
        value = int_branch_pipeline.get_branch_condition_reg(reg, value, id, PC);
    }
    return value;
}

float VectorUnit::convert(uint32_t value)
{
    switch(value & 0x7f800000)
    {
        case 0x0:
            value &= 0x80000000;
            return *(float*)&value;
        case 0x7f800000:
        {
            uint32_t result = (value & 0x80000000)|0x7f7fffff;
            return *(float*)&result;
        }
    }
    return *(float*)&value;
}

void VectorUnit::print_vectors(uint8_t a, uint8_t b)
{
    printf("A: ");
    for (int i = 0; i < 4; i++)
        printf("%f ", gpr[a].f[i]);
    printf("\nB: ");
    for (int i = 0; i < 4; i++)
        printf("%f ", gpr[b].f[i]);
    printf("\n");
}


/**
 * Code taken from PCSX2 and adapted to DobieStation
 * https://github.com/PCSX2/pcsx2/blob/1292cd505efe7c68ab87880b4fd6809a96da703c/pcsx2/VUops.cpp#L1795
 */
void VectorUnit::advance_r()
{
    int x = (R.u >> 4) & 1;
    int y = (R.u >> 22) & 1;
    R.u <<= 1;
    R.u ^= x ^ y;
    R.u = (R.u & 0x7FFFFF) | 0x3F800000;
}

uint32_t VectorUnit::cfc(int index)
{
    if (index < 16)
        return int_gpr[index].u;
    switch (index)
    {
        case 16:
            return status;
        case 17:
            return *MAC_flags & 0xFFFF;
        case 18:
            return *CLIP_flags;
        case 20:
            return R.u & 0x7FFFFF;
        case 21:
            return I.u;
        case 22:
            return Q.u;
        case 26:
            return (PC / 8);
        case 27:
            return CMSAR0;
        case 28:
            return FBRST;
        default:
            printf("[COP2] Unrecognized cfc2 from reg %d\n", index);
    }
    return 0;
}

void VectorUnit::ctc(int index, uint32_t value)
{
    if (index < 16)
    {
        printf("[COP2] Set vi%d to $%04X\n", index, value);
        set_int(index, value);
        return;
    }
    switch (index)
    {
        case 16:
            fsset(value);
            break;
        case 18:
            clip_flags = value;
            break;
        case 20:
            R.u = value;
            break;
        case 21:
            I.u = value;
            printf("[VU] I = %f\n", I.f);
            break;
        case 22:
            set_Q(value);
            printf("[VU] Q = %f\n", Q.f);
            break;
        case 27:
            CMSAR0 = (uint16_t)value;
            break;
        case 28:
            if (value & 0x2 && id == 0)
                reset();
            FBRST = value & ~0x303;
            break;
        default:
            printf("[COP2] Unrecognized ctc2 of $%08X to reg %d\n", value, index);
    }
}

void VectorUnit::branch(bool condition, int16_t imm, bool link, uint8_t linkreg)
{
    int_branch_pipeline.flush();
    if (condition)
    {
        if (branch_on)
        {
            second_branch_pending = true;
            secondbranch_PC = ((int16_t)PC + imm + 8) & 0x3fff;
            if(link)
                set_int(linkreg, (new_PC + 8) / 8);
        }
        else
        {
            branch_on = true;
            branch_delay_slot = 1;
            new_PC = ((int16_t)PC + imm + 8) & 0x3fff;
            if (link)
                set_int(linkreg, (PC + 16) / 8);
        }
    }
}

void VectorUnit::jp(uint16_t addr, bool link, uint8_t linkreg)
{
    int_branch_pipeline.flush();
    if (branch_on)
    {
        second_branch_pending = true;
        secondbranch_PC = addr & 0x3FFF;
        if (link)
            set_int(linkreg, (new_PC + 8) / 8);
    }
    else
    {
        new_PC = addr & 0x3FFF;
        branch_on = true;
        branch_delay_slot = 1;
        if (link)
            set_int(linkreg, (PC + 16) / 8);
    }
}

#define printf(fmt, ...)(0)

void VectorUnit::abs(uint32_t instr)
{
    printf("[VU] ABS: ");
    for (int i = 0; i < 4; i++)
    {
        if (_field & (1 << (3 - i)))
        {
            float result = fabs(convert(gpr[_fs_].u[i]));
            set_gpr_f(_ft_, i, result);
            printf("(%d)%f ", i, gpr[_ft_].f[i]);
        }
    }
    printf("\n");
}

void VectorUnit::add(uint32_t instr)
{
    printf("[VU] ADD: ");
    for (int i = 0; i < 4; i++)
    {
        if (_field & (1 << (3 - i)))
        {
            float result = convert(gpr[_fs_].u[i]) + convert(gpr[_ft_].u[i]);
            set_gpr_f(_fd_, i, update_mac_flags(result, i));
            printf("(%d)%f ", i, gpr[_fd_].f[i]);
        }
        else
            clear_mac_flags(i);
    }
    printf("\n");
}

void VectorUnit::adda(uint32_t instr)
{
    printf("[VU] ADDA: ");
    for (int i = 0; i < 4; i++)
    {
        if (_field & (1 << (3 - i)))
        {
            ACC.f[i] = convert(gpr[_fs_].u[i]) + convert(gpr[_ft_].u[i]);
            ACC.f[i] = update_mac_flags(ACC.f[i], i);
            printf("(%d)%f ", i, ACC.f[i]);
        }
        else
            clear_mac_flags(i);
    }
    printf("\n");
}

void VectorUnit::addabc(uint32_t instr)
{
    printf("[VU] ADDAbc: ");
    float op = convert(gpr[_ft_].u[_bc_]);
    for (int i = 0; i < 4; i++)
    {
        if (_field & (1 << (3 - i)))
        {
            ACC.f[i] = convert(gpr[_fs_].u[i]) + op;
            ACC.f[i] = update_mac_flags(ACC.f[i], i);
            printf("(%d)%f", i, ACC.f[i]);
        }
        else
            clear_mac_flags(i);
    }
    printf("\n");
}

void VectorUnit::addai(uint32_t instr)
{
    printf("[VU] ADDAi: ");
    float op = convert(I.u);
    for (int i = 0; i < 4; i++)
    {
        if (_field & (1 << (3 - i)))
        {
            float temp = convert(gpr[_fs_].u[i]) + op;
            ACC.f[i] = update_mac_flags(temp, i);            
            printf("(%d)%f ", i, ACC.f[i]);
        }
        else
            clear_mac_flags(i);
    }
    printf("\n");
}

void VectorUnit::addaq(uint32_t instr)
{
    printf("[VU] ADDAq: ");
    float op = convert(Q.u);
    for (int i = 0; i < 4; i++)
    {
        if (_field & (1 << (3 - i)))
        {
            float temp = convert(gpr[_fs_].u[i]) + op;
            ACC.f[i] = update_mac_flags(temp, i);
            printf("(%d)%f ", i, ACC.f[i]);
        }
        else
            clear_mac_flags(i);
    }
    printf("\n");
}

void VectorUnit::addbc(uint32_t instr)
{
    printf("[VU] ADDbc: ");
    float op = convert(gpr[_ft_].u[_bc_]);
    for (int i = 0; i < 4; i++)
    {
        if (_field & (1 << (3 - i)))
        {
            float temp = convert(gpr[_fs_].u[i]) + op;
            set_gpr_f(_fd_, i, update_mac_flags(temp, i));
            printf("(%d)%f ", i, gpr[_fd_].f[i]);
        }
        else
            clear_mac_flags(i);
    }
    printf("\n");
}

void VectorUnit::addi(uint32_t instr)
{
    printf("[VU] ADDi: ");
    float op = convert(I.u);
    for (int i = 0; i < 4; i++)
    {
        if (_field & (1 << (3 - i)))
        {
            float temp = convert(gpr[_fs_].u[i]) + op;
            set_gpr_f(_fd_, i, update_mac_flags(temp, i));
            printf("(%d)%f ", i, gpr[_fd_].f[i]);
        }
        else
            clear_mac_flags(i);
    }
    printf("\n");
}

void VectorUnit::addq(uint32_t instr)
{
    printf("[VU] ADDq: ");
    float op = convert(Q.u);
    for (int i = 0; i < 4; i++)
    {
        if (_field & (1 << (3 - i)))
        {
            float temp = convert(gpr[_fs_].u[i]) + op;
            set_gpr_f(_fd_, i, update_mac_flags(temp, i));
            printf("(%d)%f ", i, gpr[_fd_].f[i]);
        }
        else
            clear_mac_flags(i);
    }
    printf("\n");
}

void VectorUnit::b(uint32_t instr)
{
    int16_t imm = instr & 0x7FF;
    imm = ((int16_t)(imm << 5)) >> 5;
    imm *= 8;
    printf("[VU] B $%x (Imm $%x)\n", get_PC() + 16 + imm, imm);
    branch(true, imm, false);
}

void VectorUnit::bal(uint32_t instr)
{
    int16_t imm = instr & 0x7FF;
    imm = ((int16_t)(imm << 5)) >> 5;
    imm *= 8;

    uint8_t link_reg = (instr >> 16) & 0x1F;
    printf("[VU] BAL $%x (Imm $%x)\n", get_PC() + 16 + imm, imm);
    branch(true, imm, true, link_reg);
}

void VectorUnit::clip(uint32_t instr)
{
    printf("[VU] CLIP $%08X (%d, %d)\n", instr, _fs_, _ft_);
    clip_flags <<= 6; //Move previous clipping judgments up

    //Compare x, y, z fields of FS with the w field of FT
    float value = fabs(convert(gpr[_ft_].u[3]));

    float x = convert(gpr[_fs_].u[0]);
    float y = convert(gpr[_fs_].u[1]);
    float z = convert(gpr[_fs_].u[2]);

    printf("Compare: (%f, %f, %f) %f\n", x, y, z, value);

    clip_flags |= (x > +value);
    clip_flags |= (x < -value) << 1;
    clip_flags |= (y > +value) << 2;
    clip_flags |= (y < -value) << 3;
    clip_flags |= (z > +value) << 4;
    clip_flags |= (z < -value) << 5;
    clip_flags &= 0xFFFFFF;

    printf("New flags: $%08X\n", clip_flags);
}

void VectorUnit::div(uint32_t instr)
{
    float num = convert(gpr[_fs_].u[_fsf_]);
    float denom = convert(gpr[_ft_].u[_ftf_]);
    status = (status & 0xFCF) | ((status & 0x30) << 6);
    if (denom == 0.0)
    {
        if (num == 0.0)
            status_value = 0x10;
        else
            status_value = 0x20;

        status_pipe = 7;

        if ((gpr[_fs_].u[_fsf_] & 0x80000000) ^ (gpr[_ft_].u[_ftf_] & 0x80000000))
            new_Q_instance.u = 0xFF7FFFFF;
        else
            new_Q_instance.u = 0x7F7FFFFF;
    }
    else
    {
        new_Q_instance.f = num / denom;
        new_Q_instance.f = convert(new_Q_instance.u);
    }
    start_DIV_unit(7);
    printf("[VU] DIV: %f\n", new_Q_instance.f);
    printf("Reg1: %f\n", num);
    printf("Reg2: %f\n", denom);
}

float VectorUnit::calculate_atan(float t)
{
    //In reality, VU1 uses an approximation to derive the result. This is shown here.
    const static float atan_const[] = 
    { 
        0.999999344348907f, -0.333298563957214f, 
        0.199465364217758f, -0.139085337519646f, 
        0.096420042216778f, -0.055909886956215f,
        0.021861229091883f, -0.004054057877511f
    };

    float result = 0.785398185253143f; //pi/4

    for (int i = 0; i < 8; i++)
    {
        result += atan_const[i] * powf(t, (i * 2) + 1);
    }

    return result;
}

void VectorUnit::eatan(uint32_t instr)
{
    float x = convert(gpr[_fs_].u[_fsf_]);

    if (!id)
    {
        Errors::print_warning("[VU] EATAN called on VU0!");
        return;
    }

    //P = atan(x)
    if (x == -1.0f)
        new_P_instance.u = 0xFF7FFFFF;
    else
    {
        x = (x - 1.0f) / (x + 1.0f);

        new_P_instance.f = calculate_atan(x);
    }
    start_EFU_unit(54);
}

void VectorUnit::eatanxy(uint32_t instr)
{
    float x = convert(gpr[_fs_].u[0]);
    float y = convert(gpr[_fs_].u[1]);

    if (!id)
    {
        Errors::print_warning("[VU] EATANxy called on VU0!");
        return;
    }

    //P = atan(y/x)
    if (y + x == 0.0f)
        new_P_instance.u = 0x7F7FFFFF | (gpr[_fs_].u[1] & 0x80000000);
    else
    {
        x = (y - 1.0f) / (y + x);

        new_P_instance.f = calculate_atan(x);
    }
    start_EFU_unit(54);
}

void VectorUnit::eatanxz(uint32_t instr)
{
    float x = convert(gpr[_fs_].u[0]);
    float z = convert(gpr[_fs_].u[2]);

    if (!id)
    {
        Errors::print_warning("[VU] EATANxz called on VU0!");
        return;
    }

    //P = atan(z/x)
    if (z + x == 0.0f)
        new_P_instance.u = 0x7F7FFFFF | (gpr[_fs_].u[2] & 0x80000000);
    else
    {
        x = (z - x) / (z + x);

        new_P_instance.f = calculate_atan(x);
    }
    start_EFU_unit(54);
}

void VectorUnit::eexp(uint32_t instr)
{
    //P = exp(-reg)
    //In reality, VU1 uses an approximation to derive the result. This is shown here.
    const static float coeffs[] =
    {
        0.249998688697815, 0.031257584691048,
        0.002591371303424, 0.000171562001924,
        0.000005430199963, 0.000000690600018
    };
    if (!id)
    {
        Errors::print_warning("[VU] EEXP called on VU0!");
        return;
    }

    if (gpr[_fs_].u[_fsf_] & 0x80000000)
    {
        Errors::print_warning("[VU] EEXP called with sign bit set");
        new_P_instance.f = convert(gpr[_fs_].u[_fsf_]);
        return;
    }

    float value = 1;
    float x = convert(gpr[_fs_].u[_fsf_]);
    for (int exp = 1; exp <= 6; exp++)
        value += coeffs[exp - 1] * pow(x, exp);

    new_P_instance.f = 1.0 / value;
    start_EFU_unit(44);
}

void VectorUnit::esin(uint32_t instr)
{
    const static float coeffs[] =
    {
        -0.166666567325592, 0.008333025500178,
        -0.000198074136279, 0.000002601886990
    };
    float x = convert(gpr[_fs_].u[_fsf_]);
    new_P_instance.f = x;
    for (int c = 0; c < 4; c++) //Hah, c++
        new_P_instance.f += coeffs[c] * pow(x, (2 * c) + 3);

    start_EFU_unit(29);
}

void VectorUnit::ercpr(uint32_t instr)
{
    if (!id)
    {
        Errors::print_warning("[VU] ERCPR called on VU0!\n");
        return;
    }

    new_P_instance.f = convert(gpr[_fs_].u[_fsf_]);

    if (new_P_instance.f != 0)
        new_P_instance.f = 1.0f / new_P_instance.f;

    start_EFU_unit(12);
    printf("[VU] ERCPR: %f (%d)\n", P.f, _fs_);
}

void VectorUnit::eleng(uint32_t instr)
{
    if (!id)
    {
        Errors::print_warning("[VU] ELENG called on VU0!\n");
        return;
    }

    //P = sqrt(x^2 + y^2 + z^2)
    new_P_instance.f = pow(convert(gpr[_fs_].u[0]), 2) + pow(convert(gpr[_fs_].u[1]), 2) + pow(convert(gpr[_fs_].u[2]), 2);
    if (new_P_instance.f >= 0.0)
        new_P_instance.f = sqrt(new_P_instance.f);

    start_EFU_unit(18);
    printf("[VU] ELENG: %f (%d)\n", P.f, _fs_);
}

void VectorUnit::esqrt(uint32_t instr)
{
    if (!id)
    {
        Errors::print_warning("[VU] ESQRT called on VU0!\n");
        return;
    }

    new_P_instance.f = convert(gpr[_fs_].u[_fsf_]);
    new_P_instance.f = sqrt(fabs(new_P_instance.f));

    start_EFU_unit(12);
    printf("[VU] ESQRT: %f (%d)\n", P.f, _fs_);
}

void VectorUnit::esum(uint32_t instr)
{
    if (!id)
    {
        Errors::print_warning("[VU] ESUM called on VU0!");
        return;
    }

    new_P_instance.f = 0.0;
    for (int i = 0; i < 4; i++)
        new_P_instance.f += convert(gpr[_fs_].u[i]);

    start_EFU_unit(12);
    printf("[VU] ESUM: %f (%d)\n", new_P_instance.f, _fs_);
}

void VectorUnit::erleng(uint32_t instr)
{
    if (!id)
    {
        Errors::print_warning("[VU] ERLENG called on VU0!\n");
        return;
    }

    //P = 1 / sqrt(x^2 + y^2 + z^2)
    new_P_instance.f = pow(convert(gpr[_fs_].u[0]), 2) + pow(convert(gpr[_fs_].u[1]), 2) + pow(convert(gpr[_fs_].u[2]), 2);
    if (new_P_instance.f >= 0.0)
    {
        new_P_instance.f = sqrt(new_P_instance.f);
        if (new_P_instance.f != 0)
            new_P_instance.f = 1.0f / new_P_instance.f;
    }

    start_EFU_unit(24);
    printf("[VU] ERLENG: %f (%d)\n", P.f, _fs_);
}

void VectorUnit::ersadd(uint32_t instr)
{
    if (!id)
    {
        Errors::print_warning("[VU] ERSADD called on VU0!");
        return;
    }

    //P = 1 / (x^2 + y^2 + z^2)
    new_P_instance.f = pow(convert(gpr[_fs_].u[0]), 2) + pow(convert(gpr[_fs_].u[1]), 2) + pow(convert(gpr[_fs_].u[2]), 2);
    if (new_P_instance.f != 0)
        new_P_instance.f = 1.0f / new_P_instance.f;

    start_EFU_unit(18);
    printf("[VU] ERSADD: %f (%d)\n", P.f, _fs_);
}

void VectorUnit::ersqrt(uint32_t instr)
{
    if (!id)
    {
        Errors::print_warning("[VU] ERSQRT called on VU0!\n");
        return;
    }

    new_P_instance.f = convert(gpr[_fs_].u[_fsf_]);

    new_P_instance.f = sqrt(fabs(new_P_instance.f));
    if (new_P_instance.f != 0)
        new_P_instance.f = 1.0f / new_P_instance.f;

    start_EFU_unit(18);
    printf("[VU] ERSQRT: %f (%d)\n", P.f, _fs_);
}

void VectorUnit::esadd(uint32_t instr)
{
    if (!id)
    {
        Errors::print_warning("[VU] ESADD called on VU0!");
        return;
    }

    //P = x^2 + y^2 + z^2
    new_P_instance.f = pow(convert(gpr[_fs_].u[0]), 2) + pow(convert(gpr[_fs_].u[1]), 2) + pow(convert(gpr[_fs_].u[2]), 2);

    start_EFU_unit(11);
    printf("[VU] ESADD: %f (%d)\n", P.f, _fs_);
}

void VectorUnit::fcand(uint32_t value)
{
    printf("[VU] FCAND VI01: $%08X\n", value);
    if ((*CLIP_flags & 0xFFFFFF) & (value & 0xFFFFFF))
        set_int(1, 1);
    else
        set_int(1, 0);
}

void VectorUnit::fceq(uint32_t instr)
{
    printf("[VU] FCEQ VI01: $%08X\n", instr);
    if ((*CLIP_flags & 0xFFFFFF) == (instr & 0xFFFFFF))
        set_int(1, 1);
    else
        set_int(1, 0);
}

void VectorUnit::fcget(uint32_t instr)
{
    printf("[VU] FCGET VI%02D\n", _it_);
    set_int(_it_, *CLIP_flags & 0xFFF);
}

void VectorUnit::fcor(uint32_t instr)
{
    printf("[VU] FCOR VI01: $%08X\n", instr & 0xFFFFFF);
    if (((*CLIP_flags & 0xFFFFFF) | (instr & 0xFFFFFF)) == 0xFFFFFF)
        set_int(1, 1);
    else
        set_int(1, 0);
}

void VectorUnit::fcset(uint32_t instr)
{
    printf("[VU] FCSET: $%08X\n", instr & 0xFFFFFF);
    clip_flags = instr & 0xFFFFFF;
}

void VectorUnit::fmeq(uint32_t instr)
{
    printf("[VU] FMEQ VI%02D VI%02D: $%04X\n", _it_, _is_, int_gpr[_is_].u);
    if((*MAC_flags & 0xFFFF) == (int_gpr[_is_].u & 0xFFFF))
        set_int(_it_, 1);
    else
        set_int(_it_, 0);
}

void VectorUnit::fmand(uint32_t instr)
{
    printf("[VU] FMAND VI%02D VI%02D: $%04X\n", _it_, _is_, int_gpr[_is_].u);
    printf("MAC flags: $%08X\n", *MAC_flags);
    set_int(_it_, (*MAC_flags & 0xFFFF) & int_gpr[_is_].u);
}

void VectorUnit::fmor(uint32_t instr)
{
    printf("[VU] FMOR VI%02D VI%02D: $%04X\n", _it_, _is_, int_gpr[_is_].u);
    set_int(_it_, (*MAC_flags & 0xFFFF) | int_gpr[_is_].u);
}

void VectorUnit::fseq(uint32_t instr)
{
    uint16_t imm = (((instr >> 21) & 0x1) << 11) | (instr & 0x7FF);
    printf("[VU] FSEQ VI%02D: $%08X\n", _it_, imm);
    if ((status & 0xFFF) == imm)
        set_int(_it_, 1);
    else
        set_int(_it_, 0);
}

void VectorUnit::fsset(uint32_t instr)
{
    uint16_t imm = (((instr >> 21) & 0x1) << 11) | (instr & 0x7FF);
    printf("[VU] FSSET: $%08X\n", imm);
    status_value = imm;
    status_pipe = 4;
}

void VectorUnit::fsand(uint32_t instr)
{
    uint16_t imm = (((instr >> 21 ) & 0x1) << 11) | (instr & 0x7FF);
    printf("[VU] FSAND VI%02D: $%08X\n", _it_, imm);
    set_int(_it_, status & imm);
}

void VectorUnit::fsor(uint32_t instr)
{
    uint16_t imm = (((instr >> 21) & 0x1) << 11) | (instr & 0x7FF);
    printf("[VU] FSOR VI%02D: $%08X\n", _it_, imm);
    set_int(_it_, status | imm);
}

void VectorUnit::ftoi0(uint32_t instr)
{
    printf("[VU] FTOI0: ");
    for (int i = 0; i < 4; i++)
    {
        if (_field & (1 << (3 - i)))
        {
            gpr[_ft_].s[i] = (int32_t)convert(gpr[_fs_].u[i]);
            printf("(%d)$%08X ", i, gpr[_ft_].s[i]);
        }
    }
    printf("\n");
}

void VectorUnit::ftoi4(uint32_t instr)
{
    printf("[VU] FTOI4: ");
    for (int i = 0; i < 4; i++)
    {
        if (_field & (1 << (3 - i)))
        {
            gpr[_ft_].s[i] = (int32_t)(convert(gpr[_fs_].u[i]) * (1.0f / 0.0625f));
            printf("(%d)$%08X ", i, gpr[_ft_].s[i]);
        }
    }
    printf("\n");
}

void VectorUnit::ftoi12(uint32_t instr)
{
    printf("[VU] FTOI12: ");
    for (int i = 0; i < 4; i++)
    {
        if (_field & (1 << (3 - i)))
        {
            gpr[_ft_].s[i] = (int32_t)(convert(gpr[_fs_].u[i]) * (1.0f / 0.000244140625f));
            printf("(%d)$%08X ", i, gpr[_ft_].s[i]);
        }
    }
    printf("\n");
}

void VectorUnit::ftoi15(uint32_t instr)
{
    printf("[VU] FTOI15: ");
    for (int i = 0; i < 4; i++)
    {
        if (_field & (1 << (3 - i)))
        {
            gpr[_ft_].s[i] = (int32_t)(convert(gpr[_fs_].u[i]) * (1.0f / 0.000030517578125f));
            printf("(%d)$%08X ", i, gpr[_ft_].s[i]);
        }
    }
    printf("\n");
}

void VectorUnit::iadd(uint32_t instr)
{
    write_int(_id_, _is_, _it_);
    set_int(_id_, int_gpr[_is_].s + int_gpr[_it_].s);
    printf("[VU] IADD: $%04X (%d, %d, %d)\n", int_gpr[_id_].u, _id_, _is_, _it_);
}

void VectorUnit::iaddi(uint32_t instr)
{
    write_int(_it_, _is_);
    int16_t imm = ((instr >> 6) & 0x1f);
    imm = ((imm & 0x10 ? 0xfff0 : 0) | (imm & 0xf));
    set_int(_it_, int_gpr[_is_].s + imm);
    printf("[VU] IADDI: $%04X (%d, %d, %d)\n", int_gpr[_it_].u, _it_, _is_, imm);
}

void VectorUnit::iaddiu(uint32_t instr)
{
    write_int(_it_, _is_);
    uint16_t imm = (((instr >> 10) & 0x7800) | (instr & 0x7ff));
    set_int(_it_, int_gpr[_is_].s + imm);
    printf("[VU] IADDIU: $%04X (%d, %d, $%04X)\n", int_gpr[_it_].u, _it_, _is_, imm);
}

void VectorUnit::iand(uint32_t instr)
{
    write_int(_id_, _is_, _it_);
    set_int(_id_, int_gpr[_is_].u & int_gpr[_it_].u);
    printf("[VU] IAND: $%04X (%d, %d, %d)\n", int_gpr[_id_].u, _id_, _is_, _it_);
}

void VectorUnit::ibeq(uint32_t instr)
{
    int16_t imm = instr & 0x7FF;
    imm = ((int16_t)(imm << 5)) >> 5;
    imm *= 8;

    uint16_t reg1 = read_int_for_branch_condition(_is_).u;
    uint16_t reg2 = read_int_for_branch_condition(_it_).u;

    branch(reg1 == reg2, imm, false);
}

void VectorUnit::ibgez(uint32_t instr)
{
    int16_t imm = instr & 0x7FF;
    imm = ((int16_t)(imm << 5)) >> 5;
    imm *= 8;

    int16_t value = read_int_for_branch_condition(_is_).s;


    branch(value >= 0, imm, false);
}

void VectorUnit::ibgtz(uint32_t instr)
{
    int16_t imm = instr & 0x7FF;
    imm = ((int16_t)(imm << 5)) >> 5;
    imm *= 8;
    int16_t value = read_int_for_branch_condition(_is_).s;

    branch(value > 0, imm, false);
}

void VectorUnit::iblez(uint32_t instr)
{
    int16_t imm = instr & 0x7FF;
    imm = ((int16_t)(imm << 5)) >> 5;
    imm *= 8;
    int16_t value = read_int_for_branch_condition(_is_).s;

    branch(value <= 0, imm, false);
}

void VectorUnit::ibltz(uint32_t instr)
{
    int16_t imm = instr & 0x7FF;
    imm = ((int16_t)(imm << 5)) >> 5;
    imm *= 8;
    int16_t value = read_int_for_branch_condition(_is_).s;


    branch(value < 0, imm, false);
}

void VectorUnit::ibne(uint32_t instr)
{
    int16_t imm = instr & 0x7FF;
    imm = ((int16_t)(imm << 5)) >> 5;
    imm *= 8;

    uint16_t reg1 = read_int_for_branch_condition(_is_).u;
    uint16_t reg2 = read_int_for_branch_condition(_it_).u;

    branch(reg1 != reg2, imm, false);
}

void VectorUnit::ilw(uint32_t instr)
{
    write_int(_it_, _is_);
    int16_t offset = (instr & 0x400) ? (instr & 0x3FF) | 0xFC00 : (instr & 0x3FF);
    uint16_t addr = (int_gpr[_is_].s + offset) * 16;
    printf("[VU] ILW: $%08X ($%08X)\n", addr, offset);
    for (int i = 0; i < 4; i++)
    {
        if (_field & (1 << (3 - i)))
        {
            uint32_t word = read_data<uint32_t>(addr + (i * 4));
            printf(" $%04X ($%02X, %d, %d)", word, _field, _it_, _is_);
            set_int(_it_, word & 0xFFFF);
            break;
        }
    }
    printf("\n");
}

void VectorUnit::ilwr(uint32_t instr)
{
    write_int(_it_, _is_);
    uint32_t addr = (uint32_t)int_gpr[_is_].u << 4;
    printf("[VU] ILWR: $%08X", addr);
    for (int i = 0; i < 4; i++)
    {
        if (_field & (1 << (3 - i)))
        {
            uint32_t word = read_data<uint32_t>(addr + (i * 4));
            printf(" $%04X ($%02X, %d, %d)", word, _field, _it_, _is_);
            set_int(_it_, word & 0xFFFF);
            break;
        }
    }
    printf("\n");
}

void VectorUnit::ior(uint32_t instr)
{
    write_int(_id_, _is_, _it_);
    set_int(_id_, int_gpr[_is_].u | int_gpr[_it_].u);
    printf("[VU] IOR: $%04X (%d, %d, %d)\n", int_gpr[_id_].u, _id_, _is_, _it_);
}

void VectorUnit::isub(uint32_t instr)
{
    write_int(_id_, _is_, _it_);
    set_int(_id_, int_gpr[_is_].s - int_gpr[_it_].s);
    printf("[VU] ISUB: $%04X (%d, %d, %d)\n", int_gpr[_id_].u, _id_, _is_, _it_);
}

void VectorUnit::isubiu(uint32_t instr)
{
    write_int(_it_, _is_);
    uint16_t imm = ((instr >> 10) & 0x7800) | (instr & 0x7ff);
    set_int(_it_, int_gpr[_is_].s - imm);
    printf("[VU] ISUBIU: $%04X (%d, %d, $%04X)\n", int_gpr[_it_].u, _it_, _is_, imm);
}

void VectorUnit::isw(uint32_t instr)
{
    int16_t offset = (instr & 0x400) ? (instr & 0x3FF) | 0xFC00 : (instr & 0x3FF);
    uint16_t addr = (int_gpr[_is_].s + offset) * 16;
    printf("[VU] ISW: $%08X: $%04X ($%02X, %d, %d)\n", addr, int_gpr[_it_].u, _field, _it_, _is_);
    for (int i = 0; i < 4; i++)
    {
        if (_field & (1 << (3 - i)))
        {
            write_data<uint32_t>(addr + (i * 4), int_gpr[_it_].u);
        }
    }
}

void VectorUnit::iswr(uint32_t instr)
{
    uint32_t addr = (uint32_t)int_gpr[_is_].u << 4;
    printf("[VU] ISWR to $%08X!\n", addr);
    for (int i = 0; i < 4; i++)
    {
        if (_field & (1 << (3 - i)))
        {
            printf("($%02X, %d, %d)\n", _field, _it_, _is_);
            write_data<uint32_t>(addr + (i * 4), int_gpr[_it_].u);
        }
    }
}

void VectorUnit::itof0(uint32_t instr)
{
    printf("[VU] ITOF0: ");
    for (int i = 0; i < 4; i++)
    {
        if (_field & (1 << (3 - i)))
        {
            set_gpr_f(_ft_, i, (float)gpr[_fs_].s[i]);
            printf("(%d)%f ", i, gpr[_ft_].f[i]);
        }
    }
    printf("\n");
}

void VectorUnit::itof4(uint32_t instr)
{
    printf("[VU] ITOF4: ");
    for (int i = 0; i < 4; i++)
    {
        if (_field & (1 << (3 - i)))
        {
            gpr[_ft_].f[i] = (float)((float)gpr[_fs_].s[i] * 0.0625f);
            printf("(%d)%f ", i, gpr[_ft_].f[i]);
        }
    }
    printf("\n");
}

void VectorUnit::itof12(uint32_t instr)
{
    printf("[VU] ITOF12: ");
    for (int i = 0; i < 4; i++)
    {
        if (_field & (1 << (3 - i)))
        {
            gpr[_ft_].f[i] = (float)((float)gpr[_fs_].s[i] * 0.000244140625f);
            printf("(%d)%f ", i, gpr[_ft_].f[i]);
        }
    }
    printf("\n");
}

void VectorUnit::itof15(uint32_t instr)
{
    printf("[VU] ITOF15: ");
    for (int i = 0; i < 4; i++)
    {
        if (_field & (1 << (3 - i)))
        {
            gpr[_ft_].f[i] = (float)((float)gpr[_fs_].s[i] * 0.000030517578125);
            printf("(%d)%f ", i, gpr[_ft_].f[i]);
        }
    }
    printf("\n");
}

void VectorUnit::jr(uint32_t instr)
{
    uint8_t addr_reg = (instr >> 11) & 0x1F;
    uint16_t addr = get_int(addr_reg) * 8;
    printf("[VU] JR vi%d ($%x)\n", addr_reg, addr);
    jp(addr, false);
}

void VectorUnit::jalr(uint32_t instr)
{
    uint8_t addr_reg = (instr >> 11) & 0x1F;
    uint16_t addr = get_int(addr_reg) * 8;

    uint8_t link_reg = (instr >> 16) & 0x1F;
    // write_int(link_reg, addr_reg);
    printf("[VU] JALR vi%d ($%x) link vi%d\n", addr_reg, addr, link_reg);
    jp(addr, true, link_reg);
}

void VectorUnit::lq(uint32_t instr)
{
    int16_t imm = (int16_t)((instr & 0x400) ? (instr & 0x3ff) | 0xfc00 : (instr & 0x3ff));
    uint16_t addr = (int_gpr[_is_].s + imm) * 16;
    printf("[VU] LQ: $%08X (%d, %d, $%08X)\n", addr, _ft_, _is_, imm);
    for (int i = 0; i < 4; i++)
    {
        if (_field & (1 << (3 - i)))
        {
            set_gpr_u(_ft_, i, read_data<uint32_t>(addr + (i * 4)));
            printf("(%d)$%08X ", i, gpr[_ft_].u[i]);
        }
    }
    printf("\n");
}

void VectorUnit::lqd(uint32_t instr)
{
    write_int(_is_, _is_);
    printf("[VU] LQD: ");
    if (_is_)
        int_gpr[_is_].u--;
    uint32_t addr = (uint32_t)int_gpr[_is_].u * 16;
    for (int i = 0; i < 4; i++)
    {
        if (_field & (1 << (3 - i)))
        {
            set_gpr_u(_ft_, i, read_data<uint32_t>(addr + (i * 4)));
            printf("(%d)%f ", i, gpr[_ft_].f[i]);
        }
    }
    printf("\n");
}

void VectorUnit::lqi(uint32_t instr)
{
    write_int(_is_, _is_);
    printf("[VU] LQI: ");
    uint32_t addr = (uint32_t)int_gpr[_is_].u * 16;
    for (int i = 0; i < 4; i++)
    {
        if (_field & (1 << (3 - i)))
        {
            set_gpr_u(_ft_, i, read_data<uint32_t>(addr + (i * 4)));
            printf("(%d)%f ", i, gpr[_ft_].f[i]);
        }
    }
    printf("\n(%d: $%04X)", _is_, int_gpr[_is_]);
    if (_is_)
        int_gpr[_is_].u++;
    printf("\n");
}

void VectorUnit::madd(uint32_t instr)
{
    printf("[VU] MADD: ");
    for (int i = 0; i < 4; i++)
    {
        if (_field & (1 << (3 - i)))
        {
            float temp = convert(ACC.u[i]) + convert(gpr[_fs_].u[i]) * convert(gpr[_ft_].u[i]);
            set_gpr_f(_fd_, i, update_mac_flags(temp, i));
            printf("(%d)%f ", i, gpr[_fd_].f[i]);
        }
        else
            clear_mac_flags(i);
    }
    printf("\n");
}

void VectorUnit::madda(uint32_t instr)
{
    printf("[VU] MADDA: ");
    for (int i = 0; i < 4; i++)
    {
        if (_field & (1 << (3 - i)))
        {
            float temp = convert(ACC.u[i]) + convert(gpr[_fs_].u[i]) * convert(gpr[_ft_].u[i]);
            ACC.f[i] = update_mac_flags(temp, i);
            printf("(%d)%f ", i, ACC.f[i]);
        }
        else
            clear_mac_flags(i);
    }
    printf("\n");
}

void VectorUnit::maddabc(uint32_t instr)
{
    printf("[VU] MADDAbc: ");
    float op = convert(gpr[_ft_].u[_bc_]);
    for (int i = 0; i < 4; i++)
    {
        if (_field & (1 << (3 - i)))
        {
            float temp = convert(ACC.u[i]) + convert(gpr[_fs_].u[i]) * op;
            ACC.f[i] = update_mac_flags(temp, i);
            printf("(%d)%f ", i, ACC.f[i]);
        }
        else
            clear_mac_flags(i);
    }
    printf("\n");
}

void VectorUnit::maddai(uint32_t instr)
{
    printf("[VU] MADDAi: ");
    float op = convert(I.u);
    for (int i = 0; i < 4; i++)
    {
        if (_field & (1 << (3 - i)))
        {
            float temp = convert(ACC.u[i]) + convert(gpr[_fs_].u[i]) * op;
            ACC.f[i] = update_mac_flags(temp, i);
            printf("(%d)%f ", i, ACC.f[i]);
        }
        else
            clear_mac_flags(i);
    }
    printf("\n");
}

void VectorUnit::maddaq(uint32_t instr)
{
    printf("[VU] MADDAq: ");
    float op = convert(Q.u);
    for (int i = 0; i < 4; i++)
    {
        if (_field & (1 << (3 - i)))
        {
            float temp = convert(ACC.u[i]) + convert(gpr[_fs_].u[i]) * op;
            ACC.f[i] = update_mac_flags(temp, i);
            printf("(%d)%f ", i, ACC.f[i]);
        }
        else
            clear_mac_flags(i);
    }
    printf("\n");
}

void VectorUnit::maddbc(uint32_t instr)
{
    printf("[VU] MADDbc: ");
    float op = convert(gpr[_ft_].u[_bc_]);
    for (int i = 0; i < 4; i++)
    {
        if (_field & (1 << (3 - i)))
        {
            float temp = convert(ACC.u[i]) + convert(gpr[_fs_].u[i]) * op;
            set_gpr_f(_fd_, i, update_mac_flags(temp, i));
            printf("(%d)%f ", i, gpr[_fd_].f[i]);
        }
        else
            clear_mac_flags(i);
    }
    printf("\n");
}

void VectorUnit::maddi(uint32_t instr)
{
    printf("[VU] MADDi: ");
    float op = convert(I.u);
    for (int i = 0; i < 4; i++)
    {
        if (_field & (1 << (3 - i)))
        {
            float temp = convert(ACC.u[i]) + convert(gpr[_fs_].u[i]) * op;
            set_gpr_f(_fd_, i, update_mac_flags(temp, i));
            printf("(%d)%f ", i, gpr[_fd_].f[i]);
        }
        else
            clear_mac_flags(i);
    }
    printf("\n");
}

void VectorUnit::maddq(uint32_t instr)
{
    printf("[VU] MADDq: ");
    float op = convert(Q.u);
    for (int i = 0; i < 4; i++)
    {
        if (_field & (1 << (3 - i)))
        {
            float temp = convert(ACC.u[i]) + convert(gpr[_fs_].u[i]) * op;
            set_gpr_f(_fd_, i, update_mac_flags(temp, i));
            printf("(%d)%f ", i, gpr[_fd_].f[i]);
        }
        else
            clear_mac_flags(i);
    }
    printf("\n");
}

void VectorUnit::max(uint32_t instr)
{
    printf("[VU] MAX: ");
    for (int i = 0; i < 4; i++)
    {
        if (_field & (1 << (3 - i)))
        {
            int32_t op1 = gpr[_fs_].s[i];
            int32_t op2 = gpr[_ft_].s[i];
            set_gpr_s(_fd_, i, vu_max(op1, op2));
            printf("(%d)%f ", i, gpr[_fd_].f[i]);
        }
    }
    printf("\n");
}

void VectorUnit::maxi(uint32_t instr)
{
    printf("[VU] MAXi: ");
    int32_t op1 = (int32_t)I.u;
    for (int i = 0; i < 4; i++)
    {
        if (_field & (1 << (3 - i)))
        {
            int32_t op2 = gpr[_fs_].s[i];
            set_gpr_s(_fd_, i, vu_max(op1, op2));
            printf("(%d)%f ", i, gpr[_fd_].f[i]);
        }
    }
    printf("\n");
}

void VectorUnit::maxbc(uint32_t instr)
{
    printf("[VU] MAXbc: ");
    int32_t op2 = gpr[_ft_].s[_bc_];
    for (int i = 0; i < 4; i++)
    {
        if (_field & (1 << (3 - i)))
        {
            int32_t op1 = gpr[_fs_].s[i];
            set_gpr_s(_fd_, i, vu_max(op1, op2));
            printf("(%d)%f ", i, gpr[_fd_].f[i]);
        }
    }
    printf("\n");
}

void VectorUnit::mfir(uint32_t instr)
{
    printf("[VU] MFIR\n");
    for (int i = 0; i < 4; i++)
    {
        if (_field & (1 << (3 - i)))
            gpr[_ft_].s[i] = (int32_t)int_gpr[_is_].s;
    }
}

void VectorUnit::mfp(uint32_t instr)
{
    printf("[VU] MFP\n");
    for (int i = 0; i < 4; i++)
    {
        if (_field & (1 << (3 - i)))
            set_gpr_u(_ft_, i, P.u);
    }
}

void VectorUnit::mini(uint32_t instr)
{
    printf("[VU] MINI: ");
    for (int i = 0; i < 4; i++)
    {
        if (_field & (1 << (3 - i)))
        {
            int32_t op1 = gpr[_fs_].s[i];
            int32_t op2 = gpr[_ft_].s[i];
            set_gpr_s(_fd_, i, vu_min(op1, op2));
            printf("(%d)%f ", i, gpr[_fd_].f[i]);
        }
    }
    printf("\n");
}

void VectorUnit::minibc(uint32_t instr)
{
    printf("[VU] MINIbc: ");
    int32_t op1 = gpr[_ft_].s[_bc_];
    for (int i = 0; i < 4; i++)
    {
        if (_field & (1 << (3 - i)))
        {
            int32_t op2 = gpr[_fs_].s[i];
            set_gpr_s(_fd_, i, vu_min(op1, op2));
            printf("(%d)%f", i, gpr[_fd_].f[i]);
        }
    }
    printf("\n");
}

void VectorUnit::minii(uint32_t instr)
{
    printf("[VU] MINIi: ");
    int32_t op1 = (int32_t)I.u;
    for (int i = 0; i < 4; i++)
    {
        if (_field & (1 << (3 - i)))
        {
            int32_t op2 = gpr[_fs_].s[i];
            set_gpr_s(_fd_, i, vu_min(op1, op2));
            printf("(%d)%f", i, gpr[_fd_].f[i]);
        }
    }
    printf("\n");
}

void VectorUnit::move(uint32_t instr)
{
    printf("[VU] MOVE");
    if (_ft_)
    {
        for (int i = 0; i < 4; i++)
        {
            if (_field & (1 << (3 - i)))
                set_gpr_u(_ft_, i, gpr[_fs_].u[i]);
        }
    }
    printf("\n");
}

void VectorUnit::mr32(uint32_t instr)
{
    printf("[VU] MR32");
    uint32_t x = gpr[_fs_].u[0];
    if (_x(_field))
        set_gpr_u(_ft_, 0, gpr[_fs_].u[1]);
    if (_y(_field))
        set_gpr_u(_ft_, 1, gpr[_fs_].u[2]);
    if (_z(_field))
        set_gpr_u(_ft_, 2, gpr[_fs_].u[3]);
    if (_w(_field))
        set_gpr_u(_ft_, 3, x);
    printf("\n");
}

void VectorUnit::msubabc(uint32_t instr)
{
    printf("[VU] MSUBAbc: ");
    float op = convert(gpr[_ft_].u[_bc_]);
    for (int i = 0; i < 4; i++)
    {
        if (_field & (1 << (3 - i)))
        {
            float temp = convert(ACC.u[i]) - convert(gpr[_fs_].u[i]) * op;
            ACC.f[i] = update_mac_flags(temp, i);
            printf("(%d)%f ", i, ACC.f[i]);
        }
        else
            clear_mac_flags(i);
    }
    printf("\n");
}

void VectorUnit::msubai(uint32_t instr)
{
    printf("[VU] MSUBAi: ");
    float op = convert(I.u);
    for (int i = 0; i < 4; i++)
    {
        if (_field & (1 << (3 - i)))
        {
            float temp = convert(ACC.u[i]) - convert(gpr[_fs_].u[i]) * op;
            ACC.f[i] = update_mac_flags(temp, i);
            printf("(%d)%f ", i, ACC.f[i]);
        }
        else
            clear_mac_flags(i);
    }
    printf("\n");
}

void VectorUnit::msubaq(uint32_t instr)
{
    printf("[VU] MSUBAq: ");
    float op = convert(Q.u);
    for (int i = 0; i < 4; i++)
    {
        if (_field & (1 << (3 - i)))
        {
            float temp = convert(ACC.u[i]) - convert(gpr[_fs_].u[i]) * op;
            ACC.f[i] = update_mac_flags(temp, i);
            printf("(%d)%f ", i, ACC.f[i]);
        }
        else
            clear_mac_flags(i);
    }
    printf("\n");
}

void VectorUnit::msub(uint32_t instr)
{
    printf("[VU] MSUB: ");
    for (int i = 0; i < 4; i++)
    {
        if (_field & (1 << (3 - i)))
        {
            float temp = convert(ACC.u[i]) - convert(gpr[_fs_].u[i]) * convert(gpr[_ft_].u[i]);
            set_gpr_f(_fd_, i, update_mac_flags(temp, i));
            printf("(%d)%f ", i, gpr[_fd_].f[i]);
        }
        else
            clear_mac_flags(i);
    }
    printf("\n");
}

void VectorUnit::msuba(uint32_t instr)
{
    printf("[VU] MSUBA: ");
    for (int i = 0; i < 4; i++)
    {
        if (_field & (1 << (3 - i)))
        {
            float temp = convert(ACC.u[i]) - convert(gpr[_fs_].u[i]) * convert(gpr[_ft_].u[i]);
            ACC.f[i] = update_mac_flags(temp, i);
            printf("(%d)%f ", i, ACC.f[i]);
        }
        else
            clear_mac_flags(i);
    }
    printf("\n");
}

void VectorUnit::msubbc(uint32_t instr)
{
    printf("[VU] MSUBbc: ");
    float op = convert(gpr[_ft_].u[_bc_]);
    for (int i = 0; i < 4; i++)
    {
        if (_field & (1 << (3 - i)))
        {
            float temp = convert(ACC.u[i]) - convert(gpr[_fs_].u[i]) * op;
            set_gpr_f(_fd_, i, update_mac_flags(temp, i));
            printf("(%d)%f ", i, gpr[_fd_].f[i]);
        }
        else
            clear_mac_flags(i);
    }
    printf("\n");
}

void VectorUnit::msubi(uint32_t instr)
{
    printf("[VU] MSUBi: ");
    float op = convert(I.u);
    for (int i = 0; i < 4; i++)
    {
        if (_field & (1 << (3 - i)))
        {
            float temp = convert(ACC.u[i]) - convert(gpr[_fs_].u[i]) * op;
            set_gpr_f(_fd_, i, update_mac_flags(temp, i));
            printf("(%d)%f ", i, gpr[_fd_].f[i]);
        }
        else
            clear_mac_flags(i);
    }
    printf("\n");
}

void VectorUnit::msubq(uint32_t instr)
{
    printf("[VU] MSUBq: ");
    float op = convert(Q.u);
    for (int i = 0; i < 4; i++)
    {
        if (_field & (1 << (3 - i)))
        {
            float temp = convert(ACC.u[i]) - convert(gpr[_fs_].u[i]) * op;
            set_gpr_f(_fd_, i, update_mac_flags(temp, i));
            printf("(%d)%f ", i, gpr[_fd_].f[i]);
        }
        else
            clear_mac_flags(i);
    }
    printf("\n");
}

void VectorUnit::mtir(uint32_t instr)
{
    write_int(_it_);
    printf("[VU] MTIR: %d\n", gpr[_fs_].u[_fsf_] & 0xFFFF);
    set_int(_it_, gpr[_fs_].u[_fsf_] & 0xFFFF);
}

void VectorUnit::mul(uint32_t instr)
{
    printf("[VU] MUL: ");
    for (int i = 0; i < 4; i++)
    {
        if (_field & (1 << (3 - i)))
        {
            float result = convert(gpr[_fs_].u[i]) * convert(gpr[_ft_].u[i]);
            set_gpr_f(_fd_, i, update_mac_flags(result, i));
            printf("(%d)%f ", i, gpr[_fd_].f[i]);
        }
        else
            clear_mac_flags(i);
    }
    printf("\n");
}

void VectorUnit::mula(uint32_t instr)
{
    printf("[VU] MULA: ");
    for (int i = 0; i < 4; i++)
    {
        if (_field & (1 << (3 - i)))
        {
            float temp = convert(gpr[_fs_].u[i]) * convert(gpr[_ft_].u[i]);
            ACC.f[i] = update_mac_flags(temp, i);
            printf("(%d)%f ", i, ACC.f[i]);
        }
        else
            clear_mac_flags(i);
    }
    printf("\n");
}

void VectorUnit::mulabc(uint32_t instr)
{
    printf("[VU] MULAbc: ");
    float op = convert(gpr[_ft_].u[_bc_]);
    for (int i = 0; i < 4; i++)
    {
        if (_field & (1 << (3 - i)))
        {
            float temp = convert(gpr[_fs_].u[i]) * op;
            ACC.f[i] = update_mac_flags(temp, i);
            printf("(%d)%f ", i, ACC.f[i]);
        }
        else
            clear_mac_flags(i);
    }
    printf("\n");
}

void VectorUnit::mulai(uint32_t instr)
{
    printf("[VU] MULAi: (%f)", I.f);
    float op = convert(I.u);
    for (int i = 0; i < 4; i++)
    {
        if (_field & (1 << (3 - i)))
        {
            float temp = convert(gpr[_fs_].u[i]) * op;
            ACC.f[i] = update_mac_flags(temp, i);
            printf("(%d)%f ", i, ACC.f[i]);
        }
        else
            clear_mac_flags(i);
    }
    printf("\n");
}

void VectorUnit::mulaq(uint32_t instr)
{
    printf("[VU] MULAq: ");
    float op = convert(Q.u);
    for (int i = 0; i < 4; i++)
    {
        if (_field & (1 << (3 - i)))
        {
            float temp = convert(gpr[_fs_].u[i]) * op;
            ACC.f[i] = update_mac_flags(temp, i);
            printf("(%d)%f ", i, ACC.f[i]);
        }
        else
            clear_mac_flags(i);
    }
    printf("\n");
}

void VectorUnit::mulbc(uint32_t instr)
{
    printf("[VU] MULbc: ");
    float op = convert(gpr[_ft_].u[_bc_]);
    for (int i = 0; i < 4; i++)
    {
        if (_field & (1 << (3 - i)))
        {
            float temp = convert(gpr[_fs_].u[i]) * op;
            set_gpr_f(_fd_, i, update_mac_flags(temp, i));
            printf("(%d)%f ", i, gpr[_fd_].f[i]);
        }
        else
            clear_mac_flags(i);
    }
    printf("\n");
}

void VectorUnit::muli(uint32_t instr)
{
    printf("[VU] MULi: ");
    float op = convert(I.u);
    for (int i = 0; i < 4; i++)
    {
        if (_field & (1 << (3 - i)))
        {
            float temp = convert(gpr[_fs_].u[i]) * op;
            set_gpr_f(_fd_, i, update_mac_flags(temp, i));
            printf("(%d)%f ", i, gpr[_fd_].f[i]);
        }
        else
            clear_mac_flags(i);
    }
    printf("\n");
}

void VectorUnit::mulq(uint32_t instr)
{
    printf("[VU] MULq: ");
    float op = convert(Q.u);
    for (int i = 0; i < 4; i++)
    {
        if (_field & (1 << (3 - i)))
        {
            float temp = convert(gpr[_fs_].u[i]) * op;
            set_gpr_f(_fd_, i, update_mac_flags(temp, i));
            printf("(%d)%f ", i, gpr[_fd_].f[i]);
        }
        else
            clear_mac_flags(i);
    }
    printf("\n");
}

void VectorUnit::nop(uint32_t instr)
{
    //nop lives up to its name
}

/**
 * FDx = ACCx - FSy * FTz
 * FDy = ACCy - FSz * FTx
 * FDz = ACCz - FSx * FTy
 */
void VectorUnit::opmsub(uint32_t instr)
{
    VU_GPR temp;
    temp.f[0] = convert(ACC.u[0]) - convert(gpr[_fs_].u[1]) * convert(gpr[_ft_].u[2]);
    temp.f[1] = convert(ACC.u[1]) - convert(gpr[_fs_].u[2]) * convert(gpr[_ft_].u[0]);
    temp.f[2] = convert(ACC.u[2]) - convert(gpr[_fs_].u[0]) * convert(gpr[_ft_].u[1]);

    set_gpr_f(_fd_, 0, update_mac_flags(temp.f[0], 0));
    set_gpr_f(_fd_, 1, update_mac_flags(temp.f[1], 1));
    set_gpr_f(_fd_, 2, update_mac_flags(temp.f[2], 2));

    /*if (_fd_ == 1 && (PC == 0x510))
    {
        clear_mac_flags(0);
        clear_mac_flags(1);
        clear_mac_flags(2);
    }*/
    clear_mac_flags(3);
    printf("[VU] OPMSUB: %f, %f, %f\n", gpr[_fd_].f[0], gpr[_fd_].f[1], gpr[_fd_].f[2]);
}

/**
 * ACCx = FSy * FTz
 * ACCy = FSz * FTx
 * ACCz = FSx * FTy
 */
void VectorUnit::opmula(uint32_t instr)
{
    ACC.f[0] = convert(gpr[_fs_].u[1]) * convert(gpr[_ft_].u[2]);
    ACC.f[1] = convert(gpr[_fs_].u[2]) * convert(gpr[_ft_].u[0]);
    ACC.f[2] = convert(gpr[_fs_].u[0]) * convert(gpr[_ft_].u[1]);

    ACC.f[0] = update_mac_flags(ACC.f[0], 0);
    ACC.f[1] = update_mac_flags(ACC.f[1], 1);
    ACC.f[2] = update_mac_flags(ACC.f[2], 2);
    clear_mac_flags(3);
    printf("[VU] OPMULA: %f, %f, %f\n", ACC.f[0], ACC.f[1], ACC.f[2]);
}

void VectorUnit::rget(uint32_t instr)
{
    for (int i = 0; i < 4; i++)
    {
        if (_field & (1 << (3 - i)))
        {
            set_gpr_u(_ft_, i, R.u);
        }
    }
    printf("[VU] RGET: %f\n", R.f);
}

void VectorUnit::rinit(uint32_t instr)
{
    R.u = 0x3F800000;
    R.u |= gpr[_fs_].u[_fsf_] & 0x007FFFFF;
    printf("[VU] RINIT: %f\n", R.f);
}

void VectorUnit::rnext(uint32_t instr)
{
    advance_r();
    for (int i = 0; i < 4; i++)
    {
        if (_field & (1 << (3 - i)))
        {
            set_gpr_u(_ft_, i, R.u);
        }
    }
    printf("[VU] RNEXT: %f\n", R.f);
}

void VectorUnit::rsqrt(uint32_t instr)
{
    float denom = (convert(gpr[_ft_].u[_ftf_]));
    float num = convert(gpr[_fs_].u[_fsf_]);

    status = (status & 0xFCF) | ((status & 0x30) << 6);

    if (denom == 0.0)
    {
        printf("[VU] RSQRT by zero!\n");
        status_value = 0x20;
        status_pipe = 13;
        if (num == 0.0)
        {
            if ((gpr[_fs_].u[_fsf_] & 0x80000000) ^ (gpr[_ft_].u[_ftf_] & 0x80000000))
                new_Q_instance.u = 0x80000000;
            else
                new_Q_instance.u = 0;

            status_value |= 0x10;
            
        }
        else
        {
            if ((gpr[_fs_].u[_fsf_] & 0x80000000) ^ (gpr[_ft_].u[_ftf_] & 0x80000000))
                new_Q_instance.u = 0xFF7FFFFF;
            else
                new_Q_instance.u = 0x7F7FFFFF;
        }
    }
    else
    {
        if (denom < 0.0)
        {
            status_value = 0x10;
            status_pipe = 13;
        }

        new_Q_instance.f = num;
        new_Q_instance.f /= sqrt(fabs(denom));
        new_Q_instance.f = convert(new_Q_instance.u);
    }
    start_DIV_unit(13);
    printf("[VU] RSQRT: %f\n", new_Q_instance.f);
    printf("Reg1: %f\n", gpr[_fs_].f[_fsf_]);
    printf("Reg2: %f\n", gpr[_ft_].f[_ftf_]);
}

void VectorUnit::rxor(uint32_t instr)
{
    R.u = 0x3F800000 | ((R.u ^ gpr[_fs_].u[_fsf_]) & 0x007FFFFF);
    printf("[VU] RXOR: %f\n", R.f);
}

void VectorUnit::sq(uint32_t instr)
{
    int16_t imm = (int16_t)((instr & 0x400) ? (instr & 0x3ff) | 0xfc00 : (instr & 0x3ff));
    uint16_t addr = (int_gpr[_it_].s + imm) * 16;
    printf("[VU] SQ to $%08X!\n", addr);
    for (int i = 0; i < 4; i++)
    {
        if (_field & (1 << (3 - i)))
        {
            write_data<uint32_t>(addr + (i * 4), gpr[_fs_].u[i]);
            printf("$%08X(%d) ", gpr[_fs_].u[i], i);
        }
    }
    printf("\n");
}

void VectorUnit::sqd(uint32_t instr)
{
    write_int(_it_, _it_);
    if (_it_)
        int_gpr[_it_].u--;
    uint32_t addr = (uint32_t)int_gpr[_it_].u << 4;
    printf("[VU] SQD to $%08X!\n", addr);
    for (int i = 0; i < 4; i++)
    {
        if (_field & (1 << (3 - i)))
        {
            write_data<uint32_t>(addr + (i * 4), gpr[_fs_].u[i]);
            printf("$%08X(%d) ", gpr[_fs_].u[i], i);
        }
    }
    printf("\n");
}

void VectorUnit::sqi(uint32_t instr)
{
    write_int(_it_, _it_);
    uint32_t addr = (uint32_t)int_gpr[_it_].u << 4;
    printf("[VU] SQI to $%08X!\n", addr);
    for (int i = 0; i < 4; i++)
    {
        if (_field & (1 << (3 - i)))
        {
            write_data<uint32_t>(addr + (i * 4), gpr[_fs_].u[i]);
            printf("$%08X(%d) ", gpr[_fs_].u[i], i);
        }
    }
    printf("\n");
    if (_it_)
        int_gpr[_it_].u++;
}

void VectorUnit::vu_sqrt(uint32_t instr)
{
    status = (status & 0xfcf) | ((status & 0x30) << 6);
    if (convert(gpr[_ft_].u[_ftf_]) < 0.0)
    {
        status_value = 0x10;
        status_pipe = 7;
    }

    new_Q_instance.f = sqrt(fabs(convert(gpr[_ft_].u[_ftf_])));
    new_Q_instance.f = convert(new_Q_instance.u);
    start_DIV_unit(7);
    printf("[VU] SQRT: %f\n", new_Q_instance.f);
    printf("Source: %f\n", gpr[_ft_].f[_ftf_]);
}

void VectorUnit::sub(uint32_t instr)
{
    printf("[VU] SUB: ");
    for (int i = 0; i < 4; i++)
    {
        if (_field & (1 << (3 - i)))
        {
            float result = convert(gpr[_fs_].u[i]) - convert(gpr[_ft_].u[i]);
            set_gpr_f(_fd_, i, update_mac_flags(result, i));
            printf("(%d)%f ", i, gpr[_fd_].f[i]);
        }
        else
            clear_mac_flags(i);
    }
    printf("\n");
}

void VectorUnit::suba(uint32_t instr)
{
    printf("[VU] SUBA: ");
    for (int i = 0; i < 4; i++)
    {
        if (_field & (1 << (3 - i)))
        {
            ACC.f[i] = convert(gpr[_fs_].u[i]) - convert(gpr[_ft_].u[i]);
            ACC.f[i] = update_mac_flags(ACC.f[i], i);
            printf("(%d)%f ", i, ACC.f[i]);
        }
        else
            clear_mac_flags(i);
    }
    printf("\n");
}

void VectorUnit::subabc(uint32_t instr)
{
    printf("[VU] SUBAbc: ");
    float op = convert(gpr[_ft_].u[_bc_]);
    for (int i = 0; i < 4; i++)
    {
        if (_field & (1 << (3 - i)))
        {
            ACC.f[i] = convert(gpr[_fs_].u[i]) - op;
            ACC.f[i] = update_mac_flags(ACC.f[i], i);
            printf("(%d)%f", i, ACC.f[i]);
        }
        else
            clear_mac_flags(i);
    }
    printf("\n");
}

void VectorUnit::subai(uint32_t instr)
{
    printf("[VU] SUBAi: ");
    float op = convert(I.u);
    for (int i = 0; i < 4; i++)
    {
        if (_field & (1 << (3 - i)))
        {
            ACC.f[i] = convert(gpr[_fs_].u[i]) - op;
            ACC.f[i] = update_mac_flags(ACC.f[i], i);
            printf("(%d)%f ", i, ACC.f[i]);
        }
        else
            clear_mac_flags(i);
    }
    printf("\n");
}

void VectorUnit::subaq(uint32_t instr)
{
    printf("[VU] SUBAq: ");
    float op = convert(Q.u);
    for (int i = 0; i < 4; i++)
    {
        if (_field & (1 << (3 - i)))
        {
            ACC.f[i] = convert(gpr[_fs_].u[i]) - op;
            ACC.f[i] = update_mac_flags(ACC.f[i], i);
            printf("(%d)%f ", i, ACC.f[i]);
        }
        else
            clear_mac_flags(i);
    }
    printf("\n");
}

void VectorUnit::subbc(uint32_t instr)
{
    printf("[VU] SUBbc: ");
    float op = convert(gpr[_ft_].u[_bc_]);
    for (int i = 0; i < 4; i++)
    {
        if (_field & (1 << (3 - i)))
        {
            float temp = convert(gpr[_fs_].u[i]) - op;
            set_gpr_f(_fd_, i, update_mac_flags(temp, i));
            printf("(%d)%f ", i, gpr[_fd_].f[i]);
        }
        else
            clear_mac_flags(i);
    }
    printf("\n");
}

void VectorUnit::subi(uint32_t instr)
{
    printf("[VU] SUBi: ");
    float op = convert(I.u);
    for (int i = 0; i < 4; i++)
    {
        if (_field & (1 << (3 - i)))
        {
            float temp = convert(gpr[_fs_].u[i]) - op;
            set_gpr_f(_fd_, i, update_mac_flags(temp, i));
            printf("(%d)%f ", i, gpr[_fd_].f[i]);
        }
        else
            clear_mac_flags(i);
    }
    printf("\n");
}

void VectorUnit::subq(uint32_t instr)
{
    printf("[VU] SUBq: ");
    float op = convert(Q.u);
    for (int i = 0; i < 4; i++)
    {
        if (_field & (1 << (3 - i)))
        {
            float temp = convert(gpr[_fs_].u[i]) - op;
            set_gpr_f(_fd_, i, update_mac_flags(temp, i));
            printf("(%d)%f ", i, gpr[_fd_].f[i]);
        }
        else
            clear_mac_flags(i);
    }
    printf("\n");
}

void VectorUnit::waitp(uint32_t instr)
{
    if (!EFU_event_started)
        return;
    //Stalls actually release 1 cycle before writeback, but should be safe to write back early if we are stalling
    finish_EFU_event -= 1;
    while (cycle_count < finish_EFU_event)
    {
        cycle_count++;
        update_mac_pipeline();
        int_branch_pipeline.update();
    }
    update_DIV_EFU_pipes();
}

void VectorUnit::waitq(uint32_t instr)
{
    if (!DIV_event_started)
        return;

    while (cycle_count < finish_DIV_event)
    {
        cycle_count++;
        update_mac_pipeline();
        int_branch_pipeline.update();
    }
    update_DIV_EFU_pipes();
}

void VectorUnit::xgkick(uint32_t instr)
{
    if (!id)
    {
        Errors::print_warning("[VU] WARNING: XGKICK called on VU0!\n");
        return;
    }
    printf("[VU1] XGKICK: Addr $%08X\n", (int_gpr[_is_].u & 0x3ff) * 16);

    //If an XGKICK transfer is ongoing, completely stall the VU until the first transfer has finished.
    //Note: a real VU executes for one or two more cycles before stalling due to pipelining.
    if (transferring_GIF)
    {
        printf("[VU1] XGKICK called during active transfer, stalling VU\n");
        stalled_GIF_addr = (uint32_t)(int_gpr[_is_].u & 0x3ff) * 16;
        XGKICK_stall = true;
    }
    else
    {
        gif->request_PATH(1, true);
        transferring_GIF = true;
        GIF_addr = (uint32_t)(int_gpr[_is_].u & 0x3ff) * 16;
        XGKICK_cycles = 0;
    }
}

void VectorUnit::xitop(uint32_t instr)
{
    printf("[VU] XITOP: $%04X (%d)\n", *VIF_ITOP, _it_);
    set_int(_it_, *VIF_ITOP);
}

void VectorUnit::xtop(uint32_t instr)
{
    if (!id)
    {
        Errors::print_warning("[VU] WARNING: XTOP called on VU0!\n");
        return;
    }
    printf("[VU1] XTOP: $%04X (%d)\n", *VIF_TOP, _it_);
    set_int(_it_, *VIF_TOP);
}
