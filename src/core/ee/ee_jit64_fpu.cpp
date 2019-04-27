#include <cmath>
#include <algorithm>
#include "ee_jit64.hpp"

void EE_JIT64::floating_point_absolute_value(EmotionEngine& ee, IR::Instruction& instr)
{
    // This operation works by simply masking out the sign-bit from the float.
    // Note that an array of four masks is used here, despite only doing the
    // operation upon one float.
    // This is due to there being no "ANDSS" instruction, which means four
    // floats are masked at the same time.

    REG_64 source = alloc_fpu_reg(ee, instr.get_source(), REG_STATE::READ);
    REG_64 dest = alloc_fpu_reg(ee, instr.get_dest(), REG_STATE::WRITE);

    emitter.load_addr((uint64_t)&ee.fpu->control.u, REG_64::RAX);
    emitter.MOV8_IMM_MEM(false, REG_64::RAX);
    emitter.load_addr((uint64_t)&ee.fpu->control.o, REG_64::RAX);
    emitter.MOV8_IMM_MEM(false, REG_64::RAX);

    if (source != dest)
        emitter.MOVSS_REG(source, dest);
    emitter.load_addr((uint64_t)&FPU_MASK_ABS[0], REG_64::RAX);
    emitter.PAND_XMM_FROM_MEM(REG_64::RAX, dest);
}

void EE_JIT64::floating_point_negate(EmotionEngine& ee, IR::Instruction& instr)
{
    // This operation works by simply toggling the sign-bit in the float.
    // Note that an array of four masks is used here, despite only doing the
    // operation upon one float.
    // This is due to there being no "XORSS" instruction, which means four
    // floats are masked at the same time.

    REG_64 source = alloc_fpu_reg(ee, instr.get_source(), REG_STATE::READ);
    REG_64 dest = alloc_fpu_reg(ee, instr.get_dest(), REG_STATE::WRITE);

    emitter.load_addr((uint64_t)&ee.fpu->control.u, REG_64::RAX);
    emitter.MOV8_IMM_MEM(false, REG_64::RAX);
    emitter.load_addr((uint64_t)&ee.fpu->control.o, REG_64::RAX);
    emitter.MOV8_IMM_MEM(false, REG_64::RAX);

    if (source != dest)
        emitter.MOVSS_REG(source, dest);
    emitter.load_addr((uint64_t)&FPU_MASK_NEG[0], REG_64::RAX);
    emitter.PXOR_XMM_FROM_MEM(REG_64::RAX, dest);
}

void EE_JIT64::floating_point_maximum(EmotionEngine& ee, IR::Instruction& instr)
{
    REG_64 dest = alloc_fpu_reg(ee, instr.get_dest(), REG_STATE::WRITE);
    REG_64 source = alloc_fpu_reg(ee, instr.get_source(), REG_STATE::READ);
    REG_64 source2 = alloc_fpu_reg(ee, instr.get_source2(), REG_STATE::READ);

    emitter.load_addr((uint64_t)&ee.fpu->control.u, REG_64::RAX);
    emitter.MOV8_IMM_MEM(false, REG_64::RAX);
    emitter.load_addr((uint64_t)&ee.fpu->control.o, REG_64::RAX);
    emitter.MOV8_IMM_MEM(false, REG_64::RAX);

    if (dest != source)
    {
        emitter.MOVAPS_REG(source, dest);
        emitter.MAXPS(source2, dest);
    }
    else if (dest != source2)
    {
        emitter.MOVAPS_REG(source2, dest);
        emitter.MAXPS(source, dest);
    }

    // if dest == source && dest == source2, then no operation is performed
}

void EE_JIT64::floating_point_minimum(EmotionEngine& ee, IR::Instruction& instr)
{
    REG_64 dest = alloc_fpu_reg(ee, instr.get_dest(), REG_STATE::WRITE);
    REG_64 source = alloc_fpu_reg(ee, instr.get_source(), REG_STATE::READ);
    REG_64 source2 = alloc_fpu_reg(ee, instr.get_source2(), REG_STATE::READ);

    emitter.load_addr((uint64_t)&ee.fpu->control.u, REG_64::RAX);
    emitter.MOV8_IMM_MEM(false, REG_64::RAX);
    emitter.load_addr((uint64_t)&ee.fpu->control.o, REG_64::RAX);
    emitter.MOV8_IMM_MEM(false, REG_64::RAX);

    if (dest != source)
    {
        emitter.MOVAPS_REG(source, dest);
        emitter.MINPS(source2, dest);
    }
    else if (dest != source2)
    {
        emitter.MOVAPS_REG(source2, dest);
        emitter.MINPS(source, dest);
    }

    // if dest == source && dest == source2, then no operation is performed
}

void EE_JIT64::floating_point_square_root(EmotionEngine& ee, IR::Instruction& instr)
{
    REG_64 dest = alloc_fpu_reg(ee, instr.get_dest(), REG_STATE::WRITE);
    REG_64 source = alloc_fpu_reg(ee, instr.get_source(), REG_STATE::READ);
    floating_point_absolute_value(ee, instr);
    
    emitter.SQRTSS(source, dest);
}

void EE_JIT64::move_from_coprocessor1(EmotionEngine& ee, IR::Instruction& instr)
{
    REG_64 source = alloc_fpu_reg(ee, instr.get_source(), REG_STATE::READ);
    REG_64 dest = alloc_gpr_reg(ee, instr.get_dest(), REG_STATE::WRITE);

    emitter.MOVD_FROM_XMM(source, dest);
}

void EE_JIT64::move_to_coprocessor1(EmotionEngine& ee, IR::Instruction& instr)
{
    REG_64 source = alloc_gpr_reg(ee, instr.get_source(), REG_STATE::READ);
    REG_64 dest = alloc_fpu_reg(ee, instr.get_dest(), REG_STATE::WRITE);

    emitter.MOVD_TO_XMM(source, dest);
}

void EE_JIT64::move_xmm_reg(EmotionEngine& ee, IR::Instruction& instr)
{
    REG_64 source = alloc_fpu_reg(ee, instr.get_source(), REG_STATE::READ);
    REG_64 dest = alloc_fpu_reg(ee, instr.get_dest(), REG_STATE::WRITE);

    emitter.MOVAPS_REG(source, dest);
}