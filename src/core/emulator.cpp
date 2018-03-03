#include <cstring>
#include <cstdio>
#include <cstdlib>
#include "emulator.hpp"

Emulator::Emulator() : bios_hle(this, &gs), cpu(&bios_hle, this), dmac(this, &gif), gif(&gs), iop(this)
{
    BIOS = nullptr;
    RDRAM = nullptr;
    IOP_RAM = nullptr;
}

Emulator::~Emulator()
{
    if (RDRAM)
        delete[] RDRAM;
    if (IOP_RAM)
        delete[] IOP_RAM;
    if (BIOS)
        delete[] BIOS;
}

void Emulator::run()
{
    gs.start_frame();
    instructions_run = 0;
    while (!gs.is_frame_complete() && instructions_run < 300000)
    {
        cpu.run();
        dmac.run();
        if (instructions_run % 8 == 0)
            iop.run();
        instructions_run++;
    }
    //VBLANK start
    INTC_STAT |= (1 << 2);
    if (INTC_MASK & 0x4)
        cpu.request_interrupt();
    gs.render_CRT();
}

void Emulator::reset()
{
    if (!RDRAM)
        RDRAM = new uint8_t[1024 * 1024 * 32];
    if (!IOP_RAM)
        IOP_RAM = new uint8_t[1024 * 1024 * 2];
    if (!BIOS)
        BIOS = new uint8_t[1024 * 1024 * 4];

    bios_hle.reset();
    cpu.reset();
    dmac.reset();
    gs.reset();
    gif.reset();
    iop.reset();
    MCH_DRD = 0;
    MCH_RICM = 0;
    rdram_sdevid = 0;
    INTC_STAT = 0;
    INTC_MASK = 0;
}

uint32_t* Emulator::get_framebuffer()
{
    //This function should only be called upon ending a frame; return nullptr otherwise
    if (!gs.is_frame_complete() && instructions_run < 300000)
        return nullptr;
    return gs.get_framebuffer();
}

void Emulator::get_resolution(int &w, int &h)
{
    gs.get_resolution(w, h);
}

void Emulator::load_BIOS(uint8_t *BIOS_file)
{
    //if (BIOS)
        //delete[] BIOS;
    memcpy(BIOS, BIOS_file, 1024 * 1024 * 4);

    //Copy EE kernel into memory
    //memcpy(RDRAM, BIOS + 0xB3200, 0x13BF0);

    //The BIOS's init_main_thread reads from this memory location and crashes if nothing's there...
    //I think it's supposed to be the size of RDRAM, as the BIOS writes to memory locations based upon this value.
    //TODO: confirm that this value is correct after booting the BIOS.
    //write32(0x00013C10, 0x02000000);
}

void Emulator::load_ELF(uint8_t *ELF)
{
    return;
    if (ELF[0] != 0x7F || ELF[1] != 'E' || ELF[2] != 'L' || ELF[3] != 'F')
    {
        printf("Invalid elf\n");
        return;
    }
    printf("Valid elf\n");
    uint32_t e_entry = *(uint32_t*)&ELF[0x18];
    uint32_t e_phoff = *(uint32_t*)&ELF[0x1C];
    uint32_t e_shoff = *(uint32_t*)&ELF[0x20];
    uint16_t e_phnum = *(uint16_t*)&ELF[0x2C];
    uint16_t e_shnum = *(uint16_t*)&ELF[0x30];
    uint16_t e_shstrndx = *(uint16_t*)&ELF[0x32];

    printf("Entry: $%08X\n", e_entry);
    printf("Program header start: $%08X\n", e_phoff);
    printf("Section header start: $%08X\n", e_shoff);
    printf("Program header entries: %d\n", e_phnum);
    printf("Section header entries: %d\n", e_shnum);
    printf("Section header names index: %d\n", e_shstrndx);

    for (int i = e_phoff; i < e_phoff + (e_phnum * 0x20); i += 0x20)
    {
        uint32_t p_offset = *(uint32_t*)&ELF[i + 0x4];
        uint32_t p_paddr = *(uint32_t*)&ELF[i + 0xC];
        uint32_t p_filesz = *(uint32_t*)&ELF[i + 0x10];
        uint32_t p_memsz = *(uint32_t*)&ELF[i + 0x14];
        printf("\nProgram header\n");
        printf("p_type: $%08X\n", *(uint32_t*)&ELF[i]);
        printf("p_offset: $%08X\n", p_offset);
        printf("p_vaddr: $%08X\n", *(uint32_t*)&ELF[i + 0x8]);
        printf("p_paddr: $%08X\n", p_paddr);
        printf("p_filesz: $%08X\n", p_filesz);
        printf("p_memsz: $%08X\n", p_memsz);

        int mem_w = p_paddr;
        for (int file_w = p_offset; file_w < (p_offset + p_filesz); file_w += 4)
        {
            uint32_t word = *(uint32_t*)&ELF[file_w];
            write32(mem_w, word);
            mem_w += 4;
        }
    }

    uint32_t name_offset = ELF[e_shoff + (e_shstrndx * 0x28) + 0x10];
    printf("Name offset: $%08X\n", name_offset);

    for (int i = e_shoff; i < e_shoff + (e_shnum * 0x28); i += 0x28)
    {
        uint32_t sh_name = *(uint32_t*)&ELF[i];
        uint32_t sh_type = *(uint32_t*)&ELF[i + 0x4];
        uint32_t sh_offset = *(uint32_t*)&ELF[i + 0x10];
        uint32_t sh_size = *(uint32_t*)&ELF[i + 0x14];
        printf("\nSection header\n");
        printf("sh_type: $%08X\n", sh_type);
        printf("sh_offset: $%08X\n", sh_offset);
        printf("sh_size: $%08X\n", sh_size);

        if (sh_type == 0x3)
        {
            printf("Debug symbols found\n");
            for (int j = sh_offset; j < sh_offset + sh_size; j++)
            {
                unsigned char burp = ELF[j];
                if (!burp)
                    printf("\n");
                else
                    printf("%c", burp);
            }
        }
    }
    cpu.set_PC(e_entry);
}

uint8_t Emulator::read8(uint32_t address)
{
    if (address < 0x10000000)
        return *(uint32_t*)&RDRAM[address & 0x01FFFFFF];
    if (address >= 0x1FC00000 && address < 0x20000000)
        return BIOS[address & 0x3FFFFF];
    printf("Unrecognized read8 at physical addr $%08X\n", address);
    return 0;
    exit(1);
}

uint16_t Emulator::read16(uint32_t address)
{
    if (address < 0x10000000)
        return *(uint16_t*)&RDRAM[address & 0x01FFFFFF];
    if (address >= 0x1FC00000 && address < 0x20000000)
        return *(uint16_t*)&BIOS[address & 0x3FFFFF];
    printf("Unrecognized read16 at physical addr $%08X\n", address);
    return 0;
    exit(1);
}

uint32_t Emulator::read32(uint32_t address)
{
    if (address < 0x10000000)
        return *(uint32_t*)&RDRAM[address & 0x01FFFFFF];
    if (address >= 0x1FC00000 && address < 0x20000000)
        return *(uint32_t*)&BIOS[address & 0x3FFFFF];
    if ((address & (0xFF000000)) == 0x12000000)
        return gs.read32_privileged(address);
    if (address >= 0x10008000 && address < 0x1000F000)
        return dmac.read32(address);
    switch (address)
    {
        case 0x1000F130:
            return 0;
        case 0x1000F000:
            printf("\nRead32 INTC_STAT: $%08X", INTC_STAT);
            return INTC_STAT;
        case 0x1000F010:
            printf("\nRead32 INTC_MASK: $%08X", INTC_MASK);
            return INTC_MASK;
        case 0x1000F430:
            printf("\nRead from MCH_RICM");
            return 0;
        case 0x1000F440:
            printf("\nRead from MCH_DRD");
            if (!((MCH_RICM >> 6) & 0xF))
            {
                switch ((MCH_RICM >> 16) & 0xFFF)
                {
                    case 0x21:
                        printf("\nInit");
                        if (rdram_sdevid < 2)
                        {
                            rdram_sdevid++;
                            return 0x1F;
                        }
                        return 0;
                    case 0x23:
                        printf("\nConfigA");
                        return 0x0D0D;
                    case 0x24:
                        printf("\nConfigB");
                        return 0x0090;
                    case 0x40:
                        printf("\nDevid");
                        return MCH_RICM & 0x1F;
                }
            }
            return 0;
    }
    printf("Unrecognized read32 at physical addr $%08X\n", address);

    return 0;
    //exit(1);
}

uint64_t Emulator::read64(uint32_t address)
{
    if (address < 0x10000000)
        return *(uint64_t*)&RDRAM[address & 0x01FFFFFF];
    if (address >= 0x10008000 && address < 0x1000F000)
        return dmac.read32(address);
    if ((address & (0xFF000000)) == 0x12000000)
        return gs.read64_privileged(address);
    printf("Unrecognized read64 at physical addr $%08X\n", address);
    return 0;
    //exit(1);
}

void Emulator::write8(uint32_t address, uint8_t value)
{
    if (address < 0x10000000)
    {
        RDRAM[address & 0x01FFFFFF] = value;
        return;
    }
    switch (address)
    {
        case 0x1000F180:
            printf("\nSTDOUT: %c", value);
            return;
    }
    printf("Unrecognized write8 at physical addr $%08X of $%02X\n", address, value);
    //exit(1);
}

void Emulator::write16(uint32_t address, uint16_t value)
{
    if (address < 0x10000000)
    {
        *(uint16_t*)&RDRAM[address & 0x01FFFFFF] = value;
        return;
    }
    printf("Unrecognized write16 at physical addr $%08X of $%04X\n", address, value);
}

void Emulator::write32(uint32_t address, uint32_t value)
{
    if (address < 0x10000000)
    {
        *(uint32_t*)&RDRAM[address & 0x01FFFFFF] = value;
        return;
    }
    if ((address & (0xFF000000)) == 0x12000000)
    {
        gs.write32_privileged(address, value);
        return;
    }
    if (address >= 0x10008000 && address < 0x1000F000)
    {
        dmac.write32(address, value);
        return;
    }
    switch (address)
    {
        case 0x1000F000:
            printf("\nWrite32 INTC_STAT: $%08X", value);
            INTC_STAT &= ~(value & 0x7FFF);
            return;
        case 0x1000F010:
            printf("\nWrite32 INTC_MASK: $%08X", value);
            INTC_MASK ^= (value & 0x7FFF);
            return;
        case 0x1000F430:
            printf("\nWrite to MCH_RICM: $%08X", value);
            if ((((value >> 16) & 0xFFF) == 0x21) && (((value >> 6) & 0xF) == 1) &&
                    (((MCH_DRD >> 7) & 1) == 0))
                rdram_sdevid = 0;
            MCH_RICM = value & ~0x80000000;
            return;
        case 0x1000F440:
            printf("\nWrite to MCH_DRD: $%08X", value);
            MCH_DRD = value;
            return;
    }
    printf("Unrecognized write32 at physical addr $%08X of $%08X\n", address, value);

    //exit(1);
}

void Emulator::write64(uint32_t address, uint64_t value)
{
    if (address < 0x10000000)
    {
        *(uint64_t*)&RDRAM[address & 0x01FFFFFF] = value;
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
        return;
    }
    if (address >= 0x1C000000 && address < 0x1C200000)
    {
        *(uint64_t*)&IOP_RAM[address & 0x1FFFFF] = value;
        return;
    }
    printf("Unrecognized write64 at physical addr $%08X of $%08X_%08X\n", address, value >> 32, value & 0xFFFFFFFF);
    //exit(1);
}

uint8_t Emulator::iop_read8(uint32_t address)
{
    if (address < 0x00200000)
        return IOP_RAM[address];
    if (address >= 0x1FC00000 && address < 0x20000000)
        return BIOS[address & 0x3FFFFF];
    switch (address)
    {
        case 0x1FA00000:
            return 0;
    }
    printf("Unrecognized IOP read8 from physical addr $%08X\n", address);
    exit(1);
}

uint16_t Emulator::iop_read16(uint32_t address)
{
    if (address < 0x00200000)
        return *(uint16_t*)&IOP_RAM[address];
    if (address >= 0x1FC00000 && address < 0x20000000)
        return *(uint16_t*)&BIOS[address & 0x3FFFFF];
    printf("Unrecognized IOP read16 from physical addr $%08X\n", address);
    exit(1);
}

uint32_t Emulator::iop_read32(uint32_t address)
{
    if (address < 0x00200000)
        return *(uint32_t*)&IOP_RAM[address];

    if (address >= 0x1FC00000 && address < 0x20000000)
        return *(uint32_t*)&BIOS[address & 0x3FFFFF];
    switch (address)
    {
        case 0x1F801010:
            return 0;
        case 0x1F8010F0:
            return 0;
        case 0xFFFE0130:
            return 0;
    }
    printf("Unrecognized IOP read32 from physical addr $%08X\n", address);
    exit(1);
}

void Emulator::iop_write8(uint32_t address, uint8_t value)
{
    if (address < 0x00200000)
    {
        IOP_RAM[address] = value;
        return;
    }
    switch (address)
    {
        case 0x1F802070:
            return;
        case 0x1FA00000:
            return;
    }
    printf("Unrecognized IOP write8 to physical addr $%08X of $%02X\n", address, value);
    exit(1);
}

void Emulator::iop_write16(uint32_t address, uint16_t value)
{
    printf("Unrecognized IOP write16 to physical addr $%08X of $%04X\n", address, value);
    //exit(1);
}

void Emulator::iop_write32(uint32_t address, uint32_t value)
{
    if (address < 0x00200000)
    {
        printf("[IOP] Write to $%08X of $%08X\n", address, value);
        *(uint32_t*)&IOP_RAM[address] = value;
        return;
    }
    if (address >= 0x1F808200 && address < 0x1F808280)
    {
        printf("[IOP SIO2] Write32 to $%08X of $%08X\n", address, value);
        return;
    }
    switch (address)
    {
        case 0x1F801000:
            return;
        case 0x1F801004:
            return;
        case 0x1F801008:
            return;
        case 0x1F80100C:
            return;
        //BIOS ROM delay?
        case 0x1F801010:
            return;
        case 0x1F801014:
            return;
        case 0x1F801018:
            return;
        case 0x1F80101C:
            return;
        //Common delay?
        case 0x1F801020:
            return;
        //RAM size?
        case 0x1F801060:
            return;
        //DMAC/SIF shit
        case 0x1F8010F0:
            printf("[IOP] Write32 to $%08X of $%08X\n", address, value);
            return;
        case 0x1F802070:
            return;
        //Cache control?
        case 0xFFFE0130:
            return;
    }
    printf("Unrecognized IOP write32 to physical addr $%08X of $%08X\n", address, value);
    exit(1);
}
