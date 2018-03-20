#include <cstdio>
#include <cstdlib>
#include "iop_cop0.hpp"

IOP_Cop0::IOP_Cop0()
{

}

void IOP_Cop0::reset()
{
    status.IEc = false;
    status.KUc = false;
    status.IEp = false;
    status.KUp = false;
    status.IEo = false;
    status.KUo = false;
    status.Im = 0;
    status.bev = true;

    cause.code = 0;
    cause.bd = false;
    cause.int_pending = 0;
}

uint32_t IOP_Cop0::mfc(int cop_reg)
{
    printf("[IOP COP0] MFC: Read from %d\n", cop_reg);
    switch (cop_reg)
    {
        case 12:
        {
            uint32_t reg = 0;
            reg |= status.IEc;
            reg |= status.KUc << 1;
            reg |= status.IEp << 2;
            reg |= status.KUp << 3;
            reg |= status.IEo << 4;
            reg |= status.KUo << 5;
            reg |= status.Im << 8;
            reg |= status.IsC << 16;
            reg |= status.bev << 22;
            return reg;
        }
        case 13:
        {
            uint32_t reg = 0;
            reg |= cause.code << 2;
            reg |= cause.int_pending << 8;
            reg |= cause.bd << 31;
            return reg;
        }
        case 14:
            return EPC;
        case 15:
            return 0x1F;
        default:
            printf("[IOP COP0] MFC: Unknown cop_reg %d\n", cop_reg);
            exit(1);
    }
}

void IOP_Cop0::mtc(int cop_reg, uint32_t value)
{
    printf("[IOP COP0] MTC: Write to %d of $%08X\n", cop_reg, value);
    switch (cop_reg)
    {
        case 12:
            status.IEc = value & 1;
            status.KUc = value & (1 << 1);
            status.IEp = value & (1 << 2);
            status.KUp = value & (1 << 3);
            status.IEo = value & (1 << 4);
            status.KUo = value & (1 << 5);
            status.Im = (value >> 8) & 0xFF;
            status.IsC = value & (1 << 16);
            status.bev = value & (1 << 22);
            break;
    }
}
