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

    // No PNOT...?
    emitter.PEXTRQ_XMM(0, dest, REG_64::RAX);
    emitter.NOT64(REG_64::RAX);
    emitter.PINSRQ_XMM(0, REG_64::RAX, dest);
    emitter.PEXTRQ_XMM(1, dest, REG_64::RAX);
    emitter.NOT64(REG_64::RAX);
    emitter.PINSRQ_XMM(1, REG_64::RAX, dest);
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