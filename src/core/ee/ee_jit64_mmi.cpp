#include "ee_jit64.hpp"

void EE_JIT64::move_quadword_reg(EmotionEngine& ee, IR::Instruction& instr)
{
    REG_64 source = alloc_reg(ee, instr.get_source(), REG_TYPE::GPREXTENDED, REG_STATE::READ);
    REG_64 dest = alloc_reg(ee, instr.get_dest(), REG_TYPE::GPREXTENDED, REG_STATE::WRITE);

    emitter.MOVAPS_REG(source, dest);
}

void EE_JIT64::parallel_absolute_halfword(EmotionEngine& ee, IR::Instruction& instr)
{
    REG_64 source = alloc_reg(ee, instr.get_source(), REG_TYPE::GPREXTENDED, REG_STATE::READ);
    REG_64 dest = alloc_reg(ee, instr.get_dest(), REG_TYPE::GPREXTENDED, REG_STATE::WRITE);

    emitter.PABSW(source, dest);
}

void EE_JIT64::parallel_absolute_word(EmotionEngine& ee, IR::Instruction& instr)
{
    REG_64 source = alloc_reg(ee, instr.get_source(), REG_TYPE::GPREXTENDED, REG_STATE::READ);
    REG_64 dest = alloc_reg(ee, instr.get_dest(), REG_TYPE::GPREXTENDED, REG_STATE::WRITE);

    emitter.PABSD(source, dest);
}

void EE_JIT64::parallel_and(EmotionEngine& ee, IR::Instruction& instr)
{
    REG_64 source = alloc_reg(ee, instr.get_source(), REG_TYPE::GPREXTENDED, REG_STATE::READ);
    REG_64 source2 = alloc_reg(ee, instr.get_source2(), REG_TYPE::GPREXTENDED, REG_STATE::READ);
    REG_64 dest = alloc_reg(ee, instr.get_dest(), REG_TYPE::GPREXTENDED, REG_STATE::WRITE);

    if (dest == source)
    {
        emitter.PAND_XMM(source2, dest);
    }
    else if (dest == source2)
    {
        emitter.PAND_XMM(source, dest);
    }
    else
    {
        emitter.MOVAPS_REG(source, dest);
        emitter.PAND_XMM(source2, dest);
    }
}

void EE_JIT64::parallel_nor(EmotionEngine& ee, IR::Instruction& instr)
{
    REG_64 source = alloc_reg(ee, instr.get_source(), REG_TYPE::GPREXTENDED, REG_STATE::READ);
    REG_64 source2 = alloc_reg(ee, instr.get_source2(), REG_TYPE::GPREXTENDED, REG_STATE::READ);
    REG_64 dest = alloc_reg(ee, instr.get_dest(), REG_TYPE::GPREXTENDED, REG_STATE::WRITE);
    REG_64 XMM0 = lalloc_xmm_reg(ee, 0, REG_TYPE::XMMSCRATCHPAD, REG_STATE::SCRATCHPAD);

    if (dest == source)
    {
        emitter.POR_XMM(source2, dest);
    }
    else if (dest == source2)
    {
        emitter.POR_XMM(source, dest);
    }
    else
    {
        emitter.MOVAPS_REG(source, dest);
        emitter.POR_XMM(source2, dest);
    }

    // Bitwise NOT of destination register
    emitter.PCMPEQD_XMM(XMM0, XMM0);
    emitter.PXOR_XMM(XMM0, dest);

    free_xmm_reg(ee, XMM0);
}

void EE_JIT64::parallel_or(EmotionEngine& ee, IR::Instruction& instr)
{
    REG_64 source = alloc_reg(ee, instr.get_source(), REG_TYPE::GPREXTENDED, REG_STATE::READ);
    REG_64 source2 = alloc_reg(ee, instr.get_source2(), REG_TYPE::GPREXTENDED, REG_STATE::READ);
    REG_64 dest = alloc_reg(ee, instr.get_dest(), REG_TYPE::GPREXTENDED, REG_STATE::WRITE);

    if (dest == source)
    {
        emitter.POR_XMM(source2, dest);
    }
    else if (dest == source2)
    {
        emitter.POR_XMM(source, dest);
    }
    else
    {
        emitter.MOVAPS_REG(source, dest);
        emitter.POR_XMM(source2, dest);
    }
}

void EE_JIT64::parallel_add_byte(EmotionEngine& ee, IR::Instruction& instr)
{
    REG_64 source = alloc_reg(ee, instr.get_source(), REG_TYPE::GPREXTENDED, REG_STATE::READ);
    REG_64 source2 = alloc_reg(ee, instr.get_source2(), REG_TYPE::GPREXTENDED, REG_STATE::READ);
    REG_64 dest = alloc_reg(ee, instr.get_dest(), REG_TYPE::GPREXTENDED, REG_STATE::WRITE);

    if (dest == source)
    {
        emitter.PADDB(source2, dest);
    }
    else if (dest == source2)
    {
        emitter.PADDB(source, dest);
    }
    else
    {
        emitter.MOVAPS_REG(source, dest);
        emitter.PADDB(source2, dest);
    }
}

void EE_JIT64::parallel_add_halfword(EmotionEngine& ee, IR::Instruction& instr)
{
    REG_64 source = alloc_reg(ee, instr.get_source(), REG_TYPE::GPREXTENDED, REG_STATE::READ);
    REG_64 source2 = alloc_reg(ee, instr.get_source2(), REG_TYPE::GPREXTENDED, REG_STATE::READ);
    REG_64 dest = alloc_reg(ee, instr.get_dest(), REG_TYPE::GPREXTENDED, REG_STATE::WRITE);

    if (dest == source)
    {
        emitter.PADDW(source2, dest);
    }
    else if (dest == source2)
    {
        emitter.PADDW(source, dest);
    }
    else
    {
        emitter.MOVAPS_REG(source, dest);
        emitter.PADDW(source2, dest);
    }
}

void EE_JIT64::parallel_add_word(EmotionEngine& ee, IR::Instruction& instr)
{
    REG_64 source = alloc_reg(ee, instr.get_source(), REG_TYPE::GPREXTENDED, REG_STATE::READ);
    REG_64 source2 = alloc_reg(ee, instr.get_source2(), REG_TYPE::GPREXTENDED, REG_STATE::READ);
    REG_64 dest = alloc_reg(ee, instr.get_dest(), REG_TYPE::GPREXTENDED, REG_STATE::WRITE);

    if (dest == source)
    {
        emitter.PADDD(source2, dest);
    }
    else if (dest == source2)
    {
        emitter.PADDD(source, dest);
    }
    else
    {
        emitter.MOVAPS_REG(source, dest);
        emitter.PADDD(source2, dest);
    }
}

void EE_JIT64::parallel_add_with_signed_saturation_byte(EmotionEngine& ee, IR::Instruction& instr)
{
    REG_64 source = alloc_reg(ee, instr.get_source(), REG_TYPE::GPREXTENDED, REG_STATE::READ);
    REG_64 source2 = alloc_reg(ee, instr.get_source2(), REG_TYPE::GPREXTENDED, REG_STATE::READ);
    REG_64 dest = alloc_reg(ee, instr.get_dest(), REG_TYPE::GPREXTENDED, REG_STATE::WRITE);

    if (dest == source)
    {
        emitter.PADDSB(source2, dest);
    }
    else if (dest == source2)
    {
        emitter.PADDSB(source, dest);
    }
    else
    {
        emitter.MOVAPS_REG(source, dest);
        emitter.PADDSB(source2, dest);
    }
}

void EE_JIT64::parallel_add_with_signed_saturation_halfword(EmotionEngine& ee, IR::Instruction& instr)
{
    REG_64 source = alloc_reg(ee, instr.get_source(), REG_TYPE::GPREXTENDED, REG_STATE::READ);
    REG_64 source2 = alloc_reg(ee, instr.get_source2(), REG_TYPE::GPREXTENDED, REG_STATE::READ);
    REG_64 dest = alloc_reg(ee, instr.get_dest(), REG_TYPE::GPREXTENDED, REG_STATE::WRITE);

    if (dest == source)
    {
        emitter.PADDSW(source2, dest);
    }
    else if (dest == source2)
    {
        emitter.PADDSW(source, dest);
    }
    else
    {
        emitter.MOVAPS_REG(source, dest);
        emitter.PADDSW(source2, dest);
    }
}

void EE_JIT64::parallel_add_with_signed_saturation_word(EmotionEngine& ee, IR::Instruction& instr)
{
    // TODO
}

void EE_JIT64::parallel_add_with_unsigned_saturation_byte(EmotionEngine& ee, IR::Instruction& instr)
{
    REG_64 source = alloc_reg(ee, instr.get_source(), REG_TYPE::GPREXTENDED, REG_STATE::READ);
    REG_64 source2 = alloc_reg(ee, instr.get_source2(), REG_TYPE::GPREXTENDED, REG_STATE::READ);
    REG_64 dest = alloc_reg(ee, instr.get_dest(), REG_TYPE::GPREXTENDED, REG_STATE::WRITE);

    if (dest == source)
    {
        emitter.PADDUSB(source2, dest);
    }
    else if (dest == source2)
    {
        emitter.PADDUSB(source, dest);
    }
    else
    {
        emitter.MOVAPS_REG(source, dest);
        emitter.PADDUSB(source2, dest);
    }
}

void EE_JIT64::parallel_add_with_unsigned_saturation_halfword(EmotionEngine& ee, IR::Instruction& instr)
{
    REG_64 source = alloc_reg(ee, instr.get_source(), REG_TYPE::GPREXTENDED, REG_STATE::READ);
    REG_64 source2 = alloc_reg(ee, instr.get_source2(), REG_TYPE::GPREXTENDED, REG_STATE::READ);
    REG_64 dest = alloc_reg(ee, instr.get_dest(), REG_TYPE::GPREXTENDED, REG_STATE::WRITE);

    if (dest == source)
    {
        emitter.PADDUSW(source2, dest);
    }
    else if (dest == source2)
    {
        emitter.PADDUSW(source, dest);
    }
    else
    {
        emitter.MOVAPS_REG(source, dest);
        emitter.PADDUSW(source2, dest);
    }
}

void EE_JIT64::parallel_add_with_unsigned_saturation_word(EmotionEngine& ee, IR::Instruction& instr)
{
    // TODO
}

void EE_JIT64::parallel_compare_equal_byte(EmotionEngine& ee, IR::Instruction& instr)
{
    REG_64 source = alloc_reg(ee, instr.get_source(), REG_TYPE::GPREXTENDED, REG_STATE::READ);
    REG_64 source2 = alloc_reg(ee, instr.get_source2(), REG_TYPE::GPREXTENDED, REG_STATE::READ);
    REG_64 dest = alloc_reg(ee, instr.get_dest(), REG_TYPE::GPREXTENDED, REG_STATE::WRITE);

    if (dest == source)
    {
        emitter.PCMPEQB_XMM(source2, dest);
    }
    else if (dest == source2)
    {
        emitter.PCMPEQB_XMM(source, dest);
    }
    else
    {
        emitter.MOVAPS_REG(source, dest);
        emitter.PCMPEQB_XMM(source2, dest);
    }
}

void EE_JIT64::parallel_compare_equal_halfword(EmotionEngine& ee, IR::Instruction& instr)
{
    REG_64 source = alloc_reg(ee, instr.get_source(), REG_TYPE::GPREXTENDED, REG_STATE::READ);
    REG_64 source2 = alloc_reg(ee, instr.get_source2(), REG_TYPE::GPREXTENDED, REG_STATE::READ);
    REG_64 dest = alloc_reg(ee, instr.get_dest(), REG_TYPE::GPREXTENDED, REG_STATE::WRITE);

    if (dest == source)
    {
        emitter.PCMPEQW_XMM(source2, dest);
    }
    else if (dest == source2)
    {
        emitter.PCMPEQW_XMM(source, dest);
    }
    else
    {
        emitter.MOVAPS_REG(source, dest);
        emitter.PCMPEQW_XMM(source2, dest);
    }
}

void EE_JIT64::parallel_compare_equal_word(EmotionEngine& ee, IR::Instruction& instr)
{
    REG_64 source = alloc_reg(ee, instr.get_source(), REG_TYPE::GPREXTENDED, REG_STATE::READ);
    REG_64 source2 = alloc_reg(ee, instr.get_source2(), REG_TYPE::GPREXTENDED, REG_STATE::READ);
    REG_64 dest = alloc_reg(ee, instr.get_dest(), REG_TYPE::GPREXTENDED, REG_STATE::WRITE);

    if (dest == source)
    {
        emitter.PCMPEQD_XMM(source2, dest);
    }
    else if (dest == source2)
    {
        emitter.PCMPEQD_XMM(source, dest);
    }
    else
    {
        emitter.MOVAPS_REG(source, dest);
        emitter.PCMPEQD_XMM(source2, dest);
    }
}

void EE_JIT64::parallel_compare_greater_than_byte(EmotionEngine& ee, IR::Instruction& instr)
{
    REG_64 source = alloc_reg(ee, instr.get_source(), REG_TYPE::GPREXTENDED, REG_STATE::READ);
    REG_64 source2 = alloc_reg(ee, instr.get_source2(), REG_TYPE::GPREXTENDED, REG_STATE::READ);
    REG_64 dest = alloc_reg(ee, instr.get_dest(), REG_TYPE::GPREXTENDED, REG_STATE::WRITE);

    if (dest == source)
    {
        emitter.PCMPGTB_XMM(source2, dest);
    }
    else if (dest == source2)
    {
        // Swap source and source2 so that the comparison is in the correct order
        REG_64 XMM0 = lalloc_xmm_reg(ee, 0, REG_TYPE::XMMSCRATCHPAD, REG_STATE::SCRATCHPAD);
        emitter.MOVAPS_REG(source2, XMM0);
        emitter.MOVAPS_REG(source, dest);
        emitter.PCMPGTB_XMM(XMM0, dest);
        free_xmm_reg(ee, XMM0);
    }
    else
    {
        emitter.MOVAPS_REG(source, dest);
        emitter.PCMPGTB_XMM(source2, dest);
    }
}

void EE_JIT64::parallel_compare_greater_than_halfword(EmotionEngine& ee, IR::Instruction& instr)
{
    REG_64 source = alloc_reg(ee, instr.get_source(), REG_TYPE::GPREXTENDED, REG_STATE::READ);
    REG_64 source2 = alloc_reg(ee, instr.get_source2(), REG_TYPE::GPREXTENDED, REG_STATE::READ);
    REG_64 dest = alloc_reg(ee, instr.get_dest(), REG_TYPE::GPREXTENDED, REG_STATE::WRITE);

    if (dest == source)
    {
        emitter.PCMPGTW_XMM(source2, dest);
    }
    else if (dest == source2)
    {
        // Swap source and source2 so that the comparison is in the correct order
        REG_64 XMM0 = lalloc_xmm_reg(ee, 0, REG_TYPE::XMMSCRATCHPAD, REG_STATE::SCRATCHPAD);
        emitter.MOVAPS_REG(source2, XMM0);
        emitter.MOVAPS_REG(source, dest);
        emitter.PCMPGTW_XMM(XMM0, dest);
        free_xmm_reg(ee, XMM0);
    }
    else
    {
        emitter.MOVAPS_REG(source, dest);
        emitter.PCMPGTW_XMM(source2, dest);
    }
}

void EE_JIT64::parallel_compare_greater_than_word(EmotionEngine& ee, IR::Instruction& instr)
{
    REG_64 source = alloc_reg(ee, instr.get_source(), REG_TYPE::GPREXTENDED, REG_STATE::READ);
    REG_64 source2 = alloc_reg(ee, instr.get_source2(), REG_TYPE::GPREXTENDED, REG_STATE::READ);
    REG_64 dest = alloc_reg(ee, instr.get_dest(), REG_TYPE::GPREXTENDED, REG_STATE::WRITE);

    if (dest == source)
    {
        emitter.PCMPGTD_XMM(source2, dest);
    }
    else if (dest == source2)
    {
        // Swap source and source2 so that the comparison is in the correct order
        REG_64 XMM0 = lalloc_xmm_reg(ee, 0, REG_TYPE::XMMSCRATCHPAD, REG_STATE::SCRATCHPAD);
        emitter.MOVAPS_REG(source2, XMM0);
        emitter.MOVAPS_REG(source, dest);
        emitter.PCMPGTD_XMM(XMM0, dest);
        free_xmm_reg(ee, XMM0);
    }
    else
    {
        emitter.MOVAPS_REG(source, dest);
        emitter.PCMPGTD_XMM(source2, dest);
    }
}


void EE_JIT64::parallel_divide_word(EmotionEngine& ee, IR::Instruction& instr)
{
    REG_64 RDX = lalloc_int_reg(ee, 0, REG_TYPE::INTSCRATCHPAD, REG_STATE::SCRATCHPAD, REG_64::RDX);
    REG_64 dividend64 = lalloc_int_reg(ee, 0, REG_TYPE::INTSCRATCHPAD, REG_STATE::SCRATCHPAD);
    REG_64 divisor64 = lalloc_int_reg(ee, 0, REG_TYPE::INTSCRATCHPAD, REG_STATE::SCRATCHPAD);

    REG_64 dividend = alloc_reg(ee, instr.get_source(), REG_TYPE::GPREXTENDED, REG_STATE::READ);
    REG_64 divisor = alloc_reg(ee, instr.get_source2(), REG_TYPE::GPREXTENDED, REG_STATE::READ);
    REG_64 LO = alloc_reg(ee, (int)EE_SpecialReg::LO, REG_TYPE::GPREXTENDED, REG_STATE::WRITE);
    REG_64 HI = alloc_reg(ee, (int)EE_SpecialReg::HI, REG_TYPE::GPREXTENDED, REG_STATE::WRITE);

    emitter.MOVD_FROM_XMM(dividend, dividend64);
    emitter.MOVD_FROM_XMM(divisor, divisor64);
    emitter.CMP32_IMM(0x80000000, dividend64);
    uint8_t *label1_1 = emitter.JCC_NEAR_DEFERRED(ConditionCode::NE);
    emitter.CMP32_IMM(0xFFFFFFFF, divisor64);
    uint8_t *label1_2 = emitter.JCC_NEAR_DEFERRED(ConditionCode::NE);
    emitter.MOV64_OI((int64_t)(int32_t)0x80000000, REG_64::RAX);
    emitter.MOVQ_TO_XMM(REG_64::RAX, LO);
    emitter.MOV64_OI(0, REG_64::RAX);
    emitter.MOVQ_TO_XMM(REG_64::RAX, HI);
    uint8_t *lower_end_1 = emitter.JMP_NEAR_DEFERRED();

    emitter.set_jump_dest(label1_1);
    emitter.set_jump_dest(label1_2);
    emitter.TEST32_REG(divisor64, divisor64);
    uint8_t* label2 = emitter.JCC_NEAR_DEFERRED(ConditionCode::Z);
    emitter.MOV32_REG(dividend64, REG_64::RAX);
    emitter.CDQ();
    emitter.IDIV32(divisor64);
    emitter.MOVSX32_TO_64(REG_64::RAX, REG_64::RAX);
    emitter.MOVSX32_TO_64(RDX, RDX);
    emitter.MOVQ_TO_XMM(REG_64::RAX, LO);
    emitter.MOVQ_TO_XMM(RDX, HI);
    uint8_t *lower_end_2 = emitter.JMP_NEAR_DEFERRED();

    emitter.set_jump_dest(label2);
    emitter.TEST32_REG(dividend64, dividend64);
    emitter.SETCC_REG(ConditionCode::L, REG_64::RAX);
    emitter.SHL8_REG_1(REG_64::RAX);
    emitter.DEC8(REG_64::RAX);
    emitter.MOVSX8_TO_64(REG_64::RAX, REG_64::RAX);
    emitter.MOVSX32_TO_64(dividend64, dividend64);
    emitter.MOVQ_TO_XMM(REG_64::RAX, LO);
    emitter.MOVQ_TO_XMM(dividend64, HI);

    emitter.set_jump_dest(lower_end_1);
    emitter.set_jump_dest(lower_end_2);

    emitter.PEXTRD_XMM(2, dividend, dividend64);
    emitter.PEXTRD_XMM(2, divisor, divisor64);
    emitter.CMP32_IMM(0x80000000, dividend64);
    uint8_t *label3_1 = emitter.JCC_NEAR_DEFERRED(ConditionCode::NE);
    emitter.CMP32_IMM(0xFFFFFFFF, divisor64);
    uint8_t *label3_2 = emitter.JCC_NEAR_DEFERRED(ConditionCode::NE);
    emitter.MOV64_OI((int64_t)(int32_t)0x80000000, REG_64::RAX);
    emitter.PINSRQ_XMM(1, REG_64::RAX, LO);
    emitter.MOV64_OI(0, REG_64::RAX);
    emitter.PINSRQ_XMM(1, REG_64::RAX, HI);
    uint8_t *upper_end_1 = emitter.JMP_NEAR_DEFERRED();

    emitter.set_jump_dest(label3_1);
    emitter.set_jump_dest(label3_2);
    emitter.TEST32_REG(divisor64, divisor64);
    uint8_t* label4 = emitter.JCC_NEAR_DEFERRED(ConditionCode::Z);
    emitter.MOV32_REG(dividend64, REG_64::RAX);
    emitter.CDQ();
    emitter.IDIV32(divisor64);
    emitter.MOVSX32_TO_64(REG_64::RAX, REG_64::RAX);
    emitter.MOVSX32_TO_64(RDX, RDX);
    emitter.PINSRQ_XMM(1, REG_64::RAX, LO);
    emitter.PINSRQ_XMM(1, RDX, HI);
    uint8_t *upper_end_2 = emitter.JMP_NEAR_DEFERRED();

    emitter.set_jump_dest(label4);
    emitter.TEST32_REG(dividend64, dividend64);
    emitter.SETCC_REG(ConditionCode::L, REG_64::RAX);
    emitter.SHL8_REG_1(REG_64::RAX);
    emitter.DEC8(REG_64::RAX);
    emitter.MOVSX8_TO_64(REG_64::RAX, REG_64::RAX);
    emitter.MOVSX32_TO_64(dividend64, dividend64);
    emitter.PINSRQ_XMM(1, REG_64::RAX, LO);
    emitter.PINSRQ_XMM(1, dividend64, HI);

    emitter.set_jump_dest(upper_end_1);
    emitter.set_jump_dest(upper_end_2);

    free_int_reg(ee, RDX);
    free_int_reg(ee, divisor64);
    free_int_reg(ee, dividend64);
}

void EE_JIT64::parallel_exchange_halfword(EmotionEngine& ee, IR::Instruction& instr, bool even)
{
    REG_64 source = alloc_reg(ee, instr.get_source(), REG_TYPE::GPREXTENDED, REG_STATE::READ);
    REG_64 dest = alloc_reg(ee, instr.get_dest(), REG_TYPE::GPREXTENDED, REG_STATE::WRITE);

    if (even)
    {
        emitter.PSHUFLW(0xC6, source, dest);
        emitter.PSHUFHW(0xC6, source, dest);
    }
    else
    {
        emitter.PSHUFLW(0xD8, source, dest);
        emitter.PSHUFHW(0xD8, source, dest);
    }
}

void EE_JIT64::parallel_exchange_word(EmotionEngine& ee, IR::Instruction& instr, bool even)
{
    REG_64 source = alloc_reg(ee, instr.get_source(), REG_TYPE::GPREXTENDED, REG_STATE::READ);
    REG_64 dest = alloc_reg(ee, instr.get_dest(), REG_TYPE::GPREXTENDED, REG_STATE::WRITE);

    if (even)
        emitter.PSHUFD(0xC6, source, dest);
    else
        emitter.PSHUFD(0xD8, source, dest);
}

void EE_JIT64::parallel_maximize_halfword(EmotionEngine& ee, IR::Instruction& instr)
{
    REG_64 source = alloc_reg(ee, instr.get_source(), REG_TYPE::GPREXTENDED, REG_STATE::READ);
    REG_64 source2 = alloc_reg(ee, instr.get_source2(), REG_TYPE::GPREXTENDED, REG_STATE::READ);
    REG_64 dest = alloc_reg(ee, instr.get_dest(), REG_TYPE::GPREXTENDED, REG_STATE::WRITE);

    if (dest == source)
    {
        emitter.PMAXSW_XMM(source2, dest);
    }
    else if (dest == source2)
    {
        emitter.PMAXSW_XMM(source, dest);
    }
    else
    {
        emitter.MOVAPS_REG(source, dest);
        emitter.PMAXSW_XMM(source2, dest);
    }
}

void EE_JIT64::parallel_maximize_word(EmotionEngine& ee, IR::Instruction& instr)
{
    REG_64 source = alloc_reg(ee, instr.get_source(), REG_TYPE::GPREXTENDED, REG_STATE::READ);
    REG_64 source2 = alloc_reg(ee, instr.get_source2(), REG_TYPE::GPREXTENDED, REG_STATE::READ);
    REG_64 dest = alloc_reg(ee, instr.get_dest(), REG_TYPE::GPREXTENDED, REG_STATE::WRITE);

    if (dest == source)
    {
        emitter.PMAXSD_XMM(source2, dest);
    }
    else if (dest == source2)
    {
        emitter.PMAXSD_XMM(source, dest);
    }
    else
    {
        emitter.MOVAPS_REG(source, dest);
        emitter.PMAXSD_XMM(source2, dest);
    }
}

void EE_JIT64::parallel_minimize_halfword(EmotionEngine& ee, IR::Instruction& instr)
{
    REG_64 source = alloc_reg(ee, instr.get_source(), REG_TYPE::GPREXTENDED, REG_STATE::READ);
    REG_64 source2 = alloc_reg(ee, instr.get_source2(), REG_TYPE::GPREXTENDED, REG_STATE::READ);
    REG_64 dest = alloc_reg(ee, instr.get_dest(), REG_TYPE::GPREXTENDED, REG_STATE::WRITE);

    if (dest == source)
    {
        emitter.PMINSW_XMM(source2, dest);
    }
    else if (dest == source2)
    {
        emitter.PMINSW_XMM(source, dest);
    }
    else
    {
        emitter.MOVAPS_REG(source, dest);
        emitter.PMINSW_XMM(source2, dest);
    }
}

void EE_JIT64::parallel_minimize_word(EmotionEngine& ee, IR::Instruction& instr)
{
    REG_64 source = alloc_reg(ee, instr.get_source(), REG_TYPE::GPREXTENDED, REG_STATE::READ);
    REG_64 source2 = alloc_reg(ee, instr.get_source2(), REG_TYPE::GPREXTENDED, REG_STATE::READ);
    REG_64 dest = alloc_reg(ee, instr.get_dest(), REG_TYPE::GPREXTENDED, REG_STATE::WRITE);

    if (dest == source)
    {
        emitter.PMINSD_XMM(source2, dest);
    }
    else if (dest == source2)
    {
        emitter.PMINSD_XMM(source, dest);
    }
    else
    {
        emitter.MOVAPS_REG(source, dest);
        emitter.PMINSD_XMM(source2, dest);
    }
}

void EE_JIT64::parallel_pack_to_byte(EmotionEngine& ee, IR::Instruction& instr)
{
    REG_64 source = alloc_reg(ee, instr.get_source(), REG_TYPE::GPREXTENDED, REG_STATE::READ);
    REG_64 source2 = alloc_reg(ee, instr.get_source2(), REG_TYPE::GPREXTENDED, REG_STATE::READ);
    REG_64 dest = alloc_reg(ee, instr.get_dest(), REG_TYPE::GPREXTENDED, REG_STATE::WRITE);
    REG_64 XMM0 = lalloc_xmm_reg(ee, 0, REG_TYPE::XMMSCRATCHPAD, REG_STATE::SCRATCHPAD);
    REG_64 XMM1 = lalloc_xmm_reg(ee, 1, REG_TYPE::XMMSCRATCHPAD, REG_STATE::SCRATCHPAD);

    emitter.MOVAPS_REG(source, XMM1);
    if (source2 != dest)
        emitter.MOVAPS_REG(source2, dest);

    emitter.load_addr((uint64_t)&PPACB_MASK, REG_64::RAX);
    emitter.MOVAPS_FROM_MEM(REG_64::RAX, XMM0);

    emitter.PAND_XMM(XMM0, dest);
    emitter.PAND_XMM(XMM0, XMM1);

    emitter.PACKUSWB(XMM1, dest);
    free_xmm_reg(ee, XMM0);
    free_xmm_reg(ee, XMM1);
}

void EE_JIT64::parallel_pack_to_halfword(EmotionEngine& ee, IR::Instruction& instr)
{
    REG_64 source = alloc_reg(ee, instr.get_source(), REG_TYPE::GPREXTENDED, REG_STATE::READ);
    REG_64 source2 = alloc_reg(ee, instr.get_source2(), REG_TYPE::GPREXTENDED, REG_STATE::READ);
    REG_64 dest = alloc_reg(ee, instr.get_dest(), REG_TYPE::GPREXTENDED, REG_STATE::WRITE);
    REG_64 XMM0 = lalloc_xmm_reg(ee, 0, REG_TYPE::XMMSCRATCHPAD, REG_STATE::SCRATCHPAD);
    REG_64 XMM1 = lalloc_xmm_reg(ee, 1, REG_TYPE::XMMSCRATCHPAD, REG_STATE::SCRATCHPAD);

    emitter.MOVAPS_REG(source, XMM1);
    if (source2 != dest)
        emitter.MOVAPS_REG(source2, dest);

    emitter.load_addr((uint64_t)&PPACH_MASK, REG_64::RAX);
    emitter.MOVAPS_FROM_MEM(REG_64::RAX, XMM0);

    emitter.PAND_XMM(XMM0, dest);
    emitter.PAND_XMM(XMM0, XMM1);

    emitter.PACKUSDW(XMM1, dest);
    free_xmm_reg(ee, XMM0);
    free_xmm_reg(ee, XMM1);
}

void EE_JIT64::parallel_pack_to_word(EmotionEngine& ee, IR::Instruction& instr)
{
    REG_64 source = alloc_reg(ee, instr.get_source(), REG_TYPE::GPREXTENDED, REG_STATE::READ);
    REG_64 source2 = alloc_reg(ee, instr.get_source2(), REG_TYPE::GPREXTENDED, REG_STATE::READ);
    REG_64 dest = alloc_reg(ee, instr.get_dest(), REG_TYPE::GPREXTENDED, REG_STATE::WRITE);
    REG_64 XMM0 = lalloc_xmm_reg(ee, 0, REG_TYPE::XMMSCRATCHPAD, REG_STATE::SCRATCHPAD);

    for (int i = 0; i < 2; ++i)
    {
        emitter.PEXTRD_XMM(i * 2, source, REG_64::RAX);
        emitter.PINSRD_XMM(2 + i, REG_64::RAX, XMM0);
    }

    for (int i = 0; i < 2; ++i)
    {
        emitter.PEXTRD_XMM(i * 2, source2, REG_64::RAX);
        emitter.PINSRD_XMM(i, REG_64::RAX, XMM0);
    }

    emitter.MOVAPS_REG(XMM0, dest);

    free_xmm_reg(ee, XMM0);
}

void EE_JIT64::parallel_shift_left_logical_halfword(EmotionEngine& ee, IR::Instruction& instr)
{
    REG_64 source = alloc_reg(ee, instr.get_source(), REG_TYPE::GPREXTENDED, REG_STATE::READ);
    REG_64 dest = alloc_reg(ee, instr.get_dest(), REG_TYPE::GPREXTENDED, REG_STATE::WRITE);

    if (dest != source)
        emitter.MOVAPS_REG(source, dest);
    emitter.PSLLW(instr.get_source2(), dest);
}

void EE_JIT64::parallel_shift_left_logical_word(EmotionEngine& ee, IR::Instruction& instr)
{
    REG_64 source = alloc_reg(ee, instr.get_source(), REG_TYPE::GPREXTENDED, REG_STATE::READ);
    REG_64 dest = alloc_reg(ee, instr.get_dest(), REG_TYPE::GPREXTENDED, REG_STATE::WRITE);

    if (dest != source)
        emitter.MOVAPS_REG(source, dest);
    emitter.PSLLD(instr.get_source2(), dest);
}

void EE_JIT64::parallel_shift_right_arithmetic_halfword(EmotionEngine& ee, IR::Instruction& instr)
{
    REG_64 source = alloc_reg(ee, instr.get_source(), REG_TYPE::GPREXTENDED, REG_STATE::READ);
    REG_64 dest = alloc_reg(ee, instr.get_dest(), REG_TYPE::GPREXTENDED, REG_STATE::WRITE);

    if (dest != source)
        emitter.MOVAPS_REG(source, dest);
    emitter.PSRAW(instr.get_source2(), dest);
}

void EE_JIT64::parallel_shift_right_arithmetic_word(EmotionEngine& ee, IR::Instruction& instr)
{
    REG_64 source = alloc_reg(ee, instr.get_source(), REG_TYPE::GPREXTENDED, REG_STATE::READ);
    REG_64 dest = alloc_reg(ee, instr.get_dest(), REG_TYPE::GPREXTENDED, REG_STATE::WRITE);

    if (dest != source)
        emitter.MOVAPS_REG(source, dest);
    emitter.PSRAD(instr.get_source2(), dest);
}

void EE_JIT64::parallel_shift_right_logical_halfword(EmotionEngine& ee, IR::Instruction& instr)
{
    REG_64 source = alloc_reg(ee, instr.get_source(), REG_TYPE::GPREXTENDED, REG_STATE::READ);
    REG_64 dest = alloc_reg(ee, instr.get_dest(), REG_TYPE::GPREXTENDED, REG_STATE::WRITE);

    if (dest != source)
        emitter.MOVAPS_REG(source, dest);
    emitter.PSRLW(instr.get_source2(), dest);
}

void EE_JIT64::parallel_shift_right_logical_word(EmotionEngine& ee, IR::Instruction& instr)
{
    REG_64 source = alloc_reg(ee, instr.get_source(), REG_TYPE::GPREXTENDED, REG_STATE::READ);
    REG_64 dest = alloc_reg(ee, instr.get_dest(), REG_TYPE::GPREXTENDED, REG_STATE::WRITE);

    if (dest != source)
        emitter.MOVAPS_REG(source, dest);
    emitter.PSRLD(instr.get_source2(), dest);
}

void EE_JIT64::parallel_subtract_byte(EmotionEngine& ee, IR::Instruction& instr)
{
    REG_64 source = alloc_reg(ee, instr.get_source(), REG_TYPE::GPREXTENDED, REG_STATE::READ);
    REG_64 source2 = alloc_reg(ee, instr.get_source2(), REG_TYPE::GPREXTENDED, REG_STATE::READ);
    REG_64 dest = alloc_reg(ee, instr.get_dest(), REG_TYPE::GPREXTENDED, REG_STATE::WRITE);

    if (dest == source)
    {
        emitter.PSUBB(source2, dest);
    }
    else if (dest == source2)
    {
        REG_64 XMM0 = lalloc_xmm_reg(ee, 0, REG_TYPE::XMMSCRATCHPAD, REG_STATE::SCRATCHPAD);
        emitter.MOVAPS_REG(source2, XMM0);
        emitter.MOVAPS_REG(source, dest);
        emitter.PSUBB(XMM0, dest);
        free_xmm_reg(ee, XMM0);
    }
    else
    {
        emitter.MOVAPS_REG(source, dest);
        emitter.PSUBB(source2, dest);
    }
}

void EE_JIT64::parallel_subtract_halfword(EmotionEngine& ee, IR::Instruction& instr)
{
    REG_64 source = alloc_reg(ee, instr.get_source(), REG_TYPE::GPREXTENDED, REG_STATE::READ);
    REG_64 source2 = alloc_reg(ee, instr.get_source2(), REG_TYPE::GPREXTENDED, REG_STATE::READ);
    REG_64 dest = alloc_reg(ee, instr.get_dest(), REG_TYPE::GPREXTENDED, REG_STATE::WRITE);

    if (dest == source)
    {
        emitter.PSUBW(source2, dest);
    }
    else if (dest == source2)
    {
        REG_64 XMM0 = lalloc_xmm_reg(ee, 0, REG_TYPE::XMMSCRATCHPAD, REG_STATE::SCRATCHPAD);
        emitter.MOVAPS_REG(source2, XMM0);
        emitter.MOVAPS_REG(source, dest);
        emitter.PSUBW(XMM0, dest);
        free_xmm_reg(ee, XMM0);
    }
    else
    {
        emitter.MOVAPS_REG(source, dest);
        emitter.PSUBW(source2, dest);
    }
}

void EE_JIT64::parallel_subtract_word(EmotionEngine& ee, IR::Instruction& instr)
{
    REG_64 source = alloc_reg(ee, instr.get_source(), REG_TYPE::GPREXTENDED, REG_STATE::READ);
    REG_64 source2 = alloc_reg(ee, instr.get_source2(), REG_TYPE::GPREXTENDED, REG_STATE::READ);
    REG_64 dest = alloc_reg(ee, instr.get_dest(), REG_TYPE::GPREXTENDED, REG_STATE::WRITE);

    if (dest == source)
    {
        emitter.PSUBD(source2, dest);
    }
    else if (dest == source2)
    {
        REG_64 XMM0 = lalloc_xmm_reg(ee, 0, REG_TYPE::XMMSCRATCHPAD, REG_STATE::SCRATCHPAD);
        emitter.MOVAPS_REG(source2, XMM0);
        emitter.MOVAPS_REG(source, dest);
        emitter.PSUBD(XMM0, dest);
        free_xmm_reg(ee, XMM0);
    }
    else
    {
        emitter.MOVAPS_REG(source, dest);
        emitter.PSUBD(source2, dest);
    }
}

void EE_JIT64::parallel_subtract_with_signed_saturation_byte(EmotionEngine& ee, IR::Instruction& instr)
{
    REG_64 source = alloc_reg(ee, instr.get_source(), REG_TYPE::GPREXTENDED, REG_STATE::READ);
    REG_64 source2 = alloc_reg(ee, instr.get_source2(), REG_TYPE::GPREXTENDED, REG_STATE::READ);
    REG_64 dest = alloc_reg(ee, instr.get_dest(), REG_TYPE::GPREXTENDED, REG_STATE::WRITE);

    if (dest == source)
    {
        emitter.PSUBSB(source2, dest);
    }
    else if (dest == source2)
    {
        REG_64 XMM0 = lalloc_xmm_reg(ee, 0, REG_TYPE::XMMSCRATCHPAD, REG_STATE::SCRATCHPAD);
        emitter.MOVAPS_REG(source2, XMM0);
        emitter.MOVAPS_REG(source, dest);
        emitter.PSUBSB(XMM0, dest);
        free_xmm_reg(ee, XMM0);
    }
    else
    {
        emitter.MOVAPS_REG(source, dest);
        emitter.PSUBSB(source2, dest);
    }
}

void EE_JIT64::parallel_subtract_with_signed_saturation_halfword(EmotionEngine& ee, IR::Instruction& instr)
{
    REG_64 source = alloc_reg(ee, instr.get_source(), REG_TYPE::GPREXTENDED, REG_STATE::READ);
    REG_64 source2 = alloc_reg(ee, instr.get_source2(), REG_TYPE::GPREXTENDED, REG_STATE::READ);
    REG_64 dest = alloc_reg(ee, instr.get_dest(), REG_TYPE::GPREXTENDED, REG_STATE::WRITE);

    if (dest == source)
    {
        emitter.PSUBSW(source2, dest);
    }
    else if (dest == source2)
    {
        REG_64 XMM0 = lalloc_xmm_reg(ee, 0, REG_TYPE::XMMSCRATCHPAD, REG_STATE::SCRATCHPAD);
        emitter.MOVAPS_REG(source2, XMM0);
        emitter.MOVAPS_REG(source, dest);
        emitter.PSUBSW(XMM0, dest);
        free_xmm_reg(ee, XMM0);
    }
    else
    {
        emitter.MOVAPS_REG(source, dest);
        emitter.PSUBSW(source2, dest);
    }
}

void EE_JIT64::parallel_subtract_with_signed_saturation_word(EmotionEngine& ee, IR::Instruction& instr)
{
    // TODO
}

void EE_JIT64::parallel_subtract_with_unsigned_saturation_byte(EmotionEngine& ee, IR::Instruction& instr)
{
    REG_64 source = alloc_reg(ee, instr.get_source(), REG_TYPE::GPREXTENDED, REG_STATE::READ);
    REG_64 source2 = alloc_reg(ee, instr.get_source2(), REG_TYPE::GPREXTENDED, REG_STATE::READ);
    REG_64 dest = alloc_reg(ee, instr.get_dest(), REG_TYPE::GPREXTENDED, REG_STATE::WRITE);

    if (dest == source)
    {
        emitter.PSUBUSB(source2, dest);
    }
    else if (dest == source2)
    {
        REG_64 XMM0 = lalloc_xmm_reg(ee, 0, REG_TYPE::XMMSCRATCHPAD, REG_STATE::SCRATCHPAD);
        emitter.MOVAPS_REG(source2, XMM0);
        emitter.MOVAPS_REG(source, dest);
        emitter.PSUBUSB(XMM0, dest);
        free_xmm_reg(ee, XMM0);
    }
    else
    {
        emitter.MOVAPS_REG(source, dest);
        emitter.PSUBUSB(source2, dest);
    }
}

void EE_JIT64::parallel_subtract_with_unsigned_saturation_halfword(EmotionEngine& ee, IR::Instruction& instr)
{
    REG_64 source = alloc_reg(ee, instr.get_source(), REG_TYPE::GPREXTENDED, REG_STATE::READ);
    REG_64 source2 = alloc_reg(ee, instr.get_source2(), REG_TYPE::GPREXTENDED, REG_STATE::READ);
    REG_64 dest = alloc_reg(ee, instr.get_dest(), REG_TYPE::GPREXTENDED, REG_STATE::WRITE);

    if (dest == source)
    {
        emitter.PSUBUSW(source2, dest);
    }
    else if (dest == source2)
    {
        REG_64 XMM0 = lalloc_xmm_reg(ee, 0, REG_TYPE::XMMSCRATCHPAD, REG_STATE::SCRATCHPAD);
        emitter.MOVAPS_REG(source2, XMM0);
        emitter.MOVAPS_REG(source, dest);
        emitter.PSUBUSW(XMM0, dest);
        free_xmm_reg(ee, XMM0);
    }
    else
    {
        emitter.MOVAPS_REG(source, dest);
        emitter.PSUBUSW(source2, dest);
    }
}

void EE_JIT64::parallel_subtract_with_unsigned_saturation_word(EmotionEngine& ee, IR::Instruction& instr)
{
    // TODO
}

void EE_JIT64::parallel_reverse_halfword(EmotionEngine& ee, IR::Instruction& instr)
{
    REG_64 source = alloc_reg(ee, instr.get_source(), REG_TYPE::GPREXTENDED, REG_STATE::READ);
    REG_64 dest = alloc_reg(ee, instr.get_dest(), REG_TYPE::GPREXTENDED, REG_STATE::WRITE);

    emitter.PSHUFLW(0x1B, source, dest);
    emitter.PSHUFHW(0x1B, source, dest);
}

void EE_JIT64::parallel_rotate_3_words_left(EmotionEngine& ee, IR::Instruction& instr)
{
    REG_64 source = alloc_reg(ee, instr.get_source(), REG_TYPE::GPREXTENDED, REG_STATE::READ);
    REG_64 dest = alloc_reg(ee, instr.get_dest(), REG_TYPE::GPREXTENDED, REG_STATE::WRITE);

    emitter.PSHUFD(0xC9, source, dest);
}

void EE_JIT64::parallel_xor(EmotionEngine& ee, IR::Instruction& instr)
{
    REG_64 source = alloc_reg(ee, instr.get_source(), REG_TYPE::GPREXTENDED, REG_STATE::READ);
    REG_64 source2 = alloc_reg(ee, instr.get_source2(), REG_TYPE::GPREXTENDED, REG_STATE::READ);
    REG_64 dest = alloc_reg(ee, instr.get_dest(), REG_TYPE::GPREXTENDED, REG_STATE::WRITE);

    if (dest == source)
    {
        emitter.PXOR_XMM(source2, dest);
    }
    else if (dest == source2)
    {
        emitter.PXOR_XMM(source, dest);
    }
    else
    {
        emitter.MOVAPS_REG(source, dest);
        emitter.PXOR_XMM(source2, dest);
    }
}