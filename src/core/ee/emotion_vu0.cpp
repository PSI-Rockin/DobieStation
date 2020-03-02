#include "emotioninterpreter.hpp"
#include "vu.hpp"

void EmotionInterpreter::cop2_special(EE_InstrInfo& info, uint32_t instruction)
{
    uint8_t op = instruction & 0x3F;

    switch (op)
    {
        case 0x00:
        case 0x01:
        case 0x02:
        case 0x03:
            info.pipeline = EE_InstrInfo::Pipeline::COP2;
            info.interpreter_fn = &cop2_vaddbc;
            break;
        case 0x04:
        case 0x05:
        case 0x06:
        case 0x07:
            info.pipeline = EE_InstrInfo::Pipeline::COP2;
            info.interpreter_fn = &cop2_vsubbc;
            break;
        case 0x08:
        case 0x09:
        case 0x0A:
        case 0x0B:
            info.pipeline = EE_InstrInfo::Pipeline::COP2;
            info.interpreter_fn = &cop2_vmaddbc;
            break;
        case 0x0C:
        case 0x0D:
        case 0x0E:
        case 0x0F:
            info.pipeline = EE_InstrInfo::Pipeline::COP2;
            info.interpreter_fn = &cop2_vmsubbc;
            break;
        case 0x10:
        case 0x11:
        case 0x12:
        case 0x13:
            info.pipeline = EE_InstrInfo::Pipeline::COP2;
            info.interpreter_fn = &cop2_vmaxbc;
            break;
        case 0x14:
        case 0x15:
        case 0x16:
        case 0x17:
            info.pipeline = EE_InstrInfo::Pipeline::COP2;
            info.interpreter_fn = &cop2_vminibc;
            break;
        case 0x18:
        case 0x19:
        case 0x1A:
        case 0x1B:
            info.pipeline = EE_InstrInfo::Pipeline::COP2;
            info.interpreter_fn = &cop2_vmulbc;
            break;
        case 0x1C:
            info.pipeline = EE_InstrInfo::Pipeline::COP2;
            info.interpreter_fn = &cop2_vmulq;
            break;
        case 0x1D:
            info.pipeline = EE_InstrInfo::Pipeline::COP2;
            info.interpreter_fn = &cop2_vmaxi;
            break;
        case 0x1E:
            info.pipeline = EE_InstrInfo::Pipeline::COP2;
            info.interpreter_fn = &cop2_vmuli;
            break;
        case 0x1F:
            info.pipeline = EE_InstrInfo::Pipeline::COP2;
            info.interpreter_fn = &cop2_vminii;
            break;
        case 0x20:
            info.pipeline = EE_InstrInfo::Pipeline::COP2;
            info.interpreter_fn = &cop2_vaddq;
            break;
        case 0x21:
            info.pipeline = EE_InstrInfo::Pipeline::COP2;
            info.interpreter_fn = &cop2_vmaddq;
            break;
        case 0x22:
            info.pipeline = EE_InstrInfo::Pipeline::COP2;
            info.interpreter_fn = &cop2_vaddi;
            break;
        case 0x23:
            info.pipeline = EE_InstrInfo::Pipeline::COP2;
            info.interpreter_fn = &cop2_vmaddi;
            break;
        case 0x24:
            info.pipeline = EE_InstrInfo::Pipeline::COP2;
            info.interpreter_fn = &cop2_vsubq;
            break;
        case 0x25:
            info.pipeline = EE_InstrInfo::Pipeline::COP2;
            info.interpreter_fn = &cop2_vmsubq;
            break;
        case 0x26:
            info.pipeline = EE_InstrInfo::Pipeline::COP2;
            info.interpreter_fn = &cop2_vsubi;
            break;
        case 0x27:
            info.pipeline = EE_InstrInfo::Pipeline::COP2;
            info.interpreter_fn = &cop2_vmsubi;
            break;
        case 0x28:
            info.pipeline = EE_InstrInfo::Pipeline::COP2;
            info.interpreter_fn = &cop2_vadd;
            break;
        case 0x29:
            info.pipeline = EE_InstrInfo::Pipeline::COP2;
            info.interpreter_fn = &cop2_vmadd;
            break;
        case 0x2A:
            info.pipeline = EE_InstrInfo::Pipeline::COP2;
            info.interpreter_fn = &cop2_vmul;
            break;
        case 0x2B:
            info.pipeline = EE_InstrInfo::Pipeline::COP2;
            info.interpreter_fn = &cop2_vmax;
            break;
        case 0x2C:
            info.pipeline = EE_InstrInfo::Pipeline::COP2;
            info.interpreter_fn = &cop2_vsub;
            break;
        case 0x2D:
            info.pipeline = EE_InstrInfo::Pipeline::COP2;
            info.interpreter_fn = &cop2_vmsub;
            break;
        case 0x2E:
            info.pipeline = EE_InstrInfo::Pipeline::COP2;
            info.interpreter_fn = &cop2_vopmsub;
            break;
        case 0x2F:
            info.pipeline = EE_InstrInfo::Pipeline::COP2;
            info.interpreter_fn = &cop2_vmini;
            break;
        case 0x30:
            info.pipeline = EE_InstrInfo::Pipeline::COP2;
            info.interpreter_fn = &cop2_viadd;
            break;
        case 0x31:
            info.pipeline = EE_InstrInfo::Pipeline::COP2;
            info.interpreter_fn = &cop2_visub;
            break;
        case 0x32:
            info.pipeline = EE_InstrInfo::Pipeline::COP2;
            info.interpreter_fn = &cop2_viaddi;
            break;
        case 0x34:
            info.pipeline = EE_InstrInfo::Pipeline::COP2;
            info.interpreter_fn = &cop2_viand;
            break;
        case 0x35:
            info.pipeline = EE_InstrInfo::Pipeline::COP2;
            info.interpreter_fn = &cop2_vior;
            break;
        case 0x38:
            info.pipeline = EE_InstrInfo::Pipeline::COP2;
            info.interpreter_fn = &cop2_vcallms;
            break;
        case 0x39:
            info.pipeline = EE_InstrInfo::Pipeline::COP2;
            info.interpreter_fn = &cop2_vcallmsr;
            break;
        case 0x3C:
        case 0x3D:
        case 0x3E:
        case 0x3F:
            cop2_special2(info, instruction);
            break;
        default:
            unknown_op("cop2 special", instruction, op);
    }
}

bool EmotionInterpreter::cop2_sync(EmotionEngine& cpu, uint32_t instruction)
{
    //Apparently, any COP2 instruction that is executed while VU0 is running causes COP2 to stall, so lets do that
    //Dragons Quest 8 is a good test for this as it does COP2 while VU0 is running.
    if (cpu.vu0_wait())
    {
        cpu.set_PC(cpu.get_PC() - 4);
        return false;
    }
    return true;
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

void EmotionInterpreter::cop2_vaddbc(EmotionEngine& cpu, uint32_t instruction)
{
    if (!cop2_sync(cpu, instruction))
    {
        return;
    };

    cpu.cop2_updatevu0();
    VectorUnit& vu0 = cpu.get_VU0();
    vu0.decoder.reset();

    uint8_t bc = instruction & 0x3;
    uint8_t dest = (instruction >> 6) & 0x1F;
    uint8_t source = (instruction >> 11) & 0x1F;
    uint8_t bc_reg = (instruction >> 16) & 0x1F;
    uint8_t field = (instruction >> 21) & 0xF;
    vu0.decoder.vf_write[0] = dest;
    vu0.decoder.vf_write_field[0] = field;

    vu0.decoder.vf_read0[0] = source;
    vu0.decoder.vf_read0_field[0] = field;

    vu0.decoder.vf_read1[0] = bc_reg;
    vu0.decoder.vf_read1_field[0] = 1 << (3 - bc);

    vu0.handle_cop2_stalls();
    vu0.addbc(instruction);
}

void EmotionInterpreter::cop2_vsubbc(EmotionEngine& cpu, uint32_t instruction)
{
    if (!cop2_sync(cpu, instruction))
    {
        return;
    };

    cpu.cop2_updatevu0();
    VectorUnit& vu0 = cpu.get_VU0();
    vu0.decoder.reset();

    uint8_t bc = instruction & 0x3;
    uint8_t dest = (instruction >> 6) & 0x1F;
    uint8_t source = (instruction >> 11) & 0x1F;
    uint8_t bc_reg = (instruction >> 16) & 0x1F;
    uint8_t field = (instruction >> 21) & 0xF;
    vu0.decoder.vf_write[0] = dest;
    vu0.decoder.vf_write_field[0] = field;

    vu0.decoder.vf_read0[0] = source;
    vu0.decoder.vf_read0_field[0] = field;

    vu0.decoder.vf_read1[0] = bc_reg;
    vu0.decoder.vf_read1_field[0] = 1 << (3 - bc);

    vu0.handle_cop2_stalls();
    vu0.subbc(instruction);
}

void EmotionInterpreter::cop2_vmaddbc(EmotionEngine& cpu, uint32_t instruction)
{
    if (!cop2_sync(cpu, instruction))
    {
        return;
    };

    cpu.cop2_updatevu0();
    VectorUnit& vu0 = cpu.get_VU0();
    vu0.decoder.reset();

    uint8_t bc = instruction & 0x3;
    uint8_t dest = (instruction >> 6) & 0x1F;
    uint8_t source = (instruction >> 11) & 0x1F;
    uint8_t bc_reg = (instruction >> 16) & 0x1F;
    uint8_t field = (instruction >> 21) & 0xF;
    vu0.decoder.vf_write[0] = dest;
    vu0.decoder.vf_write_field[0] = field;

    vu0.decoder.vf_read0[0] = source;
    vu0.decoder.vf_read0_field[0] = field;

    vu0.decoder.vf_read1[0] = bc_reg;
    vu0.decoder.vf_read1_field[0] = 1 << (3 - bc);

    vu0.handle_cop2_stalls();
    vu0.maddbc(instruction);
}

void EmotionInterpreter::cop2_vmsubbc(EmotionEngine& cpu, uint32_t instruction)
{
    if (!cop2_sync(cpu, instruction))
    {
        return;
    };

    cpu.cop2_updatevu0();
    VectorUnit& vu0 = cpu.get_VU0();
    vu0.decoder.reset();

    uint8_t bc = instruction & 0x3;
    uint8_t dest = (instruction >> 6) & 0x1F;
    uint8_t source = (instruction >> 11) & 0x1F;
    uint8_t bc_reg = (instruction >> 16) & 0x1F;
    uint8_t field = (instruction >> 21) & 0xF;
    vu0.decoder.vf_write[0] = dest;
    vu0.decoder.vf_write_field[0] = field;

    vu0.decoder.vf_read0[0] = source;
    vu0.decoder.vf_read0_field[0] = field;

    vu0.decoder.vf_read1[0] = bc_reg;
    vu0.decoder.vf_read1_field[0] = 1 << (3 - bc);

    vu0.handle_cop2_stalls();
    vu0.msubbc(instruction);
}

void EmotionInterpreter::cop2_vmaxbc(EmotionEngine& cpu, uint32_t instruction)
{
    if (!cop2_sync(cpu, instruction))
    {
        return;
    };

    cpu.cop2_updatevu0();
    VectorUnit& vu0 = cpu.get_VU0();
    vu0.decoder.reset();

    uint8_t bc = instruction & 0x3;
    uint8_t dest = (instruction >> 6) & 0x1F;
    uint8_t source = (instruction >> 11) & 0x1F;
    uint8_t bc_reg = (instruction >> 16) & 0x1F;
    uint8_t field = (instruction >> 21) & 0xF;
    vu0.decoder.vf_write[0] = dest;
    vu0.decoder.vf_write_field[0] = field;

    vu0.decoder.vf_read0[0] = source;
    vu0.decoder.vf_read0_field[0] = field;

    vu0.decoder.vf_read1[0] = bc_reg;
    vu0.decoder.vf_read1_field[0] = 1 << (3 - bc);

    vu0.handle_cop2_stalls();
    vu0.maxbc(instruction);
}

void EmotionInterpreter::cop2_vminibc(EmotionEngine& cpu, uint32_t instruction)
{
    if (!cop2_sync(cpu, instruction))
    {
        return;
    };

    cpu.cop2_updatevu0();
    VectorUnit& vu0 = cpu.get_VU0();
    vu0.decoder.reset();

    uint8_t bc = instruction & 0x3;
    uint8_t dest = (instruction >> 6) & 0x1F;
    uint8_t source = (instruction >> 11) & 0x1F;
    uint8_t bc_reg = (instruction >> 16) & 0x1F;
    uint8_t field = (instruction >> 21) & 0xF;
    vu0.decoder.vf_write[0] = dest;
    vu0.decoder.vf_write_field[0] = field;

    vu0.decoder.vf_read0[0] = source;
    vu0.decoder.vf_read0_field[0] = field;

    vu0.decoder.vf_read1[0] = bc_reg;
    vu0.decoder.vf_read1_field[0] = 1 << (3 - bc);

    vu0.handle_cop2_stalls();
    vu0.minibc(instruction);
}

void EmotionInterpreter::cop2_vmulbc(EmotionEngine& cpu, uint32_t instruction)
{
    if (!cop2_sync(cpu, instruction))
    {
        return;
    };

    cpu.cop2_updatevu0();
    VectorUnit& vu0 = cpu.get_VU0();
    vu0.decoder.reset();

    uint8_t bc = instruction & 0x3;
    uint8_t dest = (instruction >> 6) & 0x1F;
    uint8_t source = (instruction >> 11) & 0x1F;
    uint8_t bc_reg = (instruction >> 16) & 0x1F;
    uint8_t field = (instruction >> 21) & 0xF;
    vu0.decoder.vf_write[0] = dest;
    vu0.decoder.vf_write_field[0] = field;

    vu0.decoder.vf_read0[0] = source;
    vu0.decoder.vf_read0_field[0] = field;

    vu0.decoder.vf_read1[0] = bc_reg;
    vu0.decoder.vf_read1_field[0] = 1 << (3 - bc);

    vu0.handle_cop2_stalls();
    vu0.mulbc(instruction);
}

void EmotionInterpreter::cop2_vmulq(EmotionEngine& cpu, uint32_t instruction)
{
    if (!cop2_sync(cpu, instruction))
    {
        return;
    };

    cpu.cop2_updatevu0();
    VectorUnit& vu0 = cpu.get_VU0();
    vu0.decoder.reset();

    uint8_t dest = (instruction >> 6) & 0x1F;
    uint8_t source = (instruction >> 11) & 0x1F;
    uint8_t field = (instruction >> 21) & 0xF;

    vu0.decoder.vf_write[0] = dest;
    vu0.decoder.vf_write_field[0] = field;

    vu0.decoder.vf_read0[0] = source;
    vu0.decoder.vf_read0_field[0] = field;

    vu0.handle_cop2_stalls();
    vu0.mulq(instruction);
}

void EmotionInterpreter::cop2_vmaxi(EmotionEngine& cpu, uint32_t instruction)
{
    if (!cop2_sync(cpu, instruction))
    {
        return;
    };

    cpu.cop2_updatevu0();
    VectorUnit& vu0 = cpu.get_VU0();
    vu0.decoder.reset();

    uint8_t dest = (instruction >> 6) & 0x1F;
    uint8_t source = (instruction >> 11) & 0x1F;
    uint8_t field = (instruction >> 21) & 0xF;

    vu0.decoder.vf_write[0] = dest;
    vu0.decoder.vf_write_field[0] = field;

    vu0.decoder.vf_read0[0] = source;
    vu0.decoder.vf_read0_field[0] = field;

    vu0.handle_cop2_stalls();
    vu0.maxi(instruction);
}

void EmotionInterpreter::cop2_vmuli(EmotionEngine& cpu, uint32_t instruction)
{
    if (!cop2_sync(cpu, instruction))
    {
        return;
    };

    cpu.cop2_updatevu0();
    VectorUnit& vu0 = cpu.get_VU0();
    vu0.decoder.reset();

    uint8_t dest = (instruction >> 6) & 0x1F;
    uint8_t source = (instruction >> 11) & 0x1F;
    uint8_t field = (instruction >> 21) & 0xF;

    vu0.decoder.vf_write[0] = dest;
    vu0.decoder.vf_write_field[0] = field;

    vu0.decoder.vf_read0[0] = source;
    vu0.decoder.vf_read0_field[0] = field;

    vu0.handle_cop2_stalls();
    vu0.muli(instruction);
}

void EmotionInterpreter::cop2_vminii(EmotionEngine& cpu, uint32_t instruction)
{
    if (!cop2_sync(cpu, instruction))
    {
        return;
    };

    cpu.cop2_updatevu0();
    VectorUnit& vu0 = cpu.get_VU0();
    vu0.decoder.reset();

    uint8_t dest = (instruction >> 6) & 0x1F;
    uint8_t source = (instruction >> 11) & 0x1F;
    uint8_t field = (instruction >> 21) & 0xF;

    vu0.decoder.vf_write[0] = dest;
    vu0.decoder.vf_write_field[0] = field;

    vu0.decoder.vf_read0[0] = source;
    vu0.decoder.vf_read0_field[0] = field;

    vu0.handle_cop2_stalls();
    vu0.minii(instruction);
}

void EmotionInterpreter::cop2_vaddq(EmotionEngine& cpu, uint32_t instruction)
{
    if (!cop2_sync(cpu, instruction))
    {
        return;
    };

    cpu.cop2_updatevu0();
    VectorUnit& vu0 = cpu.get_VU0();
    vu0.decoder.reset();

    uint8_t dest = (instruction >> 6) & 0x1F;
    uint8_t source = (instruction >> 11) & 0x1F;
    uint8_t field = (instruction >> 21) & 0xF;

    vu0.decoder.vf_write[0] = dest;
    vu0.decoder.vf_write_field[0] = field;

    vu0.decoder.vf_read0[0] = source;
    vu0.decoder.vf_read0_field[0] = field;

    vu0.handle_cop2_stalls();
    vu0.addq(instruction);
}

void EmotionInterpreter::cop2_vmaddq(EmotionEngine& cpu, uint32_t instruction)
{
    if (!cop2_sync(cpu, instruction))
    {
        return;
    };

    cpu.cop2_updatevu0();
    VectorUnit& vu0 = cpu.get_VU0();
    vu0.decoder.reset();

    uint8_t dest = (instruction >> 6) & 0x1F;
    uint8_t source = (instruction >> 11) & 0x1F;
    uint8_t field = (instruction >> 21) & 0xF;

    vu0.decoder.vf_write[0] = dest;
    vu0.decoder.vf_write_field[0] = field;

    vu0.decoder.vf_read0[0] = source;
    vu0.decoder.vf_read0_field[0] = field;

    vu0.handle_cop2_stalls();
    vu0.maddq(instruction);
}

void EmotionInterpreter::cop2_vaddi(EmotionEngine& cpu, uint32_t instruction)
{
    if (!cop2_sync(cpu, instruction))
    {
        return;
    };

    cpu.cop2_updatevu0();
    VectorUnit& vu0 = cpu.get_VU0();
    vu0.decoder.reset();

    uint8_t dest = (instruction >> 6) & 0x1F;
    uint8_t source = (instruction >> 11) & 0x1F;
    uint8_t field = (instruction >> 21) & 0xF;

    vu0.decoder.vf_write[0] = dest;
    vu0.decoder.vf_write_field[0] = field;

    vu0.decoder.vf_read0[0] = source;
    vu0.decoder.vf_read0_field[0] = field;

    vu0.handle_cop2_stalls();
    vu0.addi(instruction);
}

void EmotionInterpreter::cop2_vmaddi(EmotionEngine& cpu, uint32_t instruction)
{
    if (!cop2_sync(cpu, instruction))
    {
        return;
    };

    cpu.cop2_updatevu0();
    VectorUnit& vu0 = cpu.get_VU0();
    vu0.decoder.reset();

    uint8_t dest = (instruction >> 6) & 0x1F;
    uint8_t source = (instruction >> 11) & 0x1F;
    uint8_t field = (instruction >> 21) & 0xF;

    vu0.decoder.vf_write[0] = dest;
    vu0.decoder.vf_write_field[0] = field;

    vu0.decoder.vf_read0[0] = source;
    vu0.decoder.vf_read0_field[0] = field;

    vu0.handle_cop2_stalls();
    vu0.maddi(instruction);
}

void EmotionInterpreter::cop2_vsubq(EmotionEngine& cpu, uint32_t instruction)
{
    if (!cop2_sync(cpu, instruction))
    {
        return;
    };

    cpu.cop2_updatevu0();
    VectorUnit& vu0 = cpu.get_VU0();
    vu0.decoder.reset();

    uint8_t dest = (instruction >> 6) & 0x1F;
    uint8_t source = (instruction >> 11) & 0x1F;
    uint8_t field = (instruction >> 21) & 0xF;

    vu0.decoder.vf_write[0] = dest;
    vu0.decoder.vf_write_field[0] = field;

    vu0.decoder.vf_read0[0] = source;
    vu0.decoder.vf_read0_field[0] = field;

    vu0.handle_cop2_stalls();
    vu0.subq(instruction);
}

void EmotionInterpreter::cop2_vmsubq(EmotionEngine& cpu, uint32_t instruction)
{
    if (!cop2_sync(cpu, instruction))
    {
        return;
    };

    cpu.cop2_updatevu0();
    VectorUnit& vu0 = cpu.get_VU0();
    vu0.decoder.reset();

    uint8_t dest = (instruction >> 6) & 0x1F;
    uint8_t source = (instruction >> 11) & 0x1F;
    uint8_t field = (instruction >> 21) & 0xF;

    vu0.decoder.vf_write[0] = dest;
    vu0.decoder.vf_write_field[0] = field;

    vu0.decoder.vf_read0[0] = source;
    vu0.decoder.vf_read0_field[0] = field;

    vu0.handle_cop2_stalls();
    vu0.msubq(instruction);
}

void EmotionInterpreter::cop2_vsubi(EmotionEngine& cpu, uint32_t instruction)
{
    if (!cop2_sync(cpu, instruction))
    {
        return;
    };

    cpu.cop2_updatevu0();
    VectorUnit& vu0 = cpu.get_VU0();
    vu0.decoder.reset();

    uint8_t dest = (instruction >> 6) & 0x1F;
    uint8_t source = (instruction >> 11) & 0x1F;
    uint8_t field = (instruction >> 21) & 0xF;

    vu0.decoder.vf_write[0] = dest;
    vu0.decoder.vf_write_field[0] = field;

    vu0.decoder.vf_read0[0] = source;
    vu0.decoder.vf_read0_field[0] = field;

    vu0.handle_cop2_stalls();
    vu0.subi(instruction);
}

void EmotionInterpreter::cop2_vmsubi(EmotionEngine& cpu, uint32_t instruction)
{
    if (!cop2_sync(cpu, instruction))
    {
        return;
    };

    cpu.cop2_updatevu0();
    VectorUnit& vu0 = cpu.get_VU0();
    vu0.decoder.reset();

    uint8_t dest = (instruction >> 6) & 0x1F;
    uint8_t source = (instruction >> 11) & 0x1F;
    uint8_t field = (instruction >> 21) & 0xF;

    vu0.decoder.vf_write[0] = dest;
    vu0.decoder.vf_write_field[0] = field;

    vu0.decoder.vf_read0[0] = source;
    vu0.decoder.vf_read0_field[0] = field;

    vu0.handle_cop2_stalls();
    vu0.msubi(instruction);
}

void EmotionInterpreter::cop2_vadd(EmotionEngine& cpu, uint32_t instruction)
{
    if (!cop2_sync(cpu, instruction))
    {
        return;
    };

    cpu.cop2_updatevu0();
    VectorUnit& vu0 = cpu.get_VU0();
    vu0.decoder.reset();

    uint8_t dest = (instruction >> 6) & 0x1F;
    uint8_t reg1 = (instruction >> 11) & 0x1F;
    uint8_t reg2 = (instruction >> 16) & 0x1F;
    uint8_t field = (instruction >> 21) & 0xF;

    vu0.decoder.vf_write[0] = dest;
    vu0.decoder.vf_write_field[0] = field;

    vu0.decoder.vf_read0[0] = reg1;
    vu0.decoder.vf_read0_field[0] = field;

    vu0.decoder.vf_read1[0] = reg2;
    vu0.decoder.vf_read1_field[0] = field;

    vu0.handle_cop2_stalls();
    vu0.add(instruction);
}

void EmotionInterpreter::cop2_vmadd(EmotionEngine& cpu, uint32_t instruction)
{
    if (!cop2_sync(cpu, instruction))
    {
        return;
    };

    cpu.cop2_updatevu0();
    VectorUnit& vu0 = cpu.get_VU0();
    vu0.decoder.reset();

    uint8_t dest = (instruction >> 6) & 0x1F;
    uint8_t reg1 = (instruction >> 11) & 0x1F;
    uint8_t reg2 = (instruction >> 16) & 0x1F;
    uint8_t field = (instruction >> 21) & 0xF;

    vu0.decoder.vf_write[0] = dest;
    vu0.decoder.vf_write_field[0] = field;

    vu0.decoder.vf_read0[0] = reg1;
    vu0.decoder.vf_read0_field[0] = field;

    vu0.decoder.vf_read1[0] = reg2;
    vu0.decoder.vf_read1_field[0] = field;

    vu0.handle_cop2_stalls();
    vu0.madd(instruction);
}

void EmotionInterpreter::cop2_vmul(EmotionEngine& cpu, uint32_t instruction)
{
    if (!cop2_sync(cpu, instruction))
    {
        return;
    };

    cpu.cop2_updatevu0();
    VectorUnit& vu0 = cpu.get_VU0();
    vu0.decoder.reset();

    uint8_t dest = (instruction >> 6) & 0x1F;
    uint8_t reg1 = (instruction >> 11) & 0x1F;
    uint8_t reg2 = (instruction >> 16) & 0x1F;
    uint8_t field = (instruction >> 21) & 0xF;

    vu0.decoder.vf_write[0] = dest;
    vu0.decoder.vf_write_field[0] = field;

    vu0.decoder.vf_read0[0] = reg1;
    vu0.decoder.vf_read0_field[0] = field;

    vu0.decoder.vf_read1[0] = reg2;
    vu0.decoder.vf_read1_field[0] = field;

    vu0.handle_cop2_stalls();
    vu0.mul(instruction);
}

void EmotionInterpreter::cop2_vmax(EmotionEngine& cpu, uint32_t instruction)
{
    if (!cop2_sync(cpu, instruction))
    {
        return;
    };

    cpu.cop2_updatevu0();
    VectorUnit& vu0 = cpu.get_VU0();
    vu0.decoder.reset();

    uint8_t dest = (instruction >> 6) & 0x1F;
    uint8_t reg1 = (instruction >> 11) & 0x1F;
    uint8_t reg2 = (instruction >> 16) & 0x1F;
    uint8_t field = (instruction >> 21) & 0xF;

    vu0.decoder.vf_write[0] = dest;
    vu0.decoder.vf_write_field[0] = field;

    vu0.decoder.vf_read0[0] = reg1;
    vu0.decoder.vf_read0_field[0] = field;

    vu0.decoder.vf_read1[0] = reg2;
    vu0.decoder.vf_read1_field[0] = field;

    vu0.handle_cop2_stalls();
    vu0.max(instruction);
}

void EmotionInterpreter::cop2_vsub(EmotionEngine& cpu, uint32_t instruction)
{
    if (!cop2_sync(cpu, instruction))
    {
        return;
    };

    cpu.cop2_updatevu0();
    VectorUnit& vu0 = cpu.get_VU0();
    vu0.decoder.reset();

    uint8_t dest = (instruction >> 6) & 0x1F;
    uint8_t reg1 = (instruction >> 11) & 0x1F;
    uint8_t reg2 = (instruction >> 16) & 0x1F;
    uint8_t field = (instruction >> 21) & 0xF;

    vu0.decoder.vf_write[0] = dest;
    vu0.decoder.vf_write_field[0] = field;

    vu0.decoder.vf_read0[0] = reg1;
    vu0.decoder.vf_read0_field[0] = field;

    vu0.decoder.vf_read1[0] = reg2;
    vu0.decoder.vf_read1_field[0] = field;

    vu0.handle_cop2_stalls();
    vu0.sub(instruction);
}

void EmotionInterpreter::cop2_vmsub(EmotionEngine& cpu, uint32_t instruction)
{
    if (!cop2_sync(cpu, instruction))
    {
        return;
    };

    cpu.cop2_updatevu0();
    VectorUnit& vu0 = cpu.get_VU0();
    vu0.decoder.reset();

    uint8_t dest = (instruction >> 6) & 0x1F;
    uint8_t reg1 = (instruction >> 11) & 0x1F;
    uint8_t reg2 = (instruction >> 16) & 0x1F;
    uint8_t field = (instruction >> 21) & 0xF;

    vu0.decoder.vf_write[0] = dest;
    vu0.decoder.vf_write_field[0] = field;

    vu0.decoder.vf_read0[0] = reg1;
    vu0.decoder.vf_read0_field[0] = field;

    vu0.decoder.vf_read1[0] = reg2;
    vu0.decoder.vf_read1_field[0] = field;

    vu0.handle_cop2_stalls();
    vu0.msub(instruction);
}

void EmotionInterpreter::cop2_vopmsub(EmotionEngine& cpu, uint32_t instruction)
{
    if (!cop2_sync(cpu, instruction))
    {
        return;
    };

    cpu.cop2_updatevu0();
    VectorUnit& vu0 = cpu.get_VU0();
    vu0.decoder.reset();

    uint8_t dest = (instruction >> 6) & 0x1F;
    uint8_t reg1 = (instruction >> 11) & 0x1F;
    uint8_t reg2 = (instruction >> 16) & 0x1F;
    vu0.decoder.vf_write[0] = dest;
    vu0.decoder.vf_read0[0] = reg1;
    vu0.decoder.vf_read1[0] = reg2;
    vu0.decoder.vf_write_field[0] = 0xE; //xyz
    vu0.decoder.vf_read0_field[0] = 0xE;
    vu0.decoder.vf_read1_field[0] = 0xE;

    vu0.handle_cop2_stalls();
    vu0.opmsub(instruction);
}

void EmotionInterpreter::cop2_vmini(EmotionEngine& cpu, uint32_t instruction)
{
    if (!cop2_sync(cpu, instruction))
    {
        return;
    };

    cpu.cop2_updatevu0();
    VectorUnit& vu0 = cpu.get_VU0();
    vu0.decoder.reset();

    uint8_t dest = (instruction >> 6) & 0x1F;
    uint8_t reg1 = (instruction >> 11) & 0x1F;
    uint8_t reg2 = (instruction >> 16) & 0x1F;
    uint8_t field = (instruction >> 21) & 0xF;

    vu0.decoder.vf_write[0] = dest;
    vu0.decoder.vf_write_field[0] = field;

    vu0.decoder.vf_read0[0] = reg1;
    vu0.decoder.vf_read0_field[0] = field;

    vu0.decoder.vf_read1[0] = reg2;
    vu0.decoder.vf_read1_field[0] = field;

    vu0.handle_cop2_stalls();
    vu0.mini(instruction);
}

void EmotionInterpreter::cop2_viadd(EmotionEngine& cpu, uint32_t instruction)
{
    if (!cop2_sync(cpu, instruction))
    {
        return;
    };

    cpu.cop2_updatevu0();
    VectorUnit& vu0 = cpu.get_VU0();
    vu0.decoder.reset();

    uint8_t dest = (instruction >> 6) & 0xF;
    uint8_t reg1 = (instruction >> 11) & 0xF;
    uint8_t reg2 = (instruction >> 16) & 0xF;
    vu0.decoder.vf_write[0] = dest;
    vu0.decoder.vi_read0 = reg1;
    vu0.decoder.vi_read1 = reg2;

    vu0.handle_cop2_stalls();
    vu0.iadd(instruction);
}

void EmotionInterpreter::cop2_visub(EmotionEngine& cpu, uint32_t instruction)
{
    if (!cop2_sync(cpu, instruction))
    {
        return;
    };

    cpu.cop2_updatevu0();
    VectorUnit& vu0 = cpu.get_VU0();
    vu0.decoder.reset();

    uint8_t dest = (instruction >> 6) & 0xF;
    uint8_t reg1 = (instruction >> 11) & 0xF;
    uint8_t reg2 = (instruction >> 16) & 0xF;
    vu0.decoder.vf_write[0] = dest;
    vu0.decoder.vi_read0 = reg1;
    vu0.decoder.vi_read1 = reg2;

    vu0.handle_cop2_stalls();
    vu0.isub(instruction);
}

void EmotionInterpreter::cop2_viaddi(EmotionEngine& cpu, uint32_t instruction)
{
    if (!cop2_sync(cpu, instruction))
    {
        return;
    };

    cpu.cop2_updatevu0();
    VectorUnit& vu0 = cpu.get_VU0();
    vu0.decoder.reset();

    uint8_t reg1 = (instruction >> 11) & 0xF;
    uint8_t dest = (instruction >> 16) & 0xF;
    vu0.decoder.vf_write[0] = dest;
    vu0.decoder.vi_read0 = reg1;

    vu0.handle_cop2_stalls();
    vu0.iaddi(instruction);
}

void EmotionInterpreter::cop2_viand(EmotionEngine& cpu, uint32_t instruction)
{
    if (!cop2_sync(cpu, instruction))
    {
        return;
    };

    cpu.cop2_updatevu0();
    VectorUnit& vu0 = cpu.get_VU0();
    vu0.decoder.reset();

    uint8_t dest = (instruction >> 6) & 0xF;
    uint8_t reg1 = (instruction >> 11) & 0xF;
    uint8_t reg2 = (instruction >> 16) & 0xF;
    vu0.decoder.vf_write[0] = dest;
    vu0.decoder.vi_read0 = reg1;
    vu0.decoder.vi_read1 = reg2;

    vu0.handle_cop2_stalls();
    vu0.iand(instruction);
}

void EmotionInterpreter::cop2_vior(EmotionEngine& cpu, uint32_t instruction)
{
    if (!cop2_sync(cpu, instruction))
    {
        return;
    };

    cpu.cop2_updatevu0();
    VectorUnit& vu0 = cpu.get_VU0();
    vu0.decoder.reset();

    uint8_t dest = (instruction >> 6) & 0xF;
    uint8_t reg1 = (instruction >> 11) & 0xF;
    uint8_t reg2 = (instruction >> 16) & 0xF;
    vu0.decoder.vf_write[0] = dest;
    vu0.decoder.vi_read0 = reg1;
    vu0.decoder.vi_read1 = reg2;

    vu0.handle_cop2_stalls();
    vu0.ior(instruction);
}

void EmotionInterpreter::cop2_vcallms(EmotionEngine& cpu, uint32_t instruction)
{
    if (!cop2_sync(cpu, instruction))
    {
        return;
    };

    VectorUnit& vu0 = cpu.get_VU0();
    vu0.decoder.reset();

    uint32_t imm = (instruction >> 6) & 0x7FFF;
    imm *= 8;
    cpu.clear_interlock();
    //Sega Superstars Tennis is accurate down to the cycle when doing a QMFC2, so we need to account for the VCALLMS/R cycle also
    cpu.set_cycle_count(cpu.get_cycle_count() + 1);
    vu0.start_program(imm);
    cpu.set_cycle_count(cpu.get_cycle_count() - 1);
}

void EmotionInterpreter::cop2_vcallmsr(EmotionEngine& cpu, uint32_t instruction)
{
    if (!cop2_sync(cpu, instruction))
    {
        return;
    };

    VectorUnit& vu0 = cpu.get_VU0();
    vu0.decoder.reset();

    cpu.clear_interlock();
    //Sega Superstars Tennis is accurate down to the cycle when doing a QMFC2, so we need to account for the VCALLMS/R cycle also
    cpu.set_cycle_count(cpu.get_cycle_count() + 1);
    vu0.start_program(vu0.read_CMSAR0() * 8);
    cpu.set_cycle_count(cpu.get_cycle_count() - 1);
}

void EmotionInterpreter::cop2_special2(EE_InstrInfo& info, uint32_t instruction)
{
    uint16_t op = (instruction & 0x3) | ((instruction >> 4) & 0x7C);
    switch (op)
    {
        case 0x00:
        case 0x01:
        case 0x02:
        case 0x03:
            info.pipeline = EE_InstrInfo::Pipeline::COP2;
            info.interpreter_fn = &cop2_vaddabc;
            break;
        case 0x04:
        case 0x05:
        case 0x06:
        case 0x07:
            info.pipeline = EE_InstrInfo::Pipeline::COP2;
            info.interpreter_fn = &cop2_vsubabc;
            break;
        case 0x08:
        case 0x09:
        case 0x0A:
        case 0x0B:
            info.pipeline = EE_InstrInfo::Pipeline::COP2;
            info.interpreter_fn = &cop2_vmaddabc;
            break;
        case 0x0C:
        case 0x0D:
        case 0x0E:
        case 0x0F:
            info.pipeline = EE_InstrInfo::Pipeline::COP2;
            info.interpreter_fn = &cop2_vmsubabc;
            break;
        case 0x10:
            info.pipeline = EE_InstrInfo::Pipeline::COP2;
            info.interpreter_fn = &cop2_vitof0;
            break;
        case 0x11:
            info.pipeline = EE_InstrInfo::Pipeline::COP2;
            info.interpreter_fn = &cop2_vitof4;
            break;
        case 0x12:
            info.pipeline = EE_InstrInfo::Pipeline::COP2;
            info.interpreter_fn = &cop2_vitof12;
            break;
        case 0x13:
            info.pipeline = EE_InstrInfo::Pipeline::COP2;
            info.interpreter_fn = &cop2_vitof15;
            break;
        case 0x14:
            info.pipeline = EE_InstrInfo::Pipeline::COP2;
            info.interpreter_fn = &cop2_vftoi0;
            break;
        case 0x15:
            info.pipeline = EE_InstrInfo::Pipeline::COP2;
            info.interpreter_fn = &cop2_vftoi4;
            break;
        case 0x16:
            info.pipeline = EE_InstrInfo::Pipeline::COP2;
            info.interpreter_fn = &cop2_vftoi12;
            break;
        case 0x17:
            info.pipeline = EE_InstrInfo::Pipeline::COP2;
            info.interpreter_fn = &cop2_vftoi15;
            break;
        case 0x18:
        case 0x19:
        case 0x1A:
        case 0x1B:
            info.pipeline = EE_InstrInfo::Pipeline::COP2;
            info.interpreter_fn = &cop2_vmulabc;
            break;
        case 0x1C:
            info.pipeline = EE_InstrInfo::Pipeline::COP2;
            info.interpreter_fn = &cop2_vmulaq;
            break;
        case 0x1D:
            info.pipeline = EE_InstrInfo::Pipeline::COP2;
            info.interpreter_fn = &cop2_vabs;
            break;
        case 0x1E:
            info.pipeline = EE_InstrInfo::Pipeline::COP2;
            info.interpreter_fn = &cop2_vmulai;
            break;
        case 0x1F:
            info.pipeline = EE_InstrInfo::Pipeline::COP2;
            info.interpreter_fn = &cop2_vclip;
            break;
        case 0x20:
            info.pipeline = EE_InstrInfo::Pipeline::COP2;
            info.interpreter_fn = &cop2_vaddaq;
            break;
        case 0x21:
            info.pipeline = EE_InstrInfo::Pipeline::COP2;
            info.interpreter_fn = &cop2_vmaddaq;
            break;
        case 0x22:
            info.pipeline = EE_InstrInfo::Pipeline::COP2;
            info.interpreter_fn = &cop2_vaddai;
            break;
        case 0x23:
            info.pipeline = EE_InstrInfo::Pipeline::COP2;
            info.interpreter_fn = &cop2_vmaddai;
            break;
        case 0x25:
            info.pipeline = EE_InstrInfo::Pipeline::COP2;
            info.interpreter_fn = &cop2_vmsubaq;
            break;
        case 0x26:
            info.pipeline = EE_InstrInfo::Pipeline::COP2;
            info.interpreter_fn = &cop2_vsubai;
            break;
        case 0x27:
            info.pipeline = EE_InstrInfo::Pipeline::COP2;
            info.interpreter_fn = &cop2_vmsubai;
            break;
        case 0x28:
            info.pipeline = EE_InstrInfo::Pipeline::COP2;
            info.interpreter_fn = &cop2_vadda;
            break;
        case 0x29:
            info.pipeline = EE_InstrInfo::Pipeline::COP2;
            info.interpreter_fn = &cop2_vmadda;
            break;
        case 0x2A:
            info.pipeline = EE_InstrInfo::Pipeline::COP2;
            info.interpreter_fn = &cop2_vmula;
            break;
        case 0x2C:
            info.pipeline = EE_InstrInfo::Pipeline::COP2;
            info.interpreter_fn = &cop2_vsuba;
            break;
        case 0x2D:
            info.pipeline = EE_InstrInfo::Pipeline::COP2;
            info.interpreter_fn = &cop2_vmsuba;
            break;
        case 0x2E:
            info.pipeline = EE_InstrInfo::Pipeline::COP2;
            info.interpreter_fn = &cop2_vopmula;
            break;
        case 0x2F:
            /**
              * TODO: vnop?
              */
            info.pipeline = EE_InstrInfo::Pipeline::COP2;
            info.interpreter_fn = &cop2_vnop;
            break;
        case 0x30:
            info.pipeline = EE_InstrInfo::Pipeline::COP2;
            info.interpreter_fn = &cop2_vmove;
            break;
        case 0x31:
            info.pipeline = EE_InstrInfo::Pipeline::COP2;
            info.interpreter_fn = &cop2_vmr32;
            break;
        case 0x34:
            info.pipeline = EE_InstrInfo::Pipeline::COP2;
            info.interpreter_fn = &cop2_vlqi;
            break;
        case 0x35:
            info.pipeline = EE_InstrInfo::Pipeline::COP2;
            info.interpreter_fn = &cop2_vsqi;
            break;
        case 0x36:
            info.pipeline = EE_InstrInfo::Pipeline::COP2;
            info.interpreter_fn = &cop2_vlqd;
            break;
        case 0x37:
            info.pipeline = EE_InstrInfo::Pipeline::COP2;
            info.interpreter_fn = &cop2_vsqd;
            break;
        case 0x38:
            info.pipeline = EE_InstrInfo::Pipeline::COP2;
            info.interpreter_fn = &cop2_vdiv;
            break;
        case 0x39:
            info.pipeline = EE_InstrInfo::Pipeline::COP2;
            info.interpreter_fn = &cop2_vsqrt;
            break;
        case 0x3A:
            info.pipeline = EE_InstrInfo::Pipeline::COP2;
            info.interpreter_fn = &cop2_vrsqrt;
            break;
        case 0x3B:
            info.pipeline = EE_InstrInfo::Pipeline::COP2;
            info.interpreter_fn = &cop2_vwaitq;
            break;
        case 0x3C:
            info.pipeline = EE_InstrInfo::Pipeline::COP2;
            info.interpreter_fn = &cop2_vmtir;
            break;
        case 0x3D:
            info.pipeline = EE_InstrInfo::Pipeline::COP2;
            info.interpreter_fn = &cop2_vmfir;
            break;
        case 0x3E:
            info.pipeline = EE_InstrInfo::Pipeline::COP2;
            info.interpreter_fn = &cop2_vilwr;
            break;
        case 0x3F:
            info.pipeline = EE_InstrInfo::Pipeline::COP2;
            info.interpreter_fn = &cop2_viswr;
            break;
        case 0x40:
            info.pipeline = EE_InstrInfo::Pipeline::COP2;
            info.interpreter_fn = &cop2_vrnext;
            break;
        case 0x41:
            info.pipeline = EE_InstrInfo::Pipeline::COP2;
            info.interpreter_fn = &cop2_vrget;
            break;
        case 0x42:
            info.pipeline = EE_InstrInfo::Pipeline::COP2;
            info.interpreter_fn = &cop2_vrinit;
            break;
        case 0x43:
            info.pipeline = EE_InstrInfo::Pipeline::COP2;
            info.interpreter_fn = &cop2_vrxor;
            break;
        default:
            unknown_op("cop2 special2", instruction, op);
    }
}

void EmotionInterpreter::cop2_vaddabc(EmotionEngine& cpu, uint32_t instruction)
{
    if (!cop2_sync(cpu, instruction))
    {
        return;
    };

    cpu.cop2_updatevu0();
    VectorUnit& vu0 = cpu.get_VU0();
    vu0.decoder.reset();

    uint8_t bc = instruction & 0x3;
    uint8_t source = (instruction >> 11) & 0x1F;
    uint8_t bc_reg = (instruction >> 16) & 0x1F;
    uint8_t field = (instruction >> 21) & 0xF;

    vu0.decoder.vf_read0[0] = source;
    vu0.decoder.vf_read0_field[0] = field;

    vu0.decoder.vf_read1[0] = bc_reg;
    vu0.decoder.vf_read1_field[0] = 1 << (3 - bc);

    vu0.handle_cop2_stalls();
    vu0.addabc(instruction);
}

void EmotionInterpreter::cop2_vsubabc(EmotionEngine& cpu, uint32_t instruction)
{
    if (!cop2_sync(cpu, instruction))
    {
        return;
    };

    cpu.cop2_updatevu0();
    VectorUnit& vu0 = cpu.get_VU0();
    vu0.decoder.reset();

    uint8_t bc = instruction & 0x3;
    uint8_t source = (instruction >> 11) & 0x1F;
    uint8_t bc_reg = (instruction >> 16) & 0x1F;
    uint8_t field = (instruction >> 21) & 0xF;

    vu0.decoder.vf_read0[0] = source;
    vu0.decoder.vf_read0_field[0] = field;

    vu0.decoder.vf_read1[0] = bc_reg;
    vu0.decoder.vf_read1_field[0] = 1 << (3 - bc);

    vu0.handle_cop2_stalls();
    vu0.subabc(instruction);
}

void EmotionInterpreter::cop2_vmaddabc(EmotionEngine& cpu, uint32_t instruction)
{
    if (!cop2_sync(cpu, instruction))
    {
        return;
    };

    cpu.cop2_updatevu0();
    VectorUnit& vu0 = cpu.get_VU0();
    vu0.decoder.reset();

    uint8_t bc = instruction & 0x3;
    uint8_t source = (instruction >> 11) & 0x1F;
    uint8_t bc_reg = (instruction >> 16) & 0x1F;
    uint8_t field = (instruction >> 21) & 0xF;

    vu0.decoder.vf_read0[0] = source;
    vu0.decoder.vf_read0_field[0] = field;

    vu0.decoder.vf_read1[0] = bc_reg;
    vu0.decoder.vf_read1_field[0] = 1 << (3 - bc);

    vu0.handle_cop2_stalls();
    vu0.maddabc(instruction);
}

void EmotionInterpreter::cop2_vmsubabc(EmotionEngine& cpu, uint32_t instruction)
{
    if (!cop2_sync(cpu, instruction))
    {
        return;
    };

    cpu.cop2_updatevu0();
    VectorUnit& vu0 = cpu.get_VU0();
    vu0.decoder.reset();

    uint8_t bc = instruction & 0x3;
    uint8_t source = (instruction >> 11) & 0x1F;
    uint8_t bc_reg = (instruction >> 16) & 0x1F;
    uint8_t field = (instruction >> 21) & 0xF;

    vu0.decoder.vf_read0[0] = source;
    vu0.decoder.vf_read0_field[0] = field;

    vu0.decoder.vf_read1[0] = bc_reg;
    vu0.decoder.vf_read1_field[0] = 1 << (3 - bc);

    vu0.handle_cop2_stalls();
    vu0.msubabc(instruction);
}

void EmotionInterpreter::cop2_vitof0(EmotionEngine& cpu, uint32_t instruction)
{
    if (!cop2_sync(cpu, instruction))
    {
        return;
    };

    cpu.cop2_updatevu0();
    VectorUnit& vu0 = cpu.get_VU0();
    vu0.decoder.reset();

    uint8_t source = (instruction >> 11) & 0x1F;
    uint8_t dest = (instruction >> 16) & 0x1F;
    uint8_t field = (instruction >> 21) & 0xF;

    vu0.decoder.vf_write[0] = dest;
    vu0.decoder.vf_write_field[0] = field;
    vu0.decoder.vf_read0[0] = source;
    vu0.decoder.vf_read0_field[0] = field;

    vu0.handle_cop2_stalls();
    vu0.itof0(instruction);
}

void EmotionInterpreter::cop2_vitof4(EmotionEngine& cpu, uint32_t instruction)
{
    if (!cop2_sync(cpu, instruction))
    {
        return;
    };

    cpu.cop2_updatevu0();
    VectorUnit& vu0 = cpu.get_VU0();
    vu0.decoder.reset();

    uint8_t source = (instruction >> 11) & 0x1F;
    uint8_t dest = (instruction >> 16) & 0x1F;
    uint8_t field = (instruction >> 21) & 0xF;

    vu0.decoder.vf_write[0] = dest;
    vu0.decoder.vf_write_field[0] = field;
    vu0.decoder.vf_read0[0] = source;
    vu0.decoder.vf_read0_field[0] = field;

    vu0.handle_cop2_stalls();
    vu0.itof4(instruction);
}

void EmotionInterpreter::cop2_vitof12(EmotionEngine& cpu, uint32_t instruction)
{
    if (!cop2_sync(cpu, instruction))
    {
        return;
    };

    cpu.cop2_updatevu0();
    VectorUnit& vu0 = cpu.get_VU0();
    vu0.decoder.reset();

    uint8_t source = (instruction >> 11) & 0x1F;
    uint8_t dest = (instruction >> 16) & 0x1F;
    uint8_t field = (instruction >> 21) & 0xF;

    vu0.decoder.vf_write[0] = dest;
    vu0.decoder.vf_write_field[0] = field;
    vu0.decoder.vf_read0[0] = source;
    vu0.decoder.vf_read0_field[0] = field;

    vu0.handle_cop2_stalls();
    vu0.itof12(instruction);
}

void EmotionInterpreter::cop2_vitof15(EmotionEngine& cpu, uint32_t instruction)
{
    if (!cop2_sync(cpu, instruction))
    {
        return;
    };

    cpu.cop2_updatevu0();
    VectorUnit& vu0 = cpu.get_VU0();
    vu0.decoder.reset();

    uint8_t source = (instruction >> 11) & 0x1F;
    uint8_t dest = (instruction >> 16) & 0x1F;
    uint8_t field = (instruction >> 21) & 0xF;

    vu0.decoder.vf_write[0] = dest;
    vu0.decoder.vf_write_field[0] = field;
    vu0.decoder.vf_read0[0] = source;
    vu0.decoder.vf_read0_field[0] = field;

    vu0.handle_cop2_stalls();
    vu0.itof15(instruction);
}

void EmotionInterpreter::cop2_vftoi0(EmotionEngine& cpu, uint32_t instruction)
{
    if (!cop2_sync(cpu, instruction))
    {
        return;
    };

    cpu.cop2_updatevu0();
    VectorUnit& vu0 = cpu.get_VU0();
    vu0.decoder.reset();

    uint8_t source = (instruction >> 11) & 0x1F;
    uint8_t dest = (instruction >> 16) & 0x1F;
    uint8_t field = (instruction >> 21) & 0xF;

    vu0.decoder.vf_write[0] = dest;
    vu0.decoder.vf_write_field[0] = field;
    vu0.decoder.vf_read0[0] = source;
    vu0.decoder.vf_read0_field[0] = field;

    vu0.handle_cop2_stalls();
    vu0.ftoi0(instruction);
}

void EmotionInterpreter::cop2_vftoi4(EmotionEngine& cpu, uint32_t instruction)
{
    if (!cop2_sync(cpu, instruction))
    {
        return;
    };

    cpu.cop2_updatevu0();
    VectorUnit& vu0 = cpu.get_VU0();
    vu0.decoder.reset();

    uint8_t source = (instruction >> 11) & 0x1F;
    uint8_t dest = (instruction >> 16) & 0x1F;
    uint8_t field = (instruction >> 21) & 0xF;

    vu0.decoder.vf_write[0] = dest;
    vu0.decoder.vf_write_field[0] = field;
    vu0.decoder.vf_read0[0] = source;
    vu0.decoder.vf_read0_field[0] = field;

    vu0.handle_cop2_stalls();
    vu0.ftoi4(instruction);
}

void EmotionInterpreter::cop2_vftoi12(EmotionEngine& cpu, uint32_t instruction)
{
    if (!cop2_sync(cpu, instruction))
    {
        return;
    };

    cpu.cop2_updatevu0();
    VectorUnit& vu0 = cpu.get_VU0();
    vu0.decoder.reset();

    uint8_t source = (instruction >> 11) & 0x1F;
    uint8_t dest = (instruction >> 16) & 0x1F;
    uint8_t field = (instruction >> 21) & 0xF;

    vu0.decoder.vf_write[0] = dest;
    vu0.decoder.vf_write_field[0] = field;
    vu0.decoder.vf_read0[0] = source;
    vu0.decoder.vf_read0_field[0] = field;

    vu0.handle_cop2_stalls();
    vu0.ftoi12(instruction);
}

void EmotionInterpreter::cop2_vftoi15(EmotionEngine& cpu, uint32_t instruction)
{
    if (!cop2_sync(cpu, instruction))
    {
        return;
    };

    cpu.cop2_updatevu0();
    VectorUnit& vu0 = cpu.get_VU0();
    vu0.decoder.reset();

    uint8_t source = (instruction >> 11) & 0x1F;
    uint8_t dest = (instruction >> 16) & 0x1F;
    uint8_t field = (instruction >> 21) & 0xF;

    vu0.decoder.vf_write[0] = dest;
    vu0.decoder.vf_write_field[0] = field;
    vu0.decoder.vf_read0[0] = source;
    vu0.decoder.vf_read0_field[0] = field;

    vu0.handle_cop2_stalls();
    vu0.ftoi15(instruction);
}

void EmotionInterpreter::cop2_vmulabc(EmotionEngine& cpu, uint32_t instruction)
{
    if (!cop2_sync(cpu, instruction))
    {
        return;
    };

    cpu.cop2_updatevu0();
    VectorUnit& vu0 = cpu.get_VU0();
    vu0.decoder.reset();

    uint8_t bc = instruction & 0x3;
    uint8_t source = (instruction >> 11) & 0x1F;
    uint8_t bc_reg = (instruction >> 16) & 0x1F;
    uint8_t field = (instruction >> 21) & 0xF;

    vu0.decoder.vf_read0[0] = source;
    vu0.decoder.vf_read0_field[0] = field;

    vu0.decoder.vf_read1[0] = bc_reg;
    vu0.decoder.vf_read1_field[0] = 1 << (3 - bc);

    vu0.handle_cop2_stalls();
    vu0.mulabc(instruction);
}

void EmotionInterpreter::cop2_vmulaq(EmotionEngine& cpu, uint32_t instruction)
{
    if (!cop2_sync(cpu, instruction))
    {
        return;
    };

    cpu.cop2_updatevu0();
    VectorUnit& vu0 = cpu.get_VU0();
    vu0.decoder.reset();

    uint8_t source = (instruction >> 11) & 0x1F;
    uint8_t field = (instruction >> 21) & 0xF;

    vu0.decoder.vf_read0[0] = source;
    vu0.decoder.vf_read0_field[0] = field;

    vu0.handle_cop2_stalls();
    vu0.mulaq(instruction);
}

void EmotionInterpreter::cop2_vabs(EmotionEngine& cpu, uint32_t instruction)
{
    if (!cop2_sync(cpu, instruction))
    {
        return;
    };

    cpu.cop2_updatevu0();
    VectorUnit& vu0 = cpu.get_VU0();
    vu0.decoder.reset();

    uint8_t source = (instruction >> 11) & 0x1F;
    uint8_t dest = (instruction >> 16) & 0x1F;
    uint8_t field = (instruction >> 21) & 0xF;

    vu0.decoder.vf_write[0] = dest;
    vu0.decoder.vf_write_field[0] = field;

    vu0.decoder.vf_read0[0] = source;
    vu0.decoder.vf_read0_field[0] = field;

    vu0.handle_cop2_stalls();
    vu0.abs(instruction);
}

void EmotionInterpreter::cop2_vmulai(EmotionEngine& cpu, uint32_t instruction)
{
    if (!cop2_sync(cpu, instruction))
    {
        return;
    };

    cpu.cop2_updatevu0();
    VectorUnit& vu0 = cpu.get_VU0();
    vu0.decoder.reset();

    uint8_t source = (instruction >> 11) & 0x1F;
    uint8_t field = (instruction >> 21) & 0xF;

    vu0.decoder.vf_read0[0] = source;
    vu0.decoder.vf_read0_field[0] = field;

    vu0.handle_cop2_stalls();
    vu0.mulai(instruction);
}

void EmotionInterpreter::cop2_vclip(EmotionEngine& cpu, uint32_t instruction)
{
    if (!cop2_sync(cpu, instruction))
    {
        return;
    };

    cpu.cop2_updatevu0();
    VectorUnit& vu0 = cpu.get_VU0();
    vu0.decoder.reset();

    uint8_t reg1 = (instruction >> 11) & 0x1F;
    uint8_t reg2 = (instruction >> 16) & 0x1F;

    vu0.decoder.vf_read0[0] = reg1;
    vu0.decoder.vf_read0_field[0] = 0xE; //xyz

    vu0.decoder.vf_read1[0] = reg2;
    vu0.decoder.vf_read1_field[0] = 0x1; //w

    vu0.handle_cop2_stalls();
    vu0.clip(instruction);
}

void EmotionInterpreter::cop2_vaddaq(EmotionEngine& cpu, uint32_t instruction)
{
    if (!cop2_sync(cpu, instruction))
    {
        return;
    };

    cpu.cop2_updatevu0();
    VectorUnit& vu0 = cpu.get_VU0();
    vu0.decoder.reset();

    uint8_t source = (instruction >> 11) & 0x1F;
    uint8_t field = (instruction >> 21) & 0xF;

    vu0.decoder.vf_read0[0] = source;
    vu0.decoder.vf_read0_field[0] = field;

    vu0.handle_cop2_stalls();
    vu0.addaq(instruction);
}

void EmotionInterpreter::cop2_vmaddaq(EmotionEngine& cpu, uint32_t instruction)
{
    if (!cop2_sync(cpu, instruction))
    {
        return;
    };

    cpu.cop2_updatevu0();
    VectorUnit& vu0 = cpu.get_VU0();
    vu0.decoder.reset();

    uint8_t source = (instruction >> 11) & 0x1F;
    uint8_t field = (instruction >> 21) & 0xF;

    vu0.decoder.vf_read0[0] = source;
    vu0.decoder.vf_read0_field[0] = field;

    vu0.handle_cop2_stalls();
    vu0.maddaq(instruction);
}

void EmotionInterpreter::cop2_vmaddai(EmotionEngine& cpu, uint32_t instruction)
{
    if (!cop2_sync(cpu, instruction))
    {
        return;
    };

    cpu.cop2_updatevu0();
    VectorUnit& vu0 = cpu.get_VU0();
    vu0.decoder.reset();

    uint8_t source = (instruction >> 11) & 0x1F;
    uint8_t field = (instruction >> 21) & 0xF;

    vu0.decoder.vf_read0[0] = source;
    vu0.decoder.vf_read0_field[0] = field;

    vu0.handle_cop2_stalls();
    vu0.maddai(instruction);
}

void EmotionInterpreter::cop2_vmsubaq(EmotionEngine& cpu, uint32_t instruction)
{
    if (!cop2_sync(cpu, instruction))
    {
        return;
    };

    cpu.cop2_updatevu0();
    VectorUnit& vu0 = cpu.get_VU0();
    vu0.decoder.reset();

    uint8_t source = (instruction >> 11) & 0x1F;
    uint8_t field = (instruction >> 21) & 0xF;

    vu0.decoder.vf_read0[0] = source;
    vu0.decoder.vf_read0_field[0] = field;

    vu0.handle_cop2_stalls();
    vu0.msubaq(instruction);
}

void EmotionInterpreter::cop2_vsubai(EmotionEngine &cpu, uint32_t instruction)
{
    if (!cop2_sync(cpu, instruction))
    {
        return;
    };

    cpu.cop2_updatevu0();
    VectorUnit& vu0 = cpu.get_VU0();
    vu0.decoder.reset();

    uint8_t source = (instruction >> 11) & 0x1F;
    uint8_t field = (instruction >> 21) & 0xF;

    vu0.decoder.vf_read0[0] = source;
    vu0.decoder.vf_read0_field[0] = field;

    vu0.handle_cop2_stalls();
    vu0.subai(instruction);
}

void EmotionInterpreter::cop2_vmsubai(EmotionEngine& cpu, uint32_t instruction)
{
    if (!cop2_sync(cpu, instruction))
    {
        return;
    };

    cpu.cop2_updatevu0();
    VectorUnit& vu0 = cpu.get_VU0();
    vu0.decoder.reset();

    uint8_t source = (instruction >> 11) & 0x1F;
    uint8_t field = (instruction >> 21) & 0xF;

    vu0.decoder.vf_read0[0] = source;
    vu0.decoder.vf_read0_field[0] = field;

    vu0.handle_cop2_stalls();
    vu0.msubai(instruction);
}

void EmotionInterpreter::cop2_vadda(EmotionEngine& cpu, uint32_t instruction)
{
    if (!cop2_sync(cpu, instruction))
    {
        return;
    };

    cpu.cop2_updatevu0();
    VectorUnit& vu0 = cpu.get_VU0();
    vu0.decoder.reset();

    uint8_t reg1 = (instruction >> 11) & 0x1F;
    uint8_t reg2 = (instruction >> 16) & 0x1F;
    uint8_t field = (instruction >> 21) & 0xF;

    vu0.decoder.vf_read0[0] = reg1;
    vu0.decoder.vf_read0_field[0] = field;

    vu0.decoder.vf_read1[0] = reg2;
    vu0.decoder.vf_read1_field[0] = field;

    vu0.handle_cop2_stalls();
    vu0.adda(instruction);
}

void EmotionInterpreter::cop2_vaddai(EmotionEngine& cpu, uint32_t instruction)
{
    if (!cop2_sync(cpu, instruction))
    {
        return;
    };

    cpu.cop2_updatevu0();
    VectorUnit& vu0 = cpu.get_VU0();
    vu0.decoder.reset();

    uint8_t reg1 = (instruction >> 11) & 0x1F;
    uint8_t field = (instruction >> 21) & 0xF;

    vu0.decoder.vf_read0[0] = reg1;
    vu0.decoder.vf_read0_field[0] = field;

    vu0.handle_cop2_stalls();
    vu0.addai(instruction);
}

void EmotionInterpreter::cop2_vmadda(EmotionEngine& cpu, uint32_t instruction)
{
    if (!cop2_sync(cpu, instruction))
    {
        return;
    };

    cpu.cop2_updatevu0();
    VectorUnit& vu0 = cpu.get_VU0();
    vu0.decoder.reset();

    uint8_t reg1 = (instruction >> 11) & 0x1F;
    uint8_t reg2 = (instruction >> 16) & 0x1F;
    uint8_t field = (instruction >> 21) & 0xF;

    vu0.decoder.vf_read0[0] = reg1;
    vu0.decoder.vf_read0_field[0] = field;

    vu0.decoder.vf_read1[0] = reg2;
    vu0.decoder.vf_read1_field[0] = field;

    vu0.handle_cop2_stalls();
    vu0.madda(instruction);
}

void EmotionInterpreter::cop2_vmula(EmotionEngine& cpu, uint32_t instruction)
{
    if (!cop2_sync(cpu, instruction))
    {
        return;
    };

    cpu.cop2_updatevu0();
    VectorUnit& vu0 = cpu.get_VU0();
    vu0.decoder.reset();

    uint8_t reg1 = (instruction >> 11) & 0x1F;
    uint8_t reg2 = (instruction >> 16) & 0x1F;
    uint8_t field = (instruction >> 21) & 0xF;

    vu0.decoder.vf_read0[0] = reg1;
    vu0.decoder.vf_read0_field[0] = field;

    vu0.decoder.vf_read1[0] = reg2;
    vu0.decoder.vf_read1_field[0] = field;

    vu0.handle_cop2_stalls();
    vu0.mula(instruction);
}

void EmotionInterpreter::cop2_vsuba(EmotionEngine& cpu, uint32_t instruction)
{
    if (!cop2_sync(cpu, instruction))
    {
        return;
    };

    cpu.cop2_updatevu0();
    VectorUnit& vu0 = cpu.get_VU0();
    vu0.decoder.reset();

    uint8_t reg1 = (instruction >> 11) & 0x1F;
    uint8_t reg2 = (instruction >> 16) & 0x1F;
    uint8_t field = (instruction >> 21) & 0xF;

    vu0.decoder.vf_read0[0] = reg1;
    vu0.decoder.vf_read0_field[0] = field;

    vu0.decoder.vf_read1[0] = reg2;
    vu0.decoder.vf_read1_field[0] = field;

    vu0.handle_cop2_stalls();
    vu0.suba(instruction);
}

void EmotionInterpreter::cop2_vmsuba(EmotionEngine& cpu, uint32_t instruction)
{
    if (!cop2_sync(cpu, instruction))
    {
        return;
    };

    cpu.cop2_updatevu0();
    VectorUnit& vu0 = cpu.get_VU0();
    vu0.decoder.reset();

    uint8_t reg1 = (instruction >> 11) & 0x1F;
    uint8_t reg2 = (instruction >> 16) & 0x1F;
    uint8_t field = (instruction >> 21) & 0xF;

    vu0.decoder.vf_read0[0] = reg1;
    vu0.decoder.vf_read0_field[0] = field;

    vu0.decoder.vf_read1[0] = reg2;
    vu0.decoder.vf_read1_field[0] = field;

    vu0.handle_cop2_stalls();
    vu0.msuba(instruction);
}

void EmotionInterpreter::cop2_vopmula(EmotionEngine& cpu, uint32_t instruction)
{
    if (!cop2_sync(cpu, instruction))
    {
        return;
    };

    cpu.cop2_updatevu0();
    VectorUnit& vu0 = cpu.get_VU0();
    vu0.decoder.reset();

    uint8_t reg1 = (instruction >> 11) & 0x1F;
    uint8_t reg2 = (instruction >> 16) & 0x1F;
    vu0.decoder.vf_read0[0] = reg1;
    vu0.decoder.vf_read1[0] = reg2;
    vu0.decoder.vf_read0_field[0] = 0xE; //xyz
    vu0.decoder.vf_read1_field[0] = 0xE; //xyz

    vu0.handle_cop2_stalls();
    vu0.opmula(instruction);
}

void EmotionInterpreter::cop2_vnop(EmotionEngine& cpu, uint32_t instruction)
{
    if (!cop2_sync(cpu, instruction))
    {
        return;
    };

    cpu.cop2_updatevu0();
    VectorUnit& vu0 = cpu.get_VU0();
    vu0.decoder.reset();
}

void EmotionInterpreter::cop2_vmove(EmotionEngine& cpu, uint32_t instruction)
{
    if (!cop2_sync(cpu, instruction))
    {
        return;
    };

    cpu.cop2_updatevu0();
    VectorUnit& vu0 = cpu.get_VU0();
    vu0.decoder.reset();

    uint8_t source = (instruction >> 11) & 0x1F;
    uint8_t dest = (instruction >> 16) & 0x1F;
    uint8_t field = (instruction >> 21) & 0xF;

    vu0.decoder.vf_write[1] = dest;
    vu0.decoder.vf_write_field[1] = field;

    vu0.decoder.vf_read0[1] = source;
    vu0.decoder.vf_read0_field[1] = field;

    vu0.handle_cop2_stalls();
    vu0.move(instruction);
}

void EmotionInterpreter::cop2_vmr32(EmotionEngine& cpu, uint32_t instruction)
{
    if (!cop2_sync(cpu, instruction))
    {
        return;
    };

    cpu.cop2_updatevu0();
    VectorUnit& vu0 = cpu.get_VU0();
    vu0.decoder.reset();

    uint8_t source = (instruction >> 11) & 0x1F;
    uint8_t dest = (instruction >> 16) & 0x1F;
    uint8_t field = (instruction >> 21) & 0xF;

    vu0.decoder.vf_write[1] = dest;
    vu0.decoder.vf_write_field[1] = field;

    vu0.decoder.vf_read0[1] = source;
    vu0.decoder.vf_read0_field[1] = (field >> 1) | ((field & 0x1) << 3);

    vu0.handle_cop2_stalls();
    vu0.mr32(instruction);
}

void EmotionInterpreter::cop2_vlqi(EmotionEngine& cpu, uint32_t instruction)
{
    if (!cop2_sync(cpu, instruction))
    {
        return;
    };

    cpu.cop2_updatevu0();
    VectorUnit& vu0 = cpu.get_VU0();
    vu0.decoder.reset();

    uint8_t is = (instruction >> 11) & 0xF;
    uint8_t ft = (instruction >> 16) & 0x1F;
    uint8_t field = (instruction >> 21) & 0xF;
    vu0.decoder.vf_write[1] = ft;
    vu0.decoder.vf_write_field[1] = field;
    vu0.decoder.vf_write[0] = is;
    vu0.decoder.vi_read0 = is;

    vu0.handle_cop2_stalls();
    vu0.lqi(instruction);
}

void EmotionInterpreter::cop2_vsqi(EmotionEngine& cpu, uint32_t instruction)
{
    if (!cop2_sync(cpu, instruction))
    {
        return;
    };

    cpu.cop2_updatevu0();
    VectorUnit& vu0 = cpu.get_VU0();
    vu0.decoder.reset();

    uint32_t fs = (instruction >> 11) & 0x1F;
    uint32_t it = (instruction >> 16) & 0xF;
    uint8_t dest_field = (instruction >> 21) & 0xF;
    vu0.decoder.vf_read0[1] = fs;
    vu0.decoder.vf_read0_field[1] = dest_field;
    vu0.decoder.vf_write[0] = it;
    vu0.decoder.vi_read0 = it;

    vu0.handle_cop2_stalls();
    vu0.sqi(instruction);
}

void EmotionInterpreter::cop2_vlqd(EmotionEngine& cpu, uint32_t instruction)
{
    if (!cop2_sync(cpu, instruction))
    {
        return;
    };

    cpu.cop2_updatevu0();
    VectorUnit& vu0 = cpu.get_VU0();
    vu0.decoder.reset();

    uint32_t is = (instruction >> 11) & 0xF;
    uint32_t ft = (instruction >> 16) & 0x1F;
    uint8_t dest_field = (instruction >> 21) & 0xF;
    vu0.decoder.vf_write[1] = ft;
    vu0.decoder.vf_write_field[1] = dest_field;
    vu0.decoder.vf_write[0] = is;
    vu0.decoder.vi_read0 = is;

    vu0.handle_cop2_stalls();
    vu0.lqd(instruction);
}

void EmotionInterpreter::cop2_vsqd(EmotionEngine& cpu, uint32_t instruction)
{
    if (!cop2_sync(cpu, instruction))
    {
        return;
    };

    cpu.cop2_updatevu0();
    VectorUnit& vu0 = cpu.get_VU0();
    vu0.decoder.reset();

    uint32_t fs = (instruction >> 11) & 0x1F;
    uint32_t it = (instruction >> 16) & 0xF;
    uint8_t dest_field = (instruction >> 21) & 0xF;
    vu0.decoder.vf_read0[1] = fs;
    vu0.decoder.vf_read0_field[1] = dest_field;
    vu0.decoder.vf_write[0] = it;
    vu0.decoder.vi_read0 = it;

    vu0.handle_cop2_stalls();
    vu0.sqd(instruction);
}

void EmotionInterpreter::cop2_vdiv(EmotionEngine& cpu, uint32_t instruction)
{
    if (!cop2_sync(cpu, instruction))
    {
        return;
    };

    cpu.cop2_updatevu0();
    VectorUnit& vu0 = cpu.get_VU0();
    vu0.decoder.reset();

    uint8_t reg1 = (instruction >> 11) & 0x1F;
    uint8_t reg2 = (instruction >> 16) & 0x1F;
    uint8_t fsf = (instruction >> 21) & 0x3;
    uint8_t ftf = (instruction >> 23) & 0x3;

    vu0.decoder.vf_read0[1] = reg1;
    vu0.decoder.vf_read0_field[1] = 1 << (3 - fsf);

    vu0.decoder.vf_read1[1] = reg2;
    vu0.decoder.vf_read1_field[1] = 1 << (3 - ftf);

    vu0.waitq(instruction);
    vu0.handle_cop2_stalls();
    vu0.div(instruction);
}

void EmotionInterpreter::cop2_vsqrt(EmotionEngine& cpu, uint32_t instruction)
{
    if (!cop2_sync(cpu, instruction))
    {
        return;
    };

    cpu.cop2_updatevu0();
    VectorUnit& vu0 = cpu.get_VU0();
    vu0.decoder.reset();

    uint8_t source = (instruction >> 16) & 0x1F;
    uint8_t ftf = (instruction >> 23) & 0x3;

    vu0.decoder.vf_read0[1] = source;
    vu0.decoder.vf_read0_field[1] = 1 << (3 - ftf);

    vu0.waitq(instruction);
    vu0.handle_cop2_stalls();
    vu0.vu_sqrt(instruction);
}

void EmotionInterpreter::cop2_vrsqrt(EmotionEngine& cpu, uint32_t instruction)
{
    if (!cop2_sync(cpu, instruction))
    {
        return;
    };

    cpu.cop2_updatevu0();
    VectorUnit& vu0 = cpu.get_VU0();
    vu0.decoder.reset();

    uint8_t reg1 = (instruction >> 11) & 0x1F;
    uint8_t reg2 = (instruction >> 16) & 0x1F;
    uint8_t fsf = (instruction >> 21) & 0x3;
    uint8_t ftf = (instruction >> 23) & 0x3;

    vu0.decoder.vf_read0[1] = reg1;
    vu0.decoder.vf_read0_field[1] = 1 << (3 - fsf);

    vu0.decoder.vf_read1[1] = reg2;
    vu0.decoder.vf_read1_field[1] = 1 << (3 - ftf);

    vu0.waitq(instruction);
    vu0.handle_cop2_stalls();
    vu0.rsqrt(instruction);
}

void EmotionInterpreter::cop2_vwaitq(EmotionEngine& cpu, uint32_t instruction)
{
    if (!cop2_sync(cpu, instruction))
    {
        return;
    };

    cpu.cop2_updatevu0();
    VectorUnit& vu0 = cpu.get_VU0();
    vu0.decoder.reset();

    cpu.get_VU0().waitq(instruction);
}

void EmotionInterpreter::cop2_vmtir(EmotionEngine& cpu, uint32_t instruction)
{
    if (!cop2_sync(cpu, instruction))
    {
        return;
    };

    cpu.cop2_updatevu0();
    VectorUnit& vu0 = cpu.get_VU0();
    vu0.decoder.reset();

    uint32_t fs = (instruction >> 11) & 0x1F;
    uint32_t it = (instruction >> 16) & 0xF;
    uint8_t fsf = (instruction >> 21) & 0x3;
    vu0.decoder.vf_read0[1] = fs;
    vu0.decoder.vf_read0_field[1] = 1 << (3 - fsf);
    vu0.decoder.vf_write[0] = it;

    vu0.handle_cop2_stalls();
    vu0.mtir(instruction);
}

void EmotionInterpreter::cop2_vmfir(EmotionEngine& cpu, uint32_t instruction)
{
    if (!cop2_sync(cpu, instruction))
    {
        return;
    };

    cpu.cop2_updatevu0();
    VectorUnit& vu0 = cpu.get_VU0();
    vu0.decoder.reset();

    uint32_t is = (instruction >> 11) & 0x1F;
    uint32_t ft = (instruction >> 16) & 0x1F;
    uint8_t dest_field = (instruction >> 21) & 0xF;
    vu0.decoder.vi_read0 = is;
    vu0.decoder.vf_write[1] = ft;
    vu0.decoder.vf_write_field[1] = dest_field;

    vu0.handle_cop2_stalls();
    vu0.mfir(instruction);
}

void EmotionInterpreter::cop2_vilwr(EmotionEngine& cpu, uint32_t instruction)
{
    if (!cop2_sync(cpu, instruction))
    {
        return;
    };

    cpu.cop2_updatevu0();
    VectorUnit& vu0 = cpu.get_VU0();
    vu0.decoder.reset();

    uint8_t is = (instruction >> 11) & 0x1F;
    uint8_t it = (instruction >> 16) & 0x1F;

    vu0.decoder.vf_write[0] = it;
    vu0.decoder.vi_write_from_load = it;
    vu0.decoder.vi_read0 = is;

    vu0.handle_cop2_stalls();
    vu0.ilwr(instruction);
}

void EmotionInterpreter::cop2_viswr(EmotionEngine& cpu, uint32_t instruction)
{
    if (!cop2_sync(cpu, instruction))
    {
        return;
    };

    cpu.cop2_updatevu0();
    VectorUnit& vu0 = cpu.get_VU0();
    vu0.decoder.reset();

    uint8_t is = (instruction >> 11) & 0x1F;
    uint8_t it = (instruction >> 16) & 0x1F;
    uint8_t field = (instruction >> 21) & 0xF;

    vu0.decoder.vi_read0 = is;
    vu0.decoder.vi_read1 = it;

    vu0.handle_cop2_stalls();
    vu0.iswr(instruction);
}

void EmotionInterpreter::cop2_vrnext(EmotionEngine& cpu, uint32_t instruction)
{
    if (!cop2_sync(cpu, instruction))
    {
        return;
    };

    cpu.cop2_updatevu0();
    VectorUnit& vu0 = cpu.get_VU0();
    vu0.decoder.reset();

    uint8_t dest = (instruction >> 16) & 0x1F;
    uint8_t field = (instruction >> 21) & 0xF;

    vu0.decoder.vf_write[1] = dest;
    vu0.decoder.vf_write_field[1] = field;

    vu0.handle_cop2_stalls();
    vu0.rnext(instruction);
}

void EmotionInterpreter::cop2_vrget(EmotionEngine& cpu, uint32_t instruction)
{
    if (!cop2_sync(cpu, instruction))
    {
        return;
    };

    cpu.cop2_updatevu0();
    VectorUnit& vu0 = cpu.get_VU0();
    vu0.decoder.reset();

    uint8_t dest = (instruction >> 16) & 0x1F;
    uint8_t field = (instruction >> 21) & 0xF;

    vu0.decoder.vf_write[1] = dest;
    vu0.decoder.vf_write_field[1] = field;

    vu0.handle_cop2_stalls();
    vu0.rget(instruction);
}

void EmotionInterpreter::cop2_vrinit(EmotionEngine& cpu, uint32_t instruction)
{
    if (!cop2_sync(cpu, instruction))
    {
        return;
    };

    cpu.cop2_updatevu0();
    VectorUnit& vu0 = cpu.get_VU0();
    vu0.decoder.reset();

    uint8_t source = (instruction >> 11) & 0x1F;
    uint8_t fsf = (instruction >> 21) & 0x3;

    vu0.decoder.vf_read0[1] = source;
    vu0.decoder.vf_read0_field[1] = 1 << (3 - fsf);

    vu0.handle_cop2_stalls();
    vu0.rinit(instruction);
}

void EmotionInterpreter::cop2_vrxor(EmotionEngine& cpu, uint32_t instruction)
{
    if (!cop2_sync(cpu, instruction))
    {
        return;
    };

    cpu.cop2_updatevu0();
    VectorUnit& vu0 = cpu.get_VU0();
    vu0.decoder.reset();

    uint8_t source = (instruction >> 11) & 0x1F;
    uint8_t fsf = (instruction >> 21) & 0x3;

    vu0.decoder.vf_read0[1] = source;
    vu0.decoder.vf_read0_field[1] = 1 << (3 - fsf);

    vu0.handle_cop2_stalls();
    vu0.rxor(instruction);
}
