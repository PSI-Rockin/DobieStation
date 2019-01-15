#include <cmath>

#include "vu_jit64.hpp"

#include "../errors.hpp"

/**
 * Calling convention notes (needed for calling C++ functions within generated code)
 *
 * x64 - Two conventions exist: Microsoft ABI and System V AMD64 ABI (followed by POSIX systems).
 *
 * Microsoft:
 * Uses registers RCX, RDX, R8, R9 for the first four integer or pointer arguments (in that order), and
 * XMM0, XMM1, XMM2, XMM3 are used for floating point arguments. Additional arguments are pushed onto the
 * stack (right to left). Integer return values (similar to x86) are returned in RAX if 64 bits or less.
 * Floating point return values are returned in XMM0. Parameters less than 64 bits long are not zero extended;
 * the high bits are not zeroed.
 *
 * System V AMD64:
 * The first six integer or pointer arguments are passed in registers RDI, RSI, RDX, RCX, R8,
 * R9 (R10 is used as a static chain pointer in case of nested functions), while XMM0, XMM1, XMM2, XMM3, XMM4,
 * XMM5, XMM6 and XMM7 are used for certain floating point arguments. As in the Microsoft x64 calling convention,
 * additional arguments are passed on the stack. Integral return values up to 64 bits in size are stored in RAX
 * while values up to 128 bit are stored in RAX and RDX. Floating-point return values are similarly stored in XMM0
 * and XMM1.
 *
 * Sources:
 * https://en.wikipedia.org/wiki/X86_calling_conventions#x86-64_calling_conventions
 */

VU_JIT64::VU_JIT64() : emitter(&cache)
{
    for (int i = 0; i < 4; i++)
    {
        ftoi_table.t[0][i] = pow(2, 0);
        ftoi_table.t[1][i] = pow(2, 4);
        ftoi_table.t[2][i] = pow(2, 12);
        ftoi_table.t[3][i] = pow(2, 15);

        clamp_constant.u[i] = 0xFF7FFFFF;
        abs_constant.u[i] = 0x7FFFFFFF;
    }
}

uint8_t convert_field(uint8_t value)
{
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

void vu_stop_execution(VectorUnit& vu)
{
    vu.stop();
}

void vu_set_int(VectorUnit& vu, int dest, uint16_t value)
{
    vu.set_int(dest, value);
}

void VU_JIT64::reset()
{
    abi_int_count = 0;
    abi_xmm_count = 0;
    for (int i = 0; i < 16; i++)
    {
        xmm_regs[i].used = false;
        xmm_regs[i].locked = false;
        xmm_regs[i].age = 0;

        int_regs[i].used = false;
        int_regs[i].locked = false;
        int_regs[i].age = 0;
    }

    //Lock special registers to prevent them from being used
    int_regs[REG_64::RSP].locked = true;

    //Scratchpad registers
    int_regs[REG_64::RAX].locked = true;
    int_regs[REG_64::R15].locked = true;
    xmm_regs[REG_64::XMM0].locked = true;
    xmm_regs[REG_64::XMM1].locked = true;
    cache.flush_all_blocks();
}

uint64_t VU_JIT64::get_vf_addr(VectorUnit &vu, int index)
{
    if (index < 32)
        return (uint64_t)&vu.gpr[index];

    switch (index)
    {
        case VU_SpecialReg::ACC:
            return (uint64_t)&vu.ACC;
        case VU_SpecialReg::I:
            return (uint64_t)&vu.I;
        case VU_SpecialReg::Q:
            return (uint64_t)&vu.Q;
        default:
            Errors::die("[VU_JIT64] get_vf_addr error: Unrecognized reg %d", index);
    }
    return 0;
}

void VU_JIT64::handle_cond_branch(VectorUnit& vu)
{
    //Load the "branch happened" variable
    emitter.load_addr((uint64_t)&vu.branch_on, REG_64::RAX);
    emitter.MOV16_FROM_MEM(REG_64::RAX, REG_64::RAX);

    //The branch will point to two locations: one where the PC is set to the branch destination,
    //and another where PC = PC + 16. Our generated code will jump if the branch *fails*.
    //This is to allow for future optimizations where the recompiler may generate code beyond
    //the branch (assuming that the branch fails).
    emitter.TEST16_REG(REG_64::RAX, REG_64::RAX);

    //Because we don't know where the branch will jump to, we defer it.
    //Once the "branch succeeded" case has been finished recompiling, we can rewrite the branch offset...
    uint8_t* offset_addr = emitter.JE_NEAR_DEFERRED();
    emitter.load_addr((uint64_t)&vu.PC, REG_64::RAX);
    emitter.MOV16_IMM_MEM(cond_branch_dest, REG_64::RAX);
    cleanup_recompiler(vu, false);

    //...Which we do here.
    uint8_t* jump_dest_addr = cache.get_current_block_pos();
    cache.set_current_block_pos(offset_addr);
    int jump_offset = jump_dest_addr - offset_addr - 4;
    cache.write<uint32_t>(jump_offset);

    //And here we recompile the "branch failed" case.
    cache.set_current_block_pos(jump_dest_addr);
    emitter.load_addr((uint64_t)&vu.PC, REG_64::RAX);
    emitter.MOV16_IMM_MEM(cond_branch_fail_dest, REG_64::RAX);
    cleanup_recompiler(vu, true);
}

void VU_JIT64::update_mac_flags(VectorUnit &vu, int vf_reg, uint8_t field)
{
    REG_64 xmm_source = alloc_sse_reg(vu, vf_reg, REG_STATE::READ);
    REG_64 temp = REG_64::XMM0;

    emitter.load_addr((uint64_t)vu.MAC_flags, REG_64::R15);
    emitter.MOV32_FROM_MEM(REG_64::R15, REG_64::RAX);

    //Clear the MAC flags
    emitter.AND32_REG_IMM(0, REG_64::RAX);

    //Shuffle the vector from WZYX to XYZW
    emitter.MOVAPS_REG(xmm_source, temp);
    emitter.PSHUFD(0x1B, temp, temp);

    //Get the sign bits
    emitter.MOVMSKPS(temp, REG_64::RAX);
    emitter.SHL32_REG_IMM(4, REG_64::RAX);

    emitter.MOV32_TO_MEM(REG_64::RAX, REG_64::R15);
}

void VU_JIT64::load_const(VectorUnit &vu, IR::Instruction &instr)
{
    REG_64 dest = alloc_int_reg(vu, instr.get_dest(), REG_STATE::WRITE);
    emitter.MOV16_REG_IMM(instr.get_source(), dest);
}

void VU_JIT64::load_float_const(VectorUnit &vu, IR::Instruction &instr)
{
    if (instr.get_dest() < 33)
        Errors::die("[VU_JIT64] Unrecognized vf%d in load_float_const", instr.get_dest());

    //Load 32-bit constant into a special register
    REG_64 dest = alloc_sse_reg(vu, instr.get_dest(), REG_STATE::WRITE);
    emitter.MOV64_OI(instr.get_source(), REG_64::RAX);
    emitter.MOVD_TO_XMM(REG_64::RAX, dest);
}

void VU_JIT64::load_int(VectorUnit &vu, IR::Instruction &instr)
{
    uint8_t field = convert_field(instr.get_field());

    int field_offset = 0;
    if (field & 0x2)
        field_offset = 4;
    if (field & 0x4)
        field_offset = 8;
    if (field & 0x8)
        field_offset = 12;

    if (instr.get_base())
    {
        //IMPORTANT
        //Dest must be allocated after base in case base == dest (this is so that the base address can load)
        //Errors::die("$%02X", field);
        REG_64 base = alloc_int_reg(vu, instr.get_base(), REG_STATE::READ);
        REG_64 dest = alloc_int_reg(vu, instr.get_dest(), REG_STATE::WRITE);
        emitter.MOVZX64_REG(base, REG_64::RAX);
        emitter.SHL16_REG_IMM(4, REG_64::RAX);

        if (instr.get_source())
            emitter.ADD16_REG_IMM(instr.get_source(), REG_64::RAX);
        emitter.AND16_AX(vu.mem_mask);

        emitter.load_addr((uint64_t)&vu.data_mem[field_offset], REG_64::R15);
        emitter.ADD64_REG(REG_64::RAX, REG_64::R15);
        emitter.MOV16_FROM_MEM(REG_64::R15, dest);
    }
    else
    {
        REG_64 dest = alloc_int_reg(vu, instr.get_dest(), REG_STATE::WRITE);
        uint16_t offset = (instr.get_source() + field_offset) & 0x3FFF;
        emitter.load_addr((uint64_t)&vu.data_mem[offset], REG_64::R15);
        emitter.MOV16_FROM_MEM(REG_64::R15, dest);
    }
}

void VU_JIT64::store_int(VectorUnit &vu, IR::Instruction &instr)
{
    uint8_t field = convert_field(instr.get_field());

    int field_offset = 0;
    if (field & 0x2)
        field_offset = 4;
    if (field & 0x4)
        field_offset = 8;
    if (field & 0x8)
        field_offset = 12;

    if (instr.get_base())
    {
        //IMPORTANT
        //Dest must be allocated after base in case base == dest (this is so that the base address can load)
        //Errors::die("$%02X", field);
        REG_64 base = alloc_int_reg(vu, instr.get_base(), REG_STATE::READ);
        REG_64 source = alloc_int_reg(vu, instr.get_source(), REG_STATE::READ);
        emitter.MOVZX64_REG(base, REG_64::RAX);
        emitter.SHL16_REG_IMM(4, REG_64::RAX);

        if (instr.get_source2())
            emitter.ADD16_REG_IMM(instr.get_source2(), REG_64::RAX);
        emitter.AND16_AX(vu.mem_mask);

        emitter.load_addr((uint64_t)&vu.data_mem[field_offset], REG_64::R15);
        emitter.ADD64_REG(REG_64::RAX, REG_64::R15);
        emitter.MOV16_TO_MEM(source, REG_64::R15);
    }
    else
    {
        REG_64 source = alloc_int_reg(vu, instr.get_source(), REG_STATE::READ);
        uint16_t offset = (instr.get_source2() + field_offset) & 0x3FFF;
        emitter.load_addr((uint64_t)&vu.data_mem[offset], REG_64::R15);
        emitter.MOV16_TO_MEM(source, REG_64::R15);
    }
}

void VU_JIT64::load_quad(VectorUnit &vu, IR::Instruction &instr)
{
    uint8_t field = convert_field(instr.get_field());

    if (instr.get_base())
    {
        REG_64 base = alloc_int_reg(vu, instr.get_base(), REG_STATE::READ);
        emitter.MOVZX64_REG(base, REG_64::RAX);
        emitter.SHL16_REG_IMM(4, REG_64::RAX);
        if (instr.get_source())
            emitter.ADD16_REG_IMM(instr.get_source(), REG_64::RAX);

        emitter.AND16_AX(vu.mem_mask);

        emitter.load_addr((uint64_t)&vu.data_mem, REG_64::R15);
        emitter.ADD64_REG(REG_64::RAX, REG_64::R15);
    }
    else
    {
        uint16_t offset = instr.get_source() & 0x3FFF;
        emitter.load_addr((uint64_t)&vu.data_mem[offset], REG_64::R15);
    }

    if (field == 0xF)
    {
        REG_64 dest = alloc_sse_reg(vu, instr.get_dest(), REG_STATE::WRITE);
        emitter.MOVAPS_FROM_MEM(REG_64::R15, dest);
    }
    else
    {
        REG_64 temp = REG_64::XMM0;
        REG_64 dest = alloc_sse_reg(vu, instr.get_dest(), REG_STATE::READ_WRITE);
        emitter.MOVAPS_FROM_MEM(REG_64::R15, temp);
        emitter.BLENDPS(field, temp, dest);
    }
}

void VU_JIT64::load_quad_inc(VectorUnit &vu, IR::Instruction &instr)
{
    uint8_t field = convert_field(instr.get_field());
    REG_64 base = alloc_int_reg(vu, instr.get_base(), REG_STATE::READ_WRITE);

    //addr = (int_reg * 16) & mem_mask
    emitter.MOVZX64_REG(base, REG_64::RAX);
    emitter.SHL16_REG_IMM(4, REG_64::RAX);
    emitter.AND16_AX(vu.mem_mask);

    //vf_reg.field = *(_XMM*)&vu.data_mem[addr]
    emitter.load_addr((uint64_t)&vu.data_mem, REG_64::R15);
    emitter.ADD64_REG(REG_64::RAX, REG_64::R15);

    if (field == 0xF)
    {
        REG_64 dest = alloc_sse_reg(vu, instr.get_dest(), REG_STATE::WRITE);
        emitter.MOVAPS_FROM_MEM(REG_64::R15, dest);
    }
    else
    {
        REG_64 temp = REG_64::XMM0;
        REG_64 dest = alloc_sse_reg(vu, instr.get_dest(), REG_STATE::READ_WRITE);
        emitter.MOVAPS_FROM_MEM(REG_64::R15, temp);
        emitter.BLENDPS(field, temp, dest);
    }

    if (instr.get_base())
        emitter.INC16(base);
}

void VU_JIT64::store_quad_inc(VectorUnit &vu, IR::Instruction &instr)
{
    uint8_t field = convert_field(instr.get_field());
    REG_64 temp = REG_64::XMM0;
    REG_64 source = alloc_sse_reg(vu, instr.get_source(), REG_STATE::READ_WRITE);
    REG_64 base = alloc_int_reg(vu, instr.get_base(), REG_STATE::READ_WRITE);

    //addr = (int_reg * 16) & mem_mask
    emitter.MOVZX64_REG(base, REG_64::RAX);
    emitter.SHL16_REG_IMM(4, REG_64::RAX);
    emitter.AND16_AX(vu.mem_mask);

    emitter.load_addr((uint64_t)&vu.data_mem, REG_64::R15);
    emitter.ADD64_REG(REG_64::RAX, REG_64::R15);

    if (field == 0xF)
    {
        emitter.MOVAPS_TO_MEM(source, REG_64::R15);
    }
    else
    {
        emitter.MOVAPS_FROM_MEM(REG_64::R15, temp);
        field = 15 - field;
        emitter.BLENDPS(field, source, temp);
        emitter.MOVAPS_TO_MEM(temp, REG_64::R15);
    }

    if (instr.get_base())
        emitter.INC16(base);
}

void VU_JIT64::move_int_reg(VectorUnit &vu, IR::Instruction &instr)
{
    REG_64 dest = alloc_int_reg(vu, instr.get_dest(), REG_STATE::WRITE);
    REG_64 source = alloc_int_reg(vu, instr.get_source(), REG_STATE::READ);
    emitter.MOV16_REG(source, dest);
}

void VU_JIT64::jump(VectorUnit& vu, IR::Instruction& instr)
{
    //We just need to set the PC.
    emitter.load_addr((uint64_t)&vu.PC, REG_64::RAX);
    emitter.MOV16_IMM_MEM(instr.get_jump_dest(), REG_64::RAX);
}

void VU_JIT64::jump_and_link(VectorUnit& vu, IR::Instruction& instr)
{
    //First set the PC
    emitter.load_addr((uint64_t)&vu.PC, REG_64::RAX);
    emitter.MOV16_IMM_MEM(instr.get_jump_dest(), REG_64::RAX);

    //Then set the link register
    REG_64 link = alloc_int_reg(vu, instr.get_dest(), REG_STATE::WRITE);
    emitter.MOV16_REG_IMM(instr.get_return_addr(), link);
}

void VU_JIT64::jump_indirect(VectorUnit &vu, IR::Instruction &instr)
{
    REG_64 return_reg = alloc_int_reg(vu, instr.get_source(), REG_STATE::READ);

    emitter.MOV16_REG(return_reg, REG_64::RAX);

    //Multiply the address by eight
    emitter.SHL16_REG_IMM(3, REG_64::RAX);
    emitter.AND16_AX(vu.mem_mask);

    emitter.load_addr((uint64_t)&vu.PC, REG_64::R15);
    emitter.MOV16_TO_MEM(REG_64::RAX, REG_64::R15);
}

void VU_JIT64::branch_equal(VectorUnit &vu, IR::Instruction &instr)
{
    REG_64 op1 = alloc_int_reg(vu, instr.get_source(), REG_STATE::READ);
    REG_64 op2 = alloc_int_reg(vu, instr.get_source2(), REG_STATE::READ);

    emitter.load_addr((uint64_t)&vu.branch_on, REG_64::RAX);
    emitter.CMP16_REG(op2, op1);
    emitter.SETE_MEM(REG_64::RAX);

    cond_branch = true;
    cond_branch_dest = instr.get_jump_dest();
    cond_branch_fail_dest = instr.get_jump_fail_dest();
}

void VU_JIT64::branch_not_equal(VectorUnit &vu, IR::Instruction &instr)
{
    REG_64 op1 = alloc_int_reg(vu, instr.get_source(), REG_STATE::READ);
    REG_64 op2 = alloc_int_reg(vu, instr.get_source2(), REG_STATE::READ);

    emitter.load_addr((uint64_t)&vu.branch_on, REG_64::RAX);
    emitter.CMP16_REG(op2, op1);
    emitter.SETNE_MEM(REG_64::RAX);

    cond_branch = true;
    cond_branch_dest = instr.get_jump_dest();
    cond_branch_fail_dest = instr.get_jump_fail_dest();
}

void VU_JIT64::branch_greater_or_equal_than_zero(VectorUnit &vu, IR::Instruction &instr)
{
    REG_64 op = alloc_int_reg(vu, instr.get_source(), REG_STATE::READ);

    emitter.load_addr((uint64_t)&vu.branch_on, REG_64::RAX);
    emitter.CMP16_IMM(0, op);
    emitter.SETGE_MEM(REG_64::RAX);

    cond_branch = true;
    cond_branch_dest = instr.get_jump_dest();
    cond_branch_fail_dest = instr.get_jump_fail_dest();
}

void VU_JIT64::or_int(VectorUnit &vu, IR::Instruction &instr)
{
    //If RD == RS, RD |= RT
    if (instr.get_dest() == instr.get_source())
    {
        REG_64 op2 = alloc_int_reg(vu, instr.get_source2(), REG_STATE::READ);
        REG_64 dest = alloc_int_reg(vu, instr.get_dest(), REG_STATE::WRITE);

        emitter.OR16_REG(op2, dest);
    }
    //If RD == RT, RD |= RS
    else if (instr.get_dest() == instr.get_source2())
    {
        REG_64 op1 = alloc_int_reg(vu, instr.get_source(), REG_STATE::READ);
        REG_64 dest = alloc_int_reg(vu, instr.get_dest(), REG_STATE::WRITE);

        emitter.OR16_REG(op1, dest);
    }
    else
    {
        REG_64 op1 = alloc_int_reg(vu, instr.get_source(), REG_STATE::READ);
        REG_64 op2 = alloc_int_reg(vu, instr.get_source2(), REG_STATE::READ);
        REG_64 dest = alloc_int_reg(vu, instr.get_dest(), REG_STATE::WRITE);
        emitter.MOV16_REG(op1, dest);
        emitter.OR16_REG(op2, dest);
    }
}

void VU_JIT64::add_int_reg(VectorUnit &vu, IR::Instruction &instr)
{
    //If RD == RS, RD += RT
    if (instr.get_dest() == instr.get_source())
    {
        //IF RD == RS == RT, RD <<= 1
        if (instr.get_dest() == instr.get_source2())
        {
            REG_64 dest = alloc_int_reg(vu, instr.get_dest(), REG_STATE::READ_WRITE);
            emitter.SHL16_REG_1(dest);
        }
        else
        {
            REG_64 dest = alloc_int_reg(vu, instr.get_dest(), REG_STATE::WRITE);
            REG_64 op2 = alloc_int_reg(vu, instr.get_source2(), REG_STATE::READ);

            emitter.ADD16_REG(op2, dest);
        }
    }
    //If RD == RT, RD += RS
    else if (instr.get_dest() == instr.get_source2())
    {
        REG_64 dest = alloc_int_reg(vu, instr.get_dest(), REG_STATE::WRITE);
        REG_64 op1 = alloc_int_reg(vu, instr.get_source(), REG_STATE::READ);

        emitter.ADD16_REG(op1, dest);
    }
    //RD = RS + RT
    else
    {
        REG_64 dest = alloc_int_reg(vu, instr.get_dest(), REG_STATE::WRITE);
        REG_64 op1 = alloc_int_reg(vu, instr.get_source(), REG_STATE::READ);
        REG_64 op2 = alloc_int_reg(vu, instr.get_source2(), REG_STATE::READ);

        emitter.MOV16_REG(op1, dest);
        emitter.ADD16_REG(op2, dest);
    }
}

void VU_JIT64::add_unsigned_imm(VectorUnit &vu, IR::Instruction &instr)
{
    uint16_t imm = instr.get_source2();
    if (instr.get_dest() == instr.get_source())
    {
        REG_64 dest = alloc_int_reg(vu, instr.get_dest(), REG_STATE::READ_WRITE);
        emitter.ADD16_REG_IMM(imm, dest);
    }
    else
    {
        REG_64 dest = alloc_int_reg(vu, instr.get_dest(), REG_STATE::WRITE);
        REG_64 source = alloc_int_reg(vu, instr.get_source(), REG_STATE::READ);

        emitter.MOV16_REG(source, dest);
        emitter.ADD16_REG_IMM(imm, dest);
    }
}

void VU_JIT64::abs(VectorUnit &vu, IR::Instruction &instr)
{
    uint8_t field = convert_field(instr.get_field());
    REG_64 source = alloc_sse_reg(vu, instr.get_source(), REG_STATE::READ_WRITE);
    REG_64 dest = alloc_sse_reg(vu, instr.get_dest(), REG_STATE::READ_WRITE);

    REG_64 temp = REG_64::XMM0;

    //xmm_dest = xmm_source & 0x7FFF_FFFF
    emitter.load_addr((uint64_t)&abs_constant, REG_64::RAX);
    emitter.MOVAPS_FROM_MEM(REG_64::RAX, temp);
    emitter.PAND_XMM(source, temp);
    emitter.BLENDPS(field, temp, dest);
}

void VU_JIT64::max_vector_by_scalar(VectorUnit &vu, IR::Instruction &instr)
{
    uint8_t field = convert_field(instr.get_field());
    REG_64 source = alloc_sse_reg(vu, instr.get_source(), REG_STATE::READ_WRITE);
    REG_64 bc_reg = alloc_sse_reg(vu, instr.get_source2(), REG_STATE::READ_WRITE);
    REG_64 dest = alloc_sse_reg(vu, instr.get_dest(), REG_STATE::READ_WRITE);

    uint8_t bc = instr.get_bc();
    bc |= (bc << 6) | (bc << 4) | (bc << 2);

    REG_64 temp = REG_64::XMM0;
    emitter.MOVAPS_REG(bc_reg, temp);
    emitter.SHUFPS(bc, temp, temp);
    emitter.PMAXSD_XMM(source, temp);
    emitter.BLENDPS(field, temp, dest);
}

void VU_JIT64::add_vectors(VectorUnit &vu, IR::Instruction &instr)
{
    uint8_t field = convert_field(instr.get_field());

    REG_64 op1 = alloc_sse_reg(vu, instr.get_source(), REG_STATE::READ_WRITE);
    REG_64 op2 = alloc_sse_reg(vu, instr.get_source2(), REG_STATE::READ_WRITE);
    REG_64 dest = alloc_sse_reg(vu, instr.get_dest(), REG_STATE::READ_WRITE);
    REG_64 temp = REG_64::XMM0;

    emitter.MOVAPS_REG(op1, temp);
    emitter.ADDPS(op2, temp);
    emitter.BLENDPS(field, temp, dest);
}

void VU_JIT64::add_vector_by_scalar(VectorUnit &vu, IR::Instruction &instr)
{
    uint8_t field = convert_field(instr.get_field());
    REG_64 source = alloc_sse_reg(vu, instr.get_source(), REG_STATE::READ_WRITE);
    REG_64 bc_reg = alloc_sse_reg(vu, instr.get_source2(), REG_STATE::READ_WRITE);
    REG_64 dest = alloc_sse_reg(vu, instr.get_dest(), REG_STATE::READ_WRITE);

    uint8_t bc = instr.get_bc();
    bc |= (bc << 6) | (bc << 4) | (bc << 2);

    REG_64 temp = REG_64::XMM0;
    emitter.MOVAPS_REG(bc_reg, temp);
    emitter.SHUFPS(bc, temp, temp);
    emitter.ADDPS(source, temp);
    emitter.BLENDPS(field, temp, dest);
}

void VU_JIT64::sub_vectors(VectorUnit &vu, IR::Instruction &instr)
{
    uint8_t field = convert_field(instr.get_field());

    REG_64 op1 = alloc_sse_reg(vu, instr.get_source(), REG_STATE::READ_WRITE);
    REG_64 op2 = alloc_sse_reg(vu, instr.get_source2(), REG_STATE::READ_WRITE);
    REG_64 dest = alloc_sse_reg(vu, instr.get_dest(), REG_STATE::READ_WRITE);
    REG_64 temp = REG_64::XMM0;

    emitter.MOVAPS_REG(op1, temp);
    emitter.SUBPS(op2, temp);
    emitter.BLENDPS(field, temp, dest);
}

void VU_JIT64::sub_vector_by_scalar(VectorUnit &vu, IR::Instruction &instr)
{
    uint8_t field = convert_field(instr.get_field());
    REG_64 source = alloc_sse_reg(vu, instr.get_source(), REG_STATE::READ_WRITE);
    REG_64 bc_reg = alloc_sse_reg(vu, instr.get_source2(), REG_STATE::READ_WRITE);
    REG_64 dest = alloc_sse_reg(vu, instr.get_dest(), REG_STATE::READ_WRITE);

    uint8_t bc = instr.get_bc();
    bc |= (bc << 6) | (bc << 4) | (bc << 2);

    REG_64 temp = REG_64::XMM0;
    REG_64 temp2 = REG_64::XMM1;
    emitter.MOVAPS_REG(bc_reg, temp);
    emitter.MOVAPS_REG(source, temp2);
    emitter.SHUFPS(bc, temp, temp);
    emitter.SUBPS(temp, temp2);
    emitter.BLENDPS(field, temp2, dest);

    update_mac_flags(vu, instr.get_dest(), instr.get_field());
}

void VU_JIT64::mul_vectors(VectorUnit &vu, IR::Instruction &instr)
{
    uint8_t field = convert_field(instr.get_field());

    REG_64 op1 = alloc_sse_reg(vu, instr.get_source(), REG_STATE::READ_WRITE);
    REG_64 op2 = alloc_sse_reg(vu, instr.get_source2(), REG_STATE::READ_WRITE);
    REG_64 dest = alloc_sse_reg(vu, instr.get_dest(), REG_STATE::READ_WRITE);
    REG_64 temp = REG_64::XMM0;

    emitter.MOVAPS_REG(op1, temp);
    emitter.MULPS(op2, temp);
    emitter.BLENDPS(field, temp, dest);
}

void VU_JIT64::mul_vector_by_scalar(VectorUnit &vu, IR::Instruction &instr)
{
    uint8_t field = convert_field(instr.get_field());
    REG_64 source = alloc_sse_reg(vu, instr.get_source(), REG_STATE::READ_WRITE);
    REG_64 bc_reg = alloc_sse_reg(vu, instr.get_source2(), REG_STATE::READ_WRITE);
    REG_64 dest = alloc_sse_reg(vu, instr.get_dest(), REG_STATE::READ_WRITE);

    uint8_t bc = instr.get_bc();
    bc |= (bc << 6) | (bc << 4) | (bc << 2);

    REG_64 temp = REG_64::XMM0;
    emitter.MOVAPS_REG(bc_reg, temp);
    emitter.SHUFPS(bc, temp, temp);
    emitter.MULPS(source, temp);
    emitter.BLENDPS(field, temp, dest);
}

void VU_JIT64::madd_vectors(VectorUnit &vu, IR::Instruction &instr)
{
    uint8_t field = convert_field(instr.get_field());

    REG_64 op1 = alloc_sse_reg(vu, instr.get_source(), REG_STATE::READ_WRITE);
    REG_64 op2 = alloc_sse_reg(vu, instr.get_source2(), REG_STATE::READ_WRITE);
    REG_64 acc = alloc_sse_reg(vu, VU_SpecialReg::ACC, REG_STATE::READ_WRITE);
    REG_64 dest = alloc_sse_reg(vu, instr.get_dest(), REG_STATE::READ_WRITE);
    REG_64 temp = REG_64::XMM0;

    emitter.MOVAPS_REG(op1, temp);
    emitter.MULPS(op2, temp);
    emitter.ADDPS(acc, temp);
    emitter.BLENDPS(field, temp, dest);
}

void VU_JIT64::madd_acc_and_vectors(VectorUnit &vu, IR::Instruction &instr)
{
    uint8_t field = convert_field(instr.get_field());

    REG_64 op1 = alloc_sse_reg(vu, instr.get_source(), REG_STATE::READ_WRITE);
    REG_64 op2 = alloc_sse_reg(vu, instr.get_source2(), REG_STATE::READ_WRITE);
    REG_64 dest = alloc_sse_reg(vu, VU_SpecialReg::ACC, REG_STATE::READ_WRITE);
    REG_64 temp = REG_64::XMM0;

    emitter.MOVAPS_REG(op1, temp);
    emitter.MULPS(op2, temp);
    if (field == 0xF)
        emitter.ADDPS(temp, dest);
    else
    {
        emitter.ADDPS(dest, temp);
        emitter.BLENDPS(field, temp, dest);
    }
}

void VU_JIT64::madd_vector_by_scalar(VectorUnit &vu, IR::Instruction &instr)
{
    uint8_t field = convert_field(instr.get_field());
    REG_64 temp = REG_64::XMM0;
    REG_64 source = alloc_sse_reg(vu, instr.get_source(), REG_STATE::READ_WRITE);
    REG_64 bc_reg = alloc_sse_reg(vu, instr.get_source2(), REG_STATE::READ_WRITE);
    REG_64 dest = alloc_sse_reg(vu, instr.get_dest(), REG_STATE::READ_WRITE);
    REG_64 acc = alloc_sse_reg(vu, VU_SpecialReg::ACC, REG_STATE::READ);

    uint8_t bc = instr.get_bc();
    bc |= (bc << 6) | (bc << 4) | (bc << 2);

    emitter.MOVAPS_REG(bc_reg, temp);
    emitter.SHUFPS(bc, temp, temp);
    emitter.MULPS(source, temp);
    emitter.ADDPS(acc, temp);
    emitter.BLENDPS(field, temp, dest);
}

void VU_JIT64::madd_acc_by_scalar(VectorUnit &vu, IR::Instruction &instr)
{
    uint8_t field = convert_field(instr.get_field());

    REG_64 temp = REG_64::XMM0;
    REG_64 bc_reg = alloc_sse_reg(vu, instr.get_source2(), REG_STATE::READ);
    REG_64 source = alloc_sse_reg(vu, instr.get_source(), REG_STATE::READ);
    REG_64 dest = alloc_sse_reg(vu, VU_SpecialReg::ACC, REG_STATE::READ_WRITE);

    uint8_t bc = instr.get_bc();
    bc |= (bc << 6) | (bc << 4) | (bc << 2);

    emitter.MOVAPS_REG(bc_reg, temp);
    emitter.SHUFPS(bc, temp, temp);
    emitter.MULPS(source, temp);

    if (field == 0xF)
        emitter.ADDPS(temp, dest);
    else
    {
        emitter.ADDPS(dest, temp);
        emitter.BLENDPS(field, temp, dest);
    }
}

void VU_JIT64::msub_vector_by_scalar(VectorUnit &vu, IR::Instruction &instr)
{
    uint8_t field = convert_field(instr.get_field());
    REG_64 temp = REG_64::XMM0;
    REG_64 temp2 = REG_64::XMM1;
    REG_64 source = alloc_sse_reg(vu, instr.get_source(), REG_STATE::READ_WRITE);
    REG_64 bc_reg = alloc_sse_reg(vu, instr.get_source2(), REG_STATE::READ_WRITE);
    REG_64 dest = alloc_sse_reg(vu, instr.get_dest(), REG_STATE::READ_WRITE);
    REG_64 acc = alloc_sse_reg(vu, VU_SpecialReg::ACC, REG_STATE::READ);

    uint8_t bc = instr.get_bc();
    bc |= (bc << 6) | (bc << 4) | (bc << 2);

    emitter.MOVAPS_REG(bc_reg, temp);
    emitter.MOVAPS_REG(acc, temp2);

    emitter.SHUFPS(bc, temp, temp);
    emitter.MULPS(source, temp);
    emitter.SUBPS(temp, temp2);
    emitter.BLENDPS(field, temp2, dest);
}

void VU_JIT64::msub_acc_by_scalar(VectorUnit &vu, IR::Instruction &instr)
{
    uint8_t field = convert_field(instr.get_field());

    REG_64 temp = REG_64::XMM0;
    REG_64 temp2 = REG_64::XMM1;
    REG_64 bc_reg = alloc_sse_reg(vu, instr.get_source2(), REG_STATE::READ);
    REG_64 source = alloc_sse_reg(vu, instr.get_source(), REG_STATE::READ);
    REG_64 dest = alloc_sse_reg(vu, VU_SpecialReg::ACC, REG_STATE::READ_WRITE);

    uint8_t bc = instr.get_bc();
    bc |= (bc << 6) | (bc << 4) | (bc << 2);

    emitter.MOVAPS_REG(bc_reg, temp);
    emitter.SHUFPS(bc, temp, temp);
    emitter.MULPS(source, temp);

    if (field == 0xF)
        emitter.SUBPS(temp, dest);
    else
    {
        emitter.MOVAPS_REG(dest, temp2);
        emitter.SUBPS(temp, temp2);
        emitter.BLENDPS(field, temp2, dest);
    }
}

void VU_JIT64::div(VectorUnit &vu, IR::Instruction &instr)
{
    REG_64 num = alloc_sse_reg(vu, instr.get_source(), REG_STATE::READ);
    REG_64 denom = alloc_sse_reg(vu, instr.get_source2(), REG_STATE::READ);
    REG_64 temp = REG_64::XMM0;
    REG_64 temp2 = REG_64::XMM1;

    uint8_t num_field = instr.get_field();
    uint8_t denom_field = instr.get_field2();

    //Insert the float value in num/denom into bits 0-31 in temp/temp2, then zero out bits 32-127
    emitter.INSERTPS(num_field, 0, 0b1110, num, temp);
    emitter.INSERTPS(denom_field, 0, 0b1110, denom, temp2);

    emitter.DIVPS(temp2, temp);

    emitter.load_addr((uint64_t)&vu.new_Q_instance, REG_64::RAX);
    emitter.MOVAPS_TO_MEM(temp, REG_64::RAX);
}

void VU_JIT64::fixed_to_float(VectorUnit &vu, IR::Instruction &instr, int table_entry)
{
    uint8_t field = convert_field(instr.get_field());
    REG_64 source = alloc_sse_reg(vu, instr.get_source(), REG_STATE::READ_WRITE);
    REG_64 dest = alloc_sse_reg(vu, instr.get_dest(), REG_STATE::READ_WRITE);
    REG_64 temp = REG_64::XMM0;

    emitter.MOVAPS_REG(source, temp);

    if (table_entry)
        Errors::die("[VU_JIT64] table_entry not implemented for fixed_to_float");

    if (field == 0xF)
        emitter.CVTDQ2PS(temp, dest);
    else
    {
        emitter.CVTDQ2PS(temp, temp);
        emitter.BLENDPS(field, temp, dest);
    }
}

void VU_JIT64::float_to_fixed(VectorUnit &vu, IR::Instruction &instr, int table_entry)
{
    uint8_t field = convert_field(instr.get_field());
    REG_64 source = alloc_sse_reg(vu, instr.get_source(), REG_STATE::READ_WRITE);
    REG_64 dest = alloc_sse_reg(vu, instr.get_dest(), REG_STATE::READ_WRITE);
    REG_64 temp = REG_64::XMM0;

    emitter.MOVAPS_REG(source, temp);

    if (table_entry)
    {
        REG_64 temp2 = REG_64::XMM1;

        uint64_t addr = (uint64_t)&ftoi_table.t[table_entry][0];

        emitter.load_addr(addr, REG_64::RAX);
        emitter.MOVAPS_FROM_MEM(REG_64::RAX, temp2);

        emitter.MULPS(temp2, temp);
    }

    if (field == 0xF)
        emitter.CVTTPS2DQ(temp, dest);
    else
    {
        emitter.CVTTPS2DQ(temp, temp);
        emitter.BLENDPS(field, temp, dest);
    }
}

void VU_JIT64::move_to_int(VectorUnit &vu, IR::Instruction &instr)
{
    uint8_t field = instr.get_field();
    REG_64 source = alloc_sse_reg(vu, instr.get_source(), REG_STATE::READ);
    REG_64 dest = alloc_int_reg(vu, instr.get_dest(), REG_STATE::WRITE);

    if (field == 0)
        emitter.MOVD_FROM_XMM(source, dest);
    else
    {
        REG_64 temp = REG_64::XMM0;
        emitter.INSERTPS(field, 0, 0b1110, source, temp);
        emitter.MOVD_FROM_XMM(temp, dest);
    }

    //Unsure if this is necessary
    emitter.AND32_REG_IMM(0xFFFF, dest);
}

void VU_JIT64::mac_and(VectorUnit &vu, IR::Instruction &instr)
{
    *vu.MAC_flags = 0x80;
    emitter.load_addr((uint64_t)vu.MAC_flags, REG_64::RAX);
    emitter.MOV16_FROM_MEM(REG_64::RAX, REG_64::RAX);

    if (instr.get_dest() == instr.get_source())
    {
        REG_64 dest = alloc_int_reg(vu, instr.get_dest(), REG_STATE::READ_WRITE);
        emitter.AND16_REG(REG_64::RAX, dest);
    }
    else
    {
        REG_64 source = alloc_int_reg(vu, instr.get_source(), REG_STATE::READ);
        REG_64 dest = alloc_int_reg(vu, instr.get_dest(), REG_STATE::WRITE);

        emitter.AND16_REG(REG_64::RAX, source);
        emitter.MOV16_REG(source, dest);
    }
}

void VU_JIT64::set_clip_flags(VectorUnit &vu, IR::Instruction &instr)
{
    emitter.load_addr((uint64_t)&vu.clip_flags, REG_64::RAX);
    emitter.MOV32_IMM_MEM(instr.get_source(), REG_64::RAX);
}

void VU_JIT64::stall_q_pipeline(VectorUnit &vu, IR::Instruction &instr)
{
    //Store the pipelined Q inside the Q available to the program
    emitter.load_addr((uint64_t)&vu.new_Q_instance, REG_64::RAX);
    emitter.MOV32_FROM_MEM(REG_64::RAX, REG_64::R15);

    REG_64 q_reg = alloc_sse_reg(vu, VU_SpecialReg::Q, REG_STATE::WRITE);
    emitter.MOVD_TO_XMM(REG_64::R15, q_reg);
}

void VU_JIT64::move_xtop(VectorUnit &vu, IR::Instruction &instr)
{
    REG_64 dest = alloc_int_reg(vu, instr.get_dest(), REG_STATE::WRITE);

    emitter.load_addr((uint64_t)vu.VIF_TOP, REG_64::RAX);
    emitter.MOV16_FROM_MEM(REG_64::RAX, dest);
}

void VU_JIT64::xgkick(VectorUnit &vu, IR::Instruction &instr)
{
    emitter.load_addr((uint64_t)&vu.transferring_GIF, REG_64::RAX);
    emitter.MOV8_IMM_MEM(1, REG_64::RAX);

    REG_64 base = alloc_int_reg(vu, instr.get_base(), REG_STATE::READ);
    emitter.MOV16_REG(base, REG_64::RAX);
    emitter.SHL16_REG_IMM(4, REG_64::RAX);
    emitter.AND16_AX(vu.mem_mask);

    emitter.load_addr((uint64_t)&vu.GIF_addr, REG_64::R15);
    emitter.MOV16_TO_MEM(REG_64::RAX, REG_64::R15);
}

void VU_JIT64::stop(VectorUnit &vu, IR::Instruction &instr)
{
    prepare_abi(vu, (uint64_t)&vu);
    call_abi_func((uint64_t)vu_stop_execution);

    emitter.load_addr((uint64_t)&vu.PC, REG_64::RAX);
    emitter.MOV16_IMM_MEM(instr.get_jump_dest(), REG_64::RAX);
}

REG_64 VU_JIT64::alloc_int_reg(VectorUnit &vu, int vi_reg, REG_STATE state)
{
    if (vi_reg >= 16)
        Errors::die("[VU_JIT64] Alloc Int error: vi_reg == %d", vi_reg);

    for (int i = 0; i < 16; i++)
    {
        if (int_regs[i].used && int_regs[i].vu_reg == vi_reg)
        {
            if (state != REG_STATE::READ)
                int_regs[i].modified = true;
            int_regs[i].age = 0;
            return (REG_64)i;
        }
    }

    for (int i = 0; i < 16; i++)
    {
        if (int_regs[i].used)
            int_regs[i].age++;
    }

    int reg = 0;
    int age = 0;
    for (int i = 0; i < 16; i++)
    {
        if (int_regs[i].locked)
            continue;

        if (!int_regs[i].used)
        {
            reg = i;
            break;
        }

        if (int_regs[i].age > age)
        {
            reg = i;
            age = int_regs[i].age;
        }
    }

    if (int_regs[reg].used && int_regs[reg].modified && int_regs[reg].vu_reg)
    {
        printf("[VU_JIT64] Flushing int reg %d!\n", reg);
        int old_vi_reg = int_regs[reg].vu_reg;
        emitter.load_addr((uint64_t)&vu.int_gpr[old_vi_reg], REG_64::RAX);
        emitter.MOV64_TO_MEM((REG_64)reg, REG_64::RAX);
    }

    printf("[VU_JIT64] Allocating int reg %d (vi%d)\n", reg, vi_reg);

    if (state != REG_STATE::WRITE)
    {
        emitter.load_addr((uint64_t)&vu.int_gpr[vi_reg], REG_64::RAX);
        emitter.MOV64_FROM_MEM(REG_64::RAX, (REG_64)reg);
    }

    int_regs[reg].modified = state != REG_STATE::READ;

    int_regs[reg].vu_reg = vi_reg;
    int_regs[reg].used = true;
    int_regs[reg].age = 0;

    return (REG_64)reg;
}

REG_64 VU_JIT64::alloc_sse_reg(VectorUnit &vu, int vf_reg, REG_STATE state)
{
    //If the register is already used, return it
    for (int i = 0; i < 16; i++)
    {
        if (xmm_regs[i].used && xmm_regs[i].vu_reg == vf_reg)
        {
            if (state != REG_STATE::READ)
                xmm_regs[i].modified = true;
            xmm_regs[i].age = 0;
            return (REG_64)i;
        }
    }

    //Increase the age of each register if it's still allocated
    for (int i = 0; i < 16; i++)
    {
        if (xmm_regs[i].used)
            xmm_regs[i].age++;
    }

    //Find a new register. If no free register is available, replace the oldest register.
    int xmm = 0;
    int age = 0;
    for (int i = 0; i < 16; i++)
    {
        if (xmm_regs[i].locked)
            continue;

        if (!xmm_regs[i].used)
        {
            xmm = i;
            break;
        }

        if (xmm_regs[i].age > age)
        {
            xmm = i;
            age = xmm_regs[i].age;
        }
    }

    //If the chosen register is used, flush it back to the VU state.
    if (xmm_regs[xmm].used && xmm_regs[xmm].modified && xmm_regs[xmm].vu_reg)
    {
        printf("[VU_JIT64] Flushing xmm reg %d! (vf%d)\n", xmm, xmm_regs[xmm].vu_reg);
        int old_vf_reg = xmm_regs[xmm].vu_reg;
        emitter.load_addr(get_vf_addr(vu, old_vf_reg), REG_64::RAX);
        emitter.MOVAPS_TO_MEM((REG_64)xmm, REG_64::RAX);
    }

    printf("[VU_JIT64] Allocating xmm reg %d (vf%d)\n", xmm, vf_reg);

    if (state != REG_STATE::WRITE)
    {
        //Store the VU state register inside the newly allocated XMM register.
        emitter.load_addr(get_vf_addr(vu, vf_reg), REG_64::RAX);
        emitter.MOVAPS_FROM_MEM(REG_64::RAX, (REG_64)xmm);
    }

    xmm_regs[xmm].modified = state != REG_STATE::READ;

    xmm_regs[xmm].vu_reg = vf_reg;
    xmm_regs[xmm].used = true;
    xmm_regs[xmm].age = 0;

    return (REG_64)xmm;
}

void VU_JIT64::flush_regs(VectorUnit &vu)
{
    //Store the contents of all allocated x64 registers into the VU state.
    for (int i = 0; i < 16; i++)
    {
        int vf_reg = xmm_regs[i].vu_reg;
        int vi_reg = int_regs[i].vu_reg;
        if (xmm_regs[i].used && vf_reg && xmm_regs[i].modified)
        {
            emitter.load_addr(get_vf_addr(vu, vf_reg), REG_64::RAX);
            emitter.MOVAPS_TO_MEM((REG_64)i, REG_64::RAX);
        }

        if (int_regs[i].used && vi_reg && int_regs[i].modified)
        {
            emitter.load_addr((uint64_t)&vu.int_gpr[vi_reg], REG_64::RAX);
            emitter.MOV16_TO_MEM((REG_64)i, REG_64::RAX);
        }
    }
}

void VU_JIT64::flush_sse_reg(VectorUnit &vu, int vf_reg)
{
    for (int i = 0; i < 16; i++)
    {
        if (xmm_regs[i].used && xmm_regs[i].vu_reg == vf_reg)
        {
            emitter.load_addr(get_vf_addr(vu, vf_reg), REG_64::RAX);
            emitter.MOVAPS_TO_MEM((REG_64)i, REG_64::RAX);
        }
    }
}

void VU_JIT64::emit_instruction(VectorUnit &vu, IR::Instruction &instr)
{
    switch (instr.op)
    {
        case IR::Opcode::LoadConst:
            load_const(vu, instr);
            break;
        case IR::Opcode::LoadFloatConst:
            load_float_const(vu, instr);
            break;
        case IR::Opcode::LoadInt:
            load_int(vu, instr);
            break;
        case IR::Opcode::StoreInt:
            store_int(vu, instr);
            break;
        case IR::Opcode::LoadQuad:
            load_quad(vu, instr);
            break;
        case IR::Opcode::LoadQuadInc:
            load_quad_inc(vu, instr);
            break;
        case IR::Opcode::StoreQuadInc:
            store_quad_inc(vu, instr);
            break;
        case IR::Opcode::MoveIntReg:
            move_int_reg(vu, instr);
            break;
        case IR::Opcode::Jump:
            jump(vu, instr);
            break;
        case IR::Opcode::JumpAndLink:
            jump_and_link(vu, instr);
            break;
        case IR::Opcode::JumpIndirect:
            jump_indirect(vu, instr);
            break;
        case IR::Opcode::BranchEqual:
            branch_equal(vu, instr);
            break;
        case IR::Opcode::BranchNotEqual:
            branch_not_equal(vu, instr);
            break;
        case IR::Opcode::BranchGreaterOrEqualThanZero:
            branch_greater_or_equal_than_zero(vu, instr);
            break;
        case IR::Opcode::VAbs:
            abs(vu, instr);
            break;
        case IR::Opcode::VMaxVectorByScalar:
            max_vector_by_scalar(vu, instr);
            break;
        case IR::Opcode::VAddVectors:
            add_vectors(vu, instr);
            break;
        case IR::Opcode::VAddVectorByScalar:
            add_vector_by_scalar(vu, instr);
            break;
        case IR::Opcode::VSubVectors:
            sub_vectors(vu, instr);
            break;
        case IR::Opcode::VSubVectorByScalar:
            sub_vector_by_scalar(vu, instr);
            break;
        case IR::Opcode::VMulVectors:
            mul_vectors(vu, instr);
            break;
        case IR::Opcode::VMulVectorByScalar:
            mul_vector_by_scalar(vu, instr);
            break;
        case IR::Opcode::VMaddVectors:
            madd_vectors(vu, instr);
            break;
        case IR::Opcode::VMaddAccAndVectors:
            madd_acc_and_vectors(vu, instr);
            break;
        case IR::Opcode::VMaddVectorByScalar:
            madd_vector_by_scalar(vu, instr);
            break;
        case IR::Opcode::VMaddAccByScalar:
            madd_acc_by_scalar(vu, instr);
            break;
        case IR::Opcode::VMsubVectorByScalar:
            msub_vector_by_scalar(vu, instr);
            break;
        case IR::Opcode::VMsubAccByScalar:
            msub_acc_by_scalar(vu, instr);
            break;
        case IR::Opcode::VDiv:
            div(vu, instr);
            break;
        case IR::Opcode::VFixedToFloat0:
            fixed_to_float(vu, instr, 0);
            break;
        case IR::Opcode::VFloatToFixed0:
            float_to_fixed(vu, instr, 0);
            break;
        case IR::Opcode::VFloatToFixed4:
            float_to_fixed(vu, instr, 1);
            break;
        case IR::Opcode::OrInt:
            or_int(vu, instr);
            break;
        case IR::Opcode::AddIntReg:
            add_int_reg(vu, instr);
            break;
        case IR::Opcode::AddUnsignedImm:
            add_unsigned_imm(vu, instr);
            break;
        case IR::Opcode::VMoveToInt:
            move_to_int(vu, instr);
            break;
        case IR::Opcode::VMacAnd:
            mac_and(vu, instr);
            break;
        case IR::Opcode::SetClipFlags:
            set_clip_flags(vu, instr);
            break;
        case IR::Opcode::StallQPipeline:
            stall_q_pipeline(vu, instr);
            break;
        case IR::Opcode::MoveXTOP:
            move_xtop(vu, instr);
            break;
        case IR::Opcode::Xgkick:
            xgkick(vu, instr);
            break;
        case IR::Opcode::Stop:
            stop(vu, instr);
            break;
        default:
            Errors::die("[VU_JIT64] Unknown IR instruction");
    }
}

void VU_JIT64::recompile_block(VectorUnit& vu, IR::Block& block)
{
    cache.alloc_block(vu.get_PC());

    cond_branch = false;
    cycle_count = block.get_cycle_count();

    //Prologue
    emitter.PUSH(REG_64::RBP);
    emitter.MOV64_MR(REG_64::RSP, REG_64::RBP);

    while (block.get_instruction_count() > 0)
    {
        IR::Instruction instr = block.get_next_instr();
        emit_instruction(vu, instr);
    }

    if (cond_branch)
        handle_cond_branch(vu);
    else
        cleanup_recompiler(vu, true);

    //Switch the block's privileges from RW to RX.
    cache.set_current_block_rx();
    cache.print_current_block();
    cache.print_literal_pool();
}

uint8_t* VU_JIT64::exec_block(VectorUnit& vu)
{
    //printf("[VU_JIT64] Executing block at $%04X\n", vu.PC);
    if (cache.find_block(vu.PC) == -1)
    {
        printf("[VU_JIT64] Block not found at $%04X: recompiling\n", vu.PC);
        IR::Block block = ir.translate(vu.PC, vu.get_instr_mem());
        recompile_block(vu, block);
    }
    //if (vu.PC == 0x2A0)
        //Errors::die("hi");
    return cache.get_current_block_start();
}

void VU_JIT64::cleanup_recompiler(VectorUnit& vu, bool clear_regs)
{
    flush_regs(vu);

    if (clear_regs)
    {
        for (int i = 0; i < 16; i++)
        {
            int_regs[i].age = 0;
            int_regs[i].used = false;
            xmm_regs[i].age = 0;
            xmm_regs[i].used = false;
        }
    }

    //Return the amount of cycles to update the VUs with
    emitter.MOV16_REG_IMM(cycle_count, REG_64::RAX);

    //Epilogue
    emitter.POP(REG_64::RBP);
    emitter.RET();
}

void VU_JIT64::prepare_abi(VectorUnit& vu, uint64_t value)
{
    const static REG_64 regs[] = {RDI, RSI, RDX, RCX, R8, R9};

    if (abi_int_count >= 6)
        Errors::die("[VU_JIT64] ABI integer arguments exceeded 6!");

    REG_64 arg = regs[abi_int_count];

    //If the chosen integer argument is being used, flush it back to the VU state
    if (int_regs[arg].used)
    {
        int vi_reg = int_regs[arg].vu_reg;
        emitter.load_addr((uint64_t)&vu.int_gpr[vi_reg], REG_64::RAX);
        emitter.MOV64_TO_MEM(arg, REG_64::RAX);
        int_regs[arg].used = false;
        int_regs[arg].age = 0;
    }
    emitter.load_addr(value, regs[abi_int_count]);
    abi_int_count++;
}

void VU_JIT64::call_abi_func(uint64_t addr)
{
    emitter.CALL(addr);
    abi_int_count = 0;
    abi_xmm_count = 0;
}

int VU_JIT64::run(VectorUnit& vu)
{
    __asm__ (
        "pushq %rbx\n"
        "pushq %r12\n"
        "pushq %r13\n"
        "pushq %r14\n"
        "pushq %r15\n"
        "pushq %rdi\n"

        "callq __ZN8VU_JIT6410exec_blockER10VectorUnit\n"
        "callq *%rax\n"

        "popq %rdi\n"
        "popq %r15\n"
        "popq %r14\n"
        "popq %r13\n"
        "popq %r12\n"
        "popq %rbx\n"
    );
}
