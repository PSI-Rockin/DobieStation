#include <cmath>
#include <algorithm>
#include "ee_jit64.hpp"

void EE_JIT64::clamp_freg(EmotionEngine& ee, REG_64 freg)
{
    //reg = min_signed(reg, 0x7F7FFFFF)
    emitter.load_addr((uint64_t)&max_flt_constant, REG_64::RAX);
    emitter.PMINSD_XMM_FROM_MEM(REG_64::RAX, freg);

    //reg = min_unsigned(reg, 0xFF7FFFFF)
    emitter.load_addr((uint64_t)&min_flt_constant, REG_64::RAX);
    emitter.PMINUD_XMM_FROM_MEM(REG_64::RAX, freg);
}

void EE_JIT64::fixed_point_convert_to_floating_point(EmotionEngine& ee, IR::Instruction& instr)
{
    REG_64 source = alloc_reg(ee, instr.get_source(), REG_TYPE::FPU, REG_STATE::READ);
    REG_64 dest = alloc_reg(ee, instr.get_dest(), REG_TYPE::FPU, REG_STATE::WRITE);

    emitter.MOVD_FROM_XMM(source, REG_64::RAX);
    emitter.CVTSI2SS(REG_64::RAX, dest);
}

void EE_JIT64::floating_point_absolute_value(EmotionEngine& ee, IR::Instruction& instr)
{
    // This operation works by simply masking out the sign-bit from the float.
    // Note that an array of four masks is used here, despite only doing the
    // operation upon one float.
    // This is due to there being no "ANDSS" instruction, which means four
    // floats are masked at the same time.

    REG_64 source = alloc_reg(ee, instr.get_source(), REG_TYPE::FPU, REG_STATE::READ);
    REG_64 dest = alloc_reg(ee, instr.get_dest(), REG_TYPE::FPU, REG_STATE::WRITE);

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
    REG_64 source = alloc_reg(ee, instr.get_source(), REG_TYPE::FPU, REG_STATE::READ);
    REG_64 source2 = alloc_reg(ee, instr.get_source2(), REG_TYPE::FPU, REG_STATE::READ);
    REG_64 dest = alloc_reg(ee, instr.get_dest(), REG_TYPE::FPU, REG_STATE::WRITE);

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

    clamp_freg(ee, dest);
}

void EE_JIT64::floating_point_clear_control(EmotionEngine& ee, IR::Instruction& instr)
{
    emitter.load_addr((uint64_t)&ee.fpu->control.condition, REG_64::RAX);
    emitter.MOV8_IMM_MEM(false, REG_64::RAX);
}

void EE_JIT64::floating_point_compare_equal(EmotionEngine& ee, IR::Instruction& instr)
{
    REG_64 source = alloc_reg(ee, instr.get_source(), REG_TYPE::FPU, REG_STATE::READ);
    REG_64 source2 = alloc_reg(ee, instr.get_source2(), REG_TYPE::FPU, REG_STATE::READ);

    emitter.UCOMISS(source2, source);
    emitter.load_addr((uint64_t)&ee.fpu->control.condition, REG_64::RAX);
    emitter.SETCC_MEM(ConditionCode::E, REG_64::RAX);
}

void EE_JIT64::floating_point_compare_less_than(EmotionEngine& ee, IR::Instruction& instr)
{
    REG_64 source = alloc_reg(ee, instr.get_source(), REG_TYPE::FPU, REG_STATE::READ);
    REG_64 source2 = alloc_reg(ee, instr.get_source2(), REG_TYPE::FPU, REG_STATE::READ);

    emitter.UCOMISS(source2, source);
    emitter.load_addr((uint64_t)&ee.fpu->control.condition, REG_64::RAX);
    emitter.SETCC_MEM(ConditionCode::B, REG_64::RAX);
}

void EE_JIT64::floating_point_compare_less_than_or_equal(EmotionEngine& ee, IR::Instruction& instr)
{
    REG_64 source = alloc_reg(ee, instr.get_source(), REG_TYPE::FPU, REG_STATE::READ);
    REG_64 source2 = alloc_reg(ee, instr.get_source2(), REG_TYPE::FPU, REG_STATE::READ);

    emitter.UCOMISS(source2, source);
    emitter.load_addr((uint64_t)&ee.fpu->control.condition, REG_64::RAX);
    emitter.SETCC_MEM(ConditionCode::BE, REG_64::RAX);
}

void EE_JIT64::floating_point_convert_to_fixed_point(EmotionEngine& ee, IR::Instruction& instr)
{
    REG_64 XMM0 = lalloc_xmm_reg(ee, 0, REG_TYPE::XMMSCRATCHPAD, REG_STATE::SCRATCHPAD);
    REG_64 source = alloc_reg(ee, instr.get_source(), REG_TYPE::FPU, REG_STATE::READ);
    REG_64 dest = alloc_reg(ee, instr.get_dest(), REG_TYPE::FPU, REG_STATE::WRITE);

    // Convert single and test to see if the result is out of range. If out of range,
    // the result will be 0x80000000, which is fine for signed floats. However, the PS2
    // will expect 0x7FFFFFFF if the float was originally unsigned.
    emitter.CVTSS2SI(source, REG_64::RAX);

    emitter.CMP32_EAX(0x80000000);
    uint8_t *end0 = emitter.JCC_NEAR_DEFERRED(ConditionCode::NE);
    emitter.PXOR_XMM(XMM0, XMM0);
    emitter.UCOMISS(XMM0, source);
    uint8_t *end1 = emitter.JCC_NEAR_DEFERRED(ConditionCode::BE);
    emitter.MOV32_REG_IMM(0x7FFFFFFF, REG_64::RAX);

    emitter.set_jump_dest(end0);
    emitter.set_jump_dest(end1);
    emitter.MOVD_TO_XMM(REG_64::RAX, dest);

    free_xmm_reg(ee, XMM0);
}

void EE_JIT64::floating_point_divide(EmotionEngine& ee, IR::Instruction& instr)
{
    REG_64 R15 = alloc_reg(ee, 0, REG_TYPE::INTSCRATCHPAD, REG_STATE::SCRATCHPAD);
    REG_64 XMM0 = alloc_reg(ee, 0, REG_TYPE::XMMSCRATCHPAD, REG_STATE::SCRATCHPAD);
    REG_64 source = alloc_reg(ee, instr.get_source(), REG_TYPE::FPU, REG_STATE::READ);
    REG_64 source2 = alloc_reg(ee, instr.get_source2(), REG_TYPE::FPU, REG_STATE::READ);
    REG_64 dest = alloc_reg(ee, instr.get_dest(), REG_TYPE::FPU, REG_STATE::WRITE);

    emitter.MOVD_FROM_XMM(source2, REG_64::RAX);
    emitter.AND32_EAX(0x7F800000);
    emitter.TEST32_REG(REG_64::RAX, REG_64::RAX);
    uint8_t *normalop = emitter.JCC_NEAR_DEFERRED(ConditionCode::NZ);

    // FPU division by 0
    emitter.MOVD_FROM_XMM(source, REG_64::RAX);
    emitter.MOVD_FROM_XMM(source2, R15);
    emitter.XOR32_REG(R15, REG_64::RAX);
    emitter.MOVD_TO_XMM(REG_64::RAX, dest);
    uint8_t *exit = emitter.JMP_NEAR_DEFERRED();

    emitter.set_jump_dest(normalop);
    emitter.MOVAPS_REG(source, XMM0);
    emitter.DIVSS(source2, XMM0);
    emitter.MOVAPS_REG(XMM0, dest);
    emitter.set_jump_dest(exit);

    clamp_freg(ee, dest);
    free_int_reg(ee, R15);
    free_xmm_reg(ee, XMM0);
}

void EE_JIT64::floating_point_multiply(EmotionEngine& ee, IR::Instruction& instr)
{
    REG_64 source = alloc_reg(ee, instr.get_source(), REG_TYPE::FPU, REG_STATE::READ);
    REG_64 source2 = alloc_reg(ee, instr.get_source2(), REG_TYPE::FPU, REG_STATE::READ);
    REG_64 dest = alloc_reg(ee, instr.get_dest(), REG_TYPE::FPU, REG_STATE::WRITE);

    if (dest == source)
    {
        emitter.MULSS(source2, dest);
    }
    else if (dest == source2)
    {
        emitter.MULSS(source, dest);
    }
    else
    {
        emitter.MOVAPS_REG(source, dest);
        emitter.MULSS(source2, dest);
    }

    clamp_freg(ee, dest);
}

void EE_JIT64::floating_point_multiply_add(EmotionEngine& ee, IR::Instruction& instr)
{
    REG_64 XMM0 = lalloc_xmm_reg(ee, 0, REG_TYPE::XMMSCRATCHPAD, REG_STATE::SCRATCHPAD);
    REG_64 acc = alloc_reg(ee, (int)FPU_SpecialReg::ACC, REG_TYPE::FPU, REG_STATE::READ);
    REG_64 source = alloc_reg(ee, instr.get_source(), REG_TYPE::FPU, REG_STATE::READ);
    REG_64 source2 = alloc_reg(ee, instr.get_source2(), REG_TYPE::FPU, REG_STATE::READ);
    REG_64 dest = alloc_reg(ee, instr.get_dest(), REG_TYPE::FPU, REG_STATE::WRITE);

    emitter.MOVAPS_REG(source, XMM0);
    emitter.MULSS(source2, XMM0);
    emitter.ADDSS(acc, XMM0);
    emitter.MOVAPS_REG(XMM0, dest);

    clamp_freg(ee, dest);
    free_xmm_reg(ee, XMM0);
}

void EE_JIT64::floating_point_multiply_subtract(EmotionEngine& ee, IR::Instruction& instr)
{
    REG_64 XMM0 = lalloc_xmm_reg(ee, 0, REG_TYPE::XMMSCRATCHPAD, REG_STATE::SCRATCHPAD);
    REG_64 acc = alloc_reg(ee, (int)FPU_SpecialReg::ACC, REG_TYPE::FPU, REG_STATE::READ);
    REG_64 source = alloc_reg(ee, instr.get_source(), REG_TYPE::FPU, REG_STATE::READ);
    REG_64 source2 = alloc_reg(ee, instr.get_source2(), REG_TYPE::FPU, REG_STATE::READ);
    REG_64 dest = alloc_reg(ee, instr.get_dest(), REG_TYPE::FPU, REG_STATE::WRITE);

    emitter.MOVAPS_REG(source, XMM0);
    emitter.MULSS(source2, XMM0);
    emitter.MOVAPS_REG(acc, dest);
    emitter.SUBSS(XMM0, dest);

    clamp_freg(ee, dest);
    free_xmm_reg(ee, XMM0);
}

void EE_JIT64::floating_point_negate(EmotionEngine& ee, IR::Instruction& instr)
{
    // This operation works by simply toggling the sign-bit in the float.
    // Note that an array of four masks is used here, despite only doing the
    // operation upon one float.
    // This is due to there being no "XORSS" instruction, which means four
    // floats are masked at the same time.

    REG_64 source = alloc_reg(ee, instr.get_source(), REG_TYPE::FPU, REG_STATE::READ);
    REG_64 dest = alloc_reg(ee, instr.get_dest(), REG_TYPE::FPU, REG_STATE::WRITE);

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
    REG_64 source = alloc_reg(ee, instr.get_source(), REG_TYPE::FPU, REG_STATE::READ);
    REG_64 source2 = alloc_reg(ee, instr.get_source2(), REG_TYPE::FPU, REG_STATE::READ);
    REG_64 dest = alloc_reg(ee, instr.get_dest(), REG_TYPE::FPU, REG_STATE::WRITE);

    emitter.load_addr((uint64_t)&ee.fpu->control.u, REG_64::RAX);
    emitter.MOV8_IMM_MEM(false, REG_64::RAX);
    emitter.load_addr((uint64_t)&ee.fpu->control.o, REG_64::RAX);
    emitter.MOV8_IMM_MEM(false, REG_64::RAX);

    if (dest != source)
    {
        emitter.MOVAPS_REG(source, dest);
        emitter.MAXSS(source2, dest);
    }
    else if (dest != source2)
    {
        emitter.MOVAPS_REG(source2, dest);
        emitter.MAXSS(source, dest);
    }

    // if dest == source && dest == source2, then no operation is performed
}

void EE_JIT64::floating_point_minimum(EmotionEngine& ee, IR::Instruction& instr)
{
    REG_64 source = alloc_reg(ee, instr.get_source(), REG_TYPE::FPU, REG_STATE::READ);
    REG_64 source2 = alloc_reg(ee, instr.get_source2(), REG_TYPE::FPU, REG_STATE::READ);
    REG_64 dest = alloc_reg(ee, instr.get_dest(), REG_TYPE::FPU, REG_STATE::WRITE);

    emitter.load_addr((uint64_t)&ee.fpu->control.u, REG_64::RAX);
    emitter.MOV8_IMM_MEM(false, REG_64::RAX);
    emitter.load_addr((uint64_t)&ee.fpu->control.o, REG_64::RAX);
    emitter.MOV8_IMM_MEM(false, REG_64::RAX);

    if (dest != source)
    {
        emitter.MOVAPS_REG(source, dest);
        emitter.MINSS(source2, dest);
    }
    else if (dest != source2)
    {
        emitter.MOVAPS_REG(source2, dest);
        emitter.MINSS(source, dest);
    }

    // if dest == source && dest == source2, then no operation is performed
}

void EE_JIT64::floating_point_square_root(EmotionEngine& ee, IR::Instruction& instr)
{
    REG_64 source = alloc_reg(ee, instr.get_source(), REG_TYPE::FPU, REG_STATE::READ);
    REG_64 dest = alloc_reg(ee, instr.get_dest(), REG_TYPE::FPU, REG_STATE::WRITE);

    // abs(source)
    if (source != dest)
        emitter.MOVSS_REG(source, dest);
    emitter.load_addr((uint64_t)&FPU_MASK_ABS[0], REG_64::RAX);
    emitter.PAND_XMM_FROM_MEM(REG_64::RAX, dest);
    
    emitter.SQRTSS(source, dest);

    clamp_freg(ee, dest);
}

void EE_JIT64::floating_point_subtract(EmotionEngine& ee, IR::Instruction& instr)
{
    REG_64 source = alloc_reg(ee, instr.get_source(), REG_TYPE::FPU, REG_STATE::READ);
    REG_64 source2 = alloc_reg(ee, instr.get_source2(), REG_TYPE::FPU, REG_STATE::READ);
    REG_64 dest = alloc_reg(ee, instr.get_dest(), REG_TYPE::FPU, REG_STATE::WRITE);

    if (dest == source)
    {
        emitter.SUBSS(source2, dest);
    }
    else if (dest == source2)
    {
        emitter.SUBSS(source, dest);
        emitter.load_addr((uint64_t)&FPU_MASK_NEG[0], REG_64::RAX);
        emitter.PXOR_XMM_FROM_MEM(REG_64::RAX, dest);
    }
    else
    {
        emitter.MOVAPS_REG(source, dest);
        emitter.SUBSS(source2, dest);
    }

    clamp_freg(ee, dest);
}

void EE_JIT64::floating_point_reciprocal_square_root(EmotionEngine& ee, IR::Instruction& instr)
{

    REG_64 R15 = alloc_reg(ee, 0, REG_TYPE::INTSCRATCHPAD, REG_STATE::SCRATCHPAD);
    REG_64 XMM0 = alloc_reg(ee, 0, REG_TYPE::XMMSCRATCHPAD, REG_STATE::SCRATCHPAD);
    REG_64 XMM1 = alloc_reg(ee, 0, REG_TYPE::XMMSCRATCHPAD, REG_STATE::SCRATCHPAD);
    REG_64 source = alloc_reg(ee, instr.get_source(), REG_TYPE::FPU, REG_STATE::READ);
    REG_64 source2 = alloc_reg(ee, instr.get_source2(), REG_TYPE::FPU, REG_STATE::READ);
    REG_64 dest = alloc_reg(ee, instr.get_dest(), REG_TYPE::FPU, REG_STATE::WRITE);

    emitter.MOVD_FROM_XMM(source2, REG_64::RAX);
    emitter.AND32_EAX(0x7F800000);
    emitter.TEST32_REG(REG_64::RAX, REG_64::RAX);
    uint8_t *normalop = emitter.JCC_NEAR_DEFERRED(ConditionCode::NZ);

    // Second operand is 0
    emitter.MOVD_FROM_XMM(source, REG_64::RAX);
    emitter.MOVD_FROM_XMM(source2, R15);
    emitter.XOR32_REG(R15, REG_64::RAX);
    emitter.MOVD_TO_XMM(REG_64::RAX, dest);
    uint8_t *exit = emitter.JMP_NEAR_DEFERRED();

    emitter.set_jump_dest(normalop);
    emitter.MOVAPS_REG(source, XMM0);
    // abs(source2)
    emitter.MOVAPS_REG(source2, XMM1);
    emitter.load_addr((uint64_t)&FPU_MASK_ABS[0], REG_64::RAX);
    emitter.PAND_XMM_FROM_MEM(REG_64::RAX, XMM1);
    emitter.RSQRTSS(XMM1, XMM0);
    emitter.MOVAPS_REG(XMM0, dest);
    emitter.set_jump_dest(exit);

    clamp_freg(ee, dest);
    free_int_reg(ee, R15);
    free_xmm_reg(ee, XMM0);
    free_xmm_reg(ee, XMM1);
}

void EE_JIT64::move_from_coprocessor1(EmotionEngine& ee, IR::Instruction& instr)
{
    REG_64 source = alloc_reg(ee, instr.get_source(), REG_TYPE::FPU, REG_STATE::READ);
    REG_64 dest = alloc_reg(ee, instr.get_dest(), REG_TYPE::GPR, REG_STATE::WRITE);

    emitter.MOVD_FROM_XMM(source, dest);
    emitter.MOVSX32_TO_64(dest, dest);
}

void EE_JIT64::move_to_coprocessor1(EmotionEngine& ee, IR::Instruction& instr)
{
    REG_64 source = alloc_reg(ee, instr.get_source(), REG_TYPE::GPR, REG_STATE::READ);
    REG_64 dest = alloc_reg(ee, instr.get_dest(), REG_TYPE::FPU, REG_STATE::WRITE);

    emitter.MOVD_TO_XMM(source, dest);
}

void EE_JIT64::move_xmm_reg(EmotionEngine& ee, IR::Instruction& instr)
{
    REG_64 source = alloc_reg(ee, instr.get_source(), REG_TYPE::FPU, REG_STATE::READ);
    REG_64 dest = alloc_reg(ee, instr.get_dest(), REG_TYPE::FPU, REG_STATE::WRITE);

    emitter.MOVAPS_REG(source, dest);
}