#include <cstring>
#include <cstdio>
#include <cstdlib>
#include "emulator.hpp"

Emulator::Emulator() : bios_hle(this), cpu(&bios_hle, this), dmac(this, &gs)
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
    while (!gs.is_frame_complete())
    {
        cpu.run();
        dmac.run();
    }
}

void Emulator::reset()
{
    cpu.reset();
    dmac.reset();
    gs.reset();
    if (!RDRAM)
        RDRAM = new uint8_t[1024 * 1024 * 32];
    if (!IOP_RAM)
        IOP_RAM = new uint8_t[1024 * 1024 * 2];
    if (!BIOS)
        BIOS = new uint8_t[1024 * 1024 * 4];
    MCH_DRD = 0;
    MCH_RICM = 0;
    rdram_sdevid = 0;
    INTC_STAT = 0;
}

uint32_t* Emulator::get_framebuffer()
{
    return gs.get_framebuffer();
}

void Emulator::load_BIOS(uint8_t *BIOS_file)
{
    //if (BIOS)
        //delete[] BIOS;
    memcpy(this->BIOS, BIOS_file, 1024 * 1024 * 4);
}

void Emulator::load_ELF(uint8_t *ELF)
{
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

    printf("Entry: $%08X\n", e_entry);
    printf("Program header start: $%08X\n", e_phoff);
    printf("Section header start: $%08X\n", e_shoff);
    printf("Program header entries: %d\n", e_phnum);
    printf("Section header entries: %d\n", e_shnum);

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
            //printf("Write to $%08X of $%08X\n", mem_w, word);
            write32(mem_w, word);
            mem_w += 4;
        }
    }
    cpu.set_PC(e_entry);
}

uint8_t Emulator::read8(uint32_t address)
{
    if (address < 0x02000000)
        return *(uint32_t*)&RDRAM[address];
    if (address >= 0x1FC00000 && address < 0x20000000)
        return BIOS[address & 0x3FFFFF];
    printf("\nUnrecognized read8 at physical addr $%08X", address);
    return 0;
    exit(1);
}

uint16_t Emulator::read16(uint32_t address)
{
    if (address < 0x02000000)
        return *(uint16_t*)&RDRAM[address];
    if (address >= 0x1FC00000 && address < 0x20000000)
        return *(uint16_t*)&BIOS[address & 0x3FFFFF];
    printf("\nUnrecognized read16 at physical addr $%08X", address);
    return 0;
    exit(1);
}

uint32_t Emulator::read32(uint32_t address)
{
    if (address < 0x02000000)
        return *(uint32_t*)&RDRAM[address];
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
            return INTC_STAT;
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
    printf("\nUnrecognized read32 at physical addr $%08X", address);

    return 0;
    //exit(1);
}

uint64_t Emulator::read64(uint32_t address)
{
    if (address < 0x02000000)
        return *(uint64_t*)&RDRAM[address];
    printf("\nUnrecognized read64 at physical addr $%08X", address);
    return 0;
    //exit(1);
}

void Emulator::write8(uint32_t address, uint8_t value)
{
    if (address < 0x02000000)
    {
        RDRAM[address] = value;
        return;
    }
    switch (address)
    {
        case 0x1000F180:
            printf("\nSTDOUT: %c", value);
            return;
    }
    printf("\nUnrecognized write8 at physical addr $%08X of $%02X", address, value);
    //exit(1);
}

void Emulator::write16(uint32_t address, uint16_t value)
{
    if (address < 0x02000000)
    {
        *(uint16_t*)&RDRAM[address] = value;
        return;
    }
    printf("\nUnrecognized write16 at physical addr $%08X of $%04X", address, value);
}

void Emulator::write32(uint32_t address, uint32_t value)
{
    if (address < 0x02000000)
    {
        *(uint32_t*)&RDRAM[address] = value;
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
        case 0x1000F400:
            INTC_STAT &= ~value;
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
    printf("\nUnrecognized write32 at physical addr $%08X of $%08X", address, value);

    //exit(1);
}

void Emulator::write64(uint32_t address, uint64_t value)
{
    if (address < 0x02000000)
    {
        *(uint64_t*)&RDRAM[address] = value;
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
    printf("\nUnrecognized write64 at physical addr $%08X of $%08X_%08X", address, value >> 32, value & 0xFFFFFFFF);
    //exit(1);
}
