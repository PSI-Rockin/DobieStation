#include <cstdio>
#include <cstdlib>
#include "emotion.hpp"
#include "emotioninterpreter.hpp"
#include "emulator.hpp"

EmotionEngine::EmotionEngine(BIOS_HLE* b, Emulator* e) : bios(b), e(e)
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

void EmotionEngine::reset()
{
    PC = 0xBFC00000;
    branch_on = false;
    delay_slot = 0;
    for (int i = 0; i < 16; i++)
        gpr[i] = 0; //Clear out zeroth register
    gpr_lo[0] = 0;
    gpr_hi[0] = 0;
    cp0.mtc(9, 0);
    gpr_lo[29] = 0x70003FF0;
}

void EmotionEngine::run()
{
    if (delay_slot)
        delay_slot--;
    else if (branch_on)
    {
        PC = new_PC;
        branch_on = false;
    }
    uint32_t burp = read32(PC);
    EmotionInterpreter::interpret(*this, burp);
    cp0.count_up();
    PC += 4;
    if (PC == 0xBFC0092C)
    {
        print_state();
        exit(1);
    }
}

void EmotionEngine::print_state()
{
    for (int i = 0; i < 32; i++)
        printf("r%d: $%08X_%08X\n", i, get_gpr<uint32_t>(i), get_gpr<uint32_t>(i, 1));
}

/*uint64_t EmotionEngine::get_gpr_lo(int index)
{
    return gpr_lo[index];
}

uint64_t EmotionEngine::get_gpr_hi(int index)
{
    return gpr_hi[index];
}*/

uint32_t EmotionEngine::get_PC()
{
    return PC;
}

uint8_t EmotionEngine::read8(uint32_t address)
{
    if (address >= 0x70000000 && address < 0x70004000)
        return scratchpad[address & 0x3FFF];
    if (address >= 0x30100000 && address < 0x31FFFFFF)
        address -= 0x10000000;
    return e->read8(address & 0x1FFFFFFF);
}

uint16_t EmotionEngine::read16(uint32_t address)
{
    if (address >= 0x70000000 && address < 0x70004000)
        return *(uint16_t*)&scratchpad[address & 0x3FFF];
    if (address >= 0x30100000 && address < 0x31FFFFFF)
        address -= 0x10000000;
    return e->read16(address & 0x1FFFFFFF);
}

uint32_t EmotionEngine::read32(uint32_t address)
{;
    if (address >= 0x70000000 && address < 0x70004000)
        return *(uint32_t*)&scratchpad[address & 0x3FFC];
    if (address >= 0x30100000 && address < 0x31FFFFFF)
        address -= 0x10000000;
    return e->read32(address & 0x1FFFFFFF);
}

uint64_t EmotionEngine::read64(uint32_t address)
{
    if (address >= 0x70000000 && address < 0x70004000)
        return *(uint64_t*)&scratchpad[address & 0x3FFC];
    if (address >= 0x30100000 && address < 0x31FFFFFF)
        address -= 0x10000000;
    return e->read64(address & 0x1FFFFFFF);
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
    if (address >= 0x30100000 && address < 0x31FFFFFF)
        address -= 0x10000000;
    e->write8(address & 0x1FFFFFFF, value);
}

void EmotionEngine::write16(uint32_t address, uint16_t value)
{
    if (address >= 0x70000000 && address < 0x70004000)
    {
        *(uint32_t*)&scratchpad[address & 0x3FFC] = value;
        return;
    }
    if (address >= 0x30100000 && address < 0x31FFFFFF)
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
    if (address >= 0x30100000 && address < 0x31FFFFFF)
        address -= 0x10000000;
    e->write32(address & 0x1FFFFFFF, value);
}

void EmotionEngine::write64(uint32_t address, uint64_t value)
{
    if (address >= 0x70000000 && address < 0x70004000)
    {
        *(uint64_t*)&scratchpad[address & 0x3FFC] = value;
        return;
    }
    if (address >= 0x30100000 && address < 0x31FFFFFF)
        address -= 0x10000000;
    e->write64(address & 0x1FFFFFFF, value);
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
            bark = cp0.mfc(cop_reg);
            break;
        case 1:
            bark = fpu.get_gpr(cop_reg);
            break;
        default:
            printf("Unrecognized cop id %d in mfc\n", cop_id);
            exit(1);
    }

    set_gpr<int64_t>(reg, (int32_t)bark);
}

void EmotionEngine::mtc(int cop_id, int reg, int cop_reg)
{
    switch (cop_id)
    {
        case 0:
            cp0.mtc(cop_reg, gpr_lo[reg] & 0xFFFFFFFF);
            break;
        case 1:
            fpu.mtc(cop_reg, gpr_lo[reg] & 0xFFFFFFFF);
            break;
        default:
            printf("Unrecognized cop id %d in mtc\n", cop_id);
            exit(1);
    }
}

void EmotionEngine::mfhi(int index)
{
    set_gpr<uint64_t>(index, HI);
}

void EmotionEngine::mflo(int index)
{
    set_gpr<uint64_t>(index, LO);
}

void EmotionEngine::mflo1(int index)
{
    set_gpr<uint64_t>(index, LO1);
}

void EmotionEngine::lwc1(uint32_t addr, int index)
{
    fpu.mtc(index, read32(addr));
}

void EmotionEngine::swc1(uint32_t addr, int index)
{
    write32(addr, fpu.get_gpr(index));
}

void EmotionEngine::set_LO_HI(uint64_t a, uint64_t b, bool hi)
{
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

void EmotionEngine::hle_syscall()
{
    uint8_t op = read8(PC - 4);
    bios->hle_syscall(*this, op);
}

void EmotionEngine::syscall_exception()
{
    cp0.mtc(13, 0x20);
    cp0.mtc(14, PC);
    PC = 0xBFC00200 + 0x17C;
}

void EmotionEngine::fpu_mov_s(int dest, int source)
{
    fpu.mtc(dest, fpu.get_gpr(source));
}

void EmotionEngine::fpu_cvt_s_w(int dest, int source)
{
    fpu.cvt_s_w(dest, source);
}
