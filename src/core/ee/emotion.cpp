#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "emotion.hpp"
#include "emotiondisasm.hpp"
#include "emotioninterpreter.hpp"
#include "vu.hpp"
#include "../errors.hpp"

#include "../emulator.hpp"

//#define SKIPMPEG_ON

//#define printf(fmt, ...)(0)

EmotionEngine::EmotionEngine(Cop0* cp0, Cop1* fpu, Emulator* e, uint8_t* sp, VectorUnit* vu0, VectorUnit* vu1) :
    cp0(cp0), fpu(fpu), e(e), scratchpad(sp), vu0(vu0), vu1(vu1)
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
    cycle_count = 0;
    cycles_to_run = 0;
    branch_on = false;
    can_disassemble = false;
    wait_for_IRQ = false;
    delay_slot = 0;

    //Reset the cache
    memset(icache, 0, sizeof(icache));

    //Clear out $zero
    for (int i = 0; i < 16; i++)
        gpr[i] = 0;

    deci2size = 0;
    for (int i = 0; i < 128; i++)
        deci2handlers[i].active = false;
}

int EmotionEngine::run(int cycles)
{
    cycle_count += cycles;
    if (!wait_for_IRQ)
    {
        cycles_to_run += cycles;
        while (cycles_to_run > 0)
        {
            cycles_to_run--;

            uint32_t instruction = read_instr(PC);
            uint32_t lastPC = PC;

            if (can_disassemble)
            {
                std::string disasm = EmotionDisasm::disasm_instr(instruction, PC);
                printf("[$%08X] $%08X - %s\n", PC, instruction, disasm.c_str());
                //print_state();
            }

            EmotionInterpreter::interpret(*this, instruction);
            PC += 4;

            //Simulate dual-issue if both instructions are NOPs
            if (!instruction && !read_instr(PC))
                PC += 4;

            if (branch_on)
            {
                if (!delay_slot)
                {
                    //If the PC == LastPC it means we've reversed it to handle COP2 sync, so don't branch yet
                    if (PC != lastPC)
                    {
                        branch_on = false;
                        if (!new_PC || (new_PC & 0x3))
                        {
                            Errors::die("[EE] Jump to invalid address $%08X from $%08X\n", new_PC, PC - 8);
                        }
                        PC = new_PC;
                    }
                }
                else
                    delay_slot--;
            }
        }
    }

    if (cp0->int_enabled())
    {
        if (cp0->cause.int0_pending)
            int0();
        else if (cp0->cause.int1_pending)
            int1();
    }

    cp0->count_up(cycles);

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

void EmotionEngine::clear_interlock()
{
    e->clear_cop2_interlock();
}

bool EmotionEngine::vu0_wait()
{
    //Must interlock as if there is an Mbit it will stall
    e->interlock_cop2_check(true);
    return vu0->is_running();
}

bool EmotionEngine::check_interlock()
{
    if (!vu0->is_running())
        return false;

    return e->interlock_cop2_check(true);
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

uint32_t EmotionEngine::read_instr(uint32_t address)
{
    bool uncached = address & 0x30000000;
    if (!uncached)
    {
        int index = (address >> 6) & 0x7F;
        uint16_t tag = address >> 13;

        EE_ICacheLine* line = &icache[index];
        //Check if there's no entry in icache
        if (!line->valid[0] || line->tag[0] != tag)
        {
            if (!line->valid[1] || line->tag[1] != tag)
            {
                //Load 4 quadwords. This incurs a 10 * 4 penalty.
                //printf("[EE] I$ miss at $%08X\n", address);
                cycles_to_run -= 40;

                //If there's an invalid entry, fill it.
                //The `LFU` bit for the filled row gets flipped.
                if (!line->valid[0])
                {
                    line->valid[0] = true;
                    line->lfu[0] ^= true;
                    line->tag[0] = tag;
                }
                else if (!line->valid[1])
                {
                    line->valid[1] = true;
                    line->lfu[1] ^= true;
                    line->tag[1] = tag;
                }
                else
                {
                    //The row to fill is the XOR of the LFU bits.
                    int row_to_fill = line->lfu[0] ^ line->lfu[1];
                    line->lfu[row_to_fill] ^= true;
                    line->tag[row_to_fill] = tag;
                }
            }
        }
    }
    else
    {
        //Simulate reading from RDRAM
        //The penalty is 10 cycles for all data types, up to a quadword (128 bits).
        //However, the EE loads two instructions at once. Since we only load a word, we divide the cycles in half.
        if ((address & 0x1FFFFFFF) < 0x02000000)
            cycles_to_run -= 5;
        if (address >= 0x30100000 && address <= 0x31FFFFFF)
            address -= 0x10000000;
    }
    return e->read32(address & 0x1FFFFFFF);
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
    if (address & 0x1)
        Errors::die("[EE] Read16 from invalid address $%08X", address);
    if (address >= 0x70000000 && address < 0x70004000)
        return *(uint16_t*)&scratchpad[address & 0x3FFE];
    if (address >= 0x30100000 && address <= 0x31FFFFFF)
        address -= 0x10000000;
    return e->read16(address & 0x1FFFFFFF);
}

uint32_t EmotionEngine::read32(uint32_t address)
{
    if (address & 0x3)
        Errors::die("[EE] Read32 from invalid address $%08X", address);
    if (address >= 0x70000000 && address < 0x70004000)
        return *(uint32_t*)&scratchpad[address & 0x3FFC];
    if (address >= 0x30100000 && address <= 0x31FFFFFF)
        address -= 0x10000000;
    return e->read32(address & 0x1FFFFFFF);
}

uint64_t EmotionEngine::read64(uint32_t address)
{
    if (address & 0x7)
        Errors::die("[EE] Read64 from invalid address $%08X", address);
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
    return e->read128(address & 0x1FFFFFF0);
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
    if (address & 0x1)
        Errors::die("[EE] Write16 to invalid address $%08X: $%04X", address, value);
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
    if (address & 0x3)
        Errors::die("[EE] Write32 to invalid address $%08X: $%08X", address, value);
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
    if (address & 0x7)
        Errors::die("[EE] Write64 to invalid address $%08X: $%08X_%08X", address, value >> 32, value);
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
    e->write128(address & 0x1FFFFFF0, value);
}

void EmotionEngine::jp(uint32_t new_addr)
{
    branch_on = true;
    new_PC = new_addr;
    delay_slot = 1;

#ifdef SKIPMPEG_ON
    //skipmpeg - many thanks to PCSX2 for the code :)
    //We want to search for this pattern:
    //lw reg, 0x40(a0); jr ra; lw v0, 0(reg)
    //The idea is that if the function returns 1, the movie is over.
    if (read32(new_PC + 4) == 0x03E00008)
    {
        uint32_t code = read32(new_PC);
        uint32_t p1 = 0x8c800040;
        uint32_t p2 = 0x8c020000 | (code & 0x1f0000) << 5;
        if ((code & 0xffe0ffff)   != p1) return;
        if (read32(new_PC + 8) != p2) return;

        branch_on = false;
        delay_slot = 0;
        set_gpr<uint64_t>(2, 1); //v0 = 1
    }
#endif
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
    int64_t bark = 0;
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

void EmotionEngine::cfc(int cop_id, int reg, int cop_reg, uint32_t instruction)
{
    int32_t bark = 0;
    switch (cop_id)
    {
        case 1:
            bark = (int32_t)fpu->cfc(cop_reg);
            break;
        case 2:
            int interlock = instruction & 0x1;
            if (interlock)
            {
                if (vu0_wait())
                {
                    set_PC(get_PC() - 4);
                    return;
                }
                clear_interlock();
            }
            if (cop_reg == 29)
            {
                bark = vu0->is_running();
                bark |= vu1->is_running() << 8;
                bark |= vu0->stopped_by_tbit() << 2;
                bark |= vu1->stopped_by_tbit() << 10;
            }
            else
                bark = (int32_t)vu0->cfc(cop_reg);
            break;
    }
    set_gpr<int64_t>(reg, bark);
}

void EmotionEngine::ctc(int cop_id, int reg, int cop_reg, uint32_t instruction)
{
    uint32_t bark = get_gpr<uint32_t>(reg);
    switch (cop_id)
    {
        case 1:
            fpu->ctc(cop_reg, bark);
            break;
        case 2:
            int interlock = instruction & 0x1;
            if (interlock)
            {
                if (check_interlock())
                {
                    set_PC(get_PC() - 4);
                    return;
                }
                clear_interlock();
            }
            if (cop_reg == 31)
                vu1->start_program(bark);
            else
                vu0->ctc(cop_reg, bark);
            break;
    }
}

void EmotionEngine::invalidate_icache_indexed(uint32_t addr)
{
    int index = (addr >> 6) & 0x7F;
    int way = addr & 0x1;
    icache[index].valid[way] = false;
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

    if (cp0->status.bev)
    {
        new_addr |= 0xBFC00000;
        new_addr += 0x200;
    }

    branch_on = false;
    delay_slot = 0;
    PC = new_addr;
    unhalt();
}

void EmotionEngine::syscall_exception()
{
    uint8_t op = read8(PC - 4);
    //if (op != 0x7A)
        //printf("[EE] SYSCALL: %s (id: $%02X) called at $%08X\n", SYSCALL(op), op, PC);

    if (op == 0x7C)
    {
        deci2call(get_gpr<uint32_t>(4), get_gpr<uint32_t>(5));
        return;
    }
    if (op == 0x04)
    {
        //On a real PS2, Exit returns to OSDSYS.
        Errors::die("[EE] Exit syscall called!\n");
    }
    handle_exception(0x8000017C, 0x08);
}

void EmotionEngine::break_exception()
{
    Errors::die("[EE] BREAK opcode called (PC: $%08X)", PC);
}

void EmotionEngine::trap_exception()
{
    Errors::die("[EE] TRAP opcode called (PC: $%08X)", PC);
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
    {
        printf("[EE] Set INT0\n");
        if (cp0->int_enabled())
            int0();
    }
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
    //This hack is used for ISOs.
    if (PC == 0x82000)
        e->fast_boot();

    //BIFC0 speedhack
    if (PC >= 0x81FC0 && PC < 0x81FE0)
    {
        printf("[EE] Entering BIFCO loop\n");
        halt();
    }
    //And this is for ELFs.
    if (PC >= 0x00100000 && PC < 0x00100010)
        e->skip_BIOS();
    PC -= 4;
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

void EmotionEngine::mtps(int reg)
{
    cp0->PCCR = get_gpr<uint32_t>(reg);
}

void EmotionEngine::mtpc(int pc_reg, int reg)
{
    if (pc_reg)
        cp0->PCR1 = get_gpr<uint32_t>(reg);
    else
        cp0->PCR0 = get_gpr<uint32_t>(reg);
}

void EmotionEngine::mfps(int reg)
{
    set_gpr<int64_t>(reg, (int32_t)cp0->PCCR);
}

void EmotionEngine::mfpc(int pc_reg, int reg)
{
    int32_t pcr = 0;
    if (pc_reg)
        pcr = (int32_t)cp0->PCR1;
    else
        pcr = (int32_t)cp0->PCR0;
    printf("[EE] MFPC %d: $%08X\n", pc_reg, pcr);
    set_gpr<int64_t>(reg, pcr);
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

void EmotionEngine::cop2_bc2(int32_t offset, bool test_true, bool likely)
{
    bool passed = false;
    if (test_true)
        passed = vu1->is_running();
    else
        passed = !vu1->is_running();

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

void EmotionEngine::cop2_updatevu0()
{
    if (vu0->is_running())
    {
        uint64_t current_count = (cycle_count - cycles_to_run) >> 1;
        //if (vu0->get_cycle_count() < current_count)
            vu0->run(8/*current_count - vu0->get_cycle_count()*/);
    }
}

void EmotionEngine::cop2_special(EmotionEngine &cpu, uint32_t instruction)
{
    EmotionInterpreter::cop2_special(cpu, *vu0, instruction);
}
