#include <cmath>
#include <algorithm>
#include "ee_jit64.hpp"

// temporary wrapper function
void vu_flush_pipes(VectorUnit& vu)
{
    vu.flush_pipes();
}

uint8_t EE_JIT64::convert_field(uint8_t value)
{
    // Reverses the dest value of a COP2 instruction so that it can be used by BLENDPS
    uint8_t result = 0;
    if (value & 0x8)
        result |= 0x1;
    if (value & 0x4)
        result |= 0x2;
    if (value & 0x2)
        result |= 0x4;
    if (value & 0x1)
        result |= 0x8;
    return result;
}

void EE_JIT64::clamp_vfreg(EmotionEngine& ee, uint8_t field, REG_64 vfreg)
{
    if (needs_clamping(vfreg, field))
    {
        REG_64 XMM0 = lalloc_xmm_reg(ee, 0, REG_TYPE::XMMSCRATCHPAD, REG_STATE::SCRATCHPAD);
        emitter.MOVAPS_REG(vfreg, XMM0);

        //reg = min_signed(reg, 0x7F7FFFFF)
        emitter.load_addr((uint64_t)&max_flt_constant, REG_64::RAX);
        emitter.PMINSD_XMM_FROM_MEM(REG_64::RAX, XMM0);

        //reg = min_unsigned(reg, 0xFF7FFFFF)
        emitter.load_addr((uint64_t)&min_flt_constant, REG_64::RAX);
        emitter.PMINUD_XMM_FROM_MEM(REG_64::RAX, XMM0);

        emitter.BLENDPS(field, vfreg, XMM0);
        set_clamping(vfreg, false, field);
        free_xmm_reg(ee, XMM0);
    }
}

bool EE_JIT64::needs_clamping(int reg, uint8_t field)
{
    return xmm_regs[reg].needs_clamping & field;
}

void EE_JIT64::set_clamping(int reg, bool value, uint8_t field)
{
    if (xmm_regs[reg].type == REG_TYPE::VF)
    {
        if (value == false)
            xmm_regs[reg].needs_clamping &= ~field;
        else
            xmm_regs[reg].needs_clamping |= field;
    }
    else
        xmm_regs[reg].needs_clamping = false;
}

void EE_JIT64::update_mac_flags(EmotionEngine& ee, REG_64 reg, uint8_t field)
{
    field = convert_field(field);
    REG_64 XMM0 = lalloc_xmm_reg(ee, 0, REG_TYPE::XMMSCRATCHPAD, REG_STATE::SCRATCHPAD);
    REG_64 XMM1 = lalloc_xmm_reg(ee, 0, REG_TYPE::XMMSCRATCHPAD, REG_STATE::SCRATCHPAD);
    REG_64 R15 = lalloc_xmm_reg(ee, 0, REG_TYPE::INTSCRATCHPAD, REG_STATE::SCRATCHPAD);

    //Shuffle the vector from WZYX to XYZW
    if (reg != XMM0)
        emitter.MOVAPS_REG(reg, XMM0);
    emitter.PSHUFD(0x1B, XMM0, XMM0);

    //Get the sign bits
    emitter.MOVMSKPS(XMM0, REG_64::RAX);
    emitter.SHL32_REG_IMM(4, REG_64::RAX);

    //Get zero bits
    //First remove sign bit
    emitter.load_addr((uint64_t)&FPU_MASK_ABS[0], R15);
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

    free_xmm_reg(ee, XMM0);
    free_xmm_reg(ee, XMM1);
    free_int_reg(ee, R15);

    should_update_mac = false;
}

void EE_JIT64::store_quadword_coprocessor2(EmotionEngine& ee, IR::Instruction &instr)
{
    REG_64 source = alloc_reg(ee, instr.get_source(), REG_TYPE::VF, REG_STATE::READ);
    REG_64 dest = alloc_reg(ee, instr.get_dest(), REG_TYPE::GPR, REG_STATE::READ);
    REG_64 addr = lalloc_int_reg(ee, 0, REG_TYPE::INTSCRATCHPAD, REG_STATE::SCRATCHPAD);
    int64_t offset = instr.get_source2();

    if (offset)
        emitter.LEA32_M(dest, addr, offset);
    else
        emitter.MOV32_REG(dest, addr);

    // Due to differences in how the uint128_t struct is passed as an argument on different platforms,
    // we simply allocate space for it on the stack and pass a pointer to our wrapper function.
    // Note: The 0x1A0 here is the SQ/LQ uint128_t offset noted in recompile_block
    // TODO: Store 0x1A0 in some sort of constant
    emitter.MOVAPS_TO_MEM(source, REG_64::RSP, 0x1A0);
    prepare_abi((uint64_t)&ee);
    prepare_abi_reg(addr);
    prepare_abi_reg(REG_64::RSP, 0x1A0);
    free_int_reg(ee, addr);
    call_abi_func((uint64_t)ee_write128);
}

void EE_JIT64::load_quadword_coprocessor2(EmotionEngine& ee, IR::Instruction &instr)
{
    REG_64 source = alloc_reg(ee, instr.get_source(), REG_TYPE::GPR, REG_STATE::READ);
    REG_64 addr = lalloc_int_reg(ee, 0, REG_TYPE::INTSCRATCHPAD, REG_STATE::SCRATCHPAD);
    REG_64 dest = alloc_reg(ee, instr.get_dest(), REG_TYPE::VF, REG_STATE::WRITE);

    int64_t offset = instr.get_source2();

    if (offset)
        emitter.LEA32_M(source, addr, offset);
    else
        emitter.MOV32_REG(source, addr);

    // Due to differences in how the uint128_t struct is returned on different platforms,
    // we simply allocate space for it on the stack, which the wrapper function will store the
    // result into.
    // Note: The 0x1A0 here is the SQ/LQ uint128_t offset noted in recompile_block
    // TODO: Store 0x1A0 in some sort of constant
    // TODO: Offset in prepare_abi_reg
    prepare_abi((uint64_t)&ee);
    prepare_abi_reg(addr);
    prepare_abi_reg(REG_64::RSP, 0x1A0);
    free_int_reg(ee, addr);
    call_abi_func((uint64_t)ee_read128);
    restore_xmm_regs(std::vector<REG_64> {dest}, false);

    emitter.MOVAPS_FROM_MEM(REG_64::RSP, dest, 0x1A0);
}

void EE_JIT64::vabs(EmotionEngine& ee, IR::Instruction& instr)
{
    prepare_abi((uint64_t)ee.vu0);
    call_abi_func((uint64_t)&vu_flush_pipes);

    uint8_t field = convert_field(instr.get_field());

    REG_64 source = alloc_reg(ee, instr.get_source(), REG_TYPE::VF, REG_STATE::READ);
    REG_64 dest = alloc_reg(ee, instr.get_dest(), REG_TYPE::VF, REG_STATE::WRITE);
    REG_64 XMM0 = lalloc_xmm_reg(ee, 0, REG_TYPE::XMMSCRATCHPAD, REG_STATE::SCRATCHPAD);

    emitter.load_addr((uint64_t)&FPU_MASK_ABS[0], REG_64::RAX);
    emitter.MOVAPS_REG(source, XMM0);
    emitter.PAND_XMM_FROM_MEM(REG_64::RAX, XMM0);

    if (instr.get_dest())
        emitter.BLENDPS(field, XMM0, dest);

    free_xmm_reg(ee, XMM0);
}

void EE_JIT64::vadd_vectors(EmotionEngine& ee, IR::Instruction& instr)
{
    prepare_abi((uint64_t)ee.vu0);
    call_abi_func((uint64_t)&vu_flush_pipes);

    uint8_t field = convert_field(instr.get_field());

    REG_64 source = alloc_reg(ee, instr.get_source(), REG_TYPE::VF, REG_STATE::READ);
    REG_64 source2 = alloc_reg(ee, instr.get_source2(), REG_TYPE::VF, REG_STATE::READ);
    REG_64 dest = alloc_reg(ee, instr.get_dest(), REG_TYPE::VF, REG_STATE::WRITE);
    REG_64 XMM0 = lalloc_xmm_reg(ee, 0, REG_TYPE::XMMSCRATCHPAD, REG_STATE::SCRATCHPAD);

    clamp_vfreg(ee, field, source);
    clamp_vfreg(ee, field, source2);

    emitter.MOVAPS_REG(source, XMM0);
    emitter.ADDPS(source2, XMM0);

    set_clamping(XMM0, true, field);
    clamp_vfreg(ee, field, XMM0);

    if (instr.get_dest())
    {
        set_clamping(XMM0, false, field);
        emitter.BLENDPS(field, XMM0, dest);
    }

    update_mac_flags(ee, XMM0, field);
    free_xmm_reg(ee, XMM0);
}

void EE_JIT64::vmul_vectors(EmotionEngine& ee, IR::Instruction& instr)
{
    prepare_abi((uint64_t)ee.vu0);
    call_abi_func((uint64_t)&vu_flush_pipes);

    uint8_t field = convert_field(instr.get_field());

    REG_64 source = alloc_reg(ee, instr.get_source(), REG_TYPE::VF, REG_STATE::READ);
    REG_64 source2 = alloc_reg(ee, instr.get_source2(), REG_TYPE::VF, REG_STATE::READ);
    REG_64 dest = alloc_reg(ee, instr.get_dest(), REG_TYPE::VF, REG_STATE::WRITE);
    REG_64 XMM0 = lalloc_xmm_reg(ee, 0, REG_TYPE::XMMSCRATCHPAD, REG_STATE::SCRATCHPAD);

    clamp_vfreg(ee, field, source);
    clamp_vfreg(ee, field, source2);

    emitter.MOVAPS_REG(source, XMM0);
    emitter.MULPS(source2, XMM0);

    set_clamping(XMM0, true, field);
    clamp_vfreg(ee, field, XMM0);

    if (instr.get_dest())
    {
        set_clamping(XMM0, false, field);
        emitter.BLENDPS(field, XMM0, dest);
    }

    update_mac_flags(ee, XMM0, field);
    free_xmm_reg(ee, XMM0);
}

void EE_JIT64::vsub_vectors(EmotionEngine& ee, IR::Instruction& instr)
{
    prepare_abi((uint64_t)ee.vu0);
    call_abi_func((uint64_t)&vu_flush_pipes);

    uint8_t field = convert_field(instr.get_field());

    REG_64 source = alloc_reg(ee, instr.get_source(), REG_TYPE::VF, REG_STATE::READ);
    REG_64 source2 = alloc_reg(ee, instr.get_source2(), REG_TYPE::VF, REG_STATE::READ);
    REG_64 dest = alloc_reg(ee, instr.get_dest(), REG_TYPE::VF, REG_STATE::WRITE);
    REG_64 XMM0 = lalloc_xmm_reg(ee, 0, REG_TYPE::XMMSCRATCHPAD, REG_STATE::SCRATCHPAD);

    clamp_vfreg(ee, field, source);
    clamp_vfreg(ee, field, source2);

    emitter.MOVAPS_REG(source, XMM0);
    emitter.SUBPS(source2, XMM0);

    set_clamping(XMM0, true, field);
    clamp_vfreg(ee, field, XMM0);

    if (instr.get_dest())
    {
        set_clamping(XMM0, false, field);
        emitter.BLENDPS(field, XMM0, dest);
    }

    update_mac_flags(ee, XMM0, field);
    free_xmm_reg(ee, XMM0);
}