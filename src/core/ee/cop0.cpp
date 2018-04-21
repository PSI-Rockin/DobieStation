#include <cstdio>
#include "cop0.hpp"
#include "dmac.hpp"

Cop0::Cop0(DMAC* dmac) : dmac(dmac)
{}

void Cop0::reset()
{
    for (int i = 0; i < 32; i++)
        gpr[i] = 0;

    status.int_enable = false;
    status.exception = false;
    status.error = false;
    status.mode = 0;
    status.int0_mask = false;
    status.int1_mask = false;
    status.bus_error = false;
    status.master_int_enable = false;
    status.edi = false;
    status.ch = false;
    status.bev = true;
    status.dev = false;
    status.cu = 0x7;

    cause.code = 0;
    cause.int0_pending = false;
    cause.int1_pending = false;
    cause.timer_int_pending = false;
    cause.code2 = 0;
    cause.ce = 0;
    cause.bd2 = false;
    cause.bd = false;

    //Set processor revision id
    gpr[15] = 0x00002E20;
}

uint32_t Cop0::mfc(int index)
{
    //printf("[COP0] Move from reg%d\n", index);
    switch (index)
    {
        case 12:
        {
            uint32_t reg = 0;
            reg |= status.int_enable;
            reg |= status.exception << 1;
            reg |= status.error << 2;
            reg |= status.mode << 3;
            reg |= status.int0_mask << 10;
            reg |= status.int1_mask << 11;
            reg |= status.bus_error << 12;
            reg |= status.timer_int_mask << 15;
            reg |= status.edi << 17;
            reg |= status.ch << 18;
            reg |= status.bev << 22;
            reg |= status.dev << 23;
            reg |= status.cu << 28;
            return reg;
        }
        case 13:
        {
            uint32_t reg = 0;
            reg |= cause.code << 2;
            reg |= cause.int0_pending << 10;
            reg |= cause.int1_pending << 11;
            reg |= cause.timer_int_pending << 15;
            reg |= cause.code2 << 16;
            reg |= cause.ce << 28;
            reg |= cause.bd2 << 30;
            reg |= cause.bd << 31;
            return reg;
        }
        case 14:
            if (status.error)
                return ErrorEPC;
            else
                return EPC;
            break;
        default:
            return gpr[index];
    }
}

void Cop0::mtc(int index, uint32_t value)
{
    //printf("[COP0] Move to reg%d: $%08X\n", index, value);
    switch (index)
    {
        case 12:
            status.int_enable = value & 1;
            status.exception = value & (1 << 1);
            status.error = value & (1 << 2);
            status.mode = (value >> 3) & 0x3;
            status.int0_mask = value & (1 << 10);
            status.int1_mask = value & (1 << 11);
            status.bus_error = value & (1 << 12);
            status.timer_int_mask = value & (1 << 15);
            status.edi = value & (1 << 17);
            status.ch = value & (1 << 18);
            status.bev = value & (1 << 22);
            status.dev = value & (1 << 23);
            status.cu = (value >> 28);
            break;
        case 13:
            //No bits in CAUSE are writable
            break;
        case 14:
            if (status.error)
                ErrorEPC = value;
            else
                EPC = value;
            break;
        default:
            gpr[index] = value;
    }
}

/**
 * Coprocessor 0 is wired to the DMAC. CP0COND is true when all DMA transfers indicated in PCR have completed.
 */
bool Cop0::get_condition()
{
    uint32_t STAT = dmac->read32(0x1000E010) & 0x3FF;
    uint32_t PCR = dmac->read32(0x1000E020) & 0x3FF;
    return ((~PCR | STAT) & 0x3FF) == 0x3FF;
}

bool Cop0::int1_raised()
{
    return int_enabled() && (gpr[CAUSE] & (1 << 11));
}

bool Cop0::int_enabled()
{
    return status.master_int_enable && status.int_enable && !status.exception && !status.error;
}

bool Cop0::int_pending()
{
    if (status.int0_mask && cause.int0_pending)
        return true;
    if (status.int1_mask && cause.int1_pending)
        return true;
    return false;
}

void Cop0::count_up(int cycles)
{
    gpr[9] += cycles;
}
