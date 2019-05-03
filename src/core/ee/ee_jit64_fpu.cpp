#include <cmath>
#include <algorithm>
#include "ee_jit64.hpp"

void EE_JIT64::clamp_freg(EmotionEngine& ee, REG_64 freg)
{
    REG_64 XMM0 = lalloc_xmm_reg(ee, 0, REG_TYPE::XMMSCRATCHPAD, REG_STATE::SCRATCHPAD);
    REG_64 R15 = lalloc_int_reg(ee, 0, REG_TYPE::INTSCRATCHPAD, REG_STATE::SCRATCHPAD);

    emitter.load_addr((uint64_t)&max_flt_constant, REG_64::RAX);
    emitter.load_addr((uint64_t)&min_flt_constant, R15);

    emitter.MOVAPS_REG(freg, XMM0);
    //reg = min_signed(reg, 0x7F7FFFFF)
    emitter.PMINSD_XMM_FROM_MEM(REG_64::RAX, XMM0);

    //reg = min_unsigned(reg, 0xFF7FFFFF)
    emitter.PMINUD_XMM_FROM_MEM(R15, XMM0);

    free_xmm_reg(ee, XMM0);
    free_int_reg(ee, R15);
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
    emitter.SETCC_MEM(ConditionCode::L, REG_64::RAX);
}

void EE_JIT64::floating_point_compare_less_than_or_equal(EmotionEngine& ee, IR::Instruction& instr)
{
    REG_64 source = alloc_reg(ee, instr.get_source(), REG_TYPE::FPU, REG_STATE::READ);
    REG_64 source2 = alloc_reg(ee, instr.get_source2(), REG_TYPE::FPU, REG_STATE::READ);

    emitter.UCOMISS(source2, source);
    emitter.load_addr((uint64_t)&ee.fpu->control.condition, REG_64::RAX);
    emitter.SETCC_MEM(ConditionCode::LE, REG_64::RAX);
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
    floating_point_absolute_value(ee, instr);
    
    emitter.SQRTSS(source, dest);
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