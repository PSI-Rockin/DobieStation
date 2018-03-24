#include <cstdio>
#include <cstdlib>
#include "iop.hpp"
#include "iop_interpreter.hpp"

#include "../emulator.hpp"
#include "../ee/emotiondisasm.hpp"

IOP::IOP(Emulator* e) : e(e)
{

}

const char* IOP::REG(int id)
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

void IOP::reset()
{
    PC = 0xBFC00000;
    gpr[0] = 0;
    load_delay = 0;
    will_branch = false;
    inc_PC = true;
    can_disassemble = false;
}

uint32_t IOP::translate_addr(uint32_t addr)
{
    //KSEG0
    if (addr >= 0x80000000 && addr < 0xA0000000)
        return addr - 0x80000000;
    //KSEG1
    if (addr >= 0xA0000000 && addr < 0xC0000000)
        return addr - 0xA0000000;

    //KUSEG, KSEG2
    return addr;
}

void IOP::run()
{
    if (will_branch)
    {
        if (!load_delay)
        {
            will_branch = false;
            PC = new_PC;
        }
        else
            load_delay--;
    }
    uint32_t instr = read32(PC);
    if (can_disassemble)
        printf("[IOP] [$%08X] $%08X - %s\n", PC, instr, EmotionDisasm::disasm_instr(instr, PC).c_str());
    IOP_Interpreter::interpret(*this, instr);

    /*if (PC == 0xBFC5891C)
    {
        for (int i = 0; i < 32; i++)
        {
            printf("%s:$%08X\t", REG(i), get_gpr(i));
            if (i % 3 == 2)
                printf("\n");
        }
    }*/

    if (inc_PC)
        PC += 4;
    else
        inc_PC = true;
}

void IOP::jp(uint32_t addr)
{
    if (!will_branch)
    {
        new_PC = addr;
        will_branch = true;
        load_delay = 1;
    }
}

void IOP::branch(bool condition, int32_t offset)
{
    if (condition)
        jp(PC + offset + 4);
}

void IOP::handle_exception(uint32_t addr, uint8_t cause)
{
    inc_PC = false;
    cop0.cause.code = cause;
    if (load_delay && will_branch)
    {
        cop0.EPC = PC - 4;
        cop0.cause.bd = true;
    }
    else
    {
        cop0.EPC = PC;
        cop0.cause.bd = false;
    }
    PC = addr;
    load_delay = 0;
    will_branch = false;
}

void IOP::syscall_exception()
{
    handle_exception(0x80000080, 0x08);
    //can_disassemble = true;
}

void IOP::interrupt()
{
    handle_exception(0x80000080, 0x00);
    can_disassemble = true;
}

void IOP::mfc(int cop_id, int cop_reg, int reg)
{
    switch (cop_id)
    {
        case 0:
            set_gpr(reg, cop0.mfc(cop_reg));
            break;
        default:
            printf("\n[IOP] MFC: Unknown COP%d", cop_id);
            exit(1);
    }
}

void IOP::mtc(int cop_id, int cop_reg, int reg)
{
    uint32_t bark = get_gpr(reg);
    switch (cop_id)
    {
        case 0:
            cop0.mtc(cop_reg, bark);
            break;
        default:
            printf("\n[IOP] MTC: Unknown COP%d", cop_id);
            exit(1);
    }
}

void IOP::rfe()
{
    cop0.status.KUc = cop0.status.KUp;
    cop0.status.KUp = cop0.status.KUo;

    cop0.status.IEc = cop0.status.IEp;
    cop0.status.IEp = cop0.status.IEo;
}

uint8_t IOP::read8(uint32_t addr)
{
    return e->iop_read8(translate_addr(addr));
}

uint16_t IOP::read16(uint32_t addr)
{
    return e->iop_read16(translate_addr(addr));
}

uint32_t IOP::read32(uint32_t addr)
{
    return e->iop_read32(translate_addr(addr));
}

void IOP::write8(uint32_t addr, uint8_t value)
{
    if (cop0.status.IsC)
        return;
    e->iop_write8(translate_addr(addr), value);
}

void IOP::write16(uint32_t addr, uint16_t value)
{
    if (cop0.status.IsC)
        return;
    e->iop_write16(translate_addr(addr), value);
}

void IOP::write32(uint32_t addr, uint32_t value)
{
    if (cop0.status.IsC)
        return;
    e->iop_write32(translate_addr(addr), value);
}
