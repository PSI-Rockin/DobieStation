#include "ee_jit64.hpp"

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

void EE_JIT64::parallel_pack_to_byte(EmotionEngine& ee, IR::Instruction& instr)
{
    REG_64 source = alloc_reg(ee, instr.get_source(), REG_TYPE::GPREXTENDED, REG_STATE::READ);
    REG_64 source2 = alloc_reg(ee, instr.get_source2(), REG_TYPE::GPREXTENDED, REG_STATE::READ);
    REG_64 dest = alloc_reg(ee, instr.get_dest(), REG_TYPE::GPREXTENDED, REG_STATE::WRITE);
    REG_64 XMM0 = lalloc_xmm_reg(ee, 0, REG_TYPE::XMMSCRATCHPAD, REG_STATE::SCRATCHPAD);

    for (int i = 0; i < 8; ++i)
    {
        emitter.PEXTRB_XMM(i * 2, source, REG_64::RAX);
        emitter.PINSRB_XMM(8 + i, REG_64::RAX, XMM0);
    }

    for (int i = 0; i < 8; ++i)
    {
        emitter.PEXTRB_XMM(i * 2, source2, REG_64::RAX);
        emitter.PINSRB_XMM(i, REG_64::RAX, XMM0);
    }

    emitter.MOVAPS_REG(XMM0, dest);

    free_xmm_reg(ee, XMM0);
}

void EE_JIT64::parallel_pack_to_halfword(EmotionEngine& ee, IR::Instruction& instr)
{
    // TODO: Broken (See "Chicken Little" splash FMVs)

    REG_64 source = alloc_reg(ee, instr.get_source(), REG_TYPE::GPREXTENDED, REG_STATE::READ);
    REG_64 source2 = alloc_reg(ee, instr.get_source2(), REG_TYPE::GPREXTENDED, REG_STATE::READ);
    REG_64 dest = alloc_reg(ee, instr.get_dest(), REG_TYPE::GPREXTENDED, REG_STATE::WRITE);
    REG_64 XMM0 = lalloc_xmm_reg(ee, 0, REG_TYPE::XMMSCRATCHPAD, REG_STATE::SCRATCHPAD);

    for (int i = 0; i < 4; ++i)
    {
        emitter.PEXTRW_XMM(i * 2, source, REG_64::RAX);
        emitter.PINSRB_XMM(4 + i, REG_64::RAX, XMM0);
    }

    for (int i = 0; i < 4; ++i)
    {
        emitter.PEXTRB_XMM(i * 2, source2, REG_64::RAX);
        emitter.PINSRB_XMM(i, REG_64::RAX, XMM0);
    }

    emitter.MOVAPS_REG(XMM0, dest);

    free_xmm_reg(ee, XMM0);
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