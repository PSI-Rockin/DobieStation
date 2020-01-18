#include <cstdio>
#include <cstdlib>
#include "vu_interpreter.hpp"
#include "../errors.hpp"

#define printf(fmt, ...)(0)

#define _ft_ ((instr >> 16) & 0x1F)
#define _fs_ ((instr >> 11) & 0x1F)
#define _fd_ ((instr >> 6) & 0x1F)

#define _field ((instr >> 21) & 0xF)

#define _it_ (_ft_ & 0xF)
#define _is_ (_fs_ & 0xF)
#define _id_ (_fd_ & 0xF)


namespace VU_Interpreter
{
typedef void(VectorUnit::*vu_op)(uint32_t);
vu_op upper_op, lower_op;

void call_upper(VectorUnit &vu, uint32_t instr)
{
    (vu.*upper_op)(instr);
}

void call_lower(VectorUnit &vu, uint32_t instr)
{
    (vu.*lower_op)(instr);
}

void interpret(VectorUnit &vu, uint32_t upper_instr, uint32_t lower_instr)
{
    //WaitQ, DIV, RSQRT, SQRT
    if (((lower_instr & 0x800007FC) == 0x800003BC))
    {
        vu.waitq(0);
    }

    if ((lower_instr & (1 << 31)) && ((lower_instr >> 2) & 0x1CF) == 0x1CF)
    {
        vu.waitp(0);
    }

    vu.decoder.reset();

    //Get upper op
    upper(vu, upper_instr);

    //Get lower op
    if (!(upper_instr & (1 << 31)))
        lower(vu, lower_instr);

    // check for stalls before execution
    vu.check_for_FMAC_stall();
    
    //LOI - upper op always executes first
    if (upper_instr & (1 << 31))
    {
        (vu.*upper_op)(upper_instr);
        vu.set_I(lower_instr);
    }
    else
    {
        //If the upper op is writing to a reg the lower op is reading from, the lower op executes first
        //Also used to handle if upper and lower write to the same register, upper gets priority
        int write = vu.decoder.vf_write[0];
        int write1 = vu.decoder.vf_write[1];
        int read0 = vu.decoder.vf_read0[1];
        int read1 = vu.decoder.vf_read1[1];
        if (write && ((write == read0 || write == read1) || (write == write1)))
        {
            vu.backup_vf(false, write);

            (vu.*upper_op)(upper_instr);

            vu.backup_vf(true, write);
            vu.restore_vf(false, write);

            (vu.*lower_op)(lower_instr);

            vu.restore_vf(true, write);
        }
        else
        {
            (vu.*upper_op)(upper_instr);
            (vu.*lower_op)(lower_instr);
        }
    }

    if (upper_instr & (1 << 29) && vu.get_id() == 0)
        vu.check_interlock();

    if (upper_instr & (1 << 30))
        vu.end_execution();
}

void upper(VectorUnit &vu, uint32_t instr)
{
    uint8_t op = instr & 0x3F;
    switch (op)
    {
        case 0x00:
        case 0x01:
        case 0x02:
        case 0x03:
            addbc(vu, instr);
            break;
        case 0x04:
        case 0x05:
        case 0x06:
        case 0x07:
            subbc(vu, instr);
            break;
        case 0x08:
        case 0x09:
        case 0x0A:
        case 0x0B:
            maddbc(vu, instr);
            break;
        case 0x0C:
        case 0x0D:
        case 0x0E:
        case 0x0F:
            msubbc(vu, instr);
            break;
        case 0x10:
        case 0x11:
        case 0x12:
        case 0x13:
            maxbc(vu, instr);
            break;
        case 0x14:
        case 0x15:
        case 0x16:
        case 0x17:
            minibc(vu, instr);
            break;
        case 0x18:
        case 0x19:
        case 0x1A:
        case 0x1B:
            mulbc(vu, instr);
            break;
        case 0x1C:
            mulq(vu, instr);
            break;
        case 0x1D:
            maxi(vu, instr);
            break;
        case 0x1E:
            muli(vu, instr);
            break;
        case 0x1F:
            minii(vu, instr);
            break;
        case 0x20:
            addq(vu, instr);
            break;
        case 0x21:
            maddq(vu, instr);
            break;
        case 0x22:
            addi(vu, instr);
            break;
        case 0x23:
            maddi(vu, instr);
            break;
        case 0x24:
            subq(vu, instr);
            break;
        case 0x25:
            msubq(vu, instr);
            break;
        case 0x26:
            subi(vu, instr);
            break;
        case 0x27:
            msubi(vu, instr);
            break;
        case 0x28:
            add(vu, instr);
            break;
        case 0x29:
            madd(vu, instr);
            break;
        case 0x2A:
            mul(vu, instr);
            break;
        case 0x2B:
            max(vu, instr);
            break;
        case 0x2C:
            sub(vu, instr);
            break;
        case 0x2D:
            msub(vu, instr);
            break;
        case 0x2E:
            opmsub(vu, instr);
            break;
        case 0x2F:
            mini(vu, instr);
            break;
        case 0x3C:
        case 0x3D:
        case 0x3E:
        case 0x3F:
            upper_special(vu, instr);
            break;
        default:
            unknown_op("upper", instr, op);
    }
}

void addbc(VectorUnit &vu, uint32_t instr)
{
    uint8_t bc = instr & 0x3;
    uint8_t dest = (instr >> 6) & 0x1F;
    uint8_t source = (instr >> 11) & 0x1F;
    uint8_t bc_reg = (instr >> 16) & 0x1F;
    uint8_t field = (instr >> 21) & 0xF;
    vu.decoder.vf_write[0] = dest;
    vu.decoder.vf_write_field[0] = field;

    vu.decoder.vf_read0[0] = source;
    vu.decoder.vf_read0_field[0] = field;

    vu.decoder.vf_read1[0] = bc_reg;
    vu.decoder.vf_read1_field[0] = 1 << (3 - bc);

    upper_op = &VectorUnit::addbc;
}

void subbc(VectorUnit &vu, uint32_t instr)
{
    uint8_t bc = instr & 0x3;
    uint8_t dest = (instr >> 6) & 0x1F;
    uint8_t source = (instr >> 11) & 0x1F;
    uint8_t bc_reg = (instr >> 16) & 0x1F;
    uint8_t field = (instr >> 21) & 0xF;

    vu.decoder.vf_write[0] = dest;
    vu.decoder.vf_write_field[0] = field;

    vu.decoder.vf_read0[0] = source;
    vu.decoder.vf_read0_field[0] = field;

    vu.decoder.vf_read1[0] = bc_reg;
    vu.decoder.vf_read1_field[0] = 1 << (3 - bc);
    upper_op = &VectorUnit::subbc;
}

void maddbc(VectorUnit &vu, uint32_t instr)
{
    uint8_t bc = instr & 0x3;
    uint8_t dest = (instr >> 6) & 0x1F;
    uint8_t source = (instr >> 11) & 0x1F;
    uint8_t bc_reg = (instr >> 16) & 0x1F;
    uint8_t field = (instr >> 21) & 0xF;

    vu.decoder.vf_write[0] = dest;
    vu.decoder.vf_write_field[0] = field;

    vu.decoder.vf_read0[0] = source;
    vu.decoder.vf_read0_field[0] = field;

    vu.decoder.vf_read1[0] = bc_reg;
    vu.decoder.vf_read1_field[0] = 1 << (3 - bc);
    upper_op = &VectorUnit::maddbc;
}

void msubbc(VectorUnit &vu, uint32_t instr)
{
    uint8_t bc = instr & 0x3;
    uint8_t dest = (instr >> 6) & 0x1F;
    uint8_t source = (instr >> 11) & 0x1F;
    uint8_t bc_reg = (instr >> 16) & 0x1F;
    uint8_t field = (instr >> 21) & 0xF;

    vu.decoder.vf_write[0] = dest;
    vu.decoder.vf_write_field[0] = field;

    vu.decoder.vf_read0[0] = source;
    vu.decoder.vf_read0_field[0] = field;

    vu.decoder.vf_read1[0] = bc_reg;
    vu.decoder.vf_read1_field[0] = 1 << (3 - bc);
    upper_op = &VectorUnit::msubbc;
}

void maxbc(VectorUnit &vu, uint32_t instr)
{
    uint8_t bc = instr & 0x3;
    uint8_t dest = (instr >> 6) & 0x1F;
    uint8_t source = (instr >> 11) & 0x1F;
    uint8_t bc_reg = (instr >> 16) & 0x1F;
    uint8_t field = (instr >> 21) & 0xF;

    vu.decoder.vf_write[0] = dest;
    vu.decoder.vf_write_field[0] = field;

    vu.decoder.vf_read0[0] = source;
    vu.decoder.vf_read0_field[0] = field;

    vu.decoder.vf_read1[0] = bc_reg;
    vu.decoder.vf_read1_field[0] = 1 << (3 - bc);
    upper_op = &VectorUnit::maxbc;
}

void minibc(VectorUnit &vu, uint32_t instr)
{
    uint8_t bc = instr & 0x3;
    uint8_t dest = (instr >> 6) & 0x1F;
    uint8_t source = (instr >> 11) & 0x1F;
    uint8_t bc_reg = (instr >> 16) & 0x1F;
    uint8_t field = (instr >> 21) & 0xF;

    vu.decoder.vf_write[0] = dest;
    vu.decoder.vf_write_field[0] = field;

    vu.decoder.vf_read0[0] = source;
    vu.decoder.vf_read0_field[0] = field;

    vu.decoder.vf_read1[0] = bc_reg;
    vu.decoder.vf_read1_field[0] = 1 << (3 - bc);
    upper_op = &VectorUnit::minibc;
}

void mulbc(VectorUnit &vu, uint32_t instr)
{
    uint8_t bc = instr & 0x3;
    uint8_t dest = (instr >> 6) & 0x1F;
    uint8_t source = (instr >> 11) & 0x1F;
    uint8_t bc_reg = (instr >> 16) & 0x1F;
    uint8_t field = (instr >> 21) & 0xF;

    vu.decoder.vf_write[0] = dest;
    vu.decoder.vf_write_field[0] = field;

    vu.decoder.vf_read0[0] = source;
    vu.decoder.vf_read0_field[0] = field;

    vu.decoder.vf_read1[0] = bc_reg;
    vu.decoder.vf_read1_field[0] = 1 << (3 - bc);
    upper_op = &VectorUnit::mulbc;
}

void mulq(VectorUnit &vu, uint32_t instr)
{
    uint8_t dest = (instr >> 6) & 0x1F;
    uint8_t source = (instr >> 11) & 0x1F;
    uint8_t field = (instr >> 21) & 0xF;

    vu.decoder.vf_write[0] = dest;
    vu.decoder.vf_write_field[0] = field;

    vu.decoder.vf_read0[0] = source;
    vu.decoder.vf_read0_field[0] = field;
    upper_op = &VectorUnit::mulq;
}

void maxi(VectorUnit &vu, uint32_t instr)
{
    uint8_t dest = (instr >> 6) & 0x1F;
    uint8_t source = (instr >> 11) & 0x1F;
    uint8_t field = (instr >> 21) & 0xF;

    vu.decoder.vf_write[0] = dest;
    vu.decoder.vf_write_field[0] = field;

    vu.decoder.vf_read0[0] = source;
    vu.decoder.vf_read0_field[0] = field;
    upper_op = &VectorUnit::maxi;
}

void muli(VectorUnit &vu, uint32_t instr)
{
    uint8_t dest = (instr >> 6) & 0x1F;
    uint8_t source = (instr >> 11) & 0x1F;
    uint8_t field = (instr >> 21) & 0xF;

    vu.decoder.vf_write[0] = dest;
    vu.decoder.vf_write_field[0] = field;

    vu.decoder.vf_read0[0] = source;
    vu.decoder.vf_read0_field[0] = field;
    upper_op = &VectorUnit::muli;
}

void minii(VectorUnit &vu, uint32_t instr)
{
    uint8_t dest = (instr >> 6) & 0x1F;
    uint8_t source = (instr >> 11) & 0x1F;
    uint8_t field = (instr >> 21) & 0xF;

    vu.decoder.vf_write[0] = dest;
    vu.decoder.vf_write_field[0] = field;

    vu.decoder.vf_read0[0] = source;
    vu.decoder.vf_read0_field[0] = field;
    upper_op = &VectorUnit::minii;
}

void addq(VectorUnit &vu, uint32_t instr)
{
    uint8_t dest = (instr >> 6) & 0x1F;
    uint8_t source = (instr >> 11) & 0x1F;
    uint8_t field = (instr >> 21) & 0xF;

    vu.decoder.vf_write[0] = dest;
    vu.decoder.vf_write_field[0] = field;
    vu.decoder.vf_read0[0] = source;
    vu.decoder.vf_read0_field[0] = field;
    upper_op = &VectorUnit::addq;
}

void maddq(VectorUnit &vu, uint32_t instr)
{
    uint8_t dest = (instr >> 6) & 0x1F;
    uint8_t source = (instr >> 11) & 0x1F;
    uint8_t field = (instr >> 21) & 0xF;

    vu.decoder.vf_write[0] = dest;
    vu.decoder.vf_write_field[0] = field;

    vu.decoder.vf_read0[0] = source;
    vu.decoder.vf_read0_field[0] = field;
    upper_op = &VectorUnit::maddq;
}

void addi(VectorUnit &vu, uint32_t instr)
{
    uint8_t dest = (instr >> 6) & 0x1F;
    uint8_t source = (instr >> 11) & 0x1F;
    uint8_t field = (instr >> 21) & 0xF;

    vu.decoder.vf_write[0] = dest;
    vu.decoder.vf_write_field[0] = field;
    vu.decoder.vf_read0[0] = source;
    vu.decoder.vf_read0_field[0] = field;
    upper_op = &VectorUnit::addi;
}

void maddi(VectorUnit &vu, uint32_t instr)
{
    uint8_t dest = (instr >> 6) & 0x1F;
    uint8_t source = (instr >> 11) & 0x1F;
    uint8_t field = (instr >> 21) & 0xF;

    vu.decoder.vf_write[0] = dest;
    vu.decoder.vf_write_field[0] = field;

    vu.decoder.vf_read0[0] = source;
    vu.decoder.vf_read0_field[0] = field;
    upper_op = &VectorUnit::maddi;
}

void subq(VectorUnit &vu, uint32_t instr)
{
    uint8_t dest = (instr >> 6) & 0x1F;
    uint8_t source = (instr >> 11) & 0x1F;
    uint8_t field = (instr >> 21) & 0xF;

    vu.decoder.vf_write[0] = dest;
    vu.decoder.vf_write_field[0] = field;

    vu.decoder.vf_read0[0] = source;
    vu.decoder.vf_read0_field[0] = field;
    upper_op = &VectorUnit::subq;
}

void msubq(VectorUnit &vu, uint32_t instr)
{
    uint8_t dest = (instr >> 6) & 0x1F;
    uint8_t source = (instr >> 11) & 0x1F;
    uint8_t field = (instr >> 21) & 0xF;

    vu.decoder.vf_write[0] = dest;
    vu.decoder.vf_write_field[0] = field;

    vu.decoder.vf_read0[0] = source;
    vu.decoder.vf_read0_field[0] = field;
    upper_op = &VectorUnit::msubq;
}

void subi(VectorUnit &vu, uint32_t instr)
{
    uint8_t dest = (instr >> 6) & 0x1F;
    uint8_t source = (instr >> 11) & 0x1F;
    uint8_t field = (instr >> 21) & 0xF;

    vu.decoder.vf_write[0] = dest;
    vu.decoder.vf_write_field[0] = field;

    vu.decoder.vf_read0[0] = source;
    vu.decoder.vf_read0_field[0] = field;
    upper_op = &VectorUnit::subi;
}

void msubi(VectorUnit &vu, uint32_t instr)
{
    uint8_t dest = (instr >> 6) & 0x1F;
    uint8_t source = (instr >> 11) & 0x1F;
    uint8_t field = (instr >> 21) & 0xF;

    vu.decoder.vf_write[0] = dest;
    vu.decoder.vf_write_field[0] = field;

    vu.decoder.vf_read0[0] = source;
    vu.decoder.vf_read0_field[0] = field;
    upper_op = &VectorUnit::msubi;
}

void add(VectorUnit &vu, uint32_t instr)
{
    uint8_t dest = (instr >> 6) & 0x1F;
    uint8_t reg1 = (instr >> 11) & 0x1F;
    uint8_t reg2 = (instr >> 16) & 0x1F;
    uint8_t field = (instr >> 21) & 0xF;

    vu.decoder.vf_write[0] = dest;
    vu.decoder.vf_write_field[0] = field;

    vu.decoder.vf_read0[0] = reg1;
    vu.decoder.vf_read0_field[0] = field;

    vu.decoder.vf_read1[0] = reg2;
    vu.decoder.vf_read1_field[0] = field;
    upper_op = &VectorUnit::add;
}

void madd(VectorUnit &vu, uint32_t instr)
{
    uint8_t dest = (instr >> 6) & 0x1F;
    uint8_t reg1 = (instr >> 11) & 0x1F;
    uint8_t reg2 = (instr >> 16) & 0x1F;
    uint8_t field = (instr >> 21) & 0xF;

    vu.decoder.vf_write[0] = dest;
    vu.decoder.vf_write_field[0] = field;

    vu.decoder.vf_read0[0] = reg1;
    vu.decoder.vf_read0_field[0] = field;

    vu.decoder.vf_read1[0] = reg2;
    vu.decoder.vf_read1_field[0] = field;
    upper_op = &VectorUnit::madd;
}

void mul(VectorUnit &vu, uint32_t instr)
{
    uint8_t dest = (instr >> 6) & 0x1F;
    uint8_t reg1 = (instr >> 11) & 0x1F;
    uint8_t reg2 = (instr >> 16) & 0x1F;
    uint8_t field = (instr >> 21) & 0xF;

    vu.decoder.vf_write[0] = dest;
    vu.decoder.vf_write_field[0] = field;

    vu.decoder.vf_read0[0] = reg1;
    vu.decoder.vf_read0_field[0] = field;

    vu.decoder.vf_read1[0] = reg2;
    vu.decoder.vf_read1_field[0] = field;
    upper_op = &VectorUnit::mul;
}

void max(VectorUnit &vu, uint32_t instr)
{
    uint8_t dest = (instr >> 6) & 0x1F;
    uint8_t reg1 = (instr >> 11) & 0x1F;
    uint8_t reg2 = (instr >> 16) & 0x1F;
    uint8_t field = (instr >> 21) & 0xF;

    vu.decoder.vf_write[0] = dest;
    vu.decoder.vf_write_field[0] = field;

    vu.decoder.vf_read0[0] = reg1;
    vu.decoder.vf_read0_field[0] = field;

    vu.decoder.vf_read1[0] = reg2;
    vu.decoder.vf_read1_field[0] = field;
    upper_op = &VectorUnit::max;
}

void sub(VectorUnit &vu, uint32_t instr)
{
    uint8_t dest = (instr >> 6) & 0x1F;
    uint8_t reg1 = (instr >> 11) & 0x1F;
    uint8_t reg2 = (instr >> 16) & 0x1F;
    uint8_t field = (instr >> 21) & 0xF;

    vu.decoder.vf_write[0] = dest;
    vu.decoder.vf_write_field[0] = field;

    vu.decoder.vf_read0[0] = reg1;
    vu.decoder.vf_read0_field[0] = field;

    vu.decoder.vf_read1[0] = reg2;
    vu.decoder.vf_read1_field[0] = field;
    upper_op = &VectorUnit::sub;
}

void msub(VectorUnit &vu, uint32_t instr)
{
    uint8_t dest = (instr >> 6) & 0x1F;
    uint8_t reg1 = (instr >> 11) & 0x1F;
    uint8_t reg2 = (instr >> 16) & 0x1F;
    uint8_t field = (instr >> 21) & 0xF;
    vu.decoder.vf_write[0] = dest;
    vu.decoder.vf_read0[0] = reg1;
    vu.decoder.vf_read1[0] = reg2;
    vu.decoder.vf_write_field[0] = field;
    vu.decoder.vf_read0_field[0] = field;
    vu.decoder.vf_read1_field[0] = field;
    upper_op = &VectorUnit::msub;
}

void opmsub(VectorUnit &vu, uint32_t instr)
{
    uint8_t dest = (instr >> 6) & 0x1F;
    uint8_t reg1 = (instr >> 11) & 0x1F;
    uint8_t reg2 = (instr >> 16) & 0x1F;
    vu.decoder.vf_write[0] = dest;
    vu.decoder.vf_read0[0] = reg1;
    vu.decoder.vf_read1[0] = reg2;
    vu.decoder.vf_write_field[0] = 0xE; //xyz
    vu.decoder.vf_read0_field[0] = 0xE;
    vu.decoder.vf_read1_field[0] = 0xE;
    upper_op = &VectorUnit::opmsub;
}

void mini(VectorUnit &vu, uint32_t instr)
{
    uint8_t dest = (instr >> 6) & 0x1F;
    uint8_t reg1 = (instr >> 11) & 0x1F;
    uint8_t reg2 = (instr >> 16) & 0x1F;
    uint8_t field = (instr >> 21) & 0xF;

    vu.decoder.vf_write[0] = dest;
    vu.decoder.vf_write_field[0] = field;

    vu.decoder.vf_read0[0] = reg1;
    vu.decoder.vf_read0_field[0] = field;

    vu.decoder.vf_read1[0] = reg2;
    vu.decoder.vf_read1_field[0] = field;
    upper_op = &VectorUnit::mini;
}

void upper_special(VectorUnit &vu, uint32_t instr)
{
    uint16_t op = (instr & 0x3) | ((instr >> 4) & 0x7C);
    switch (op)
    {
        case 0x00:
        case 0x01:
        case 0x02:
        case 0x03:
            addabc(vu, instr);
            break;
        case 0x04:
        case 0x05:
        case 0x06:
        case 0x07:
            subabc(vu, instr);
            break;
        case 0x08:
        case 0x09:
        case 0x0A:
        case 0x0B:
            maddabc(vu, instr);
            break;
        case 0x0C:
        case 0x0D:
        case 0x0E:
        case 0x0F:
            msubabc(vu, instr);
            break;
        case 0x10:
            itof0(vu, instr);
            break;
        case 0x11:
            itof4(vu, instr);
            break;
        case 0x12:
            itof12(vu, instr);
            break;
        case 0x13:
            itof15(vu, instr);
            break;
        case 0x14:
            ftoi0(vu, instr);
            break;
        case 0x15:
            ftoi4(vu, instr);
            break;
        case 0x16:
            ftoi12(vu, instr);
            break;
        case 0x17:
            ftoi15(vu, instr);
            break;
        case 0x18:
        case 0x19:
        case 0x1A:
        case 0x1B:
            mulabc(vu, instr);
            break;
        case 0x1C:
            mulaq(vu, instr);
            break;
        case 0x1D:
            abs(vu, instr);
            break;
        case 0x1E:
            mulai(vu, instr);
            break;
        case 0x1F:
            clip(vu, instr);
            break;
        case 0x20:
            addaq(vu, instr);
            break;
        case 0x21:
            maddaq(vu, instr);
            break;
        case 0x22:
            addai(vu, instr);
            break;
        case 0x23:
            maddai(vu, instr);
            break;
        case 0x24:
            subaq(vu, instr);
            break;
        case 0x25:
            msubaq(vu, instr);
            break;
        case 0x26:
            subai(vu, instr);
            break;
        case 0x27:
            msubai(vu, instr);
            break;
        case 0x28:
            adda(vu, instr);
            break;
        case 0x29:
            madda(vu, instr);
            break;
        case 0x2A:
            mula(vu, instr);
            break;
        case 0x2C:
            suba(vu, instr);
            break;
        case 0x2D:
            msuba(vu, instr);
            break;
        case 0x2E:
            opmula(vu, instr);
            break;
        case 0x2F:
        case 0x30:
            /**
              * NOP
              */
            upper_op = &VectorUnit::nop;
            break;
        default:
            unknown_op("upper special", instr, op);
    }
}

void addabc(VectorUnit &vu, uint32_t instr)
{
    uint8_t bc = instr & 0x3;
    uint8_t source = (instr >> 11) & 0x1F;
    uint8_t bc_reg = (instr >> 16) & 0x1F;
    uint8_t field = (instr >> 21) & 0xF;

    vu.decoder.vf_read0[0] = source;
    vu.decoder.vf_read0_field[0] = field;

    vu.decoder.vf_read1[0] = bc_reg;
    vu.decoder.vf_read1_field[0] = 1 << (3 - bc);
    upper_op = &VectorUnit::addabc;
}

void subabc(VectorUnit &vu, uint32_t instr)
{
    uint8_t bc = instr & 0x3;
    uint8_t source = (instr >> 11) & 0x1F;
    uint8_t bc_reg = (instr >> 16) & 0x1F;
    uint8_t field = (instr >> 21) & 0xF;

    vu.decoder.vf_read0[0] = source;
    vu.decoder.vf_read0_field[0] = field;

    vu.decoder.vf_read1[0] = bc_reg;
    vu.decoder.vf_read1_field[0] = 1 << (3 - bc);
    upper_op = &VectorUnit::subabc;
}

void maddabc(VectorUnit &vu, uint32_t instr)
{
    uint8_t bc = instr & 0x3;
    uint8_t source = (instr >> 11) & 0x1F;
    uint8_t bc_reg = (instr >> 16) & 0x1F;
    uint8_t field = (instr >> 21) & 0xF;

    vu.decoder.vf_read0[0] = source;
    vu.decoder.vf_read0_field[0] = field;

    vu.decoder.vf_read1[0] = bc_reg;
    vu.decoder.vf_read1_field[0] = 1 << (3 - bc);
    upper_op = &VectorUnit::maddabc;
}

void msubabc(VectorUnit &vu, uint32_t instr)
{
    uint8_t bc = instr & 0x3;
    uint8_t source = (instr >> 11) & 0x1F;
    uint8_t bc_reg = (instr >> 16) & 0x1F;
    uint8_t field = (instr >> 21) & 0xF;

    vu.decoder.vf_read0[0] = source;
    vu.decoder.vf_read0_field[0] = field;

    vu.decoder.vf_read1[0] = bc_reg;
    vu.decoder.vf_read1_field[0] = 1 << (3 - bc);
    upper_op = &VectorUnit::msubabc;
}

void itof0(VectorUnit &vu, uint32_t instr)
{
    uint8_t source = (instr >> 11) & 0x1F;
    uint8_t dest = (instr >> 16) & 0x1F;
    uint8_t field = (instr >> 21) & 0xF;

    vu.decoder.vf_write[0] = dest;
    vu.decoder.vf_write_field[0] = field;
    vu.decoder.vf_read0[0] = source;
    vu.decoder.vf_read0_field[0] = field;
    upper_op = &VectorUnit::itof0;
}

void itof4(VectorUnit &vu, uint32_t instr)
{
    uint8_t source = (instr >> 11) & 0x1F;
    uint8_t dest = (instr >> 16) & 0x1F;
    uint8_t field = (instr >> 21) & 0xF;

    vu.decoder.vf_write[0] = dest;
    vu.decoder.vf_write_field[0] = field;
    vu.decoder.vf_read0[0] = source;
    vu.decoder.vf_read0_field[0] = field;
    upper_op = &VectorUnit::itof4;
}

void itof12(VectorUnit &vu, uint32_t instr)
{
    uint8_t source = (instr >> 11) & 0x1F;
    uint8_t dest = (instr >> 16) & 0x1F;
    uint8_t field = (instr >> 21) & 0xF;

    vu.decoder.vf_write[0] = dest;
    vu.decoder.vf_write_field[0] = field;
    vu.decoder.vf_read0[0] = source;
    vu.decoder.vf_read0_field[0] = field;
    upper_op = &VectorUnit::itof12;
}

void itof15(VectorUnit &vu, uint32_t instr)
{
    uint8_t source = (instr >> 11) & 0x1F;
    uint8_t dest = (instr >> 16) & 0x1F;
    uint8_t field = (instr >> 21) & 0xF;

    vu.decoder.vf_write[0] = dest;
    vu.decoder.vf_write_field[0] = field;
    vu.decoder.vf_read0[0] = source;
    vu.decoder.vf_read0_field[0] = field;
    upper_op = &VectorUnit::itof15;
}

void ftoi0(VectorUnit &vu, uint32_t instr)
{
    uint8_t source = (instr >> 11) & 0x1F;
    uint8_t dest = (instr >> 16) & 0x1F;
    uint8_t field = (instr >> 21) & 0xF;

    vu.decoder.vf_write[0] = dest;
    vu.decoder.vf_write_field[0] = field;
    vu.decoder.vf_read0[0] = source;
    vu.decoder.vf_read0_field[0] = field;
    upper_op = &VectorUnit::ftoi0;
}

void ftoi4(VectorUnit &vu, uint32_t instr)
{
    uint8_t source = (instr >> 11) & 0x1F;
    uint8_t dest = (instr >> 16) & 0x1F;
    uint8_t field = (instr >> 21) & 0xF;

    vu.decoder.vf_write[0] = dest;
    vu.decoder.vf_write_field[0] = field;
    vu.decoder.vf_read0[0] = source;
    vu.decoder.vf_read0_field[0] = field;
    upper_op = &VectorUnit::ftoi4;
}

void ftoi12(VectorUnit &vu, uint32_t instr)
{
    uint8_t source = (instr >> 11) & 0x1F;
    uint8_t dest = (instr >> 16) & 0x1F;
    uint8_t field = (instr >> 21) & 0xF;

    vu.decoder.vf_write[0] = dest;
    vu.decoder.vf_write_field[0] = field;
    vu.decoder.vf_read0[0] = source;
    vu.decoder.vf_read0_field[0] = field;
    upper_op = &VectorUnit::ftoi12;
}

void ftoi15(VectorUnit &vu, uint32_t instr)
{
    uint8_t source = (instr >> 11) & 0x1F;
    uint8_t dest = (instr >> 16) & 0x1F;
    uint8_t field = (instr >> 21) & 0xF;

    vu.decoder.vf_write[0] = dest;
    vu.decoder.vf_write_field[0] = field;
    vu.decoder.vf_read0[0] = source;
    vu.decoder.vf_read0_field[0] = field;
    upper_op = &VectorUnit::ftoi15;
}

void mulabc(VectorUnit &vu, uint32_t instr)
{
    uint8_t bc = instr & 0x3;
    uint8_t source = (instr >> 11) & 0x1F;
    uint8_t bc_reg = (instr >> 16) & 0x1F;
    uint8_t field = (instr >> 21) & 0xF;

    vu.decoder.vf_read0[0] = source;
    vu.decoder.vf_read0_field[0] = field;

    vu.decoder.vf_read1[0] = bc_reg;
    vu.decoder.vf_read1_field[0] = 1 << (3 - bc);
    upper_op = &VectorUnit::mulabc;
}

void mulaq(VectorUnit &vu, uint32_t instr)
{
    uint8_t source = (instr >> 11) & 0x1F;
    uint8_t field = (instr >> 21) & 0xF;

    vu.decoder.vf_read0[0] = source;
    vu.decoder.vf_read0_field[0] = field;
    upper_op = &VectorUnit::mulaq;
}

void abs(VectorUnit &vu, uint32_t instr)
{
    uint8_t source = (instr >> 11) & 0x1F;
    uint8_t dest = (instr >> 16) & 0x1F;
    uint8_t field = (instr >> 21) & 0xF;

    vu.decoder.vf_write[0] = dest;
    vu.decoder.vf_write_field[0] = field;

    vu.decoder.vf_read0[0] = source;
    vu.decoder.vf_read0_field[0] = field;
    upper_op = &VectorUnit::abs;
}

void mulai(VectorUnit &vu, uint32_t instr)
{
    uint8_t source = (instr >> 11) & 0x1F;
    uint8_t field = (instr >> 21) & 0xF;

    vu.decoder.vf_read0[0] = source;
    vu.decoder.vf_read0_field[0] = field;
    upper_op = &VectorUnit::mulai;
}

void clip(VectorUnit &vu, uint32_t instr)
{
    uint8_t reg1 = (instr >> 11) & 0x1F;
    uint8_t reg2 = (instr >> 16) & 0x1F;

    vu.decoder.vf_read0[0] = reg1;
    vu.decoder.vf_read0_field[0] = 0xE; //xyz

    vu.decoder.vf_read1[0] = reg2;
    vu.decoder.vf_read1_field[0] = 0x1; //w
    upper_op = &VectorUnit::clip;
}

void addai(VectorUnit &vu, uint32_t instr)
{
    uint8_t source = (instr >> 11) & 0x1F;
    uint8_t field = (instr >> 21) & 0xF;

    vu.decoder.vf_read0[0] = source;
    vu.decoder.vf_read0_field[0] = field;
    upper_op = &VectorUnit::addai;
}

void addaq(VectorUnit &vu, uint32_t instr)
{
    uint8_t source = (instr >> 11) & 0x1F;
    uint8_t field = (instr >> 21) & 0xF;

    vu.decoder.vf_read0[0] = source;
    vu.decoder.vf_read0_field[0] = field;
    upper_op = &VectorUnit::addaq;
}

void maddaq(VectorUnit &vu, uint32_t instr)
{
    uint8_t source = (instr >> 11) & 0x1F;
    uint8_t field = (instr >> 21) & 0xF;

    vu.decoder.vf_read0[0] = source;
    vu.decoder.vf_read0_field[0] = field;
    upper_op = &VectorUnit::maddaq;
}

void maddai(VectorUnit &vu, uint32_t instr)
{
    uint8_t source = (instr >> 11) & 0x1F;
    uint8_t field = (instr >> 21) & 0xF;

    vu.decoder.vf_read0[0] = source;
    vu.decoder.vf_read0_field[0] = field;
    upper_op = &VectorUnit::maddai;
}

void subaq(VectorUnit &vu, uint32_t instr)
{
    uint8_t source = (instr >> 11) & 0x1F;
    uint8_t field = (instr >> 21) & 0xF;

    vu.decoder.vf_read0[0] = source;
    vu.decoder.vf_read0_field[0] = field;
    upper_op = &VectorUnit::subaq;
}

void msubaq(VectorUnit &vu, uint32_t instr)
{
    uint8_t source = (instr >> 11) & 0x1F;
    uint8_t field = (instr >> 21) & 0xF;

    vu.decoder.vf_read0[0] = source;
    vu.decoder.vf_read0_field[0] = field;
    upper_op = &VectorUnit::msubaq;
}

void subai(VectorUnit &vu, uint32_t instr)
{
    uint8_t source = (instr >> 11) & 0x1F;
    uint8_t field = (instr >> 21) & 0xF;

    vu.decoder.vf_read0[0] = source;
    vu.decoder.vf_read0_field[0] = field;
    upper_op = &VectorUnit::subai;
}

void msubai(VectorUnit &vu, uint32_t instr)
{
    uint8_t source = (instr >> 11) & 0x1F;
    uint8_t field = (instr >> 21) & 0xF;

    vu.decoder.vf_read0[0] = source;
    vu.decoder.vf_read0_field[0] = field;
    upper_op = &VectorUnit::msubai;
}

void mula(VectorUnit &vu, uint32_t instr)
{
    uint8_t reg1 = (instr >> 11) & 0x1F;
    uint8_t reg2 = (instr >> 16) & 0x1F;
    uint8_t field = (instr >> 21) & 0xF;

    vu.decoder.vf_read0[0] = reg1;
    vu.decoder.vf_read0_field[0] = field;

    vu.decoder.vf_read1[0] = reg2;
    vu.decoder.vf_read1_field[0] = field;
    upper_op = &VectorUnit::mula;
}

void adda(VectorUnit &vu, uint32_t instr)
{
    uint8_t reg1 = (instr >> 11) & 0x1F;
    uint8_t reg2 = (instr >> 16) & 0x1F;
    uint8_t field = (instr >> 21) & 0xF;

    vu.decoder.vf_read0[0] = reg1;
    vu.decoder.vf_read0_field[0] = field;

    vu.decoder.vf_read1[0] = reg2;
    vu.decoder.vf_read1_field[0] = field;
    upper_op = &VectorUnit::adda;
}

void suba(VectorUnit &vu, uint32_t instr)
{
    uint8_t reg1 = (instr >> 11) & 0x1F;
    uint8_t reg2 = (instr >> 16) & 0x1F;
    uint8_t field = (instr >> 21) & 0xF;

    vu.decoder.vf_read0[0] = reg1;
    vu.decoder.vf_read0_field[0] = field;

    vu.decoder.vf_read1[0] = reg2;
    vu.decoder.vf_read1_field[0] = field;
    upper_op = &VectorUnit::suba;
}

void madda(VectorUnit &vu, uint32_t instr)
{
    uint8_t reg1 = (instr >> 11) & 0x1F;
    uint8_t reg2 = (instr >> 16) & 0x1F;
    uint8_t field = (instr >> 21) & 0xF;

    vu.decoder.vf_read0[0] = reg1;
    vu.decoder.vf_read0_field[0] = field;

    vu.decoder.vf_read1[0] = reg2;
    vu.decoder.vf_read1_field[0] = field;
    upper_op = &VectorUnit::madda;
}

void opmula(VectorUnit &vu, uint32_t instr)
{
    uint8_t reg1 = (instr >> 11) & 0x1F;
    uint8_t reg2 = (instr >> 16) & 0x1F;
    vu.decoder.vf_read0[0] = reg1;
    vu.decoder.vf_read1[0] = reg2;
    vu.decoder.vf_read0_field[0] = 0xE; //xyz
    vu.decoder.vf_read1_field[0] = 0xE; //xyz
    upper_op = &VectorUnit::opmula;
}

void msuba(VectorUnit &vu, uint32_t instr)
{
    uint8_t reg1 = (instr >> 11) & 0x1F;
    uint8_t reg2 = (instr >> 16) & 0x1F;
    uint8_t field = (instr >> 21) & 0xF;

    vu.decoder.vf_read0[0] = reg1;
    vu.decoder.vf_read0_field[0] = field;

    vu.decoder.vf_read1[0] = reg2;
    vu.decoder.vf_read1_field[0] = field;
    upper_op = &VectorUnit::msuba;
}

void lower(VectorUnit &vu, uint32_t instr)
{
    if (instr & (1 << 31))
        lower1(vu, instr);
    else
        lower2(vu, instr);
}

void lower1(VectorUnit &vu, uint32_t instr)
{
    uint8_t op = instr & 0x3F;
    switch (op)
    {
        case 0x30:
            iadd(vu, instr);
            break;
        case 0x31:
            isub(vu, instr);
            break;
        case 0x32:
            iaddi(vu, instr);
            break;
        case 0x34:
            iand(vu, instr);
            break;
        case 0x35:
            ior(vu, instr);
            break;
        case 0x3C:
        case 0x3D:
        case 0x3E:
        case 0x3F:
            lower1_special(vu, instr);
            break;
        default:
            unknown_op("lower1", instr, op);
    }
}

void iadd(VectorUnit &vu, uint32_t instr)
{
    /*uint8_t dest = (instr >> 6) & 0x1F;
    uint8_t reg1 = (instr >> 11) & 0x1F;
    uint8_t reg2 = (instr >> 16) & 0x1F;*/
    vu.decoder.vi_write = (instr >> 6) & 0xF;
    vu.decoder.vi_read0 = _is_;
    vu.decoder.vi_read1 = _it_;

    lower_op = &VectorUnit::iadd;
}

void isub(VectorUnit &vu, uint32_t instr)
{
    /*uint8_t dest = (instr >> 6) & 0x1F;
    uint8_t reg1 = (instr >> 11) & 0x1F;
    uint8_t reg2 = (instr >> 16) & 0x1F;*/
    vu.decoder.vi_write = (instr >> 6) & 0xF;
    vu.decoder.vi_read0 = _is_;
    vu.decoder.vi_read1 = _it_;
    lower_op = &VectorUnit::isub;
}

void iaddi(VectorUnit &vu, uint32_t instr)
{
    /*int8_t imm = (instr >> 6) & 0x1F;
    imm = ((int8_t)(imm << 3)) >> 3;
    uint8_t source = (instr >> 11) & 0x1F;
    uint8_t dest = (instr >> 16) & 0x1F;*/
    vu.decoder.vi_write = (instr >> 16) & 0xF;
    vu.decoder.vi_read0 = _is_;
    lower_op = &VectorUnit::iaddi;
}

void iand(VectorUnit &vu, uint32_t instr)
{
    /*uint8_t dest = (instr >> 6) & 0x1F;
    uint8_t reg1 = (instr >> 11) & 0x1F;
    uint8_t reg2 = (instr >> 16) & 0x1F;*/
    vu.decoder.vi_write = (instr >> 6) & 0xF;
    vu.decoder.vi_read0 = _is_;
    vu.decoder.vi_read1 = _it_;

    lower_op = &VectorUnit::iand;
}

void ior(VectorUnit &vu, uint32_t instr)
{
    /*uint8_t dest = (instr >> 6) & 0x1F;
    uint8_t reg1 = (instr >> 11) & 0x1F;
    uint8_t reg2 = (instr >> 16) & 0x1F;*/
    vu.decoder.vi_write = (instr >> 6) & 0xF;
    vu.decoder.vi_read0 = _is_;
    vu.decoder.vi_read1 = _it_;
    lower_op = &VectorUnit::ior;
}

void lower1_special(VectorUnit &vu, uint32_t instr)
{
    uint8_t op = (instr & 0x3) | ((instr >> 4) & 0x7C);
    switch (op)
    {
        case 0x30:
            move(vu, instr);
            break;
        case 0x31:
            mr32(vu, instr);
            break;
        case 0x34:
            lqi(vu, instr);
            break;
        case 0x35:
            sqi(vu, instr);
            break;
        case 0x36:
            lqd(vu, instr);
            break;
        case 0x37:
            sqd(vu, instr);
            break;
        case 0x38:
            div(vu, instr);
            break;
        case 0x39:
            vu_sqrt(vu, instr);
            break;
        case 0x3A:
            rsqrt(vu, instr);
            break;
        case 0x3B:
            waitq(vu, instr);
            break;
        case 0x3C:
            mtir(vu, instr);
            break;
        case 0x3D:
            mfir(vu, instr);
            break;
        case 0x3E:
            ilwr(vu, instr);
            break;
        case 0x3F:
            iswr(vu, instr);
            break;
        case 0x40:
            rnext(vu, instr);
            break;
        case 0x41:
            rget(vu, instr);
            break;
        case 0x42:
            rinit(vu, instr);
            break;
        case 0x43:
            rxor(vu, instr);
            break;
        case 0x64:
            mfp(vu, instr);
            break;
        case 0x68:
            xtop(vu, instr);
            break;
        case 0x69:
            xitop(vu, instr);
            break;
        case 0x6C:
            xgkick(vu, instr);
            break;
        case 0x70:
            esadd(vu, instr);
            break;
        case 0x71:
            ersadd(vu, instr);
            break;
        case 0x72:
            eleng(vu, instr);
            break;
        case 0x73:
            erleng(vu, instr);
            break;
        case 0x74:
            eatanxy(vu, instr);
            break;
        case 0x75:
            eatanxz(vu, instr);
            break;
        case 0x76:
            esum(vu, instr);
            break;
        case 0x78:
            esqrt(vu, instr);
            break;
        case 0x79:
            ersqrt(vu, instr);
            break;
        case 0x7A:
            ercpr(vu, instr);
            break;
        case 0x7B:
            //waitp executed before upper instruction
            lower_op = &VectorUnit::nop;
            break;
        case 0x7C:
            esin(vu, instr);
            break;
        case 0x7D:
            eatan(vu, instr);
            break;
        case 0x7E:
            eexp(vu, instr);
            break;
        default:
            unknown_op("lower1 special", instr, op);
    }
}

void move(VectorUnit &vu, uint32_t instr)
{
    uint8_t source = (instr >> 11) & 0x1F;
    uint8_t dest = (instr >> 16) & 0x1F;
    uint8_t field = (instr >> 21) & 0xF;

    vu.decoder.vf_write[1] = dest;
    vu.decoder.vf_write_field[1] = field;

    vu.decoder.vf_read0[1] = source;
    vu.decoder.vf_read0_field[1] = field;
    lower_op = &VectorUnit::move;
}

void mr32(VectorUnit &vu, uint32_t instr)
{
    uint8_t source = (instr >> 11) & 0x1F;
    uint8_t dest = (instr >> 16) & 0x1F;
    uint8_t field = (instr >> 21) & 0xF;

    vu.decoder.vf_write[1] = dest;
    vu.decoder.vf_write_field[1] = field;

    vu.decoder.vf_read0[1] = source;
    vu.decoder.vf_read0_field[1] = (field >> 1) | ((field & 0x1) << 3);
    lower_op = &VectorUnit::mr32;
}

void lqi(VectorUnit &vu, uint32_t instr)
{
    uint8_t is = (instr >> 11) & 0xF;
    uint8_t ft = (instr >> 16) & 0x1F;
    uint8_t field = (instr >> 21) & 0xF;
    vu.decoder.vf_write[1] = ft;
    vu.decoder.vf_write_field[1] = field;
    vu.decoder.vi_write = is;
    vu.decoder.vi_read0 = _is_;
    lower_op = &VectorUnit::lqi;
}

void sqi(VectorUnit& vu, uint32_t instr)
{
    uint32_t fs = (instr >> 11) & 0x1F;
    uint32_t it = (instr >> 16) & 0xF;
    uint8_t dest_field = (instr >> 21) & 0xF;
    vu.decoder.vf_read0[1] = fs;
    vu.decoder.vf_read0_field[1] = dest_field;
    vu.decoder.vi_write = it;
    vu.decoder.vi_read0 = _it_;
    lower_op = &VectorUnit::sqi;
}

void lqd(VectorUnit &vu, uint32_t instr)
{
    uint32_t is = (instr >> 11) & 0xF;
    uint32_t ft = (instr >> 16) & 0x1F;
    uint8_t dest_field = (instr >> 21) & 0xF;
    vu.decoder.vf_write[1] = ft;
    vu.decoder.vf_write_field[1] = dest_field;
    vu.decoder.vi_write = is;
    vu.decoder.vi_read0 = _is_;
    lower_op = &VectorUnit::lqd;
}

void sqd(VectorUnit &vu, uint32_t instr)
{
    uint32_t fs = (instr >> 11) & 0x1F;
    uint32_t it = (instr >> 16) & 0xF;
    uint8_t dest_field = (instr >> 21) & 0xF;
    vu.decoder.vf_read0[1] = fs;
    vu.decoder.vf_read0_field[1] = dest_field;
    vu.decoder.vi_write = it;
    vu.decoder.vi_read0 = _it_;
    lower_op = &VectorUnit::sqd;
}

void div(VectorUnit &vu, uint32_t instr)
{
    uint8_t reg1 = (instr >> 11) & 0x1F;
    uint8_t reg2 = (instr >> 16) & 0x1F;
    uint8_t fsf = (instr >> 21) & 0x3;
    uint8_t ftf = (instr >> 23) & 0x3;

    vu.decoder.vf_read0[1] = reg1;
    vu.decoder.vf_read0_field[1] = 1 << (3 - fsf);

    vu.decoder.vf_read1[1] = reg2;
    vu.decoder.vf_read1_field[1] = 1 << (3 - ftf);
    lower_op = &VectorUnit::div;
}

void vu_sqrt(VectorUnit &vu, uint32_t instr)
{
    uint8_t source = (instr >> 16) & 0x1F;
    uint8_t ftf = (instr >> 23) & 0x3;

    vu.decoder.vf_read0[1] = source;
    vu.decoder.vf_read0_field[1] = 1 << (3 - ftf);
    lower_op = &VectorUnit::vu_sqrt;
}

void rsqrt(VectorUnit &vu, uint32_t instr)
{
    uint8_t reg1 = (instr >> 11) & 0x1F;
    uint8_t reg2 = (instr >> 16) & 0x1F;
    uint8_t fsf = (instr >> 21) & 0x3;
    uint8_t ftf = (instr >> 23) & 0x3;

    vu.decoder.vf_read0[1] = reg1;
    vu.decoder.vf_read0_field[1] = 1 << (3 - fsf);

    vu.decoder.vf_read1[1] = reg2;
    vu.decoder.vf_read1_field[1] = 1 << (3 - ftf);
    lower_op = &VectorUnit::rsqrt;
}

void waitq(VectorUnit &vu, uint32_t instr)
{
    //waitq should always execute before upper
    vu.waitq(instr);
    lower_op = &VectorUnit::nop;
}

void mtir(VectorUnit &vu, uint32_t instr)
{
    uint32_t fs = (instr >> 11) & 0x1F;
    uint32_t it = (instr >> 16) & 0xF;
    uint8_t fsf = (instr >> 21) & 0x3;
    vu.decoder.vf_read0[1] = fs;
    vu.decoder.vf_read0_field[1] = 1 << (3 - fsf);
    vu.decoder.vi_write = it;
    lower_op = &VectorUnit::mtir;
}

void mfir(VectorUnit &vu, uint32_t instr)
{
    uint32_t is = (instr >> 11) & 0x1F;
    uint32_t ft = (instr >> 16) & 0x1F;
    uint8_t dest_field = (instr >> 21) & 0xF;
    vu.decoder.vi_read0 = is;
    vu.decoder.vf_write[1] = ft;
    vu.decoder.vf_write_field[1] = dest_field;
    vu.decoder.vi_read0 = _is_;
    lower_op = &VectorUnit::mfir;
}

void ilwr(VectorUnit &vu, uint32_t instr)
{
    /*uint8_t is = (instr >> 11) & 0x1F;
    uint8_t it = (instr >> 16) & 0x1F;
    uint8_t field = (instr >> 21) & 0xF;*/
    vu.decoder.vi_write = (instr >> 16) & 0xF;
    vu.decoder.vi_write_from_load = _it_;
    vu.decoder.vi_read0 = _is_;

    lower_op = &VectorUnit::ilwr;
}

void iswr(VectorUnit &vu, uint32_t instr)
{
    /*uint8_t is = (instr >> 11) & 0x1F;
    uint8_t it = (instr >> 16) & 0x1F;
    uint8_t field = (instr >> 21) & 0xF;*/
    vu.decoder.vi_read0 = _is_;
    vu.decoder.vi_read1 = _it_;

    lower_op = &VectorUnit::iswr;
}

void rnext(VectorUnit &vu, uint32_t instr)
{
    uint8_t dest = (instr >> 16) & 0x1F;
    uint8_t field = (instr >> 21) & 0xF;

    vu.decoder.vf_write[1] = dest;
    vu.decoder.vf_write_field[1] = field;
    lower_op = &VectorUnit::rnext;
}

void rget(VectorUnit &vu, uint32_t instr)
{
    uint8_t dest = (instr >> 16) & 0x1F;
    uint8_t field = (instr >> 21) & 0xF;

    vu.decoder.vf_write[1] = dest;
    vu.decoder.vf_write_field[1] = field;
    lower_op = &VectorUnit::rget;
}

void rinit(VectorUnit &vu, uint32_t instr)
{
    uint8_t source = (instr >> 11) & 0x1F;
    uint8_t fsf = (instr >> 21) & 0x3;

    vu.decoder.vf_read0[1] = source;
    vu.decoder.vf_read0_field[1] = 1 << (3 - fsf);
    lower_op = &VectorUnit::rinit;
}

void rxor(VectorUnit &vu, uint32_t instr)
{
    uint8_t source = (instr >> 11) & 0x1F;
    uint8_t fsf = (instr >> 21) & 0x3;

    vu.decoder.vf_read0[1] = source;
    vu.decoder.vf_read0_field[1] = 1 << (3 - fsf);
    lower_op = &VectorUnit::rxor;
}

void mfp(VectorUnit &vu, uint32_t instr)
{
    uint8_t dest = (instr >> 16) & 0x1F;
    uint8_t field = (instr >> 21) & 0xF;

    vu.decoder.vf_write[1] = dest;
    vu.decoder.vf_write_field[1] = field;
    lower_op = &VectorUnit::mfp;
}

void xtop(VectorUnit &vu, uint32_t instr)
{
    uint8_t it = (instr >> 16) & 0xF;
    vu.decoder.vi_write = it;
    lower_op = &VectorUnit::xtop;
}

void xitop(VectorUnit &vu, uint32_t instr)
{
    uint8_t it = (instr >> 16) & 0xF;
    vu.decoder.vi_write = it;
    lower_op = &VectorUnit::xitop;
}

void xgkick(VectorUnit &vu, uint32_t instr)
{
    uint8_t is = (instr >> 11) & 0x1F;
    vu.decoder.vi_read0 = _is_;
    lower_op = &VectorUnit::xgkick;
}

void esadd(VectorUnit &vu, uint32_t instr)
{
    uint8_t source = (instr >> 11) & 0x1F;

    vu.decoder.vf_read0[1] = source;
    vu.decoder.vf_read0_field[1] = 0xE; //xyz
    lower_op = &VectorUnit::esadd;
}

void ercpr(VectorUnit &vu, uint32_t instr)
{
    uint8_t source = (instr >> 11) & 0x1F;
    uint8_t fsf = (instr >> 21) & 0x3;

    vu.decoder.vf_read0[1] = source;
    vu.decoder.vf_read0_field[1] = 1 << (3 - fsf);
    lower_op = &VectorUnit::ercpr;
}

void ersadd(VectorUnit &vu, uint32_t instr)
{
    uint8_t source = (instr >> 11) & 0x1F;

    vu.decoder.vf_read0[1] = source;
    vu.decoder.vf_read0_field[1] = 0xE; //xyz
    lower_op = &VectorUnit::ersadd;
}

void eleng(VectorUnit &vu, uint32_t instr)
{
    uint8_t source = (instr >> 11) & 0x1F;

    vu.decoder.vf_read0[1] = source;
    vu.decoder.vf_read0_field[1] = 0xE; //xyz
    lower_op = &VectorUnit::eleng;
}

void erleng(VectorUnit &vu, uint32_t instr)
{
    uint8_t source = (instr >> 11) & 0x1F;

    vu.decoder.vf_read0[1] = source;
    vu.decoder.vf_read0_field[1] = 0xE; //xyz
    lower_op = &VectorUnit::erleng;
}

void esum(VectorUnit &vu, uint32_t instr)
{
    uint8_t source = (instr >> 11) & 0x1F;

    vu.decoder.vf_read0[1] = source;
    vu.decoder.vf_read0_field[1] = 0xF; //xyzw
    lower_op = &VectorUnit::esum;
}

void esqrt(VectorUnit &vu, uint32_t instr)
{
    uint8_t source = (instr >> 11) & 0x1F;
    uint8_t fsf = (instr >> 21) & 0x3;

    vu.decoder.vf_read0[1] = source;
    vu.decoder.vf_read0_field[1] = 1 << (3 - fsf);
    lower_op = &VectorUnit::esqrt;
}

void ersqrt(VectorUnit &vu, uint32_t instr)
{
    uint8_t source = (instr >> 11) & 0x1F;
    uint8_t fsf = (instr >> 21) & 0x3;

    vu.decoder.vf_read0[1] = source;
    vu.decoder.vf_read0_field[1] = 1 << (3 - fsf);
    lower_op = &VectorUnit::ersqrt;
}

void esin(VectorUnit &vu, uint32_t instr)
{
    uint8_t source = (instr >> 11) & 0x1F;
    uint8_t fsf = (instr >> 21) & 0x3;

    vu.decoder.vf_read0[1] = source;
    vu.decoder.vf_read0_field[1] = 1 << (3 - fsf);
    lower_op = &VectorUnit::esin;
}

void eatan(VectorUnit &vu, uint32_t instr)
{
    uint8_t source = (instr >> 11) & 0x1F;
    uint8_t fsf = (instr >> 21) & 0x3;

    vu.decoder.vf_read0[1] = source;
    vu.decoder.vf_read0_field[1] = 1 << (3 - fsf);
    lower_op = &VectorUnit::eatan;
}

void eatanxy(VectorUnit &vu, uint32_t instr)
{
    uint8_t source = (instr >> 11) & 0x1F;

    vu.decoder.vf_read0[1] = source;
    vu.decoder.vf_read0_field[1] = 0xC; //xy
    lower_op = &VectorUnit::eatanxy;
}

void eatanxz(VectorUnit &vu, uint32_t instr)
{
    uint8_t source = (instr >> 11) & 0x1F;

    vu.decoder.vf_read0[1] = source;
    vu.decoder.vf_read0_field[1] = 0xA; //xz
    lower_op = &VectorUnit::eatanxz;
}

void eexp(VectorUnit &vu, uint32_t instr)
{
    uint8_t source = (instr >> 11) & 0x1F;
    uint8_t fsf = (instr >> 21) & 0x3;

    vu.decoder.vf_read0[1] = source;
    vu.decoder.vf_read0_field[1] = 1 << (3 - fsf);
    lower_op = &VectorUnit::eexp;
}

void lower2(VectorUnit &vu, uint32_t instr)
{
    uint8_t op = (instr >> 25) & 0x7F;
    switch (op)
    {
        case 0x00:
            lq(vu, instr);
            break;
        case 0x01:
            sq(vu, instr);
            break;
        case 0x04:
            ilw(vu, instr);
            break;
        case 0x05:
            isw(vu, instr);
            break;
        case 0x08:
            iaddiu(vu, instr);
            break;
        case 0x09:
            isubiu(vu, instr);
            break;
        case 0x10:
            fceq(vu, instr);
            break;
        case 0x11:
            fcset(vu, instr);
            break;
        case 0x12:
            fcand(vu, instr);
            break;
        case 0x13:
            fcor(vu, instr);
            break;
        case 0x14:
            fseq(vu, instr);
            break;
        case 0x15:
            fsset(vu, instr);
            break;
        case 0x16:
            fsand(vu, instr);
            break;
        case 0x17:
            fsor(vu, instr);
            break;
        case 0x18:
            fmeq(vu, instr);
            break;
        case 0x1A:
            fmand(vu, instr);
            break;
        case 0x1B:
            fmor(vu, instr);
            break;
        case 0x1C:
            fcget(vu, instr);
            break;
        case 0x20:
            b(vu, instr);
            break;
        case 0x21:
            bal(vu, instr);
            break;
        case 0x24:
            jr(vu, instr);
            break;
        case 0x25:
            jalr(vu, instr);
            break;
        case 0x28:
            ibeq(vu, instr);
            break;
        case 0x29:
            ibne(vu, instr);
            break;
        case 0x2C:
            ibltz(vu, instr);
            break;
        case 0x2D:
            ibgtz(vu, instr);
            break;
        case 0x2E:
            iblez(vu, instr);
            break;
        case 0x2F:
            ibgez(vu, instr);
            break;
        default:
            unknown_op("lower2", instr, op);
    }
}

void lq(VectorUnit &vu, uint32_t instr)
{
    uint8_t is = (instr >> 11) & 0x1F;
    uint8_t ft = (instr >> 16) & 0x1F;
    uint8_t field = (instr >> 21) & 0xF;

    vu.decoder.vi_read0 = _is_;
    vu.decoder.vf_write[1] = ft;
    vu.decoder.vf_write_field[1] = field;
    lower_op = &VectorUnit::lq;
}

void sq(VectorUnit &vu, uint32_t instr)
{
    uint8_t fs = (instr >> 11) & 0x1F;
    uint8_t it = (instr >> 16) & 0x1F;
    uint8_t field = (instr >> 21) & 0xF;

    vu.decoder.vi_read0 = _it_;
    vu.decoder.vf_read0[1] = fs;
    vu.decoder.vf_read0_field[1] = field;
    lower_op = &VectorUnit::sq;
}

void ilw(VectorUnit &vu, uint32_t instr)
{
    /*int16_t imm = instr & 0x7FF;
    imm = ((int16_t)(imm << 5)) >> 5;
    imm *= 16;
    uint8_t is = (instr >> 11) & 0x1F;
    uint8_t it = (instr >> 16) & 0x1F;
    uint8_t field = (instr >> 21) & 0xF;*/
    vu.decoder.vi_write = (instr >> 16) & 0xF;
    vu.decoder.vi_write_from_load = _it_;
    vu.decoder.vi_read0 = _is_;

    lower_op = &VectorUnit::ilw;
}

void isw(VectorUnit &vu, uint32_t instr)
{
    /*int16_t imm = instr & 0x7FF;
    imm = ((int16_t)(imm << 5)) >> 5;
    imm *= 16;
    uint8_t is = (instr >> 11) & 0x1F;
    uint8_t it = (instr >> 16) & 0x1F;
    uint8_t field = (instr >> 21) & 0xF;*/
    vu.decoder.vi_read0 = _is_;
    vu.decoder.vi_read1 = _it_;

    lower_op = &VectorUnit::isw;
}

void iaddiu(VectorUnit &vu, uint32_t instr)
{
    /*uint16_t imm = instr & 0x7FF;
    imm |= ((instr >> 21) & 0xF) << 11;
    uint8_t source = (instr >> 11) & 0x1F;
    uint8_t dest = (instr >> 16) & 0x1F;*/
    vu.decoder.vi_write = (instr >> 16) & 0xF;
    vu.decoder.vi_read0 = _is_;
    lower_op = &VectorUnit::iaddiu;
}

void isubiu(VectorUnit &vu, uint32_t instr)
{
    /*uint16_t imm = instr & 0x7FF;
    imm |= ((instr >> 21) & 0xF) << 11;
    uint8_t source = (instr >> 11) & 0x1F;
    uint8_t dest = (instr >> 16) & 0x1F;*/
    vu.decoder.vi_write = (instr >> 16) & 0xF;
    vu.decoder.vi_read0 = _is_;
    lower_op = &VectorUnit::isubiu;
}

void fceq(VectorUnit &vu, uint32_t instr)
{
    lower_op = &VectorUnit::fceq;
}

void fcset(VectorUnit &vu, uint32_t instr)
{
    lower_op = &VectorUnit::fcset;
}

void fcand(VectorUnit &vu, uint32_t instr)
{
    lower_op = &VectorUnit::fcand;
}

void fcor(VectorUnit &vu, uint32_t instr)
{
    lower_op = &VectorUnit::fcor;
}

void fseq(VectorUnit &vu, uint32_t instr)
{
    lower_op = &VectorUnit::fseq;
}

void fsset(VectorUnit &vu, uint32_t instr)
{
    lower_op = &VectorUnit::fsset;
}

void fsand(VectorUnit &vu, uint32_t instr)
{
    lower_op = &VectorUnit::fsand;
}

void fsor(VectorUnit &vu, uint32_t instr)
{
    lower_op = &VectorUnit::fsor;
}

void fmeq(VectorUnit &vu, uint32_t instr)
{
    vu.decoder.vi_read0 = _is_;
    lower_op = &VectorUnit::fmeq;
}

void fmand(VectorUnit &vu, uint32_t instr)
{
    vu.decoder.vi_read0 = _is_;
    lower_op = &VectorUnit::fmand;
}

void fmor(VectorUnit &vu, uint32_t instr)
{
    vu.decoder.vi_read0 = _is_;
    lower_op = &VectorUnit::fmor;
}

void fcget(VectorUnit &vu, uint32_t instr)
{
    lower_op = &VectorUnit::fcget;
}

void b(VectorUnit &vu, uint32_t instr)
{
    lower_op = &VectorUnit::b;
}

void bal(VectorUnit &vu, uint32_t instr)
{
    lower_op = &VectorUnit::bal;
}

void jr(VectorUnit &vu, uint32_t instr)
{
    vu.decoder.vi_read0 = (instr >> 11) & 0xF;
    lower_op = &VectorUnit::jr;
}

void jalr(VectorUnit &vu, uint32_t instr)
{
    vu.decoder.vi_read0 = (instr >> 11) & 0xF;
    lower_op = &VectorUnit::jalr;
}

void ibeq(VectorUnit &vu, uint32_t instr)
{
    vu.decoder.vi_read0 = (instr >> 11) & 0xF;
    vu.decoder.vi_read1 = (instr >> 16) & 0xF;
    lower_op = &VectorUnit::ibeq;
}

void ibne(VectorUnit &vu, uint32_t instr)
{
    vu.decoder.vi_read0 = (instr >> 11) & 0xF;
    vu.decoder.vi_read1 = (instr >> 16) & 0xF;
    lower_op = &VectorUnit::ibne;
}

void ibltz(VectorUnit &vu, uint32_t instr)
{
    vu.decoder.vi_read0 = (instr >> 11) & 0xF;
    lower_op = &VectorUnit::ibltz;
}

void ibgtz(VectorUnit &vu, uint32_t instr)
{
    vu.decoder.vi_read0 = (instr >> 11) & 0xF;
    lower_op = &VectorUnit::ibgtz;
}

void iblez(VectorUnit &vu, uint32_t instr)
{
    vu.decoder.vi_read0 = (instr >> 11) & 0xF;
    lower_op = &VectorUnit::iblez;
}

void ibgez(VectorUnit &vu, uint32_t instr)
{
    vu.decoder.vi_read0 = (instr >> 11) & 0xF;
    lower_op = &VectorUnit::ibgez;
}

void unknown_op(const char *type, uint32_t instruction, uint16_t op)
{
    Errors::die("[VU_Interpreter] Unknown %s op $%02X: $%08X\n", type, op, instruction);
}

} //VU_Interpreter
