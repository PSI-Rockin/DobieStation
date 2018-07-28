#include <cstdio>
#include <cstdlib>
#include "emotion.hpp"
#include "emotiondisasm.hpp"
#include "emotioninterpreter.hpp"
#include "vu.hpp"
#include "../errors.hpp"

#include "../emulator.hpp"

//#define printf(fmt, ...)(0)

EmotionEngine::EmotionEngine(Cop0* cp0, Cop1* fpu, Emulator* e, uint8_t* sp, VectorUnit* vu0) :
    cp0(cp0), fpu(fpu), e(e), scratchpad(sp), vu0(vu0)
{
    reset();
}

const char* EmotionEngine::REG(int id)
{
    static const char* names[] =
    {
        "zero", "at", "v0", "v1",
        "a0", "a1", "a2", "a3",
        "t0", "t1", "t2", "t3",
        "t4", "t5", "t6", "t7",
        "s0", "s1", "s2", "s3",
        "s4", "s5", "s6", "s7",
        "t8", "t9", "k0", "k1",
        "gp", "sp", "fp", "ra"
    };
    return names[id];
}

const char* EmotionEngine::SYSCALL(int id)
{
    //Taken from PCSX2
    //https://github.com/PCSX2/pcsx2/blob/af3e55af63dd23075c08eb6b181a6fe62793d8c0/pcsx2/R5900OpcodeImpl.cpp#L92
    static const char* names[256]=
    {
        "RFU000_FullReset", "ResetEE",				"SetGsCrt",				"RFU003",
        "Exit",				"RFU005",				"LoadExecPS2",			"ExecPS2",
        "RFU008",			"RFU009",				"AddSbusIntcHandler",	"RemoveSbusIntcHandler",
        "Interrupt2Iop",	"SetVTLBRefillHandler", "SetVCommonHandler",	"SetVInterruptHandler",
    //0x10
        "AddIntcHandler",	"RemoveIntcHandler",	"AddDmacHandler",		"RemoveDmacHandler",
        "_EnableIntc",		"_DisableIntc",			"_EnableDmac",			"_DisableDmac",
        "_SetAlarm",		"_ReleaseAlarm",		"_iEnableIntc",			"_iDisableIntc",
        "_iEnableDmac",		"_iDisableDmac",		"_iSetAlarm",			"_iReleaseAlarm",
    //0x20
        "CreateThread",			"DeleteThread",		"StartThread",			"ExitThread",
        "ExitDeleteThread",		"TerminateThread",	"iTerminateThread",		"DisableDispatchThread",
        "EnableDispatchThread",		"ChangeThreadPriority", "iChangeThreadPriority",	"RotateThreadReadyQueue",
        "iRotateThreadReadyQueue",	"ReleaseWaitThread",	"iReleaseWaitThread",		"GetThreadId",
    //0x30
        "ReferThreadStatus","iReferThreadStatus",	"SleepThread",		"WakeupThread",
        "_iWakeupThread",   "CancelWakeupThread",	"iCancelWakeupThread",	"SuspendThread",
        "iSuspendThread",   "ResumeThread",		"iResumeThread",	"JoinThread",
        "RFU060",	    "RFU061",			"EndOfHeap",		 "RFU063",
    //0x40
        "CreateSema",	    "DeleteSema",	"SignalSema",		"iSignalSema",
        "WaitSema",	    "PollSema",		"iPollSema",		"ReferSemaStatus",
        "iReferSemaStatus", "RFU073",		"SetOsdConfigParam", 	"GetOsdConfigParam",
        "GetGsHParam",	    "GetGsVParam",	"SetGsHParam",		"SetGsVParam",
    //0x50
        "RFU080_CreateEventFlag",	"RFU081_DeleteEventFlag",
        "RFU082_SetEventFlag",		"RFU083_iSetEventFlag",
        "RFU084_ClearEventFlag",	"RFU085_iClearEventFlag",
        "RFU086_WaitEventFlag",		"RFU087_PollEventFlag",
        "RFU088_iPollEventFlag",	"RFU089_ReferEventFlagStatus",
        "RFU090_iReferEventFlagStatus", "RFU091_GetEntryAddress",
        "EnableIntcHandler_iEnableIntcHandler",
        "DisableIntcHandler_iDisableIntcHandler",
        "EnableDmacHandler_iEnableDmacHandler",
        "DisableDmacHandler_iDisableDmacHandler",
    //0x60
        "KSeg0",				"EnableCache",	"DisableCache",			"GetCop0",
        "FlushCache",			"RFU101",		"CpuConfig",			"iGetCop0",
        "iFlushCache",			"RFU105",		"iCpuConfig", 			"sceSifStopDma",
        "SetCPUTimerHandler",	"SetCPUTimer",	"SetOsdConfigParam2",	"SetOsdConfigParam2",
    //0x70
        "GsGetIMR_iGsGetIMR",				"GsGetIMR_iGsPutIMR",	"SetPgifHandler", 				"SetVSyncFlag",
        "RFU116",							"print", 				"sceSifDmaStat_isceSifDmaStat", "sceSifSetDma_isceSifSetDma",
        "sceSifSetDChain_isceSifSetDChain", "sceSifSetReg",			"sceSifGetReg",					"ExecOSD",
        "Deci2Call",						"PSMode",				"MachineType",					"GetMemorySize",
    };
    id &= 0xFF;
    id = (int8_t)id;
    if (id < 0)
        id = -id;
    return names[id];
}

void EmotionEngine::reset()
{
    PC = 0xBFC00000;
    branch_on = false;
    increment_PC = true;
    can_disassemble = false;
    delay_slot = 0;

    //Clear out $zero
    for (int i = 0; i < 16; i++)
        gpr[i] = 0;

    deci2size = 0;
    for (int i = 0; i < 128; i++)
        deci2handlers[i].active = false;
}

int EmotionEngine::run(int cycles_to_run)
{
    int cycles = cycles_to_run;
    static int calls = 0;
    while (cycles_to_run)
    {
        cycles_to_run--;
        uint32_t instruction = read32(PC);
        if (PC == 0x37BDD4)
        {
            //calls++;
            //if (calls == 3)
                //can_disassemble = true;
            printf("readBufBeginPut\n");
        }
        if (PC == 0x0037BE28)
            printf("readBufBeginGet\n");
        if (PC == 0x0037Be50)
            printf("sceMpegDemuxPssRing\n");
        if (PC == 0x00218060)
        {
            calls++;
            //if (calls == 3)
                //can_disassemble = true;
        }
        if ((PC < 0x81FC0 || PC >= 0x81FE0))
        {
            if (can_disassemble) {
                std::string disasm = EmotionDisasm::disasm_instr(instruction, PC);
                printf("[$%08X] $%08X - %s\n", PC, instruction, disasm.c_str());
                print_state();
            }
        }
        else
        {
            if ((PC >= 0x81FC0 && PC < 0x81FE0) && !branch_on)
            {
                break;
            }
        }
        EmotionInterpreter::interpret(*this, instruction);
        if (increment_PC)
            PC += 4;
        else
            increment_PC = true;

        if (branch_on)
        {
            if (!delay_slot)
            {
                branch_on = false;
                if (!new_PC || (new_PC & 0x3))
                {
                    Errors::die("[EE] Jump to invalid address $%08X from $%08X\n", new_PC, PC - 8);
                }
                if (PC >= 0x82000 && new_PC == 0x81FC0)
                    printf("[EE] Entering BIFCO loop\n");
                PC = new_PC;
                /*if (PC == 0x1001E0)
                    PC = 0x100204;
                if (PC == 0x10021C)
                    PC = 0x10022C;*/
                //Temporary hack for Atelier Iris - Skip intro movies
                //if (PC == 0x0029e9cc)
                    //PC = 0x0029E984;
                if (PC < 0x80000000 && PC >= 0x00100000)
                    if (e->skip_BIOS())
                        return 0;
            }
            else
                delay_slot--;
        }
    }

    cp0->count_up(cycles);

    if (cp0->int_enabled())
    {
        if (cp0->cause.int0_pending)
            int0();
        else if (cp0->cause.int1_pending)
            int1();
    }
    return cycles;
}

void EmotionEngine::print_state()
{
    for (int i = 1; i < 32; i++)
    {
        printf("%s:$%08X_%08X_%08X_%08X", REG(i), get_gpr<uint32_t>(i, 3), get_gpr<uint32_t>(i, 2), get_gpr<uint32_t>(i, 1), get_gpr<uint32_t>(i));
        if ((i & 1) == 1)
            printf("\n");
        else
            printf("\t");
    }
    //printf("lo:$%08X_%08X_%08X_%08X\t", LO1 >> 32, LO1, LO >> 32, LO);
    //printf("hi:$%08X_%08X_%08X_%08X\t", HI1 >> 32, HI1, HI >> 32, HI);
    printf("\n");
}

void EmotionEngine::set_disassembly(bool dis)
{
    can_disassemble = dis;
}

uint32_t EmotionEngine::get_PC()
{
    return PC;
}

uint64_t EmotionEngine::get_LO()
{
    return LO;
}

uint64_t EmotionEngine::get_LO1()
{
    return LO1;
}

uint64_t EmotionEngine::get_HI()
{
    return HI;
}

uint64_t EmotionEngine::get_HI1()
{
    return HI1;
}

uint64_t EmotionEngine::get_SA()
{
    return SA;
}

uint8_t EmotionEngine::read8(uint32_t address)
{
    if (address >= 0x70000000 && address < 0x70004000)
        return scratchpad[address & 0x3FFF];
    if (address >= 0x30100000 && address <= 0x31FFFFFF)
        address -= 0x10000000;
    return e->read8(address & 0x1FFFFFFF);
}

uint16_t EmotionEngine::read16(uint32_t address)
{
    if (address >= 0x70000000 && address < 0x70004000)
        return *(uint16_t*)&scratchpad[address & 0x3FFE];
    if (address >= 0x30100000 && address <= 0x31FFFFFF)
        address -= 0x10000000;
    return e->read16(address & 0x1FFFFFFF);
}

uint32_t EmotionEngine::read32(uint32_t address)
{
    if (address >= 0x70000000 && address < 0x70004000)
        return *(uint32_t*)&scratchpad[address & 0x3FFC];
    if (address >= 0x30100000 && address <= 0x31FFFFFF)
        address -= 0x10000000;
    return e->read32(address & 0x1FFFFFFF);
}

uint64_t EmotionEngine::read64(uint32_t address)
{
    if (address >= 0x70000000 && address < 0x70004000)
        return *(uint64_t*)&scratchpad[address & 0x3FF8];
    if (address >= 0x30100000 && address <= 0x31FFFFFF)
        address -= 0x10000000;
    return e->read64(address & 0x1FFFFFFF);
}

uint128_t EmotionEngine::read128(uint32_t address)
{
    if (address >= 0x70000000 && address < 0x70004000)
        return *(uint128_t*)&scratchpad[address & 0x3FF0];
    if (address >= 0x30100000 && address <= 0x31FFFFFF)
        address -= 0x10000000;
    return e->read128(address & 0x1FFFFFFF);
}

/*void EmotionEngine::set_gpr_lo(int index, uint64_t value)
{
    if (index)
        gpr_lo[index] = value;
}*/

void EmotionEngine::set_PC(uint32_t addr)
{
    PC = addr;
}

void EmotionEngine::write8(uint32_t address, uint8_t value)
{
    if (address >= 0x70000000 && address < 0x70004000)
    {
        scratchpad[address & 0x3FFF] = value;
        return;
    }
    if (address >= 0x30100000 && address <= 0x31FFFFFF)
        address -= 0x10000000;
    e->write8(address & 0x1FFFFFFF, value);
}

void EmotionEngine::write16(uint32_t address, uint16_t value)
{
    if (address >= 0x70000000 && address < 0x70004000)
    {
        *(uint16_t*)&scratchpad[address & 0x3FFE] = value;
        return;
    }
    if (address >= 0x30100000 && address <= 0x31FFFFFF)
        address -= 0x10000000;
    e->write16(address & 0x1FFFFFFF, value);
}

void EmotionEngine::write32(uint32_t address, uint32_t value)
{
    if (address >= 0x70000000 && address < 0x70004000)
    {
        *(uint32_t*)&scratchpad[address & 0x3FFC] = value;
        return;
    }
    if (address >= 0x30100000 && address <= 0x31FFFFFF)
        address -= 0x10000000;
    e->write32(address & 0x1FFFFFFF, value);
}

void EmotionEngine::write64(uint32_t address, uint64_t value)
{
    if (address >= 0x70000000 && address < 0x70004000)
    {
        *(uint64_t*)&scratchpad[address & 0x3FF8] = value;
        return;
    }
    if (address >= 0x30100000 && address <= 0x31FFFFFF)
        address -= 0x10000000;
    e->write64(address & 0x1FFFFFFF, value);
}

void EmotionEngine::write128(uint32_t address, uint128_t value)
{
    if (address >= 0x70000000 && address < 0x70004000)
    {
        *(uint128_t*)&scratchpad[address & 0x3FF0] = value;
        return;
    }
    if (address >= 0x30100000 && address <= 0x31FFFFFF)
        address -= 0x10000000;
    e->write128(address & 0x1FFFFFFF, value);
}

void EmotionEngine::jp(uint32_t new_addr)
{
    branch_on = true;
    new_PC = new_addr;
    delay_slot = 1;
}

void EmotionEngine::branch(bool condition, int offset)
{
    if (condition)
    {
        branch_on = true;
        new_PC = PC + offset + 4;
        delay_slot = 1;
    }
}

void EmotionEngine::branch_likely(bool condition, int offset)
{
    if (condition)
    {
        branch_on = true;
        new_PC = PC + offset + 4;
        delay_slot = 1;
    }
    else
        PC += 4;
}

void EmotionEngine::mfc(int cop_id, int reg, int cop_reg)
{
    int64_t bark;
    switch (cop_id)
    {
        case 0:
            bark = cp0->mfc(cop_reg);
            break;
        case 1:
            bark = fpu->get_gpr(cop_reg);
            break;
        default:
            Errors::die("Unrecognized cop id %d in mfc\n", cop_id);
    }

    set_gpr<int64_t>(reg, (int32_t)bark);
}

void EmotionEngine::mtc(int cop_id, int reg, int cop_reg)
{
    switch (cop_id)
    {
        case 0:
            cp0->mtc(cop_reg, get_gpr<uint32_t>(reg));
            break;
        case 1:
            fpu->mtc(cop_reg, get_gpr<uint32_t>(reg));
            break;
        default:
            Errors::die("Unrecognized cop id %d in mtc\n", cop_id);
    }
}

void EmotionEngine::cfc(int cop_id, int reg, int cop_reg)
{
    int32_t bark = 0;
    switch (cop_id)
    {
        case 1:
            bark = (int32_t)fpu->cfc(cop_reg);
            break;
        case 2:
            bark = (int32_t)vu0->cfc(cop_reg);
            break;
    }
    set_gpr<int64_t>(reg, bark);
}

void EmotionEngine::ctc(int cop_id, int reg, int cop_reg)
{
    uint32_t bark = get_gpr<uint32_t>(reg);
    switch (cop_id)
    {
        case 1:
            fpu->ctc(cop_reg, bark);
            break;
        case 2:
            vu0->ctc(cop_reg, bark);
            break;
    }
}

void EmotionEngine::mfhi(int index)
{
    set_gpr<uint64_t>(index, HI);
}

void EmotionEngine::mthi(int index)
{
    HI = get_gpr<uint64_t>(index);
}

void EmotionEngine::mflo(int index)
{
    set_gpr<uint64_t>(index, LO);
}

void EmotionEngine::mtlo(int index)
{
    LO = get_gpr<uint64_t>(index);
}

void EmotionEngine::mfhi1(int index)
{
    set_gpr<uint64_t>(index, HI1);
}

void EmotionEngine::mthi1(int index)
{
    HI1 = get_gpr<uint64_t>(index);
}

void EmotionEngine::mflo1(int index)
{
    set_gpr<uint64_t>(index, LO1);
}

void EmotionEngine::mtlo1(int index)
{
    LO1 = get_gpr<uint64_t>(index);
}

void EmotionEngine::mfsa(int index)
{
    set_gpr<uint64_t>(index, SA);
}

void EmotionEngine::mtsa(int index)
{
    SA = get_gpr<uint64_t>(index) & 0xF;
}

void EmotionEngine::pmfhi(int index)
{
    set_gpr<uint64_t>(index, HI);
    set_gpr<uint64_t>(index, HI1, 1);
}

void EmotionEngine::pmflo(int index)
{
    set_gpr<uint64_t>(index, LO);
    set_gpr<uint64_t>(index, LO1, 1);
}

void EmotionEngine::pmthi(int index)
{
    HI = get_gpr<uint64_t>(index);
    HI1 = get_gpr<uint64_t>(index, 1);
}

void EmotionEngine::pmtlo(int index)
{
    LO = get_gpr<uint64_t>(index);
    LO1 = get_gpr<uint64_t>(index, 1);
}

void EmotionEngine::set_SA(uint64_t value)
{
    SA = value;
}

void EmotionEngine::lwc1(uint32_t addr, int index)
{
    fpu->mtc(index, read32(addr));
}

void EmotionEngine::lqc2(uint32_t addr, int index)
{
    //printf("LQC2 $%08X: ", addr);
    for (int i = 0; i < 4; i++)
    {
        uint32_t bark = read32(addr + (i << 2));
        //printf("$%08X ", bark);
        vu0->set_gpr_u(index, i, bark);
    }
    //printf("\n");
}

void EmotionEngine::swc1(uint32_t addr, int index)
{
    write32(addr, fpu->get_gpr(index));
}

void EmotionEngine::sqc2(uint32_t addr, int index)
{
    //printf("SQC2 $%08X: ", addr);
    for (int i = 0; i < 4; i++)
    {
        uint32_t bark = vu0->get_gpr_u(index, i);
        //printf("$%08X ", bark);
        write32(addr + (i << 2), bark);
    }
    //printf("\n");
}

void EmotionEngine::set_LO_HI(uint64_t a, uint64_t b, bool hi)
{
    a = (int64_t)a;
    b = (int64_t)b;
    if (hi)
    {
        LO1 = a;
        HI1 = b;
    }
    else
    {
        LO = a;
        HI = b;
    }
}

/**
 * EE exception handling logic:
 * EXL (bit 1) in STATUS is set
 * Exception code (bits 2-6) are set according to exception type
 * If the EE is in a branch delay slot, set BD (bit 31) in CAUSE
 * EPC is set to the current value of PC
 */
void EmotionEngine::handle_exception(uint32_t new_addr, uint8_t code)
{
    cp0->status.exception = true;
    cp0->cause.code = code;

    if (branch_on)
    {
        cp0->cause.bd = true;
        cp0->mtc(14, PC - 4);
    }
    else
    {
        cp0->cause.bd = false;
        cp0->mtc(14, PC);
    }

    branch_on = false;
    delay_slot = 0;
    PC = new_addr;
    increment_PC = false;
}

void EmotionEngine::hle_syscall()
{
    uint8_t op = read8(PC - 4);
    //bios->hle_syscall(*this, op);
}

void EmotionEngine::syscall_exception()
{
    uint8_t op = read8(PC - 4);
    if (op != 0x7A)
        printf("[EE] SYSCALL: %s (id: $%02X) called at $%08X\n", SYSCALL(op), op, PC);

    if (op == 0x7C)
    {
        deci2call(get_gpr<uint32_t>(4), get_gpr<uint32_t>(5));
        return;
    }
    if (op == 0x04)
    {
        //On a real PS2, Exit returns to OSDSYS, but that isn't functional yet.
        Errors::die("[EE] Exit syscall called!\n");
    }
    handle_exception(0x80000180, 0x08);
}

void EmotionEngine::deci2call(uint32_t func, uint32_t param)
{
    switch (func)
    {
        case 0x01:
        {
            printf("Deci2Open\n");
            int id = deci2size;
            deci2size++;
            deci2handlers[id].active = true;
            deci2handlers[id].device = read32(param);
            deci2handlers[id].addr = read32(param + 4);
            set_gpr<uint64_t>(2, id);
        }
            break;
        case 0x03:
        {
            printf("Deci2Send\n");
            int id = read32(param);
            if (deci2handlers[id].active)
            {
                uint32_t addr = read32(deci2handlers[id].addr + 0x10);
                printf("Str addr: $%08X\n", addr);
                int len = read32(addr) - 0x0C;
                uint32_t str = addr + 0x0C;
                printf("Len: %d\n", len);
                e->ee_deci2send(str, len);
            }
            set_gpr<uint64_t>(2, 1);
        }
            break;
        case 0x04:
        {
            printf("Deci2Poll\n");
            int id = read32(param);
            if (deci2handlers[id].active)
                write32(deci2handlers[id].addr + 0x0C, 0);
            set_gpr<uint64_t>(2, 1);
        }
            break;
        case 0x10:
            printf("kputs\n");
            e->ee_kputs(param);
            break;
    }
}

void EmotionEngine::int0()
{
    if (cp0->status.int0_mask)
    {
        printf("[EE] INT0!\n");
        handle_exception(0x80000200, 0);
    }
}

void EmotionEngine::int1()
{
    if (cp0->status.int1_mask)
    {
        printf("[EE] INT1!\n");
        //can_disassemble = true;
        handle_exception(0x80000200, 0);
    }
}

void EmotionEngine::set_int0_signal(bool value)
{
    cp0->cause.int0_pending = value;
    if (value)
        printf("[EE] Set INT0\n");
}

void EmotionEngine::set_int1_signal(bool value)
{
    cp0->cause.int1_pending = value;
    if (value)
        printf("[EE] Set INT1\n");
}

void EmotionEngine::eret()
{
    //printf("[EE] Return from exception\n");
    if (cp0->status.error)
    {
        PC = cp0->ErrorEPC;
        cp0->status.error = false;
    }
    else
    {
        PC = cp0->EPC;
        cp0->status.exception = false;
    }
    increment_PC = false;
}

void EmotionEngine::ei()
{
    if (cp0->status.edi || cp0->status.mode == 0)
        cp0->status.master_int_enable = true;
}

void EmotionEngine::di()
{
    if (cp0->status.edi || cp0->status.mode == 0)
        cp0->status.master_int_enable = false;
}

void EmotionEngine::cp0_bc0(int32_t offset, bool test_true, bool likely)
{
    bool passed = false;
    if (test_true)
        passed = cp0->get_condition();
    else
        passed = !cp0->get_condition();

    if (likely)
        branch_likely(passed, offset);
    else
        branch(passed, offset);
}

void EmotionEngine::fpu_cop_s(uint32_t instruction)
{
    EmotionInterpreter::cop_s(*fpu, instruction);
}

void EmotionEngine::fpu_bc1(int32_t offset, bool test_true, bool likely)
{
    bool passed = false;
    if (test_true)
        passed = fpu->get_condition();
    else
        passed = !fpu->get_condition();

    if (likely)
        branch_likely(passed, offset);
    else
        branch(passed, offset);
}

void EmotionEngine::fpu_cvt_s_w(int dest, int source)
{
    fpu->cvt_s_w(dest, source);
}

void EmotionEngine::qmfc2(int dest, int cop_reg)
{
    for (int i = 0; i < 4; i++)
    {
        set_gpr<uint32_t>(dest, vu0->get_gpr_u(cop_reg, i), i);
    }
}

void EmotionEngine::qmtc2(int source, int cop_reg)
{
    for (int i = 0; i < 4; i++)
    {
        uint32_t bark = get_gpr<uint32_t>(source, i);
        vu0->set_gpr_u(cop_reg, i, bark);
    }
}

void EmotionEngine::cop2_special(uint32_t instruction)
{
    EmotionInterpreter::cop2_special(*vu0, instruction);
}
