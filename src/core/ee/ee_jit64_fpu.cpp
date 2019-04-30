#include <cmath>
#include <algorithm>
#include "ee_jit64.hpp"

// Tests a register for underflow/overflow and clamps as necessary
// Sets the O/U/SO/SU FPU flags if specified.
void EE_JIT64::fpu_test_overflow_underflow(EmotionEngine& ee, REG_64 test, bool set_flags)
{
    // Test overflow
    emitter.MOVD_FROM_XMM(test, REG_64::RAX);
    emitter.AND32_EAX(0x7F800000);
    emitter.CMP32_EAX(0x7F800000);
    uint8_t *underflow = emitter.JCC_NEAR_DEFERRED(ConditionCode::NE);

    // Overflow has occurred
    emitter.MOVD_FROM_XMM(test, REG_64::RAX);
    emitter.XOR32_EAX(0x00800000);
    emitter.OR32_EAX(0x007FFFFF);
    emitter.MOVD_TO_XMM(REG_64::RAX, test);
    if (set_flags)
    {
        emitter.load_addr(ee.fpu->control.o, REG_64::RAX);
        emitter.MOV8_IMM_MEM(true, REG_64::RAX);
        emitter.load_addr(ee.fpu->control.so, REG_64::RAX);
        emitter.MOV8_IMM_MEM(true, REG_64::RAX);
    }
    uint8_t *exit0 = emitter.JMP_NEAR_DEFERRED();

    // Test underflow
    emitter.set_jump_dest(underflow);
    emitter.UCOMISS(test, test);
    uint8_t *exit1 = emitter.JCC_NEAR_DEFERRED(ConditionCode::NP);
    emitter.MOVD_FROM_XMM(test, REG_64::RAX);
    emitter.OR32_EAX(0x80000000);
    emitter.CMP32_EAX(0x80000000);
    uint8_t *exit2 = emitter.JCC_NEAR_DEFERRED(ConditionCode::E);

    // Underflow has occurred
    // We treat denormals as zero.
    emitter.MOVD_FROM_XMM(test, REG_64::RAX);
    emitter.AND32_EAX(0x80000000); // only preserves sign (e.g. +0/-0)
    emitter.MOVD_TO_XMM(REG_64::RAX, test);
    if (set_flags)
    {
        emitter.load_addr(ee.fpu->control.u, REG_64::RAX);
        emitter.MOV8_IMM_MEM(true, REG_64::RAX);
        emitter.load_addr(ee.fpu->control.su, REG_64::RAX);
        emitter.MOV8_IMM_MEM(true, REG_64::RAX);
    }

    // End of function
    emitter.set_jump_dest(exit0);
    emitter.set_jump_dest(exit1);
    emitter.set_jump_dest(exit2);
}

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

void EE_JIT64::floating_point_add(EmotionEngine& ee, IR::Instruction& instr)
{
    REG_64 source = alloc_fpu_reg(ee, instr.get_source(), REG_STATE::READ);
    REG_64 source2 = alloc_fpu_reg(ee, instr.get_source2(), REG_STATE::READ);
    REG_64 dest = alloc_fpu_reg(ee, instr.get_dest(), REG_STATE::WRITE);

    if (dest == source)
    {
        emitter.ADDSS(source2, dest);
    }
    else if (dest == source2)
    {
        emitter.ADDSS(source, dest);
    }
    else
    {
        emitter.MOVAPS_REG(source, dest);
        emitter.ADDSS(source2, dest);
    }

    // Clamp and set flags
    fpu_test_overflow_underflow(ee, dest, true);
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
    REG_64 source = alloc_fpu_reg(ee, instr.get_source(), REG_STATE::READ);
    REG_64 source2 = alloc_fpu_reg(ee, instr.get_source2(), REG_STATE::READ);
    REG_64 dest = alloc_fpu_reg(ee, instr.get_dest(), REG_STATE::WRITE);

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
    REG_64 source = alloc_fpu_reg(ee, instr.get_source(), REG_STATE::READ);
    REG_64 source2 = alloc_fpu_reg(ee, instr.get_source2(), REG_STATE::READ);
    REG_64 dest = alloc_fpu_reg(ee, instr.get_dest(), REG_STATE::WRITE);

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
    REG_64 source = alloc_fpu_reg(ee, instr.get_source(), REG_STATE::READ);
    REG_64 dest = alloc_fpu_reg(ee, instr.get_dest(), REG_STATE::WRITE);
    floating_point_absolute_value(ee, instr);
    
    emitter.SQRTSS(source, dest);
}

void EE_JIT64::move_from_coprocessor1(EmotionEngine& ee, IR::Instruction& instr)
{
    REG_64 source = alloc_fpu_reg(ee, instr.get_source(), REG_STATE::READ);
    REG_64 dest = alloc_gpr_reg(ee, instr.get_dest(), REG_STATE::WRITE);

    emitter.MOVD_FROM_XMM(source, dest);
    emitter.MOVSX32_TO_64(dest, dest);
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