#include <cstdio>
#include <cstdlib>
#include "vu_interpreter.hpp"
#include "../errors.hpp"

void VU_Interpreter::interpret(VectorUnit &vu, uint32_t upper_instr, uint32_t lower_instr)
{
    upper(vu, upper_instr);

    //LOI
    if (upper_instr & (1 << 31))
        vu.set_I(lower_instr);
    else
        lower(vu, lower_instr);

    if (upper_instr & (1 << 30))
        vu.end_execution();
}

void VU_Interpreter::upper(VectorUnit &vu, uint32_t instr)
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

void VU_Interpreter::addbc(VectorUnit &vu, uint32_t instr)
{
    uint8_t bc = instr & 0x3;
    uint8_t dest = (instr >> 6) & 0x1F;
    uint8_t source = (instr >> 11) & 0x1F;
    uint8_t bc_reg = (instr >> 16) & 0x1F;
    uint8_t field = (instr >> 21) & 0xF;
    vu.addbc(bc, field, dest, source, bc_reg);
}

void VU_Interpreter::subbc(VectorUnit &vu, uint32_t instr)
{
    uint8_t bc = instr & 0x3;
    uint8_t dest = (instr >> 6) & 0x1F;
    uint8_t source = (instr >> 11) & 0x1F;
    uint8_t bc_reg = (instr >> 16) & 0x1F;
    uint8_t field = (instr >> 21) & 0xF;
    vu.subbc(bc, field, dest, source, bc_reg);
}

void VU_Interpreter::maddbc(VectorUnit &vu, uint32_t instr)
{
    uint8_t bc = instr & 0x3;
    uint8_t dest = (instr >> 6) & 0x1F;
    uint8_t source = (instr >> 11) & 0x1F;
    uint8_t bc_reg = (instr >> 16) & 0x1F;
    uint8_t field = (instr >> 21) & 0xF;
    vu.maddbc(bc, field, dest, source, bc_reg);
}

void VU_Interpreter::msubbc(VectorUnit &vu, uint32_t instr)
{
    uint8_t bc = instr & 0x3;
    uint8_t dest = (instr >> 6) & 0x1F;
    uint8_t source = (instr >> 11) & 0x1F;
    uint8_t bc_reg = (instr >> 16) & 0x1F;
    uint8_t field = (instr >> 21) & 0xF;
    vu.msubbc(bc, field, dest, source, bc_reg);
}

void VU_Interpreter::maxbc(VectorUnit &vu, uint32_t instr)
{
    uint8_t bc = instr & 0x3;
    uint8_t dest = (instr >> 6) & 0x1F;
    uint8_t source = (instr >> 11) & 0x1F;
    uint8_t bc_reg = (instr >> 16) & 0x1F;
    uint8_t field = (instr >> 21) & 0xF;
    vu.maxbc(bc, field, dest, source, bc_reg);
}

void VU_Interpreter::minibc(VectorUnit &vu, uint32_t instr)
{
    uint8_t bc = instr & 0x3;
    uint8_t dest = (instr >> 6) & 0x1F;
    uint8_t source = (instr >> 11) & 0x1F;
    uint8_t bc_reg = (instr >> 16) & 0x1F;
    uint8_t field = (instr >> 21) & 0xF;
    vu.minibc(bc, field, dest, source, bc_reg);
}

void VU_Interpreter::mulbc(VectorUnit &vu, uint32_t instr)
{
    uint8_t bc = instr & 0x3;
    uint8_t dest = (instr >> 6) & 0x1F;
    uint8_t source = (instr >> 11) & 0x1F;
    uint8_t bc_reg = (instr >> 16) & 0x1F;
    uint8_t field = (instr >> 21) & 0xF;
    vu.mulbc(bc, field, dest, source, bc_reg);
}

void VU_Interpreter::mulq(VectorUnit &vu, uint32_t instr)
{
    uint8_t dest = (instr >> 6) & 0x1F;
    uint8_t source = (instr >> 11) & 0x1F;
    uint8_t field = (instr >> 21) & 0xF;
    vu.mulq(field, dest, source);
}

void VU_Interpreter::muli(VectorUnit &vu, uint32_t instr)
{
    uint8_t dest = (instr >> 6) & 0x1F;
    uint8_t source = (instr >> 11) & 0x1F;
    uint8_t field = (instr >> 21) & 0xF;
    vu.muli(field, dest, source);
}

void VU_Interpreter::minii(VectorUnit &vu, uint32_t instr)
{
    uint8_t dest = (instr >> 6) & 0x1F;
    uint8_t source = (instr >> 11) & 0x1F;
    uint8_t field = (instr >> 21) & 0xF;
    vu.minii(field, dest, source);
}

void VU_Interpreter::addq(VectorUnit &vu, uint32_t instr)
{
    uint8_t dest = (instr >> 6) & 0x1F;
    uint8_t source = (instr >> 11) & 0x1F;
    uint8_t field = (instr >> 21) & 0xF;
    vu.addq(field, dest, source);
}

void VU_Interpreter::maddq(VectorUnit &vu, uint32_t instr)
{
    uint8_t dest = (instr >> 6) & 0x1F;
    uint8_t source = (instr >> 11) & 0x1F;
    uint8_t field = (instr >> 21) & 0xF;
    vu.maddq(field, dest, source);
}

void VU_Interpreter::addi(VectorUnit &vu, uint32_t instr)
{
    uint8_t dest = (instr >> 6) & 0x1F;
    uint8_t source = (instr >> 11) & 0x1F;
    uint8_t field = (instr >> 21) & 0xF;
    vu.addi(field, dest, source);
}

void VU_Interpreter::maddi(VectorUnit &vu, uint32_t instr)
{
    uint8_t dest = (instr >> 6) & 0x1F;
    uint8_t source = (instr >> 11) & 0x1F;
    uint8_t field = (instr >> 21) & 0xF;
    vu.maddi(field, dest, source);
}

void VU_Interpreter::subq(VectorUnit &vu, uint32_t instr)
{
    uint8_t dest = (instr >> 6) & 0x1F;
    uint8_t source = (instr >> 11) & 0x1F;
    uint8_t field = (instr >> 21) & 0xF;
    vu.subq(field, dest, source);
}

void VU_Interpreter::subi(VectorUnit &vu, uint32_t instr)
{
    uint8_t dest = (instr >> 6) & 0x1F;
    uint8_t source = (instr >> 11) & 0x1F;
    uint8_t field = (instr >> 21) & 0xF;
    vu.subi(field, dest, source);
}

void VU_Interpreter::msubi(VectorUnit &vu, uint32_t instr)
{
    uint8_t dest = (instr >> 6) & 0x1F;
    uint8_t source = (instr >> 11) & 0x1F;
    uint8_t field = (instr >> 21) & 0xF;
    vu.msubi(field, dest, source);
}

void VU_Interpreter::add(VectorUnit &vu, uint32_t instr)
{
    uint8_t dest = (instr >> 6) & 0x1F;
    uint8_t reg1 = (instr >> 11) & 0x1F;
    uint8_t reg2 = (instr >> 16) & 0x1F;
    uint8_t field = (instr >> 21) & 0xF;
    vu.add(field, dest, reg1, reg2);
}

void VU_Interpreter::madd(VectorUnit &vu, uint32_t instr)
{
    uint8_t dest = (instr >> 6) & 0x1F;
    uint8_t reg1 = (instr >> 11) & 0x1F;
    uint8_t reg2 = (instr >> 16) & 0x1F;
    uint8_t field = (instr >> 21) & 0xF;
    vu.madd(field, dest, reg1, reg2);
}

void VU_Interpreter::mul(VectorUnit &vu, uint32_t instr)
{
    uint8_t dest = (instr >> 6) & 0x1F;
    uint8_t reg1 = (instr >> 11) & 0x1F;
    uint8_t reg2 = (instr >> 16) & 0x1F;
    uint8_t field = (instr >> 21) & 0xF;
    vu.mul(field, dest, reg1, reg2);
}

void VU_Interpreter::max(VectorUnit &vu, uint32_t instr)
{
    uint8_t dest = (instr >> 6) & 0x1F;
    uint8_t reg1 = (instr >> 11) & 0x1F;
    uint8_t reg2 = (instr >> 16) & 0x1F;
    uint8_t field = (instr >> 21) & 0xF;
    vu.max(field, dest, reg1, reg2);
}

void VU_Interpreter::sub(VectorUnit &vu, uint32_t instr)
{
    uint8_t dest = (instr >> 6) & 0x1F;
    uint8_t reg1 = (instr >> 11) & 0x1F;
    uint8_t reg2 = (instr >> 16) & 0x1F;
    uint8_t field = (instr >> 21) & 0xF;
    vu.sub(field, dest, reg1, reg2);
}

void VU_Interpreter::msub(VectorUnit &vu, uint32_t instr)
{
    uint8_t dest = (instr >> 6) & 0x1F;
    uint8_t reg1 = (instr >> 11) & 0x1F;
    uint8_t reg2 = (instr >> 16) & 0x1F;
    uint8_t field = (instr >> 21) & 0xF;
    vu.msub(field, dest, reg1, reg2);
}

void VU_Interpreter::opmsub(VectorUnit &vu, uint32_t instr)
{
    uint8_t dest = (instr >> 6) & 0x1F;
    uint8_t reg1 = (instr >> 11) & 0x1F;
    uint8_t reg2 = (instr >> 16) & 0x1F;
    vu.opmsub(dest, reg1, reg2);
}

void VU_Interpreter::mini(VectorUnit &vu, uint32_t instr)
{
    uint8_t dest = (instr >> 6) & 0x1F;
    uint8_t reg1 = (instr >> 11) & 0x1F;
    uint8_t reg2 = (instr >> 16) & 0x1F;
    uint8_t field = (instr >> 21) & 0xF;
    vu.mini(field, dest, reg1, reg2);
}

void VU_Interpreter::upper_special(VectorUnit &vu, uint32_t instr)
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
        case 0x08:
        case 0x09:
        case 0x0A:
        case 0x0B:
            maddabc(vu, instr);
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
        case 0x1D:
            abs(vu, instr);
            break;
        case 0x1E:
            mulai(vu, instr);
            break;
        case 0x1F:
            clip(vu, instr);
            break;
        case 0x22:
            addai(vu, instr);
            break;
        case 0x23:
            maddai(vu, instr);
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
        case 0x2E:
            opmula(vu, instr);
            break;
        case 0x2F:
            /**
              * NOP
              */
            break;
        default:
            unknown_op("upper special", instr, op);
    }
}

void VU_Interpreter::addabc(VectorUnit &vu, uint32_t instr)
{
    uint8_t bc = instr & 0x3;
    uint8_t source = (instr >> 11) & 0x1F;
    uint8_t bc_reg = (instr >> 16) & 0x1F;
    uint8_t field = (instr >> 21) & 0xF;
    vu.addabc(bc, field, source, bc_reg);
}

void VU_Interpreter::maddabc(VectorUnit &vu, uint32_t instr)
{
    uint8_t bc = instr & 0x3;
    uint8_t source = (instr >> 11) & 0x1F;
    uint8_t bc_reg = (instr >> 16) & 0x1F;
    uint8_t field = (instr >> 21) & 0xF;
    vu.maddabc(bc, field, source, bc_reg);
}

void VU_Interpreter::itof0(VectorUnit &vu, uint32_t instr)
{
    uint8_t source = (instr >> 11) & 0x1F;
    uint8_t dest = (instr >> 16) & 0x1F;
    uint8_t field = (instr >> 21) & 0xF;
    vu.itof0(field, dest, source);
}

void VU_Interpreter::itof4(VectorUnit &vu, uint32_t instr)
{
    uint8_t source = (instr >> 11) & 0x1F;
    uint8_t dest = (instr >> 16) & 0x1F;
    uint8_t field = (instr >> 21) & 0xF;
    vu.itof4(field, dest, source);
}

void VU_Interpreter::itof12(VectorUnit &vu, uint32_t instr)
{
    uint8_t source = (instr >> 11) & 0x1F;
    uint8_t dest = (instr >> 16) & 0x1F;
    uint8_t field = (instr >> 21) & 0xF;
    vu.itof12(field, dest, source);
}

void VU_Interpreter::ftoi0(VectorUnit &vu, uint32_t instr)
{
    uint8_t source = (instr >> 11) & 0x1F;
    uint8_t dest = (instr >> 16) & 0x1F;
    uint8_t field = (instr >> 21) & 0xF;
    vu.ftoi0(field, dest, source);
}

void VU_Interpreter::ftoi4(VectorUnit &vu, uint32_t instr)
{
    uint8_t source = (instr >> 11) & 0x1F;
    uint8_t dest = (instr >> 16) & 0x1F;
    uint8_t field = (instr >> 21) & 0xF;
    vu.ftoi4(field, dest, source);
}

void VU_Interpreter::ftoi12(VectorUnit &vu, uint32_t instr)
{
    uint8_t source = (instr >> 11) & 0x1F;
    uint8_t dest = (instr >> 16) & 0x1F;
    uint8_t field = (instr >> 21) & 0xF;
    vu.ftoi12(field, dest, source);
}

void VU_Interpreter::ftoi15(VectorUnit &vu, uint32_t instr)
{
    uint8_t source = (instr >> 11) & 0x1F;
    uint8_t dest = (instr >> 16) & 0x1F;
    uint8_t field = (instr >> 21) & 0xF;
    vu.ftoi15(field, dest, source);
}

void VU_Interpreter::mulabc(VectorUnit &vu, uint32_t instr)
{
    uint8_t bc = instr & 0x3;
    uint8_t source = (instr >> 11) & 0x1F;
    uint8_t bc_reg = (instr >> 16) & 0x1F;
    uint8_t field = (instr >> 21) & 0xF;
    vu.mulabc(bc, field, source, bc_reg);
}

void VU_Interpreter::abs(VectorUnit &vu, uint32_t instr)
{
    uint8_t source = (instr >> 11) & 0x1F;
    uint8_t dest = (instr >> 16) & 0x1F;
    uint8_t field = (instr >> 21) & 0xF;
    vu.abs(field, dest, source);
}

void VU_Interpreter::mulai(VectorUnit &vu, uint32_t instr)
{
    uint8_t source = (instr >> 11) & 0x1F;
    uint8_t field = (instr >> 21) & 0xF;
    vu.mulai(field, source);
}

void VU_Interpreter::clip(VectorUnit &vu, uint32_t instr)
{
    uint8_t reg1 = (instr >> 11) & 0x1F;
    uint8_t reg2 = (instr >> 16) & 0x1F;
    vu.clip(reg1, reg2);
}

void VU_Interpreter::addai(VectorUnit &vu, uint32_t instr)
{
    uint8_t source = (instr >> 11) & 0x1F;
    uint8_t field = (instr >> 21) & 0xF;
    vu.addai(field, source);
}

void VU_Interpreter::maddai(VectorUnit &vu, uint32_t instr)
{
    uint8_t source = (instr >> 11) & 0x1F;
    uint8_t field = (instr >> 21) & 0xF;
    vu.maddai(field, source);
}

void VU_Interpreter::subai(VectorUnit &vu, uint32_t instr)
{
    uint8_t source = (instr >> 11) & 0x1F;
    uint8_t field = (instr >> 21) & 0xF;
    vu.subai(field, source);
}

void VU_Interpreter::msubai(VectorUnit &vu, uint32_t instr)
{
    uint8_t source = (instr >> 11) & 0x1F;
    uint8_t field = (instr >> 21) & 0xF;
    vu.msubai(field, source);
}

void VU_Interpreter::mula(VectorUnit &vu, uint32_t instr)
{
    uint8_t reg1 = (instr >> 11) & 0x1F;
    uint8_t reg2 = (instr >> 16) & 0x1F;
    uint8_t field = (instr >> 21) & 0xF;
    vu.mula(field, reg1, reg2);
}

void VU_Interpreter::adda(VectorUnit &vu, uint32_t instr)
{
    uint8_t reg1 = (instr >> 11) & 0x1F;
    uint8_t reg2 = (instr >> 16) & 0x1F;
    uint8_t field = (instr >> 21) & 0xF;
    vu.adda(field, reg1, reg2);
}

void VU_Interpreter::madda(VectorUnit &vu, uint32_t instr)
{
    uint8_t reg1 = (instr >> 11) & 0x1F;
    uint8_t reg2 = (instr >> 16) & 0x1F;
    uint8_t field = (instr >> 21) & 0xF;
    vu.madda(field, reg1, reg2);
}

void VU_Interpreter::opmula(VectorUnit &vu, uint32_t instr)
{
    uint8_t reg1 = (instr >> 11) & 0x1F;
    uint8_t reg2 = (instr >> 16) & 0x1F;
    vu.opmula(reg1, reg2);
}

void VU_Interpreter::lower(VectorUnit &vu, uint32_t instr)
{
    if (instr & (1 << 31))
        lower1(vu, instr);
    else
        lower2(vu, instr);
}

void VU_Interpreter::lower1(VectorUnit &vu, uint32_t instr)
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

void VU_Interpreter::iadd(VectorUnit &vu, uint32_t instr)
{
    uint8_t dest = (instr >> 6) & 0x1F;
    uint8_t reg1 = (instr >> 11) & 0x1F;
    uint8_t reg2 = (instr >> 16) & 0x1F;
    vu.iadd(dest, reg1, reg2);
}

void VU_Interpreter::isub(VectorUnit &vu, uint32_t instr)
{
    uint8_t dest = (instr >> 6) & 0x1F;
    uint8_t reg1 = (instr >> 11) & 0x1F;
    uint8_t reg2 = (instr >> 16) & 0x1F;
    vu.isub(dest, reg1, reg2);
}

void VU_Interpreter::iaddi(VectorUnit &vu, uint32_t instr)
{
    int8_t imm = (instr >> 6) & 0x1F;
    imm = ((int8_t)(imm << 3)) >> 3;
    uint8_t source = (instr >> 11) & 0x1F;
    uint8_t dest = (instr >> 16) & 0x1F;

    vu.iaddi(dest, source, imm);
}

void VU_Interpreter::iand(VectorUnit &vu, uint32_t instr)
{
    uint8_t dest = (instr >> 6) & 0x1F;
    uint8_t reg1 = (instr >> 11) & 0x1F;
    uint8_t reg2 = (instr >> 16) & 0x1F;
    vu.iand(dest, reg1, reg2);
}

void VU_Interpreter::ior(VectorUnit &vu, uint32_t instr)
{
    uint8_t dest = (instr >> 6) & 0x1F;
    uint8_t reg1 = (instr >> 11) & 0x1F;
    uint8_t reg2 = (instr >> 16) & 0x1F;
    vu.ior(dest, reg1, reg2);
}

void VU_Interpreter::lower1_special(VectorUnit &vu, uint32_t instr)
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
        case 0x72:
            eleng(vu, instr);
            break;
        case 0x73:
            erleng(vu, instr);
            break;
        case 0x78:
            esqrt(vu, instr);
            break;
        case 0x7B:
            /**
              * WAITP
              */
            break;
        default:
            unknown_op("lower1 special", instr, op);
    }
}

void VU_Interpreter::move(VectorUnit &vu, uint32_t instr)
{
    uint8_t source = (instr >> 11) & 0x1F;
    uint8_t dest = (instr >> 16) & 0x1F;
    uint8_t field = (instr >> 21) & 0xF;
    vu.move(field, dest, source);
}

void VU_Interpreter::mr32(VectorUnit &vu, uint32_t instr)
{
    uint8_t source = (instr >> 11) & 0x1F;
    uint8_t dest = (instr >> 16) & 0x1F;
    uint8_t field = (instr >> 21) & 0xF;
    vu.mr32(field, dest, source);
}

void VU_Interpreter::lqi(VectorUnit &vu, uint32_t instr)
{
    uint8_t is = (instr >> 11) & 0x1F;
    uint8_t ft = (instr >> 16) & 0x1F;
    uint8_t field = (instr >> 21) & 0xF;
    vu.lqi(field, ft, is);
}

void VU_Interpreter::sqi(VectorUnit& vu, uint32_t instr)
{
    uint32_t fs = (instr >> 11) & 0x1F;
    uint32_t it = (instr >> 16) & 0x1F;
    uint8_t dest_field = (instr >> 21) & 0xF;
    vu.sqi(dest_field, fs, it);
}

void VU_Interpreter::lqd(VectorUnit &vu, uint32_t instr)
{
    uint32_t fs = (instr >> 11) & 0x1F;
    uint32_t it = (instr >> 16) & 0x1F;
    uint8_t dest_field = (instr >> 21) & 0xF;
    vu.lqd(dest_field, fs, it);
}

void VU_Interpreter::sqd(VectorUnit &vu, uint32_t instr)
{
    uint32_t fs = (instr >> 11) & 0x1F;
    uint32_t it = (instr >> 16) & 0x1F;
    uint8_t dest_field = (instr >> 21) & 0xF;
    vu.sqd(dest_field, fs, it);
}

void VU_Interpreter::div(VectorUnit &vu, uint32_t instr)
{
    uint8_t reg1 = (instr >> 11) & 0x1F;
    uint8_t reg2 = (instr >> 16) & 0x1F;
    uint8_t fsf = (instr >> 21) & 0x3;
    uint8_t ftf = (instr >> 23) & 0x3;
    vu.div(ftf, fsf, reg1, reg2);
}

void VU_Interpreter::waitq(VectorUnit &vu, uint32_t instr)
{
    vu.waitq();
}

void VU_Interpreter::mtir(VectorUnit &vu, uint32_t instr)
{
    uint32_t fs = (instr >> 11) & 0x1F;
    uint32_t it = (instr >> 16) & 0x1F;
    uint8_t fsf = (instr >> 21) & 0x3;
    vu.mtir(fsf, it, fs);
}

void VU_Interpreter::mfir(VectorUnit &vu, uint32_t instr)
{
    uint32_t is = (instr >> 11) & 0x1F;
    uint32_t ft = (instr >> 16) & 0x1F;
    uint8_t dest_field = (instr >> 21) & 0xF;
    vu.mfir(dest_field, ft, is);
}

void VU_Interpreter::ilwr(VectorUnit &vu, uint32_t instr)
{
    uint8_t is = (instr >> 11) & 0x1F;
    uint8_t it = (instr >> 16) & 0x1F;
    uint8_t field = (instr >> 21) & 0xF;
    vu.ilwr(field, it, is);
}

void VU_Interpreter::iswr(VectorUnit &vu, uint32_t instr)
{
    uint8_t is = (instr >> 11) & 0x1F;
    uint8_t it = (instr >> 16) & 0x1F;
    uint8_t field = (instr >> 21) & 0xF;
    vu.iswr(field, it, is);
}

void VU_Interpreter::rnext(VectorUnit &vu, uint32_t instr)
{
    uint8_t dest = (instr >> 16) & 0x1F;
    uint8_t field = (instr >> 21) & 0xF;
    vu.rnext(field, dest);
}

void VU_Interpreter::rget(VectorUnit &vu, uint32_t instr)
{
    uint8_t dest = (instr >> 16) & 0x1F;
    uint8_t field = (instr >> 21) & 0xF;
    vu.rget(field, dest);
}

void VU_Interpreter::rinit(VectorUnit &vu, uint32_t instr)
{
    uint8_t source = (instr >> 11) & 0x1F;
    uint8_t fsf = (instr >> 21) & 0x3;
    vu.rinit(fsf, source);
}

void VU_Interpreter::mfp(VectorUnit &vu, uint32_t instr)
{
    uint8_t dest = (instr >> 16) & 0x1F;
    uint8_t field = (instr >> 21) & 0xF;
    vu.mfp(field, dest);
}

void VU_Interpreter::xtop(VectorUnit &vu, uint32_t instr)
{
    uint8_t it = (instr >> 16) & 0x1F;
    vu.xtop(it);
}

void VU_Interpreter::xitop(VectorUnit &vu, uint32_t instr)
{
    uint8_t it = (instr >> 16) & 0x1F;
    vu.xitop(it);
}

void VU_Interpreter::xgkick(VectorUnit &vu, uint32_t instr)
{
    uint8_t is = (instr >> 11) & 0x1F;
    vu.xgkick(is);
}

void VU_Interpreter::eleng(VectorUnit &vu, uint32_t instr)
{
    uint8_t source = (instr >> 11) & 0x1F;
    vu.eleng(source);
}

void VU_Interpreter::erleng(VectorUnit &vu, uint32_t instr)
{
    uint8_t source = (instr >> 11) & 0x1F;
    vu.erleng(source);
}

void VU_Interpreter::esqrt(VectorUnit &vu, uint32_t instr)
{
    uint8_t source = (instr >> 11) & 0x1F;
    uint8_t fsf = (instr >> 21) & 0x3;
    vu.esqrt(fsf, source);
}

void VU_Interpreter::lower2(VectorUnit &vu, uint32_t instr)
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
        case 0x11:
            fcset(vu, instr);
            break;
        case 0x12:
            fcand(vu, instr);
            break;
        case 0x13:
            fcor(vu, instr);
            break;
        case 0x1A:
            fmand(vu, instr);
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

void VU_Interpreter::lq(VectorUnit &vu, uint32_t instr)
{
    int32_t imm = instr & 0x7FF;
    imm = ((int16_t)(imm << 5)) >> 5;
    imm *= 16;
    uint8_t is = (instr >> 11) & 0x1F;
    uint8_t ft = (instr >> 16) & 0x1F;
    uint8_t field = (instr >> 21) & 0xF;
    vu.lq(field, ft, is, imm);
}

void VU_Interpreter::sq(VectorUnit &vu, uint32_t instr)
{
    int32_t imm = instr & 0x7FF;
    imm = ((int16_t)(imm << 5)) >> 5;
    imm *= 16;
    uint8_t fs = (instr >> 11) & 0x1F;
    uint8_t it = (instr >> 16) & 0x1F;
    uint8_t field = (instr >> 21) & 0xF;
    vu.sq(field, fs, it, imm);
}

void VU_Interpreter::ilw(VectorUnit &vu, uint32_t instr)
{
    int32_t imm = instr & 0x7FF;
    imm = ((int16_t)(imm << 5)) >> 5;
    imm *= 16;
    uint8_t is = (instr >> 11) & 0x1F;
    uint8_t it = (instr >> 16) & 0x1F;
    uint8_t field = (instr >> 21) & 0xF;
    vu.ilw(field, it, is, imm);
}

void VU_Interpreter::isw(VectorUnit &vu, uint32_t instr)
{
    int32_t imm = instr & 0x7FF;
    imm = ((int16_t)(imm << 5)) >> 5;
    imm *= 16;
    uint8_t is = (instr >> 11) & 0x1F;
    uint8_t it = (instr >> 16) & 0x1F;
    uint8_t field = (instr >> 21) & 0xF;
    vu.isw(field, it, is, imm);
}

void VU_Interpreter::iaddiu(VectorUnit &vu, uint32_t instr)
{
    uint16_t imm = instr & 0x7FF;
    imm |= ((instr >> 21) & 0xF) << 11;
    uint8_t source = (instr >> 11) & 0x1F;
    uint8_t dest = (instr >> 16) & 0x1F;
    vu.iaddiu(dest, source, imm);
}

void VU_Interpreter::isubiu(VectorUnit &vu, uint32_t instr)
{
    uint16_t imm = instr & 0x7FF;
    imm |= ((instr >> 21) & 0xF) << 11;
    uint8_t source = (instr >> 11) & 0x1F;
    uint8_t dest = (instr >> 16) & 0x1F;
    vu.isubiu(dest, source, imm);
}

void VU_Interpreter::fcset(VectorUnit &vu, uint32_t instr)
{
    vu.fcset(instr & 0xFFFFFF);
}

void VU_Interpreter::fcand(VectorUnit &vu, uint32_t instr)
{
    vu.fcand(instr & 0xFFFFFF);
}

void VU_Interpreter::fcor(VectorUnit &vu, uint32_t instr)
{
    vu.fcor(instr & 0xFFFFFF);
}

void VU_Interpreter::fmand(VectorUnit &vu, uint32_t instr)
{
    uint8_t is = (instr >> 11) & 0x1F;
    uint8_t it = (instr >> 16) & 0x1F;
    vu.fmand(it, is);
}

void VU_Interpreter::fcget(VectorUnit &vu, uint32_t instr)
{
    uint8_t it = (instr >> 16) & 0x1F;
    vu.fcget(it);
}

void VU_Interpreter::b(VectorUnit &vu, uint32_t instr)
{
    int32_t imm = instr & 0x7FF;
    imm = ((int16_t)(imm << 5)) >> 5;
    imm *= 8;
    vu.branch(true, imm);
}

void VU_Interpreter::bal(VectorUnit &vu, uint32_t instr)
{
    int32_t imm = instr & 0x7FF;
    imm = ((int16_t)(imm << 5)) >> 5;
    imm *= 8;

    uint8_t link_reg = (instr >> 16) & 0x1F;
    vu.set_int(link_reg, (vu.get_PC() + 16) / 8);
    vu.branch(true, imm);
}

void VU_Interpreter::jr(VectorUnit &vu, uint32_t instr)
{
    uint8_t addr_reg = (instr >> 11) & 0x1F;
    uint16_t addr = vu.get_int(addr_reg) * 8;
    vu.jp(addr);
}

void VU_Interpreter::jalr(VectorUnit &vu, uint32_t instr)
{
    uint8_t addr_reg = (instr >> 11) & 0x1F;
    uint16_t addr = vu.get_int(addr_reg) * 8;

    uint8_t link_reg = (instr >> 16) & 0x1F;
    vu.set_int(link_reg, (vu.get_PC() + 16) / 8);
    vu.jp(addr);
}

void VU_Interpreter::ibeq(VectorUnit &vu, uint32_t instr)
{
    int32_t imm = instr & 0x7FF;
    imm = ((int16_t)(imm << 5)) >> 5;
    imm *= 8;

    uint16_t is = (instr >> 11) & 0x1F;
    uint16_t it = (instr >> 16) & 0x1F;
    is = vu.get_int(is);
    it = vu.get_int(it);
    vu.branch(is == it, imm);
}

void VU_Interpreter::ibne(VectorUnit &vu, uint32_t instr)
{
    int32_t imm = instr & 0x7FF;
    imm = ((int16_t)(imm << 5)) >> 5;
    imm *= 8;

    uint16_t is = (instr >> 11) & 0x1F;
    uint16_t it = (instr >> 16) & 0x1F;
    is = vu.get_int(is);
    it = vu.get_int(it);
    vu.branch(is != it, imm);
}

void VU_Interpreter::ibltz(VectorUnit &vu, uint32_t instr)
{
    int32_t imm = instr & 0x7FF;
    imm = ((int16_t)(imm << 5)) >> 5;
    imm *= 8;

    int16_t reg = (int16_t)vu.get_int((instr >> 11) & 0x1F);
    vu.branch(reg < 0, imm);
}

void VU_Interpreter::ibgtz(VectorUnit &vu, uint32_t instr)
{
    int32_t imm = instr & 0x7FF;
    imm = ((int16_t)(imm << 5)) >> 5;
    imm *= 8;

    int16_t reg = (int16_t)vu.get_int((instr >> 11) & 0x1F);
    vu.branch(reg > 0, imm);
}

void VU_Interpreter::iblez(VectorUnit &vu, uint32_t instr)
{
    int32_t imm = instr & 0x7FF;
    imm = ((int16_t)(imm << 5)) >> 5;
    imm *= 8;

    int16_t reg = (int16_t)vu.get_int((instr >> 11) & 0x1F);
    vu.branch(reg <= 0, imm);
}

void VU_Interpreter::ibgez(VectorUnit &vu, uint32_t instr)
{
    int32_t imm = instr & 0x7FF;
    imm = ((int16_t)(imm << 5)) >> 5;
    imm *= 8;

    int16_t reg = (int16_t)vu.get_int((instr >> 11) & 0x1F);
    vu.branch(reg >= 0, imm);
}

void VU_Interpreter::unknown_op(const char *type, uint32_t instruction, uint16_t op)
{
    Errors::die("[VU_Interpreter] Unknown %s op $%02X: $%08X\n", type, op, instruction);
}
