#include <cmath>
#include <algorithm>
#include "ee_jit64.hpp"

void EE_JIT64::clamp_freg(REG_64 freg)
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

    clamp_freg(source);
    clamp_freg(source2);

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

    clamp_freg(dest);
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

    clamp_freg(source);
    clamp_freg(source2);

    emitter.UCOMISS(source2, source);
    emitter.load_addr((uint64_t)&ee.fpu->control.condition, REG_64::RAX);
    emitter.SETCC_MEM(ConditionCode::E, REG_64::RAX);
}

void EE_JIT64::floating_point_compare_less_than(EmotionEngine& ee, IR::Instruction& instr)
{
    REG_64 source = alloc_reg(ee, instr.get_source(), REG_TYPE::FPU, REG_STATE::READ);
    REG_64 source2 = alloc_reg(ee, instr.get_source2(), REG_TYPE::FPU, REG_STATE::READ);

    clamp_freg(source);
    clamp_freg(source2);

    emitter.UCOMISS(source2, source);
    emitter.load_addr((uint64_t)&ee.fpu->control.condition, REG_64::RAX);
    emitter.SETCC_MEM(ConditionCode::B, REG_64::RAX);
}

void EE_JIT64::floating_point_compare_less_than_or_equal(EmotionEngine& ee, IR::Instruction& instr)
{
    REG_64 source = alloc_reg(ee, instr.get_source(), REG_TYPE::FPU, REG_STATE::READ);
    REG_64 source2 = alloc_reg(ee, instr.get_source2(), REG_TYPE::FPU, REG_STATE::READ);

    clamp_freg(source);
    clamp_freg(source2);

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
    clamp_freg(source);

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
    emitter.AND32_REG_IMM(0x80000000, REG_64::RAX);
    emitter.OR32_REG_IMM(0x7F7FFFFF, REG_64::RAX);
    emitter.MOVD_TO_XMM(REG_64::RAX, dest);
    uint8_t *exit = emitter.JMP_NEAR_DEFERRED();

    emitter.set_jump_dest(normalop);
    emitter.MOVAPS_REG(source, XMM0);
    emitter.DIVSS(source2, XMM0);
    emitter.MOVAPS_REG(XMM0, dest);
    emitter.set_jump_dest(exit);

    clamp_freg(dest);
    free_int_reg(ee, R15);
    free_xmm_reg(ee, XMM0);
}

void EE_JIT64::floating_point_multiply(EmotionEngine& ee, IR::Instruction& instr)
{
    REG_64 source = alloc_reg(ee, instr.get_source(), REG_TYPE::FPU, REG_STATE::READ);
    REG_64 source2 = alloc_reg(ee, instr.get_source2(), REG_TYPE::FPU, REG_STATE::READ);
    REG_64 dest = alloc_reg(ee, instr.get_dest(), REG_TYPE::FPU, REG_STATE::WRITE);

    clamp_freg(source);
    clamp_freg(source2);

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

    clamp_freg(dest);
}

void EE_JIT64::floating_point_multiply_add(EmotionEngine& ee, IR::Instruction& instr)
{
    REG_64 XMM0 = lalloc_xmm_reg(ee, 0, REG_TYPE::XMMSCRATCHPAD, REG_STATE::SCRATCHPAD);
    REG_64 acc = alloc_reg(ee, (int)FPU_SpecialReg::ACC, REG_TYPE::FPU, REG_STATE::READ);
    REG_64 source = alloc_reg(ee, instr.get_source(), REG_TYPE::FPU, REG_STATE::READ);
    REG_64 source2 = alloc_reg(ee, instr.get_source2(), REG_TYPE::FPU, REG_STATE::READ);
    REG_64 dest = alloc_reg(ee, instr.get_dest(), REG_TYPE::FPU, REG_STATE::WRITE);

    clamp_freg(source);
    clamp_freg(source2);
    clamp_freg(acc);

    emitter.MOVAPS_REG(source, XMM0);
    emitter.MULSS(source2, XMM0);
    emitter.ADDSS(acc, XMM0);
    emitter.MOVAPS_REG(XMM0, dest);

    clamp_freg(dest);
    free_xmm_reg(ee, XMM0);
}

void EE_JIT64::floating_point_multiply_subtract(EmotionEngine& ee, IR::Instruction& instr)
{
    REG_64 XMM0 = lalloc_xmm_reg(ee, 0, REG_TYPE::XMMSCRATCHPAD, REG_STATE::SCRATCHPAD);
    REG_64 acc = alloc_reg(ee, (int)FPU_SpecialReg::ACC, REG_TYPE::FPU, REG_STATE::READ);
    REG_64 source = alloc_reg(ee, instr.get_source(), REG_TYPE::FPU, REG_STATE::READ);
    REG_64 source2 = alloc_reg(ee, instr.get_source2(), REG_TYPE::FPU, REG_STATE::READ);
    REG_64 dest = alloc_reg(ee, instr.get_dest(), REG_TYPE::FPU, REG_STATE::WRITE);

    clamp_freg(source);
    clamp_freg(source2);
    clamp_freg(acc);

    emitter.MOVAPS_REG(source, XMM0);
    emitter.MULSS(source2, XMM0);
    emitter.MOVAPS_REG(acc, dest);
    emitter.SUBSS(XMM0, dest);

    clamp_freg(dest);
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
    REG_64 temp = lalloc_xmm_reg(ee, 0, REG_TYPE::XMMSCRATCHPAD, REG_STATE::SCRATCHPAD, REG_64::XMM0);
    REG_64 source = alloc_reg(ee, instr.get_source(), REG_TYPE::FPU, REG_STATE::READ);
    REG_64 source2 = alloc_reg(ee, instr.get_source2(), REG_TYPE::FPU, REG_STATE::READ);
    REG_64 dest = alloc_reg(ee, instr.get_dest(), REG_TYPE::FPU, REG_STATE::WRITE);
    REG_64 temp2 = lalloc_xmm_reg(ee, 0, REG_TYPE::XMMSCRATCHPAD, REG_STATE::SCRATCHPAD);
    REG_64 temp3 = lalloc_xmm_reg(ee, 0, REG_TYPE::XMMSCRATCHPAD, REG_STATE::SCRATCHPAD);

    emitter.load_addr((uint64_t)&ee.fpu->control.u, REG_64::RAX);
    emitter.MOV8_IMM_MEM(false, REG_64::RAX);
    emitter.load_addr((uint64_t)&ee.fpu->control.o, REG_64::RAX);
    emitter.MOV8_IMM_MEM(false, REG_64::RAX);

    emitter.MOVAPS_REG(source, temp2);
    emitter.MOVAPS_REG(source2, temp3);

    emitter.MOVAPS_REG(source2, temp);
    emitter.PMAXSD_XMM(source, temp);
    emitter.MOVAPS_REG(temp, dest);

    emitter.MOVAPS_REG(temp2, temp);
    emitter.PAND_XMM(temp3, temp); //mask
    emitter.PMINSD_XMM(temp3, temp2);
    emitter.BLENDVPS_XMM0(temp2, dest);

    free_xmm_reg(ee, temp);
    free_xmm_reg(ee, temp2);
    free_xmm_reg(ee, temp3);
}

void EE_JIT64::floating_point_minimum(EmotionEngine& ee, IR::Instruction& instr)
{
    REG_64 temp = lalloc_xmm_reg(ee, 0, REG_TYPE::XMMSCRATCHPAD, REG_STATE::SCRATCHPAD, REG_64::XMM0);
    REG_64 source = alloc_reg(ee, instr.get_source(), REG_TYPE::FPU, REG_STATE::READ);
    REG_64 source2 = alloc_reg(ee, instr.get_source2(), REG_TYPE::FPU, REG_STATE::READ);
    REG_64 dest = alloc_reg(ee, instr.get_dest(), REG_TYPE::FPU, REG_STATE::WRITE);
    REG_64 temp2 = lalloc_xmm_reg(ee, 0, REG_TYPE::XMMSCRATCHPAD, REG_STATE::SCRATCHPAD);
    REG_64 temp3 = lalloc_xmm_reg(ee, 0, REG_TYPE::XMMSCRATCHPAD, REG_STATE::SCRATCHPAD);

    emitter.load_addr((uint64_t)&ee.fpu->control.u, REG_64::RAX);
    emitter.MOV8_IMM_MEM(false, REG_64::RAX);
    emitter.load_addr((uint64_t)&ee.fpu->control.o, REG_64::RAX);
    emitter.MOV8_IMM_MEM(false, REG_64::RAX);

    emitter.MOVAPS_REG(source, temp2);
    emitter.MOVAPS_REG(source2, temp3);

    emitter.MOVAPS_REG(source2, temp);
    emitter.PMINSD_XMM(source, temp);
    emitter.MOVAPS_REG(temp, dest);

    emitter.MOVAPS_REG(temp2, temp);
    emitter.PAND_XMM(temp3, temp); //mask
    emitter.PMAXSD_XMM(temp3, temp2);
    emitter.BLENDVPS_XMM0(temp2, dest);

    free_xmm_reg(ee, temp);
    free_xmm_reg(ee, temp2);
    free_xmm_reg(ee, temp3);
}

void EE_JIT64::floating_point_square_root(EmotionEngine& ee, IR::Instruction& instr)
{
    REG_64 source = alloc_reg(ee, instr.get_source(), REG_TYPE::FPU, REG_STATE::READ);
    REG_64 dest = alloc_reg(ee, instr.get_dest(), REG_TYPE::FPU, REG_STATE::WRITE);
    REG_64 XMM0 = lalloc_xmm_reg(ee, -1, REG_TYPE::XMMSCRATCHPAD, REG_STATE::SCRATCHPAD, (REG_64)-1);

    clamp_freg(source);

    // dest = abs(source)
    emitter.MOVD_FROM_XMM(source, REG_64::RAX);
    emitter.AND32_EAX(0x7FFFFFFF);
    emitter.MOVD_TO_XMM(REG_64::RAX, dest);

    emitter.SQRTSS(dest, dest);

    clamp_freg(dest);

    free_xmm_reg(ee, XMM0);
}

void EE_JIT64::floating_point_subtract(EmotionEngine& ee, IR::Instruction& instr)
{
    REG_64 source = alloc_reg(ee, instr.get_source(), REG_TYPE::FPU, REG_STATE::READ);
    REG_64 source2 = alloc_reg(ee, instr.get_source2(), REG_TYPE::FPU, REG_STATE::READ);
    REG_64 dest = alloc_reg(ee, instr.get_dest(), REG_TYPE::FPU, REG_STATE::WRITE);

    clamp_freg(source);
    clamp_freg(source2);

    if (dest == source)
    {
        emitter.SUBSS(source2, dest);
    }
    else if (dest == source2)
    {
        // Would technically be nicer to do a backwards SUB and Negate, but this causes bugs in some situations
        REG_64 XMM0 = lalloc_xmm_reg(ee, 0, REG_TYPE::XMMSCRATCHPAD, REG_STATE::SCRATCHPAD);
        emitter.MOVAPS_REG(source, XMM0);
        emitter.SUBSS(dest, XMM0);
        emitter.MOVAPS_REG(XMM0, dest);
        free_xmm_reg(ee, XMM0);
    }
    else
    {
        emitter.MOVAPS_REG(source, dest);
        emitter.SUBSS(source2, dest);
    }

    clamp_freg(dest);
}

void EE_JIT64::floating_point_reciprocal_square_root(EmotionEngine& ee, IR::Instruction& instr)
{
    REG_64 XMM0 = lalloc_xmm_reg(ee, 0, REG_TYPE::XMMSCRATCHPAD, REG_STATE::SCRATCHPAD);
    REG_64 source = alloc_reg(ee, instr.get_source(), REG_TYPE::FPU, REG_STATE::READ);
    REG_64 source2 = alloc_reg(ee, instr.get_source2(), REG_TYPE::FPU, REG_STATE::READ);
    REG_64 dest = alloc_reg(ee, instr.get_dest(), REG_TYPE::FPU, REG_STATE::WRITE);

    emitter.MOVD_FROM_XMM(source2, REG_64::RAX);
    emitter.AND32_EAX(0x7F800000);
    emitter.TEST32_REG(REG_64::RAX, REG_64::RAX);
    uint8_t *normalop = emitter.JCC_NEAR_DEFERRED(ConditionCode::NZ);

    // Second operand's exponent is 0
    // dest = (source & 0x80000000) | 0x7F7FFFFF
    emitter.MOVD_FROM_XMM(source, REG_64::RAX);
    emitter.AND32_EAX(0x80000000);
    emitter.OR32_REG_IMM(0x7F7FFFFF, REG_64::RAX);
    emitter.MOVD_TO_XMM(REG_64::RAX, dest);
    uint8_t *exit = emitter.JMP_NEAR_DEFERRED();

    // Second operand's exponent is nonzero
    emitter.set_jump_dest(normalop);
    
    // dest = clamp(clamp(source) / sqrt(clamp(fabsf(source2))))
    emitter.MOVD_FROM_XMM(source2, REG_64::RAX);
    if (source != dest)
        emitter.MOVSS_REG(source, dest);
    emitter.AND32_EAX(0x7FFFFFFF);
    emitter.MOVD_TO_XMM(REG_64::RAX, XMM0);
    clamp_freg(dest);
    clamp_freg(XMM0);
    emitter.SQRTSS(XMM0, XMM0);
    emitter.DIVSS(XMM0, dest);
    clamp_freg(dest);

    emitter.set_jump_dest(exit);
    free_xmm_reg(ee, XMM0);
}

void EE_JIT64::move_control_word_from_floating_point(EmotionEngine& ee, IR::Instruction& instr)
{
    REG_64 dest = alloc_reg(ee, instr.get_dest(), REG_TYPE::GPR, REG_STATE::WRITE);

    switch (instr.get_source())
    {
        case 0x0:
            emitter.MOV64_OI(0x2E00, dest);
            break;
        case 0x1F:
        {
            REG_64 R15 = lalloc_int_reg(ee, 0, REG_TYPE::INTSCRATCHPAD, REG_STATE::SCRATCHPAD);
            REG_64 R14 = lalloc_int_reg(ee, 0, REG_TYPE::INTSCRATCHPAD, REG_STATE::SCRATCHPAD);

            emitter.load_addr((uint64_t)&ee.fpu->control, R15);

            emitter.MOV8_FROM_MEM(R15, R14, offsetof(COP1_CONTROL, su));
            emitter.MOVZX8_TO_32(R14, R14);
            emitter.SHL32_REG_IMM(3, R14);
            emitter.OR32_REG(R14, dest);

            emitter.MOV8_FROM_MEM(R15, R14, offsetof(COP1_CONTROL, so));
            emitter.MOVZX8_TO_32(R14, R14);
            emitter.SHL32_REG_IMM(4, R14);
            emitter.OR32_REG(R14, dest);

            emitter.MOV8_FROM_MEM(R15, R14, offsetof(COP1_CONTROL, sd));
            emitter.MOVZX8_TO_32(R14, R14);
            emitter.SHL32_REG_IMM(5, R14);
            emitter.OR32_REG(R14, dest);

            emitter.MOV8_FROM_MEM(R15, R14, offsetof(COP1_CONTROL, si));
            emitter.MOVZX8_TO_32(R14, R14);
            emitter.SHL32_REG_IMM(6, R14);
            emitter.OR32_REG(R14, dest);

            emitter.MOV8_FROM_MEM(R15, R14, offsetof(COP1_CONTROL, u));
            emitter.MOVZX8_TO_32(R14, R14);
            emitter.SHL32_REG_IMM(14, R14);
            emitter.OR32_REG(R14, dest);

            emitter.MOV8_FROM_MEM(R15, R14, offsetof(COP1_CONTROL, o));
            emitter.MOVZX8_TO_32(R14, R14);
            emitter.SHL32_REG_IMM(15, R14);
            emitter.OR32_REG(R14, dest);

            emitter.MOV8_FROM_MEM(R15, R14, offsetof(COP1_CONTROL, d));
            emitter.MOVZX8_TO_32(R14, R14);
            emitter.SHL32_REG_IMM(16, R14);
            emitter.OR32_REG(R14, dest);

            emitter.MOV8_FROM_MEM(R15, R14, offsetof(COP1_CONTROL, i));
            emitter.MOVZX8_TO_32(R14, R14);
            emitter.SHL32_REG_IMM(17, R14);
            emitter.OR32_REG(R14, dest);

            emitter.MOV8_FROM_MEM(R15, R14, offsetof(COP1_CONTROL, condition));
            emitter.MOVZX8_TO_32(R14, R14);
            emitter.SHL32_REG_IMM(23, R14);
            emitter.OR32_REG(R14, dest);

            free_int_reg(ee, R15);
            free_int_reg(ee, R14);
            break;
        }
        default:
            emitter.XOR32_REG(dest, dest);
    }
}

void EE_JIT64::move_control_word_to_floating_point(EmotionEngine& ee, IR::Instruction& instr)
{
    REG_64 R15 = lalloc_int_reg(ee, 0, REG_TYPE::INTSCRATCHPAD, REG_STATE::SCRATCHPAD);
    REG_64 source = alloc_reg(ee, instr.get_source(), REG_TYPE::GPR, REG_STATE::READ);

    emitter.load_addr((uint64_t)&ee.fpu->control, R15);
    emitter.MOV32_REG(source, REG_64::EAX);

    emitter.TEST32_REG_IMM((1 << 3), REG_64::EAX);
    emitter.SETCC_MEM(ConditionCode::NZ, R15, offsetof(COP1_CONTROL, su));

    emitter.TEST32_REG_IMM((1 << 4), REG_64::EAX);
    emitter.SETCC_MEM(ConditionCode::NZ, R15, offsetof(COP1_CONTROL, so));

    emitter.TEST32_REG_IMM((1 << 5), REG_64::EAX);
    emitter.SETCC_MEM(ConditionCode::NZ, R15, offsetof(COP1_CONTROL, sd));

    emitter.TEST32_REG_IMM((1 << 6), REG_64::EAX);
    emitter.SETCC_MEM(ConditionCode::NZ, R15, offsetof(COP1_CONTROL, si));

    emitter.TEST32_REG_IMM((1 << 14), REG_64::EAX);
    emitter.SETCC_MEM(ConditionCode::NZ, R15, offsetof(COP1_CONTROL, u));

    emitter.TEST32_REG_IMM((1 << 15), REG_64::EAX);
    emitter.SETCC_MEM(ConditionCode::NZ, R15, offsetof(COP1_CONTROL, o));

    emitter.TEST32_REG_IMM((1 << 16), REG_64::EAX);
    emitter.SETCC_MEM(ConditionCode::NZ, R15, offsetof(COP1_CONTROL, d));

    emitter.TEST32_REG_IMM((1 << 17), REG_64::EAX);
    emitter.SETCC_MEM(ConditionCode::NZ, R15, offsetof(COP1_CONTROL, i));

    emitter.TEST32_REG_IMM((1 << 23), REG_64::EAX);
    emitter.SETCC_MEM(ConditionCode::NZ, R15, offsetof(COP1_CONTROL, condition));

    free_int_reg(ee, R15);
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

void EE_JIT64::load_word_coprocessor1(EmotionEngine& ee, IR::Instruction &instr)
{
    REG_64 source = alloc_reg(ee, instr.get_source(), REG_TYPE::GPR, REG_STATE::READ);
    REG_64 addr = lalloc_int_reg(ee, 0, REG_TYPE::INTSCRATCHPAD, REG_STATE::SCRATCHPAD);
    REG_64 dest = alloc_reg(ee, instr.get_dest(), REG_TYPE::FPU, REG_STATE::WRITE);

    int64_t offset = instr.get_source2();

    if (offset)
        emitter.LEA32_M(source, addr, offset);
    else
        emitter.MOV32_REG(source, addr);
    prepare_abi((uint64_t)&ee);
    prepare_abi_reg(addr);
    call_abi_func((uint64_t)ee_read32);
    free_int_reg(ee, addr);
    restore_xmm_regs(std::vector<REG_64> {dest}, false);

    emitter.MOVD_TO_XMM(REG_64::RAX, dest);
}

void EE_JIT64::store_word_coprocessor1(EmotionEngine& ee, IR::Instruction& instr)
{
    REG_64 source = alloc_reg(ee, instr.get_source(), REG_TYPE::FPU, REG_STATE::READ);
    REG_64 dest = alloc_reg(ee, instr.get_dest(), REG_TYPE::GPR, REG_STATE::READ);
    REG_64 addr = lalloc_int_reg(ee, 0, REG_TYPE::INTSCRATCHPAD, REG_STATE::SCRATCHPAD);
    int64_t offset = instr.get_source2();

    if (offset)
        emitter.LEA32_M(dest, addr, offset);
    else
        emitter.MOV32_REG(dest, addr);
    prepare_abi((uint64_t)&ee);
    prepare_abi_reg(addr);
    prepare_abi_reg_from_xmm(source);
    free_int_reg(ee, addr);
    call_abi_func((uint64_t)ee_write32);
}