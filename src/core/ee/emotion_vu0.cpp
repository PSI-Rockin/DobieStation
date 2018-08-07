#include "emotioninterpreter.hpp"
#include "vu.hpp"

void EmotionInterpreter::cop2_special(VectorUnit &vu0, uint32_t instruction)
{
    /**
      * FIXME: IMPORTANT!
      * We're flushing pipelines for VU0, as accurately handling COP2's pipelining is painful.
      * I don't yet have a good solution for this that doesn't murder performance.
      */
    vu0.flush_pipes();
    uint8_t op = instruction & 0x3F;

    switch (op)
    {
        case 0x00:
        case 0x01:
        case 0x02:
        case 0x03:
            cop2_vaddbc(vu0, instruction);
            break;
        case 0x04:
        case 0x05:
        case 0x06:
        case 0x07:
            cop2_vsubbc(vu0, instruction);
            break;
        case 0x08:
        case 0x09:
        case 0x0A:
        case 0x0B:
            cop2_vmaddbc(vu0, instruction);
            break;
        case 0x0C:
        case 0x0D:
        case 0x0E:
        case 0x0F:
            cop2_vmsubbc(vu0, instruction);
            break;
        case 0x10:
        case 0x11:
        case 0x12:
        case 0x13:
            cop2_vmaxbc(vu0, instruction);
            break;
        case 0x14:
        case 0x15:
        case 0x16:
        case 0x17:
            cop2_vminibc(vu0, instruction);
            break;
        case 0x18:
        case 0x19:
        case 0x1A:
        case 0x1B:
            cop2_vmulbc(vu0, instruction);
            break;
        case 0x1C:
            cop2_vmulq(vu0, instruction);
            break;
        case 0x1E:
            cop2_vmuli(vu0, instruction);
            break;
        case 0x1F:
            cop2_vminii(vu0, instruction);
            break;
        case 0x20:
            cop2_vaddq(vu0, instruction);
            break;
        case 0x21:
            cop2_vmaddq(vu0, instruction);
            break;
        case 0x24:
            cop2_vsubq(vu0, instruction);
            break;
        case 0x26:
            cop2_vsubi(vu0, instruction);
            break;
        case 0x27:
            cop2_vmsubi(vu0, instruction);
            break;
        case 0x28:
            cop2_vadd(vu0, instruction);
            break;
        case 0x29:
            cop2_vmadd(vu0, instruction);
            break;
        case 0x2A:
            cop2_vmul(vu0, instruction);
            break;
        case 0x2B:
            cop2_vmax(vu0, instruction);
            break;
        case 0x2C:
            cop2_vsub(vu0, instruction);
            break;
        case 0x2E:
            cop2_vopmsub(vu0, instruction);
            break;
        case 0x2F:
            cop2_vmini(vu0, instruction);
            break;
        case 0x30:
            cop2_viadd(vu0, instruction);
            break;
        case 0x31:
            cop2_visub(vu0, instruction);
            break;
        case 0x32:
            cop2_viaddi(vu0, instruction);
            break;
        case 0x34:
            cop2_viand(vu0, instruction);
            break;
        case 0x38:
            cop2_vcallms(vu0, instruction);
            break;
        case 0x39:
            cop2_vcallmsr(vu0, instruction);
            break;
        case 0x3C:
        case 0x3D:
        case 0x3E:
        case 0x3F:
            cop2_special2(vu0, instruction);
            break;
        default:
            unknown_op("cop2 special", instruction, op);
    }
}

void EmotionInterpreter::cop2_bc2(EmotionEngine &cpu, uint32_t instruction)
{
    const static bool likely[] = { false, false, true, true };
    const static bool op_true[] = { false, true, false, true };
    int32_t offset = ((int16_t)(instruction & 0xFFFF)) << 2;
    uint8_t op = (instruction >> 16) & 0x1F;
    if (op > 3)
    {
        unknown_op("bc2", instruction, op);
    }
    cpu.cop2_bc2(offset, op_true[op], likely[op]);
}

void EmotionInterpreter::cop2_vaddbc(VectorUnit &vu0, uint32_t instruction)
{
    uint8_t bc = instruction & 0x3;
    uint8_t dest = (instruction >> 6) & 0x1F;
    uint8_t source = (instruction >> 11) & 0x1F;
    uint8_t bc_reg = (instruction >> 16) & 0x1F;
    uint8_t field = (instruction >> 21) & 0xF;
    vu0.addbc(bc, field, dest, source, bc_reg);
}

void EmotionInterpreter::cop2_vsubbc(VectorUnit &vu0, uint32_t instruction)
{
    uint8_t bc = instruction & 0x3;
    uint8_t dest = (instruction >> 6) & 0x1F;
    uint8_t source = (instruction >> 11) & 0x1F;
    uint8_t bc_reg = (instruction >> 16) & 0x1F;
    uint8_t field = (instruction >> 21) & 0xF;
    vu0.subbc(bc, field, dest, source, bc_reg);
}

void EmotionInterpreter::cop2_vmaddbc(VectorUnit &vu0, uint32_t instruction)
{
    uint8_t bc = instruction & 0x3;
    uint8_t dest = (instruction >> 6) & 0x1F;
    uint8_t source = (instruction >> 11) & 0x1F;
    uint8_t bc_reg = (instruction >> 16) & 0x1F;
    uint8_t field = (instruction >> 21) & 0xF;
    vu0.maddbc(bc, field, dest, source, bc_reg);
}

void EmotionInterpreter::cop2_vmsubbc(VectorUnit &vu0, uint32_t instruction)
{
    uint8_t bc = instruction & 0x3;
    uint8_t dest = (instruction >> 6) & 0x1F;
    uint8_t source = (instruction >> 11) & 0x1F;
    uint8_t bc_reg = (instruction >> 16) & 0x1F;
    uint8_t field = (instruction >> 21) & 0xF;
    vu0.msubbc(bc, field, dest, source, bc_reg);
}

void EmotionInterpreter::cop2_vmaxbc(VectorUnit &vu0, uint32_t instruction)
{
    uint8_t bc = instruction & 0x3;
    uint8_t dest = (instruction >> 6) & 0x1F;
    uint8_t source = (instruction >> 11) & 0x1F;
    uint8_t bc_reg = (instruction >> 16) & 0x1F;
    uint8_t field = (instruction >> 21) & 0xF;
    vu0.maxbc(bc, field, dest, source, bc_reg);
}

void EmotionInterpreter::cop2_vminibc(VectorUnit &vu0, uint32_t instruction)
{
    uint8_t bc = instruction & 0x3;
    uint8_t dest = (instruction >> 6) & 0x1F;
    uint8_t source = (instruction >> 11) & 0x1F;
    uint8_t bc_reg = (instruction >> 16) & 0x1F;
    uint8_t field = (instruction >> 21) & 0xF;
    vu0.minibc(bc, field, dest, source, bc_reg);
}

void EmotionInterpreter::cop2_vmulbc(VectorUnit &vu0, uint32_t instruction)
{
    uint8_t bc = instruction & 0x3;
    uint8_t dest = (instruction >> 6) & 0x1F;
    uint8_t source = (instruction >> 11) & 0x1F;
    uint8_t bc_reg = (instruction >> 16) & 0x1F;
    uint8_t field = (instruction >> 21) & 0xF;
    vu0.mulbc(bc, field, dest, source, bc_reg);
}

void EmotionInterpreter::cop2_vmulq(VectorUnit &vu0, uint32_t instruction)
{
    uint8_t dest = (instruction >> 6) & 0x1F;
    uint8_t source = (instruction >> 11) & 0x1F;
    uint8_t field = (instruction >> 21) & 0xF;
    vu0.mulq(field, dest, source);
}

void EmotionInterpreter::cop2_vmuli(VectorUnit &vu0, uint32_t instruction)
{
    uint8_t dest = (instruction >> 6) & 0x1F;
    uint8_t source = (instruction >> 11) & 0x1F;
    uint8_t field = (instruction >> 21) & 0xF;
    vu0.muli(field, dest, source);
}

void EmotionInterpreter::cop2_vminii(VectorUnit &vu0, uint32_t instruction)
{
    uint8_t dest = (instruction >> 6) & 0x1F;
    uint8_t source = (instruction >> 11) & 0x1F;
    uint8_t field = (instruction >> 21) & 0xF;
    vu0.minii(field, dest, source);
}

void EmotionInterpreter::cop2_vaddq(VectorUnit &vu0, uint32_t instruction)
{
    uint8_t dest = (instruction >> 6) & 0x1F;
    uint8_t source = (instruction >> 11) & 0x1F;
    uint8_t field = (instruction >> 21) & 0xF;
    vu0.addq(field, dest, source);
}

void EmotionInterpreter::cop2_vmaddq(VectorUnit &vu0, uint32_t instruction)
{
    uint8_t dest = (instruction >> 6) & 0x1F;
    uint8_t source = (instruction >> 11) & 0x1F;
    uint8_t field = (instruction >> 21) & 0xF;
    vu0.maddq(field, dest, source);
}

void EmotionInterpreter::cop2_vsubq(VectorUnit &vu0, uint32_t instruction)
{
    uint8_t dest = (instruction >> 6) & 0x1F;
    uint8_t source = (instruction >> 11) & 0x1F;
    uint8_t field = (instruction >> 21) & 0xF;
    vu0.subq(field, dest, source);
}

void EmotionInterpreter::cop2_vsubi(VectorUnit &vu0, uint32_t instruction)
{
    uint8_t dest = (instruction >> 6) & 0x1F;
    uint8_t source = (instruction >> 11) & 0x1F;
    uint8_t field = (instruction >> 21) & 0xF;
    vu0.subi(field, dest, source);
}

void EmotionInterpreter::cop2_vmsubi(VectorUnit &vu0, uint32_t instruction)
{
    uint8_t dest = (instruction >> 6) & 0x1F;
    uint8_t source = (instruction >> 11) & 0x1F;
    uint8_t field = (instruction >> 21) & 0xF;
    vu0.msubi(field, dest, source);
}

void EmotionInterpreter::cop2_vadd(VectorUnit &vu0, uint32_t instruction)
{
    uint8_t dest = (instruction >> 6) & 0x1F;
    uint8_t reg1 = (instruction >> 11) & 0x1F;
    uint8_t reg2 = (instruction >> 16) & 0x1F;
    uint8_t field = (instruction >> 21) & 0xF;
    vu0.add(field, dest, reg1, reg2);
}

void EmotionInterpreter::cop2_vmadd(VectorUnit &vu0, uint32_t instruction)
{
    uint8_t dest = (instruction >> 6) & 0x1F;
    uint8_t reg1 = (instruction >> 11) & 0x1F;
    uint8_t reg2 = (instruction >> 16) & 0x1F;
    uint8_t field = (instruction >> 21) & 0xF;
    vu0.madd(field, dest, reg1, reg2);
}

void EmotionInterpreter::cop2_vmul(VectorUnit &vu0, uint32_t instruction)
{
    uint8_t dest = (instruction >> 6) & 0x1F;
    uint8_t reg1 = (instruction >> 11) & 0x1F;
    uint8_t reg2 = (instruction >> 16) & 0x1F;
    uint8_t field = (instruction >> 21) & 0xF;
    vu0.mul(field, dest, reg1, reg2);
}

void EmotionInterpreter::cop2_vmax(VectorUnit &vu0, uint32_t instruction)
{
    uint8_t dest = (instruction >> 6) & 0x1F;
    uint8_t reg1 = (instruction >> 11) & 0x1F;
    uint8_t reg2 = (instruction >> 16) & 0x1F;
    uint8_t field = (instruction >> 21) & 0xF;
    vu0.max(field, dest, reg1, reg2);
}

void EmotionInterpreter::cop2_vsub(VectorUnit &vu0, uint32_t instruction)
{
    uint8_t dest = (instruction >> 6) & 0x1F;
    uint8_t reg1 = (instruction >> 11) & 0x1F;
    uint8_t reg2 = (instruction >> 16) & 0x1F;
    uint8_t field = (instruction >> 21) & 0xF;
    vu0.sub(field, dest, reg1, reg2);
}

void EmotionInterpreter::cop2_vopmsub(VectorUnit &vu0, uint32_t instruction)
{
    uint8_t dest = (instruction >> 6) & 0x1F;
    uint8_t reg1 = (instruction >> 11) & 0x1F;
    uint8_t reg2 = (instruction >> 16) & 0x1F;
    vu0.opmsub(dest, reg1, reg2);
}

void EmotionInterpreter::cop2_vmini(VectorUnit &vu0, uint32_t instruction)
{
    uint8_t dest = (instruction >> 6) & 0x1F;
    uint8_t reg1 = (instruction >> 11) & 0x1F;
    uint8_t reg2 = (instruction >> 16) & 0x1F;
    uint8_t field = (instruction >> 21) & 0xF;
    vu0.mini(field, dest, reg1, reg2);
}

void EmotionInterpreter::cop2_viadd(VectorUnit &vu0, uint32_t instruction)
{
    uint8_t dest = (instruction >> 6) & 0x1F;
    uint8_t reg1 = (instruction >> 11) & 0x1F;
    uint8_t reg2 = (instruction >> 16) & 0x1F;
    vu0.iadd(dest, reg1, reg2);
}

void EmotionInterpreter::cop2_visub(VectorUnit &vu0, uint32_t instruction)
{
    uint8_t dest = (instruction >> 6) & 0x1F;
    uint8_t reg1 = (instruction >> 11) & 0x1F;
    uint8_t reg2 = (instruction >> 16) & 0x1F;
    vu0.isub(dest, reg1, reg2);
}

void EmotionInterpreter::cop2_viaddi(VectorUnit &vu0, uint32_t instruction)
{
    int8_t imm = (instruction >> 6) & 0x1F;
    imm = ((int8_t)(imm << 3)) >> 3;
    uint8_t source = (instruction >> 11) & 0x1F;
    uint8_t dest = (instruction >> 16) & 0x1F;

    vu0.iaddi(dest, source, imm);
}

void EmotionInterpreter::cop2_viand(VectorUnit &vu0, uint32_t instruction)
{
    uint8_t dest = (instruction >> 6) & 0x1F;
    uint8_t reg1 = (instruction >> 11) & 0x1F;
    uint8_t reg2 = (instruction >> 16) & 0x1F;
    vu0.iand(dest, reg1, reg2);
}

void EmotionInterpreter::cop2_vcallms(VectorUnit &vu0, uint32_t instruction)
{
    uint32_t imm = (instruction >> 6) & 0x7FFF;
    imm *= 8;
    //TODO: Proper handling of EE stalling so we don't completely freeze the emulation of everything until VU0 is done.
    while (vu0.is_running())
        vu0.run(8);
    vu0.mscal(imm);
}

void EmotionInterpreter::cop2_vcallmsr(VectorUnit &vu0, uint32_t instruction)
{
    //TODO: As in VCALLMS
    while (vu0.is_running())
        vu0.run(8);
    vu0.callmsr();
}

void EmotionInterpreter::cop2_special2(VectorUnit &vu0, uint32_t instruction)
{
    uint16_t op = (instruction & 0x3) | ((instruction >> 4) & 0x7C);
    switch (op)
    {
        case 0x00:
        case 0x01:
        case 0x02:
        case 0x03:
            cop2_vaddabc(vu0, instruction);
            break;
        case 0x08:
        case 0x09:
        case 0x0A:
        case 0x0B:
            cop2_vmaddabc(vu0, instruction);
            break;
        case 0x0C:
        case 0x0D:
        case 0x0E:
        case 0x0F:
            cop2_vmsubabc(vu0, instruction);
            break;
        case 0x10:
            cop2_vitof0(vu0, instruction);
            break;
        case 0x11:
            cop2_vitof4(vu0, instruction);
            break;
        case 0x12:
            cop2_vitof12(vu0, instruction);
            break;
        case 0x13:
            cop2_vitof15(vu0, instruction);
            break;
        case 0x14:
            cop2_vftoi0(vu0, instruction);
            break;
        case 0x15:
            cop2_vftoi4(vu0, instruction);
            break;
        case 0x16:
            cop2_vftoi12(vu0, instruction);
            break;
        case 0x17:
            cop2_vftoi15(vu0, instruction);
            break;
        case 0x18:
        case 0x19:
        case 0x1A:
        case 0x1B:
            cop2_vmulabc(vu0, instruction);
            break;
        case 0x1C:
            cop2_vmulaq(vu0, instruction);
            break;
        case 0x1D:
            cop2_vabs(vu0, instruction);
            break;
        case 0x1E:
            cop2_vmulai(vu0, instruction);
            break;
        case 0x1F:
            cop2_vclip(vu0, instruction);
            break;
        case 0x23:
            cop2_vmaddai(vu0, instruction);
            break;
        case 0x27:
            cop2_vmsubai(vu0, instruction);
            break;
        case 0x28:
            cop2_vadda(vu0, instruction);
            break;
        case 0x29:
            cop2_vmadda(vu0, instruction);
            break;
        case 0x2A:
            cop2_vmula(vu0, instruction);
            break;
        case 0x2C:
            cop2_vsuba(vu0, instruction);
            break;
        case 0x2E:
            cop2_vopmula(vu0, instruction);
            break;
        case 0x2F:
            /**
              * TODO: vnop?
              */
            break;
        case 0x30:
            cop2_vmove(vu0, instruction);
            break;
        case 0x31:
            cop2_vmr32(vu0, instruction);
            break;
        case 0x34:
            cop2_vlqi(vu0, instruction);
            break;
        case 0x35:
            cop2_vsqi(vu0, instruction);
            break;
        case 0x36:
            cop2_vlqd(vu0, instruction);
            break;
        case 0x38:
            cop2_vdiv(vu0, instruction);
            break;
        case 0x39:
            cop2_vsqrt(vu0, instruction);
            break;
        case 0x3A:
            cop2_vrsqrt(vu0, instruction);
            break;
        case 0x3B:
            vu0.waitq();
            break;
        case 0x3C:
            cop2_vmtir(vu0, instruction);
            break;
        case 0x3D:
            cop2_vmfir(vu0, instruction);
            break;
        case 0x3F:
            cop2_viswr(vu0, instruction);
            break;
        case 0x40:
            cop2_vrnext(vu0, instruction);
            break;
        case 0x41:
            cop2_vrget(vu0, instruction);
            break;
        case 0x42:
            cop2_vrinit(vu0, instruction);
            break;
        case 0x43:
            cop2_vrxor(vu0, instruction);
            break;
        default:
            unknown_op("cop2 special2", instruction, op);
    }
}

void EmotionInterpreter::cop2_vaddabc(VectorUnit &vu0, uint32_t instruction)
{
    uint8_t bc = instruction & 0x3;
    uint8_t source = (instruction >> 11) & 0x1F;
    uint8_t bc_reg = (instruction >> 16) & 0x1F;
    uint8_t field = (instruction >> 21) & 0xF;
    vu0.addabc(bc, field, source, bc_reg);
}

void EmotionInterpreter::cop2_vmaddabc(VectorUnit &vu0, uint32_t instruction)
{
    uint8_t bc = instruction & 0x3;
    uint8_t source = (instruction >> 11) & 0x1F;
    uint8_t bc_reg = (instruction >> 16) & 0x1F;
    uint8_t field = (instruction >> 21) & 0xF;
    vu0.maddabc(bc, field, source, bc_reg);
}

void EmotionInterpreter::cop2_vmsubabc(VectorUnit &vu0, uint32_t instruction)
{
    uint8_t bc = instruction & 0x3;
    uint8_t source = (instruction >> 11) & 0x1F;
    uint8_t bc_reg = (instruction >> 16) & 0x1F;
    uint8_t field = (instruction >> 21) & 0xF;
    vu0.msubabc(bc, field, source, bc_reg);
}

void EmotionInterpreter::cop2_vitof0(VectorUnit &vu0, uint32_t instruction)
{
    uint8_t source = (instruction >> 11) & 0x1F;
    uint8_t dest = (instruction >> 16) & 0x1F;
    uint8_t field = (instruction >> 21) & 0xF;
    vu0.itof0(field, dest, source);
}

void EmotionInterpreter::cop2_vitof4(VectorUnit &vu0, uint32_t instruction)
{
    uint8_t source = (instruction >> 11) & 0x1F;
    uint8_t dest = (instruction >> 16) & 0x1F;
    uint8_t field = (instruction >> 21) & 0xF;
    vu0.itof4(field, dest, source);
}

void EmotionInterpreter::cop2_vitof12(VectorUnit &vu0, uint32_t instruction)
{
    uint8_t source = (instruction >> 11) & 0x1F;
    uint8_t dest = (instruction >> 16) & 0x1F;
    uint8_t field = (instruction >> 21) & 0xF;
    vu0.itof12(field, dest, source);
}

void EmotionInterpreter::cop2_vitof15(VectorUnit &vu0, uint32_t instruction)
{
    uint8_t source = (instruction >> 11) & 0x1F;
    uint8_t dest = (instruction >> 16) & 0x1F;
    uint8_t field = (instruction >> 21) & 0xF;
    vu0.itof15(field, dest, source);
}

void EmotionInterpreter::cop2_vftoi0(VectorUnit &vu0, uint32_t instruction)
{
    uint8_t source = (instruction >> 11) & 0x1F;
    uint8_t dest = (instruction >> 16) & 0x1F;
    uint8_t field = (instruction >> 21) & 0xF;
    vu0.ftoi0(field, dest, source);
}

void EmotionInterpreter::cop2_vftoi4(VectorUnit &vu0, uint32_t instruction)
{
    uint8_t source = (instruction >> 11) & 0x1F;
    uint8_t dest = (instruction >> 16) & 0x1F;
    uint8_t field = (instruction >> 21) & 0xF;
    vu0.ftoi4(field, dest, source);
}

void EmotionInterpreter::cop2_vftoi12(VectorUnit &vu0, uint32_t instruction)
{
    uint8_t source = (instruction >> 11) & 0x1F;
    uint8_t dest = (instruction >> 16) & 0x1F;
    uint8_t field = (instruction >> 21) & 0xF;
    vu0.ftoi12(field, dest, source);
}

void EmotionInterpreter::cop2_vftoi15(VectorUnit &vu0, uint32_t instruction)
{
    uint8_t source = (instruction >> 11) & 0x1F;
    uint8_t dest = (instruction >> 16) & 0x1F;
    uint8_t field = (instruction >> 21) & 0xF;
    vu0.ftoi15(field, dest, source);
}

void EmotionInterpreter::cop2_vmulabc(VectorUnit &vu0, uint32_t instruction)
{
    uint8_t bc = instruction & 0x3;
    uint8_t source = (instruction >> 11) & 0x1F;
    uint8_t bc_reg = (instruction >> 16) & 0x1F;
    uint8_t field = (instruction >> 21) & 0xF;
    vu0.mulabc(bc, field, source, bc_reg);
}

void EmotionInterpreter::cop2_vmulaq(VectorUnit &vu0, uint32_t instruction)
{
    uint8_t source = (instruction >> 11) & 0x1F;
    uint8_t field = (instruction >> 21) & 0xF;
    vu0.mulaq(field, source);
}

void EmotionInterpreter::cop2_vabs(VectorUnit &vu0, uint32_t instruction)
{
    uint8_t source = (instruction >> 11) & 0x1F;
    uint8_t dest = (instruction >> 16) & 0x1F;
    uint8_t field = (instruction >> 21) & 0xF;
    vu0.abs(field, dest, source);
}

void EmotionInterpreter::cop2_vmulai(VectorUnit &vu0, uint32_t instruction)
{
    uint8_t source = (instruction >> 11) & 0x1F;
    uint8_t field = (instruction >> 21) & 0xF;
    vu0.mulai(field, source);
}

void EmotionInterpreter::cop2_vclip(VectorUnit &vu0, uint32_t instruction)
{
    uint8_t reg1 = (instruction >> 11) & 0x1F;
    uint8_t reg2 = (instruction >> 16) & 0x1F;
    vu0.clip(reg1, reg2);
}

void EmotionInterpreter::cop2_vmaddai(VectorUnit &vu0, uint32_t instruction)
{
    uint8_t source = (instruction >> 11) & 0x1F;
    uint8_t field = (instruction >> 21) & 0xF;
    vu0.maddai(field, source);
}

void EmotionInterpreter::cop2_vmsubai(VectorUnit &vu0, uint32_t instruction)
{
    uint8_t source = (instruction >> 11) & 0x1F;
    uint8_t field = (instruction >> 21) & 0xF;
    vu0.msubai(field, source);
}

void EmotionInterpreter::cop2_vadda(VectorUnit &vu0, uint32_t instruction)
{
    uint8_t reg1 = (instruction >> 11) & 0x1F;
    uint8_t reg2 = (instruction >> 16) & 0x1F;
    uint8_t field = (instruction >> 21) & 0xF;
    vu0.adda(field, reg1, reg2);
}

void EmotionInterpreter::cop2_vmadda(VectorUnit &vu0, uint32_t instruction)
{
    uint8_t reg1 = (instruction >> 11) & 0x1F;
    uint8_t reg2 = (instruction >> 16) & 0x1F;
    uint8_t field = (instruction >> 21) & 0xF;
    vu0.madda(field, reg1, reg2);
}

void EmotionInterpreter::cop2_vmula(VectorUnit &vu0, uint32_t instruction)
{
    uint8_t reg1 = (instruction >> 11) & 0x1F;
    uint8_t reg2 = (instruction >> 16) & 0x1F;
    uint8_t field = (instruction >> 21) & 0xF;
    vu0.mula(field, reg1, reg2);
}

void EmotionInterpreter::cop2_vsuba(VectorUnit &vu0, uint32_t instruction)
{
    uint8_t reg1 = (instruction >> 11) & 0x1F;
    uint8_t reg2 = (instruction >> 16) & 0x1F;
    uint8_t field = (instruction >> 21) & 0xF;
    vu0.suba(field, reg1, reg2);
}

void EmotionInterpreter::cop2_vopmula(VectorUnit &vu0, uint32_t instruction)
{
    uint8_t reg1 = (instruction >> 11) & 0x1F;
    uint8_t reg2 = (instruction >> 16) & 0x1F;
    vu0.opmula(reg1, reg2);
}

void EmotionInterpreter::cop2_vmove(VectorUnit &vu0, uint32_t instruction)
{
    uint8_t source = (instruction >> 11) & 0x1F;
    uint8_t dest = (instruction >> 16) & 0x1F;
    uint8_t field = (instruction >> 21) & 0xF;
    vu0.move(field, dest, source);
}

void EmotionInterpreter::cop2_vmr32(VectorUnit &vu0, uint32_t instruction)
{
    uint8_t source = (instruction >> 11) & 0x1F;
    uint8_t dest = (instruction >> 16) & 0x1F;
    uint8_t field = (instruction >> 21) & 0xF;
    vu0.mr32(field, dest, source);
}

void EmotionInterpreter::cop2_vlqi(VectorUnit &vu0, uint32_t instruction)
{
    uint32_t is = (instruction >> 11) & 0x1F;
    uint32_t dest = (instruction >> 16) & 0x1F;
    uint8_t dest_field = (instruction >> 21) & 0xF;
    vu0.lqi(dest_field, dest, is);
}

void EmotionInterpreter::cop2_vsqi(VectorUnit &vu0, uint32_t instruction)
{
    uint32_t fs = (instruction >> 11) & 0x1F;
    uint32_t it = (instruction >> 16) & 0x1F;
    uint8_t dest_field = (instruction >> 21) & 0xF;
    vu0.sqi(dest_field, fs, it);
}

void EmotionInterpreter::cop2_vlqd(VectorUnit &vu0, uint32_t instruction)
{
    uint32_t is = (instruction >> 11) & 0x1F;
    uint32_t dest = (instruction >> 16) & 0x1F;
    uint8_t dest_field = (instruction >> 21) & 0xF;
    vu0.lqd(dest_field, dest, is);
}

void EmotionInterpreter::cop2_vdiv(VectorUnit &vu0, uint32_t instruction)
{
    uint8_t reg1 = (instruction >> 11) & 0x1F;
    uint8_t reg2 = (instruction >> 16) & 0x1F;
    uint8_t fsf = (instruction >> 21) & 0x3;
    uint8_t ftf = (instruction >> 23) & 0x3;
    vu0.div(ftf, fsf, reg1, reg2);
}

void EmotionInterpreter::cop2_vsqrt(VectorUnit &vu0, uint32_t instruction)
{
    uint8_t reg = (instruction >> 16) & 0x1F;
    uint8_t ftf = (instruction >> 23) & 0x3;
    vu0.vu_sqrt(ftf, reg);
}

void EmotionInterpreter::cop2_vrsqrt(VectorUnit &vu0, uint32_t instruction)
{
    uint8_t reg1 = (instruction >> 11) & 0x1F;
    uint8_t reg2 = (instruction >> 16) & 0x1F;
    uint8_t fsf = (instruction >> 21) & 0x3;
    uint8_t ftf = (instruction >> 23) & 0x3;
    vu0.rsqrt(ftf, fsf, reg1, reg2);
}

void EmotionInterpreter::cop2_vmtir(VectorUnit &vu0, uint32_t instruction)
{
    uint32_t fs = (instruction >> 11) & 0x1F;
    uint32_t it = (instruction >> 16) & 0x1F;
    uint8_t fsf = (instruction >> 21) & 0x3;
    vu0.mtir(fsf, it, fs);
}

void EmotionInterpreter::cop2_vmfir(VectorUnit &vu0, uint32_t instruction)
{
    uint32_t is = (instruction >> 11) & 0x1F;
    uint32_t ft = (instruction >> 16) & 0x1F;
    uint8_t dest_field = (instruction >> 21) & 0xF;
    vu0.mfir(dest_field, ft, is);
}

void EmotionInterpreter::cop2_viswr(VectorUnit &vu0, uint32_t instruction)
{
    uint32_t is = (instruction >> 11) & 0x1F;
    uint32_t it = (instruction >> 16) & 0x1F;
    uint8_t dest_field = (instruction >> 21) & 0xF;
    vu0.iswr(dest_field, it, is);
}

void EmotionInterpreter::cop2_vrnext(VectorUnit &vu0, uint32_t instruction)
{
    uint32_t ft = (instruction >> 16) & 0x1F;
    uint8_t field = (instruction >> 21) & 0xF;
    vu0.rnext(field, ft);
}

void EmotionInterpreter::cop2_vrget(VectorUnit &vu0, uint32_t instruction)
{
    uint32_t ft = (instruction >> 16) & 0x1F;
    uint8_t field = (instruction >> 21) & 0xF;
    vu0.rget(field, ft);
}

void EmotionInterpreter::cop2_vrinit(VectorUnit &vu0, uint32_t instruction)
{
    uint32_t fs = (instruction >> 11) & 0x1F;
    uint8_t fsf = (instruction >> 21) & 0x3;
    vu0.rinit(fsf, fs);
}

void EmotionInterpreter::cop2_vrxor(VectorUnit &vu0, uint32_t instruction)
{
    uint32_t fs = (instruction >> 11) & 0x1F;
    uint8_t fsf = (instruction >> 21) & 0x3;
    vu0.rxor(fsf, fs);
}
