#include <cmath>
#include <algorithm>
#include "ee_jit64.hpp"
#include "vu.hpp"

uint8_t convert_field(uint8_t value);

void EE_JIT64::clamp_vfreg(EmotionEngine& ee, uint8_t field, REG_64 xmm_reg)
{
    if (needs_clamping(xmm_reg, field))
    {
        REG_64 XMM0 = lalloc_xmm_reg(ee, 0, REG_TYPE::XMMSCRATCHPAD, REG_STATE::SCRATCHPAD);
        REG_64 R15 = lalloc_int_reg(ee, 0, REG_TYPE::INTSCRATCHPAD, REG_STATE::SCRATCHPAD);

        emitter.load_addr((uint64_t)&max_flt_constant, REG_64::RAX);
        emitter.load_addr((uint64_t)&min_flt_constant, R15);

        emitter.MOVAPS_REG(xmm_reg, XMM0);
        //reg = min_signed(reg, 0x7F7FFFFF)
        emitter.PMINSD_XMM_FROM_MEM(REG_64::RAX, XMM0);

        //reg = min_unsigned(reg, 0xFF7FFFFF)
        emitter.PMINUD_XMM_FROM_MEM(R15, XMM0);

        emitter.BLENDPS(field, XMM0, xmm_reg);
        set_clamping(xmm_reg, false, field);

        free_xmm_reg(ee, XMM0);
        free_int_reg(ee, R15);
    }
}

bool EE_JIT64::needs_clamping(int xmmreg, uint8_t field)
{
    return xmm_regs[xmmreg].needs_clamping & field;
}

void EE_JIT64::set_clamping(int xmmreg, bool value, uint8_t field)
{
    if (xmm_regs[xmmreg].reg)
    {
        if (value == false)
            xmm_regs[xmmreg].needs_clamping &= ~field;
        else
            xmm_regs[xmmreg].needs_clamping |= field;
    }
    else
        xmm_regs[xmmreg].needs_clamping = false;
}

void EE_JIT64::update_mac_flags(EmotionEngine &ee, REG_64 xmm_reg, uint8_t field)
{
    field = convert_field(field);
    REG_64 XMM0 = lalloc_xmm_reg(ee, 0, REG_TYPE::XMMSCRATCHPAD, REG_STATE::SCRATCHPAD);
    REG_64 XMM1 = lalloc_xmm_reg(ee, 0, REG_TYPE::XMMSCRATCHPAD, REG_STATE::SCRATCHPAD);
    REG_64 R15 = lalloc_int_reg(ee, 0, REG_TYPE::INTSCRATCHPAD, REG_STATE::SCRATCHPAD);

    //Shuffle the vector from WZYX to XYZW
    if (xmm_reg != XMM0)
        emitter.MOVAPS_REG(xmm_reg, XMM0);
    emitter.PSHUFD(0x1B, XMM0, XMM0);

    //Get the sign bits
    emitter.MOVMSKPS(XMM0, REG_64::RAX);
    emitter.SHL32_REG_IMM(4, REG_64::RAX);

    //Get zero bits
    //First remove sign bit
    emitter.load_addr((uint64_t)&abs_constant, R15);
    emitter.MOVAPS_FROM_MEM(R15, XMM1);
    emitter.PAND_XMM(XMM1, XMM0);

    //Then compare with 0
    emitter.XORPS(XMM1, XMM1);
    emitter.CMPEQPS(XMM1, XMM0);
    emitter.MOVMSKPS(XMM0, R15);
    emitter.OR32_REG(R15, REG_64::RAX);
    emitter.AND32_EAX((field << 4) | field);

    emitter.load_addr((uint64_t)&ee.vu0->new_MAC_flags, R15);
    emitter.MOV16_TO_MEM(REG_64::RAX, R15);

    should_update_mac = false;
    free_xmm_reg(ee, XMM0);
    free_xmm_reg(ee, XMM1);
    free_int_reg(ee, R15);
}

void EE_JIT64::add_vector_by_scalar(EmotionEngine& ee, IR::Instruction& instr)
{
    uint8_t field = convert_field(instr.get_field());
    REG_64 source = alloc_reg(ee, instr.get_source(), REG_TYPE::VF, REG_STATE::READ_WRITE);
    REG_64 bc_reg = alloc_reg(ee, instr.get_source2(), REG_TYPE::VF, REG_STATE::READ_WRITE);
    REG_64 dest = alloc_reg(ee, instr.get_dest(), REG_TYPE::VF, REG_STATE::READ_WRITE);

    uint8_t bc = instr.get_bc();

    clamp_vfreg(ee, field, source);
    bc |= (bc << 6) | (bc << 4) | (bc << 2);

    REG_64 temp = lalloc_xmm_reg(ee, 0, REG_TYPE::XMMSCRATCHPAD, REG_STATE::SCRATCHPAD);
    emitter.MOVAPS_REG(bc_reg, temp);
    emitter.SHUFPS(bc, temp, temp);
    set_clamping(temp, true, field);
    clamp_vfreg(ee, field, temp);

    emitter.ADDPS(source, temp);
    set_clamping(temp, true, field);
    clamp_vfreg(ee, field, temp);

    if (instr.get_dest())
    {
        set_clamping(dest, false, field);
        emitter.BLENDPS(field, temp, dest);
    }

    if (should_update_mac)
        update_mac_flags(ee, temp, field);

    free_xmm_reg(ee, temp);
}