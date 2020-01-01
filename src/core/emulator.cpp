#include <cfenv>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <sstream>
#include "emulator.hpp"
#include "errors.hpp"

#include "ee/vu_jit.hpp"
#include "ee/ee_jit.hpp"

#define CYCLES_PER_FRAME 4900000
#define VBLANK_START_CYCLES CYCLES_PER_FRAME * 0.75

//These constants are used for the fast boot hack for .isos
#define EELOAD_START 0x82000
#define EELOAD_SIZE 0x20000

Emulator::Emulator() :
    cdvd(this, &iop_dma),
    cp0(&dmac),
    cpu(&cp0, &fpu, this, &vu0, &vu1),
    dmac(&cpu, this, &gif, &ipu, &sif, &vif0, &vif1, &vu0, &vu1),
    gif(&gs, &dmac),
    gs(&intc),
    iop(this),
    iop_dma(this, &cdvd, &sif, &sio2, &spu, &spu2),
    iop_timers(this),
    intc(this, &cpu),
    ipu(&intc, &dmac),
    timers(&intc),
    sio2(this, &pad, &memcard),
    spu(1, this, &iop_dma),
    spu2(2, this, &iop_dma),
    vif0(nullptr, &vu0, &intc, &dmac, 0),
    vif1(&gif, &vu1, &intc, &dmac, 1),
    vu0(0, this, &intc, &cpu, &vu1),
    vu1(1, this, &intc, &cpu, &vu0),
    sif(&iop_dma, &dmac)
{
    BIOS = nullptr;
    RDRAM = nullptr;
    IOP_RAM = nullptr;
    SPU_RAM = nullptr;
    ELF_file = nullptr;
    ELF_size = 0;
    gsdump_single_frame = false;
    ee_log.open("ee_log.txt", std::ios::out);
    set_ee_mode(CPU_MODE::DONT_CARE);
    set_vu1_mode(CPU_MODE::DONT_CARE);
}

Emulator::~Emulator()
{
    if (ee_log.is_open())
        ee_log.close();
    delete[] RDRAM;
    delete[] IOP_RAM;
    delete[] BIOS;
    delete[] SPU_RAM;
    delete[] ELF_file;
}

void Emulator::run()
{
    gs.start_frame();
    VBLANK_sent = false;
    const int originalRounding = fegetround();
    fesetround(FE_TOWARDZERO);
    if (save_requested)
        save_state(save_state_path.c_str());
    if (load_requested)
        load_state(save_state_path.c_str());
    if (gsdump_requested)
    {
        gsdump_requested = false;
        gs.send_dump_request();
        gsdump_running = !gsdump_running;
    }
    else if (gsdump_single_frame)
    {
        gs.send_dump_request();
        if (gsdump_running)
        {
            gsdump_running = false;
            gsdump_single_frame = false;
        }
        else
        {
            gsdump_running = true;
        }
    }

    frame_ended = false;

    add_ee_event(VBLANK_START, &Emulator::vblank_start, VBLANK_START_CYCLES);
    add_ee_event(VBLANK_END, &Emulator::vblank_end, CYCLES_PER_FRAME);
    
    while (!frame_ended)
    {
        int ee_cycles = scheduler.calculate_run_cycles();
        int bus_cycles = scheduler.get_bus_run_cycles();
        int iop_cycles = scheduler.get_iop_run_cycles();
        scheduler.update_cycle_counts();

        cpu.run(ee_cycles);
        dmac.run(bus_cycles);
        timers.run(bus_cycles);
        ipu.run();
        vif0.update(bus_cycles);
        vif1.update(bus_cycles);
        gif.run(bus_cycles);
        vu0.run(bus_cycles);
        vu1_run_func(vu1, bus_cycles);

        iop_timers.run(iop_cycles);
        iop_dma.run(iop_cycles);
        iop.run(iop_cycles);
        iop.interrupt_check(IOP_I_CTRL && (IOP_I_MASK & IOP_I_STAT));

        scheduler.process_events(this);
    }
    fesetround(originalRounding);
}

void Emulator::reset()
{
    save_requested = false;
    load_requested = false;
    gsdump_requested = false;
    iop_i_ctrl_delay = 0;
    ee_stdout = "";
    frames = 0;
    skip_BIOS_hack = NONE;
    if (!RDRAM)
        RDRAM = new uint8_t[1024 * 1024 * 32];
    if (!IOP_RAM)
        IOP_RAM = new uint8_t[1024 * 1024 * 2];
    if (!BIOS)
        BIOS = new uint8_t[1024 * 1024 * 4];
    if (!SPU_RAM)
        SPU_RAM = new uint8_t[1024 * 1024 * 2];

    cdvd.reset();
    cp0.reset();
    cp0.init_mem_pointers(RDRAM, BIOS, (uint8_t*)&scratchpad);
    cpu.reset();
    cpu.init_tlb();
    dmac.reset(RDRAM, (uint8_t*)&scratchpad);
    fpu.reset();
    gs.reset();
    gif.reset();
    iop.reset();
    iop_dma.reset(IOP_RAM);
    iop_timers.reset();
    intc.reset();
    ipu.reset();
    pad.reset();
    scheduler.reset();
    sif.reset();
    sio2.reset();
    spu.reset(SPU_RAM);
    spu2.reset(SPU_RAM);
    timers.reset();
    vif0.reset();
    vif1.reset();
    vu0.reset();
    vu1.reset();
    VU_JIT::reset();
    EE_JIT::reset(true);

    MCH_DRD = 0;
    MCH_RICM = 0;
    rdram_sdevid = 0;
    IOP_I_STAT = 0;
    IOP_I_MASK = 0;
    IOP_I_CTRL = 0;
    IOP_POST = 0;
    clear_cop2_interlock();

    // HLE method to zero out IOP memory
    memset(IOP_RAM, 0, 0x00200000);

    iop_scratchpad_start = 0x1F800000;

    add_iop_event(SPU_SAMPLE, &Emulator::gen_sound_sample, 768);
}

void Emulator::print_state()
{
    printf("---EE STATE\n");
    cpu.print_state();
    printf("---IOP STATE\n");
    iop.print_state();
}

void Emulator::vblank_start()
{
    VBLANK_sent = true;
    gs.set_VBLANK(true);
    timers.gate(true, true);
    cdvd.vsync();
    //cpu.set_disassembly(frames >= 223 && frames < 225);
    printf("VSYNC FRAMES: %d\n", frames);
    gs.assert_VSYNC();
    iop_request_IRQ(0);
}

void Emulator::vblank_end()
{
    //VBLANK end
    iop_request_IRQ(11);
    gs.set_VBLANK(false);
    timers.gate(true, false);
    frame_ended = true;
    frames++;
    gs.render_CRT();
}

void Emulator::cdvd_event()
{
    cdvd.handle_N_command();
}

void Emulator::gen_sound_sample()
{
    spu.gen_sample();
    spu2.gen_sample();
    add_iop_event(SPU_SAMPLE, &Emulator::gen_sound_sample, 768);
}

void Emulator::ee_irq_check()
{
    //printf("[EE] INT0 check\n");
    intc.int0_check();
}

void Emulator::press_button(PAD_BUTTON button)
{
    pad.press_button(button);
}

void Emulator::release_button(PAD_BUTTON button)
{
    pad.release_button(button);
}

void Emulator::update_joystick(JOYSTICK joystick, JOYSTICK_AXIS axis, uint8_t val)
{
    pad.update_joystick(joystick, axis, val);
}

uint32_t* Emulator::get_framebuffer()
{
    //This function should only be called upon ending a frame; return nullptr otherwise
    if (!frame_ended)
        return nullptr;
    return gs.get_framebuffer();
}

void Emulator::get_resolution(int &w, int &h)
{
    gs.get_resolution(w, h);
}

void Emulator::get_inner_resolution(int &w, int &h)
{
    gs.get_inner_resolution(w, h);
}

bool Emulator::skip_BIOS()
{
    if (skip_BIOS_hack == LOAD_ELF)
    {
        execute_ELF();
        skip_BIOS_hack = NONE;
        return true;
    }
    return false;
}

void Emulator::fast_boot()
{
    if (skip_BIOS_hack == LOAD_DISC)
    {
        //First we need to determine the name of the game executable
        //This is done by finding SYSTEM.CNF on the game disc, then getting the name of the executable it points to.
        uint32_t system_cnf_size;
        char* system_cnf = (char*)cdvd.read_file("SYSTEM.CNF;1", system_cnf_size);
        if (!system_cnf)
        {
            Errors::die("[Emulator] Failed to load SYSTEM.CNF!\n");
        }
        std::string exec_name = "";
        //Search for cdrom0:
        int pos = 0;
        while (strncmp("cdrom0:", system_cnf + pos, 7))
            pos++;

        printf("[Emulator] Found 'cdrom0:'\n");
        pos += 8;

        //Search for end of file name
        while (system_cnf[pos] != ';')
        {
            exec_name += system_cnf[pos];
            pos++;
        }

        delete[] system_cnf;

        std::string path = "cdrom0:\\";
        path += exec_name + ";1";

        //Next we need to find the string "rom0:OSDSYS"
        for (uint32_t str = EELOAD_START; str < EELOAD_START + EELOAD_SIZE; str += 8)
        {
            if (!strcmp((char*)&RDRAM[str], "rom0:OSDSYS"))
            {
                printf("OSDSYS string found at $%08X\n", str);
                strcpy((char*)&RDRAM[str], path.c_str());
            }
        }

        skip_BIOS_hack = NONE;
    }
}

void Emulator::set_skip_BIOS_hack(SKIP_HACK type)
{
    skip_BIOS_hack = type;
}

void Emulator::set_ee_mode(CPU_MODE mode)
{
    switch (mode)
    {
        case CPU_MODE::INTERPRETER:
            cpu.set_run_func(&EmotionEngine::run_interpreter);
            break;
        case CPU_MODE::JIT:
        default:
            cpu.set_run_func(&EmotionEngine::run_jit);
            break;
    }
}

void Emulator::set_vu1_mode(CPU_MODE mode)
{
    switch (mode)
    {
        case CPU_MODE::INTERPRETER:
            vu1_run_func = &VectorUnit::run;
            break;
        case CPU_MODE::JIT:
        default:
            vu1_run_func = &VectorUnit::run_jit;
            break;
    }
}

void Emulator::load_BIOS(const uint8_t *BIOS_file)
{
    if (!BIOS)
        BIOS = new uint8_t[1024 * 1024 * 4];

    memcpy(BIOS, BIOS_file, 1024 * 1024 * 4);
}

void Emulator::load_ELF(const uint8_t *ELF, uint32_t size)
{
    if (ELF[0] != 0x7F || ELF[1] != 'E' || ELF[2] != 'L' || ELF[3] != 'F')
    {
        printf("Invalid elf\n");
        return;
    }
    printf("Valid elf\n");
    delete[] ELF_file;
    ELF_file = new uint8_t[size];
    ELF_size = size;
    memcpy(ELF_file, ELF, size);
}

bool Emulator::load_CDVD(const char *name, CDVD_CONTAINER type)
{
    return cdvd.load_disc(name, type);
}

void Emulator::execute_ELF()
{
    if (!ELF_file)
    {
        Errors::die("[Emulator] ELF not loaded!\n");
    }
    printf("[Emulator] Loading ELF into memory...\n");
    uint32_t e_entry = *(uint32_t*)&ELF_file[0x18];
    uint32_t e_phoff = *(uint32_t*)&ELF_file[0x1C];
    uint32_t e_shoff = *(uint32_t*)&ELF_file[0x20];
    uint16_t e_phnum = *(uint16_t*)&ELF_file[0x2C];
    uint16_t e_shnum = *(uint16_t*)&ELF_file[0x30];
    uint16_t e_shstrndx = *(uint16_t*)&ELF_file[0x32];

    printf("Entry: $%08X\n", e_entry);
    printf("Program header start: $%08X\n", e_phoff);
    printf("Section header start: $%08X\n", e_shoff);
    printf("Program header entries: %d\n", e_phnum);
    printf("Section header entries: %d\n", e_shnum);
    printf("Section header names index: %d\n", e_shstrndx);

    for (unsigned int i = e_phoff; i < e_phoff + (e_phnum * 0x20); i += 0x20)
    {
        uint32_t p_offset = *(uint32_t*)&ELF_file[i + 0x4];
        uint32_t p_paddr = *(uint32_t*)&ELF_file[i + 0xC];
        uint32_t p_filesz = *(uint32_t*)&ELF_file[i + 0x10];
        uint32_t p_memsz = *(uint32_t*)&ELF_file[i + 0x14];
        printf("\nProgram header\n");
        printf("p_type: $%08X\n", *(uint32_t*)&ELF_file[i]);
        printf("p_offset: $%08X\n", p_offset);
        printf("p_vaddr: $%08X\n", *(uint32_t*)&ELF_file[i + 0x8]);
        printf("p_paddr: $%08X\n", p_paddr);
        printf("p_filesz: $%08X\n", p_filesz);
        printf("p_memsz: $%08X\n", p_memsz);

        int mem_w = p_paddr;
        for (unsigned int file_w = p_offset; file_w < (p_offset + p_filesz); file_w += 4)
        {
            uint32_t word = *(uint32_t*)&ELF_file[file_w];
            *(uint32_t*)&RDRAM[mem_w] = word;
            mem_w += 4;
        }
    }
    cpu.set_PC(e_entry);
}

void Emulator::clear_cop2_interlock()
{
    cop2_interlock = false;
    vu_interlock = false;
}

bool Emulator::check_cop2_interlock()
{
   return vu_interlock;
}

bool Emulator::interlock_cop2_check(bool isCOP2)
{
    if (isCOP2)
    {
        cop2_interlock = true;
        //If the interlock is set on COP2 and not the VU (yet) then wait
        if (!vu_interlock)
            return true;
        else 
            return false;
    }
    else
    {
        vu_interlock = true;
        //If the interlock is set on VU0 and not COP2 (yet) then wait
        if (!cop2_interlock)
            return true;
        else
            return false;
    }
}

uint8_t Emulator::read8(uint32_t address)
{
    if (address >= 0x1C000000 && address < 0x1C200000)
        return IOP_RAM[address & 0x1FFFFF];
    if (address >= 0x10008000 && address < 0x1000F000)
        return dmac.read8(address);
    switch (address)
    {
        case 0x1F40200F:
            return cdvd.read_disc_type();
        case 0x1F402017:
            return cdvd.read_S_status();
        case 0x1F402018:
            return cdvd.read_S_data();
    }
    printf("Unrecognized read8 at physical addr $%08X\n", address);
    return 0;
}

uint16_t Emulator::read16(uint32_t address)
{
    if (address >= 0x10000000 && address < 0x10002000)
        return (uint16_t)timers.read32(address);
    if (address >= 0x10008000 && address < 0x1000F000)
        return dmac.read16(address);
    if (address >= 0x1C000000 && address < 0x1C200000)
        return *(uint16_t*)&IOP_RAM[address & 0x1FFFFF];
    switch (address)
    {
        case 0x10003C30:
            return vif1.get_mark() & 0xFFFF;
        case 0x1A000006:
            return 1;
    }
    printf("Unrecognized read16 at physical addr $%08X\n", address);
    return 0;
}

uint32_t Emulator::read32(uint32_t address)
{
    if (address >= 0x10000000 && address < 0x10002000)
        return timers.read32(address);
    if ((address & (0xFF000000)) == 0x12000000)
        return gs.read32_privileged(address);
    if (address >= 0x10008000 && address < 0x1000F000)
        return dmac.read32(address);
    if (address >= 0x1C000000 && address < 0x1C200000)
        return *(uint32_t*)&IOP_RAM[address & 0x1FFFFF];
    if (address >= 0x11000000 && address < 0x11004000)
        return vu0.read_instr<uint32_t>(address);
    if (address >= 0x11004000 && address < 0x11008000)
        return vu0.read_mem<uint32_t>(address);
    if (address >= 0x11008000 && address < 0x1100C000)
        return vu1.read_instr<uint32_t>(address);
    if (address >= 0x1100C000 && address < 0x11010000)
        return vu1.read_mem<uint32_t>(address);
    switch (address)
    {
        case 0x10002000:
            return ipu.read_command();
        case 0x10002010:
            return ipu.read_control();
        case 0x10002020:
            return ipu.read_BP();
        case 0x10002030:
            return ipu.read_top();
        case 0x10003020:
            return gif.read_STAT();
        case 0x10003800:
            return vif0.get_stat();
        case 0x10003850:
            return vif0.get_mode();
        case 0x10003900:
        case 0x10003910:
        case 0x10003920:
        case 0x10003930:
            return vif0.get_row(address);
        case 0x10003C00:
            return vif1.get_stat();
        case 0x10003C20:
            return vif1.get_err();
        case 0x10003C30:
            return vif1.get_mark();
        case 0x10003C50:
            return vif1.get_mode();
        case 0x10003C80:
            return vif1.get_code();
        case 0x10003CE0:
            return vif1.get_top();
        case 0x10003D00:
        case 0x10003D10:
        case 0x10003D20:
        case 0x10003D30:
            return vif1.get_row(address);
        case 0x1000F000:
            //printf("\nRead32 INTC_STAT: $%08X", intc.read_stat());
            return intc.read_stat();
        case 0x1000F010:
            printf("Read32 INTC_MASK: $%08X\n", intc.read_mask());
            return intc.read_mask();
        case 0x1000F130:
            return 0;
        case 0x1000F200:
            return sif.get_mscom();
        case 0x1000F210:
            return sif.get_smcom();
        case 0x1000F220:
            return sif.get_msflag();
        case 0x1000F230:
            return sif.get_smflag();
        case 0x1000F240:
            printf("[EE] Read BD4: $%08X\n", sif.get_control() | 0xF0000102);
            return sif.get_control() | 0xF0000102;
        case 0x1000F430:
            //printf("Read from MCH_RICM\n");
            return 0;
        case 0x1000F440:
            //printf("Read from MCH_DRD\n");
            if (!((MCH_RICM >> 6) & 0xF))
            {
                switch ((MCH_RICM >> 16) & 0xFFF)
                {
                    case 0x21:
                        //printf("Init\n");
                        if (rdram_sdevid < 2)
                        {
                            rdram_sdevid++;
                            return 0x1F;
                        }
                        return 0;
                    case 0x23:
                        //printf("ConfigA\n");
                        return 0x0D0D;
                    case 0x24:
                        //printf("ConfigB\n");
                        return 0x0090;
                    case 0x40:
                        //printf("Devid\n");
                        return MCH_RICM & 0x1F;
                }
            }
            return 0;
        case 0x1000F520:
            return dmac.read_master_disable();
    }
    printf("Unrecognized read32 at physical addr $%08X\n", address);
    return 0;
}

uint64_t Emulator::read64(uint32_t address)
{
    if (address >= 0x10000000 && address < 0x10002000)
        return timers.read32(address);
    if (address >= 0x10008000 && address < 0x1000F000)
        return dmac.read32(address);
    if ((address & (0xFF000000)) == 0x12000000)
        return gs.read64_privileged(address);
    if (address >= 0x1C000000 && address < 0x1C200000)
        return *(uint64_t*)&IOP_RAM[address & 0x1FFFFF];
    switch (address)
    {
        case 0x10002000:
            return ipu.read_command();
        case 0x10002010:
            return ipu.read_control();
        case 0x10002020:
            return ipu.read_BP();
        case 0x10002030:
            return ipu.read_top();
    }
    printf("Unrecognized read64 at physical addr $%08X\n", address);
    return 0;
}

uint128_t Emulator::read128(uint32_t address)
{
    if (address >= 0x11000000 && address < 0x11004000)
        return vu0.read_instr<uint128_t>(address);
    if (address >= 0x11004000 && address < 0x11008000)
        return vu0.read_mem<uint128_t>(address);
    if (address >= 0x11008000 && address < 0x1100C000)
        return vu1.read_instr<uint128_t>(address);
    if (address >= 0x1100C000 && address < 0x11010000)
        return vu1.read_mem<uint128_t>(address);

    printf("Unrecognized read128 at physical addr $%08X\n", address);
    return uint128_t::from_u32(0);
}

void Emulator::write8(uint32_t address, uint8_t value)
{
    if (address >= 0x10008000 && address < 0x1000F000)
    {
        dmac.write8(address, value);
        return;
    }
    if (address >= 0x1C000000 && address < 0x1C200000)
    {
        IOP_RAM[address & 0x1FFFFF] = value;
        return;
    }
    switch (address)
    {
        case 0x1000F180:
            ee_log << value;
            ee_log.flush();
            return;
    }
    Errors::print_warning("Unrecognized write8 at physical addr $%08X of $%02X\n", address, value);
}

void Emulator::write16(uint32_t address, uint16_t value)
{
    if (address >= 0x10008000 && address < 0x1000F000)
    {
        dmac.write16(address, value);
        return;
    }
    if (address >= 0x1C000000 && address < 0x1C200000)
    {
        *(uint16_t*)&IOP_RAM[address & 0x1FFFFF] = value;
        return;
    }
    if (address >= 0x1A000000 && address < 0x1FC00000)
    {
        printf("[EE] Unrecognized write16 to IOP addr $%08X of $%04X\n", address, value);
        return;
    }
    printf("Unrecognized write16 at physical addr $%08X of $%04X\n", address, value);
}

void Emulator::write32(uint32_t address, uint32_t value)
{
    if (address >= 0x1C000000 && address < 0x1C200000)
    {
        *(uint32_t*)&IOP_RAM[address & 0x1FFFFF] = value;
        return;
    }
    if (address >= 0x10000000 && address < 0x10002000)
    {
        timers.write32(address, value);
        return;
    }
    if ((address & (0xFF000000)) == 0x12000000)
    {
        gs.write32_privileged(address, value);
        gs.wake_gs_thread();
        return;
    }
    if (address >= 0x10008000 && address < 0x1000F000)
    {
        dmac.write32(address, value);
        return;
    }
    if (address >= 0x1A000000 && address < 0x1FC00000)
    {
        printf("[EE] Unrecognized write32 to IOP addr $%08X of $%08X\n", address, value);
        return;
    }
    if (address >= 0x11000000 && address < 0x11004000)
    {
        vu0.write_instr<uint32_t>(address, value);
        return;
    }
    if (address >= 0x11004000 && address < 0x11008000)
    {
        vu0.write_mem<uint32_t>(address, value);
        return;
    }
    if (address >= 0x11008000 && address < 0x1100C000)
    {
        vu1.write_instr<uint32_t>(address, value);
        return;
    }
    if (address >= 0x1100C000 && address < 0x11010000)
    {
        vu1.write_mem<uint32_t>(address, value);
        return;
    }

    switch (address)
    {
        case 0x10002000:
            ipu.write_command(value);
            return;
        case 0x10002010:
            ipu.write_control(value);
            return;
        case 0x10003010:
            gif.write_MODE(value);
            return;
        case 0x10003810:
            vif0.set_fbrst(value);
            return;
        case 0x10003820:
            vif0.set_err(value);
            return;
        case 0x10003830:
            vif0.set_mark(value);
            return;
        case 0x10003c00:
            vif1.set_stat(value);
            return;
        case 0x10003C10:
            vif1.set_fbrst(value);
            return;
        case 0x10003C20:
            vif1.set_err(value);
            return;
        case 0x10003C30:
            vif1.set_mark(value);
            return;
        case 0x1000F000:
            printf("Write32 INTC_STAT: $%08X\n", value);
            intc.write_stat(value);
            return;
        case 0x1000F010:
            printf("Write32 INTC_MASK: $%08X\n", value);
            intc.write_mask(value);
            return;
        case 0x1000F200:
            sif.set_mscom(value);
            return;
        case 0x1000F210:
            return;
        case 0x1000F220:
            printf("[EE] Write32 msflag: $%08X\n", value);
            sif.set_msflag(value);
            return;
        case 0x1000F230:
            printf("[EE] Write32 smflag: $%08X\n", value);
            sif.reset_smflag(value);
            return;
        case 0x1000F240:
            printf("[EE] Write BD4: $%08X\n", value);
            sif.set_control_EE(value);
            return;
        case 0x1000F430:
            //printf("Write to MCH_RICM: $%08X\n", value);
            if ((((value >> 16) & 0xFFF) == 0x21) && (((value >> 6) & 0xF) == 1) &&
                    (((MCH_DRD >> 7) & 1) == 0))
                rdram_sdevid = 0;
            MCH_RICM = value & ~0x80000000;
            return;
        case 0x1000F440:
            //printf("Write to MCH_DRD: $%08X\n", value);
            MCH_DRD = value;
            return;
        case 0x1000F590:
            dmac.write_master_disable(value);
            return;
    }
    Errors::print_warning("Unrecognized write32 at physical addr $%08X of $%08X\n", address, value);
}

void Emulator::write64(uint32_t address, uint64_t value)
{
    if (address >= 0x1C000000 && address < 0x1C200000)
    {
        *(uint64_t*)&IOP_RAM[address & 0x1FFFFF] = value;
        return;
    }
    if (address >= 0x10000000 && address < 0x10002000)
    {
        timers.write32(address, value);
        return;
    }
    if (address >= 0x10008000 && address < 0x1000F000)
    {
        dmac.write32(address, value);
        return;
    }
    if ((address & (0xFF000000)) == 0x12000000)
    {
        gs.write64_privileged(address, value);
        gs.wake_gs_thread();
        return;
    }
    if (address >= 0x11000000 && address < 0x11004000)
    {
        vu0.write_instr<uint64_t>(address, value);
        return;
    }
    if (address >= 0x11004000 && address < 0x11008000)
    {
        vu0.write_mem<uint64_t>(address, value);
        return;
    }
    if (address >= 0x11008000 && address < 0x1100C000)
    {
        vu1.write_instr<uint64_t>(address, value);
        return;
    }
    if (address >= 0x1100C000 && address < 0x11010000)
    {
        vu1.write_mem<uint64_t>(address, value);
        return;
    }
    Errors::print_warning("Unrecognized write64 at physical addr $%08X of $%08X_%08X\n", address, value >> 32, value & 0xFFFFFFFF);
}

void Emulator::write128(uint32_t address, uint128_t value)
{
    if (address >= 0x11000000 && address < 0x11010000)
    {
        if (address < 0x11004000)
        {
            vu0.write_instr<uint128_t>(address & 0xFFF, value);
            return;
        }
        if (address < 0x11008000)
        {
            vu0.write_mem<uint128_t>(address & 0xFFF, value);
            return;
        }
        if (address < 0x1100C000)
        {
            vu1.write_instr<uint128_t>(address, value);
            return;
        }
        vu1.write_mem<uint128_t>(address, value);
        return;
    }
    switch (address)
    {
        case 0x10004000:
            vif0.feed_DMA(value);
            return;
        case 0x10005000:
            vif1.feed_DMA(value);
            return;
        case 0x10006000:
            gif.send_PATH3(value);
            return;
        case 0x10007010:
            ipu.write_FIFO(value);
            return;
    }
    Errors::print_warning("Unrecognized write128 at physical addr $%08X of $%08X_%08X_%08X_%08X\n", address,
           value._u32[3], value._u32[2], value._u32[1], value._u32[0]);
}

void Emulator::ee_kputs(uint32_t param)
{
    if (param > 1024 * 1024 * 32)
        return;
    param = *(uint32_t*)&RDRAM[param];
    printf("Param: $%08X\n", param);
    char c;
    do
    {
        c = RDRAM[param & 0x1FFFFFF];
        ee_log << c;
        param++;
    } while (c);
    ee_log.flush();
}

void Emulator::ee_deci2send(uint32_t addr, int len)
{
    if(len > 0x10000)
    {
        Errors::die("Tried to deci2send %d bytes!\n", len);
    }

    while (len > 0)
    {
        char c = RDRAM[addr & 0x1FFFFFF];
        ee_log << c;
        addr++;
        len--;
    }
    ee_log.flush();
}

uint8_t Emulator::iop_read8(uint32_t address)
{
    if (address < 0x00200000)
    {
        //printf("[IOP] Read8 from $%08X: $%02X\n", address, IOP_RAM[address]);
        return IOP_RAM[address];
    }
    if (address >= 0x1FC00000 && address < 0x20000000)
        return BIOS[address & 0x3FFFFF];
    switch (address)
    {
        case 0x1F402004:
            return cdvd.read_N_command();
        case 0x1F402005:
            return cdvd.read_N_status();
        case 0x1F402008:
            return cdvd.read_ISTAT();
        case 0x1F40200A:
            return cdvd.read_drive_status();
        case 0x1F40200F:
            return cdvd.read_disc_type();
        case 0x1F402013:
            return 4;
        case 0x1F402016:
            return cdvd.read_S_command();
        case 0x1F402017:
            return cdvd.read_S_status();
        case 0x1F402018:
            return cdvd.read_S_data();
        case 0x1F402020:
        case 0x1F402021:
        case 0x1F402022:
        case 0x1F402023:
        case 0x1F402024:
            return cdvd.read_cdkey(address - 0x1F402020);
        case 0x1F402028:
        case 0x1F402029:
        case 0x1F40202A:
        case 0x1F40202B:
        case 0x1F40202C:
            return cdvd.read_cdkey(address - 0x1F402023);
        case 0x1F402030:
        case 0x1F402031:
        case 0x1F402032:
        case 0x1F402033:
        case 0x1F402034:
            return cdvd.read_cdkey(address - 0x1F402026);
        case 0x1F402038:
            return cdvd.read_cdkey(15);
        case 0x1F808264:
            return sio2.read_serial();
        case 0x1FA00000:
            return IOP_POST;
    }
    if (address >= iop_scratchpad_start && address < iop_scratchpad_start + 0x400)
        return iop_scratchpad[address & 0x3FF];
    printf("Unrecognized IOP read8 from physical addr $%08X\n", address);
    return 0;
}

uint16_t Emulator::iop_read16(uint32_t address)
{
    if (address < 0x00200000)
        return *(uint16_t*)&IOP_RAM[address];
    if (address >= 0x1FC00000 && address < 0x20000000)
        return *(uint16_t*)&BIOS[address & 0x3FFFFF];
    if (address >= 0x1F900000 && address < 0x1F900400)
        return spu.read16(address);
    if (address >= 0x1F900400 && address < 0x1F900800)
        return spu2.read16(address);
    switch (address)
    {
        case 0x1F801100:
            return iop_timers.read_counter(0);
        case 0x1F801104:
            return iop_timers.read_control(0);
        case 0x1F801108:
            return iop_timers.read_target(0);
        case 0x1F801110:
            return iop_timers.read_counter(1);
        case 0x1F801114:
            return iop_timers.read_control(1);
        case 0x1F801118:
            return iop_timers.read_target(1);
        case 0x1F801120:
            return iop_timers.read_counter(2);
        case 0x1F801124:
            return iop_timers.read_control(2);
        case 0x1F801128:
            return iop_timers.read_target(2);
        case 0x1F801480:
            return iop_timers.read_counter(3) & 0xFFFF;
        case 0x1F801482:
            return iop_timers.read_counter(3) >> 16;
        case 0x1F801484:
            return iop_timers.read_control(3);
        case 0x1F801488:
            return iop_timers.read_target(3) & 0xFFFF;
        case 0x1F80148A:
            return iop_timers.read_target(3) >> 16;
        case 0x1F801490:
            return iop_timers.read_counter(4) & 0xFFFF;
        case 0x1F801492:
            return iop_timers.read_counter(4) >> 16;
        case 0x1F801494:
            return iop_timers.read_control(4);
        case 0x1F801498:
            return iop_timers.read_target(4) & 0xFFFF;
        case 0x1F80149A:
            return iop_timers.read_target(4) >> 16;
        case 0x1F8014A0:
            return iop_timers.read_counter(5) & 0xFFFF;
        case 0x1F8014A2:
            return iop_timers.read_counter(5) >> 16;
        case 0x1F8014A4:
            return iop_timers.read_control(5);
        case 0x1F8014A8:
            return iop_timers.read_target(5) & 0xFFFF;
        case 0x1F8014AA:
            return iop_timers.read_target(5) >> 16;
    }
    if (address >= iop_scratchpad_start && address < iop_scratchpad_start + 0x400)
        return *(uint16_t*)&iop_scratchpad[address & 0x3FF];
    printf("Unrecognized IOP read16 from physical addr $%08X\n", address);
    return 0;
}

uint32_t Emulator::iop_read32(uint32_t address)
{
    if (address < 0x00200000)
        return *(uint32_t*)&IOP_RAM[address];
    if (address >= 0x1FC00000 && address < 0x20000000)
        return *(uint32_t*)&BIOS[address & 0x3FFFFF];
    switch (address)
    {
        case 0x1D000000:
            return sif.get_mscom();
        case 0x1D000010:
            return sif.get_smcom();
        case 0x1D000020:
            return sif.get_msflag();
        case 0x1D000030:
            return sif.get_smflag();
        case 0x1D000040:
            printf("[IOP] Read BD4: $%08X\n", sif.get_control() | 0xF0000002);
            return sif.get_control() | 0xF0000002;
        case 0x1F801070:
            return IOP_I_STAT;
        case 0x1F801074:
            return IOP_I_MASK;
        case 0x1F801078:
        {
            //I_CTRL is reset when read
            uint32_t value = IOP_I_CTRL;
            IOP_I_CTRL = 0;
            return value;
        }
        case 0x1F8010B0:
            return iop_dma.get_chan_addr(3);
        case 0x1F8010B8:
            return iop_dma.get_chan_control(3);
        case 0x1F8010C0:
            return iop_dma.get_chan_addr(4);
        case 0x1F8010C8:
            return iop_dma.get_chan_control(4);
        case 0x1F8010F0:
            return iop_dma.get_DPCR();
        case 0x1F8010F4:
            return iop_dma.get_DICR();
        case 0x1F801100:
            return iop_timers.read_counter(0);
        case 0x1F801104:
            return iop_timers.read_control(0);
        case 0x1F801108:
            return iop_timers.read_target(0);
        case 0x1F801110:
            return iop_timers.read_counter(1);
        case 0x1F801114:
            return iop_timers.read_control(1);
        case 0x1F801118:
            return iop_timers.read_target(1);
        case 0x1F801120:
            return iop_timers.read_counter(2);
        case 0x1F801124:
            return iop_timers.read_control(2);
        case 0x1F801128:
            return iop_timers.read_target(2);
        case 0x1F801450:
            return 0;
        case 0x1F801480:
            return iop_timers.read_counter(3);
        case 0x1F801484:            
            return iop_timers.read_control(3);
        case 0x1F801488:            
            return iop_timers.read_target(3);
        case 0x1F801490:
            return iop_timers.read_counter(4);
        case 0x1F801494:
            return iop_timers.read_control(4);
        case 0x1F801498:
            return iop_timers.read_target(4);
        case 0x1F8014A0:
            return iop_timers.read_counter(5);
        case 0x1F8014A4:
            return iop_timers.read_control(5);
        case 0x1F8014A8:
            return iop_timers.read_target(5);
        case 0x1F801500:
            return iop_dma.get_chan_addr(8);
        case 0x1F801508:
            return iop_dma.get_chan_control(8);
        case 0x1F801528:
            return iop_dma.get_chan_control(10);
        case 0x1F801548:
            return iop_dma.get_chan_control(12);
        case 0x1F801558:
            return iop_dma.get_chan_control(13);
        case 0x1F801570:
            return iop_dma.get_DPCR2();
        case 0x1F801574:
            return iop_dma.get_DICR2();
        case 0x1F801578:
            return 0; //No clue
        case 0x1F808268:
            return sio2.get_control();
        case 0x1F80826C:
            return sio2.get_RECV1();
        case 0x1F808270:
            return sio2.get_RECV2();
        case 0x1F808274:
            return sio2.get_RECV3();
        case 0x1F808410:
            return 8; // Some sort of FireWire thing
        case 0xFFFE0130: //Cache control?
            return 0;
    }
    if (address >= iop_scratchpad_start && address < iop_scratchpad_start + 0x400)
        return *(uint32_t*)&iop_scratchpad[address & 0x3FF];
    Errors::print_warning("Unrecognized IOP read32 from physical addr $%08X\n", address);
    return 0;
}

void Emulator::iop_write8(uint32_t address, uint8_t value)
{
    if (address < 0x00200000)
    {
        //printf("[IOP] Write to $%08X of $%02X\n", address, value);
        IOP_RAM[address] = value;
        return;
    }
    switch (address)
    {
        case 0x1F402004:
            cdvd.send_N_command(value);
            return;
        case 0x1F402005:
            cdvd.write_N_data(value);
            return;
        case 0x1F402006:
            printf("[CDVD] Write to mode: $%02X\n", value);
            return;
        case 0x1F402007:
            cdvd.write_BREAK();
            return;
        case 0x1F402008:
            cdvd.write_ISTAT(value);
            return;
        case 0x1F402016:
            cdvd.send_S_command(value);
            return;
        case 0x1F402017:
            cdvd.write_S_data(value);
            return;
        //POST2?
        case 0x1F802070:
            return;
        case 0x1F808260:
            sio2.write_serial(value);
            return;
        case 0x1FA00000:
            //Register intended to be displayed on an external 7 segment display
            //Used to indicate how far along the boot process is
            IOP_POST = value;
            printf("[IOP] POST: $%02X\n", value);
            return;
    }
    if (address >= iop_scratchpad_start && address < iop_scratchpad_start + 0x400)
    {
        iop_scratchpad[address & 0x3FF] = value;
        return;
    }
    Errors::print_warning("Unrecognized IOP write8 to physical addr $%08X of $%02X\n", address, value);
}

void Emulator::iop_write16(uint32_t address, uint16_t value)
{
    if (address < 0x00200000)
    {
        //printf("[IOP] Write16 to $%08X of $%08X\n", address, value);
        *(uint16_t*)&IOP_RAM[address] = value;
        return;
    }
    if (address >= 0x1F900000 && address < 0x1F900400)
    {
        spu.write16(address, value);
        return;
    }
    if (address >= 0x1F900400 && address < 0x1F900800)
    {
        spu2.write16(address, value);
        return;
    }
    switch (address)
    {
        case 0x1F8010B4:
            iop_dma.set_chan_size(3, value);
            return;
        case 0x1F8010B6:
            iop_dma.set_chan_count(3, value);
            return;
        case 0x1F8010C4:
            iop_dma.set_chan_size(4, value);
            return;
        case 0x1F8010C6:
            iop_dma.set_chan_count(4, value);
            return;
        case 0x1F801100:
            iop_timers.write_counter(0, value);
            return;
        case 0x1F801104:
            iop_timers.write_control(0, value);
            return;
        case 0x1F801108:
            iop_timers.write_target(0, value);
            return;
        case 0x1F801110:
            iop_timers.write_counter(1, value);
            return;
        case 0x1F801114:
            iop_timers.write_control(1, value);
            return;
        case 0x1F801118:
            iop_timers.write_target(1, value);
            return;
        case 0x1F801120:
            iop_timers.write_counter(2, value);
            return;
        case 0x1F801124:
            iop_timers.write_control(2, value);
            return;
        case 0x1F801128:
            iop_timers.write_target(2, value);
            return;
        case 0x1F801480:
            iop_timers.write_counter(3, value | (iop_timers.read_counter(3) & 0xFFFF0000));
            return;
        case 0x1F801482:
            iop_timers.write_counter(3, ((uint32_t)value << 16) | (iop_timers.read_counter(3) & 0xFFFF));
            return;
        case 0x1F801484:
            iop_timers.write_control(3, value);
            return;
        case 0x1F801488:
            iop_timers.write_target(3, value | (iop_timers.read_target(3) & 0xFFFF0000));
            return;
        case 0x1F80148A:
            iop_timers.write_target(3, ((uint32_t)value << 16) | (iop_timers.read_target(3) & 0xFFFF));
            return;
        case 0x1F801490:
            iop_timers.write_counter(4, value | (iop_timers.read_counter(4) & 0xFFFF0000));
            return;
        case 0x1F801492:
            iop_timers.write_counter(4, ((uint32_t)value << 16) | (iop_timers.read_counter(4) & 0xFFFF));
            return;
        case 0x1F801494:
            iop_timers.write_control(4, value);
            return;
        case 0x1F801498:
            iop_timers.write_target(4, value | (iop_timers.read_target(4) & 0xFFFF0000));
            return;
        case 0x1F80149A:
            iop_timers.write_target(4, (uint32_t)(value << 16) | (iop_timers.read_target(4) & 0xFFFF));
            return;
        case 0x1F8014A0:
            iop_timers.write_counter(5, value | (iop_timers.read_counter(5) & 0xFFFF0000));
            return;
        case 0x1F8014A2:
            iop_timers.write_counter(5, ((uint32_t)value << 16) | (iop_timers.read_counter(5) & 0xFFFF));
            return;
        case 0x1F8014A4:
            iop_timers.write_control(5, value);
            return;
        case 0x1F8014A8:
            iop_timers.write_target(5, value | (iop_timers.read_target(5) & 0xFFFF0000));
            return;
        case 0x1F8014AA:
            iop_timers.write_target(5, ((uint32_t)value << 16) | (iop_timers.read_target(5) & 0xFFFF));
            return;
        case 0x1F801504:
            iop_dma.set_chan_size(8, value);
            return;
        case 0x1F801506:
            iop_dma.set_chan_count(8, value);
            return;
        case 0x1F801524:
            iop_dma.set_chan_size(10, value);
            return;
        case 0x1F801534:
            iop_dma.set_chan_size(11, value);
            return;
        case 0x1F801536:
            iop_dma.set_chan_count(11, value);
            return;
    }
    if (address >= iop_scratchpad_start && address < iop_scratchpad_start + 0x400)
    {
        *(uint16_t*)&iop_scratchpad[address & 0x3FF] = value;
        return;
    }
    Errors::print_warning("Unrecognized IOP write16 to physical addr $%08X of $%04X\n", address, value);
}

void Emulator::iop_write32(uint32_t address, uint32_t value)
{
    if (address < 0x00200000)
    {
        //printf("[IOP] Write to $%08X of $%08X\n", address, value);
        *(uint32_t*)&IOP_RAM[address] = value;
        return;
    }
    //SIO2 send buffers
    if (address >= 0x1F808200 && address < 0x1F808240)
    {
        int index = address - 0x1F808200;
        sio2.set_send3(index >> 2, value);
        return;
    }
    if (address >= 0x1F808240 && address < 0x1F808260)
    {
        int index = address - 0x1F808240;
        if (address & 0x4)
            sio2.set_send2(index >> 3, value);
        else
            sio2.set_send1(index >> 3, value);
        return;
    }
    switch (address)
    {
        case 0x1D000000:
            //Read only
            return;
        case 0x1D000010:
            sif.set_smcom(value);
            return;
        case 0x1D000020:
            sif.reset_msflag(value);
            return;
        case 0x1D000030:
            printf("[IOP] Set smflag: $%08X\n", value);
            sif.set_smflag(value);
            return;
        case 0x1D000040:
            printf("[IOP] Write BD4: $%08X\n", value);
            sif.set_control_IOP(value);
            return;
        case 0x1F801010:
            printf("[IOP] SIF2/GPU SSBUS: $%08X\n", value);
            return;
        case 0x1F801014:
            printf("[IOP] SPU SSBUS: $%08X\n", value);
            return;
        case 0x1F801070:
            //printf("[IOP] I_STAT: $%08X\n", value);
            IOP_I_STAT &= value;
            iop.interrupt_check(IOP_I_CTRL && (IOP_I_MASK & IOP_I_STAT));
            return;
        case 0x1F801074:
            //printf("[IOP] I_MASK: $%08X\n", value);
            IOP_I_MASK = value;
            iop.interrupt_check(IOP_I_CTRL && (IOP_I_MASK & IOP_I_STAT));
            return;
        case 0x1F801078:
            if (!IOP_I_CTRL && (value & 0x1))
                iop_i_ctrl_delay = 4;
            IOP_I_CTRL = value & 0x1;
            //iop.interrupt_check(IOP_I_CTRL && (IOP_I_MASK & IOP_I_STAT));
            //printf("[IOP] I_CTRL: $%08X\n", value);
            return;
        //CDVD DMA
        case 0x1F8010B0:
            iop_dma.set_chan_addr(3, value);
            return;
        case 0x1F8010B4:
            iop_dma.set_chan_block(3, value);
            return;
        case 0x1F8010B8:
            iop_dma.set_chan_control(3, value);
            return;
        //SPU DMA
        case 0x1F8010C0:
            iop_dma.set_chan_addr(4, value);
            return;
        case 0x1F8010C4:
            iop_dma.set_chan_block(4, value);
            return;
        case 0x1F8010C8:
            iop_dma.set_chan_control(4, value);
            return;
        case 0x1F8010F0:
            iop_dma.set_DPCR(value);
            return;
        case 0x1F8010F4:
            iop_dma.set_DICR(value);
            return;
        case 0x1F801100:
            iop_timers.write_counter(0, value);
            return;
        case 0x1F801104:
            iop_timers.write_control(0, value);
            return;
        case 0x1F801108:
            iop_timers.write_target(0, value);
            return;
        case 0x1F801110:
            iop_timers.write_counter(1, value);
            return;
        case 0x1F801114:
            iop_timers.write_control(1, value);
            return;
        case 0x1F801118:
            iop_timers.write_target(1, value);
            return;
        case 0x1F801120:
            iop_timers.write_counter(2, value);
            return;
        case 0x1F801124:
            iop_timers.write_control(2, value);
            return;
        case 0x1F801128:
            iop_timers.write_target(2, value);
            return;
        case 0x1F801404:
            return;
        case 0x1F801450:
            //Config reg? Do nothing to prevent log spam
            return;
        case 0x1F801480:
            iop_timers.write_counter(3, value);
            return;
        case 0x1F801484:
            iop_timers.write_control(3, value);
            return;
        case 0x1F801488:
            iop_timers.write_target(3, value);
            return;
        case 0x1F801490:
            iop_timers.write_counter(4, value);
            return;
        case 0x1F801494:
            iop_timers.write_control(4, value);
            return;
        case 0x1F801498:
            iop_timers.write_target(4, value);
            return;
        case 0x1F8014A0:
            iop_timers.write_counter(5, value);
            return;
        case 0x1F8014A4:
            iop_timers.write_control(5, value);
            return;
        case 0x1F8014A8:
            iop_timers.write_target(5, value);
            return;
        //SPU2 DMA
        case 0x1F801500:
            iop_dma.set_chan_addr(8, value);
            return;
        case 0x1F801504:
            iop_dma.set_chan_block(8, value);
            return;
        case 0x1F801508:
            iop_dma.set_chan_control(8, value);
            return;
        //SIF0 DMA
        case 0x1F801520:
            iop_dma.set_chan_addr(10, value);
            return;
        case 0x1F801524:
            iop_dma.set_chan_block(10, value);
            return;
        case 0x1F801528:
            iop_dma.set_chan_control(10, value);
            return;
        case 0x1F80152C:
            iop_dma.set_chan_tag_addr(10, value);
            return;
        //SIF1 DMA
        case 0x1F801530:
            iop_dma.set_chan_addr(11, value);
            return;
        case 0x1F801534:
            iop_dma.set_chan_block(11, value);
            return;
        case 0x1F801538:
            iop_dma.set_chan_control(11, value);
            return;
        //SIO2in DMA
        case 0x1F801540:
            iop_dma.set_chan_addr(12, value);
            return;
        case 0x1F801544:
            iop_dma.set_chan_block(12, value);
            return;
        case 0x1F801548:
            iop_dma.set_chan_control(12, value);
            return;
        //SIO2out DMA
        case 0x1F801550:
            iop_dma.set_chan_addr(13, value);
            return;
        case 0x1F801554:
            iop_dma.set_chan_block(13, value);
            return;
        case 0x1F801558:
            iop_dma.set_chan_control(13, value);
            return;
        case 0x1F801570:
            iop_dma.set_DPCR2(value);
            return;
        case 0x1F801574:
            iop_dma.set_DICR2(value);
            return;
        case 0x1F801578:
            return;
        case 0x1F808268:
            sio2.set_control(value);
            return;
        //POST2?
        case 0x1F802070:
            return;
        //Cache control?
        case 0xFFFE0130:
            return;
    }
    if (address == 0xFFFE0144)
    {
        printf("[IOP] Scratchpad start: $%08X\n", value);
        iop_scratchpad_start = value;
        return;
    }
    if (address >= iop_scratchpad_start && address < iop_scratchpad_start + 0x400)
    {
        *(uint32_t*)&iop_scratchpad[address & 0x3FF] = value;
        return;
    }
    Errors::print_warning("Unrecognized IOP write32 to physical addr $%08X of $%08X\n", address, value);
}

void Emulator::iop_request_IRQ(int index)
{
    printf("[IOP] Requesting IRQ %d\n", index);
    uint32_t new_stat = IOP_I_STAT | (1 << index);
    IOP_I_STAT = new_stat;
    iop.interrupt_check(IOP_I_CTRL && (IOP_I_MASK & IOP_I_STAT));
}

void Emulator::iop_ksprintf()
{
    uint32_t msg_pointer = iop.get_gpr(6);
    uint32_t arg_pointer = iop.get_gpr(7);

    uint32_t width;
    printf("[IOP Debug] ksprintf: %s\n", (char*)&IOP_RAM[msg_pointer]);
    while (IOP_RAM[msg_pointer])
    {
        char c = IOP_RAM[msg_pointer];
        width = 8;
        if (c == '%')
        {
            msg_pointer++;
            while (IOP_RAM[msg_pointer] >= '0' && IOP_RAM[msg_pointer] <= '9')
            {
                //Hacky, but it works as long as the width is a single digit
                width = IOP_RAM[msg_pointer] - '0';
                msg_pointer++;
            }

            switch (IOP_RAM[msg_pointer])
            {
                case 's':
                {
                    uint32_t str_pointer = *(uint32_t*)&IOP_RAM[arg_pointer];
                    ee_log << (char*)&IOP_RAM[str_pointer];
                }
                    break;
                case 'd':
                    ee_log << *(int32_t*)&IOP_RAM[arg_pointer];
                    printf("[IOP Debug] %d\n", *(uint32_t*)&IOP_RAM[arg_pointer]);
                    break;
                case 'x':
                case 'X':
                    ee_log << std::hex << *(uint32_t*)&IOP_RAM[arg_pointer];
                    printf("[IOP Debug] $%08X\n", *(uint32_t*)&IOP_RAM[arg_pointer]);
                    break;
                default:
                    break;
            }
            arg_pointer += 4;
        }
        else
            ee_log << c;
        msg_pointer++;
    }
    ee_log.flush();
}

void Emulator::iop_puts()
{
    uint32_t pointer = iop.get_gpr(5);
    uint32_t len = iop.get_gpr(6);
    //printf("[IOP] ($%08X, $%08X) puts: ", pointer, len);
    /*for (int i = 4; i < 8; i++)
    {
        printf("$%08X", iop.get_gpr(i));
    }*/

    //Little sanity check to prevent crashing the emulator
    if (len >= 2048)
    {
        printf("[IOP] puts len over 2048!\n");
        len = 2048;
    }
    while (len)
    {
        ee_log << IOP_RAM[pointer & 0x1FFFFF];
        printf("puts: %c\n", IOP_RAM[pointer & 0x1FFFFF]);
        pointer++;
        len--;
    }
    ee_log.flush();
    //printf("\n");
}

GraphicsSynthesizer& Emulator::get_gs()
{
    return gs;
}

void Emulator::request_gsdump_toggle()
{
    gsdump_requested = true;
}
void Emulator::request_gsdump_single_frame()
{
    gsdump_single_frame = true;
}

void Emulator::add_ee_event(EVENT_ID id, event_func func, uint64_t delta_time_to_run)
{
    SchedulerEvent event;
    event.id = id;
    event.func = func;
    event.time_to_run = scheduler.get_ee_cycles() + delta_time_to_run;

    scheduler.add_event(event);
}

void Emulator::add_iop_event(EVENT_ID id, event_func func, uint64_t delta_time_to_run)
{
    SchedulerEvent event;
    event.id = id;
    event.func = func;
    event.time_to_run = (scheduler.get_iop_cycles() + delta_time_to_run) << 3;

    scheduler.add_event(event);
}
