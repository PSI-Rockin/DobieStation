#include <cmath>
#include <algorithm>

#include "vu_jit64.hpp"
#include "vu_interpreter.hpp"
#include "../gif.hpp"

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

VU_JIT64::VU_JIT64() : jit_block("VU"), emitter(&jit_block)
{
    prologue_block = nullptr;
    for (int i = 0; i < 4; i++)
    {
        ftoi_table[0].f[i] = pow(2, 0);
        ftoi_table[1].f[i] = pow(2, 4);
        ftoi_table[2].f[i] = pow(2, 12);
        ftoi_table[3].f[i] = pow(2, 15);

        itof_table[0].f[i] = 1.0f / ftoi_table[0].f[i];
        itof_table[1].f[i] = 1.0f / ftoi_table[1].f[i];
        itof_table[2].f[i] = 1.0f / ftoi_table[2].f[i];
        itof_table[3].f[i] = 1.0f / ftoi_table[3].f[i];

        abs_constant.u[i] = 0x7FFFFFFF;
        max_flt_constant.u[i] = 0x7F7FFFFF;
        min_flt_constant.u[i] = 0xFF7FFFFF;
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
    //printf("[VU_JIT64] Stopped execution\n");
    vu.stop();
}

void vu_tbit_stop_execution(VectorUnit& vu)
{
    //printf("[VU_JIT64] Stopped execution by T-Bit\n");
    vu.stop_by_tbit();
}

void vu_set_int(VectorUnit& vu, int dest, uint16_t value)
{
    vu.set_int(dest, value);
}

void vu_update_pipelines(VectorUnit& vu, int cycles)
{
    for (int i = 0; i < cycles; i++)
    {
        vu.MAC_pipeline[3] = vu.MAC_pipeline[2];
        vu.MAC_pipeline[2] = vu.MAC_pipeline[1];
        vu.MAC_pipeline[1] = vu.MAC_pipeline[0];
        vu.MAC_pipeline[0] = vu.new_MAC_flags;

        vu.CLIP_pipeline[3] = vu.CLIP_pipeline[2];
        vu.CLIP_pipeline[2] = vu.CLIP_pipeline[1];
        vu.CLIP_pipeline[1] = vu.CLIP_pipeline[0];
        vu.CLIP_pipeline[0] = vu.clip_flags;

        vu.update_status();
    }
}

void vu_clip(VectorUnit& vu, uint32_t imm)
{
    vu.clip(imm);
}

void vu_update_xgkick(VectorUnit& vu, int cycles)
{
    if (vu.transferring_GIF)
    {
        vu.gif->request_PATH(1, true);
        while (cycles > 0 && vu.gif->path_active(1, true))
        {
            cycles--;
            vu.handle_XGKICK();
        }
    }
}

void interpreter_upper(VectorUnit& vu, uint32_t instr)
{
    VU_Interpreter::upper(vu, instr);
    VU_Interpreter::call_upper(vu, instr);
}

void interpreter_lower(VectorUnit& vu, uint32_t instr)
{
    VU_Interpreter::lower(vu, instr);
    VU_Interpreter::call_lower(vu, instr);
}

void VU_JIT64::reset(bool clear_cache)
{
    abi_int_count = 0;
    abi_xmm_count = 0;
    for (int i = 0; i < 16; i++)
    {
        xmm_regs[i].used = false;
        xmm_regs[i].locked = false;
        xmm_regs[i].age = 0;
        xmm_regs[i].needs_clamping = false;

        int_regs[i].used = false;
        int_regs[i].locked = false;
        int_regs[i].age = 0;
    }

    //Lock special registers to prevent them from being used
    int_regs[REG_64::RSP].locked = true;

    //Scratchpad registers
    int_regs[REG_64::RAX].locked = true;
    int_regs[REG_64::R15].locked = true;
    int_regs[REG_64::RDI].locked = true;
    int_regs[REG_64::RSI].locked = true;
    xmm_regs[REG_64::XMM0].locked = true;
    xmm_regs[REG_64::XMM1].locked = true;

    if (clear_cache)
    {
        jit_heap.flush_all_blocks();
        create_prologue_block();
    }

    ir.reset_instr_info();

    should_update_mac = false;
    prev_pc = 0xFFFFFFFF;
    current_program = 0;
}

void VU_JIT64::set_current_program(uint32_t crc)
{
    reset(false);
    current_program = crc;
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
        case VU_SpecialReg::P:
            return (uint64_t)&vu.P;
        case VU_SpecialReg::R:
            return (uint64_t)&vu.R;
        default:
            Errors::die("[VU_JIT64] get_vf_addr error: Unrecognized reg %d", index);
    }
    return 0;
}

void VU_JIT64::clamp_vfreg(uint8_t field, REG_64 xmm_reg)
{
    if (needs_clamping(xmm_reg, field))
    {
        REG_64 temp_reg = REG_64::XMM1;
        //Some ops use XMM1 as their final destination, so use the other reg
        if(xmm_reg == temp_reg)
            temp_reg = REG_64::XMM0;

        emitter.load_addr((uint64_t)&max_flt_constant, REG_64::RAX);
        emitter.load_addr((uint64_t)&min_flt_constant, REG_64::R15);

        emitter.MOVAPS_REG(xmm_reg, temp_reg);
        //reg = min_signed(reg, 0x7F7FFFFF)
        emitter.PMINSD_XMM_FROM_MEM(REG_64::RAX, temp_reg);

        //reg = min_unsigned(reg, 0xFF7FFFFF)
        emitter.PMINUD_XMM_FROM_MEM(REG_64::R15, temp_reg);

        emitter.BLENDPS(field, temp_reg, xmm_reg);
        set_clamping(xmm_reg, false, field);
    }
}

void VU_JIT64::sse_abs(REG_64 source, REG_64 dest)
{
    emitter.load_addr((uint64_t)&abs_constant, REG_64::RAX);
    emitter.MOVAPS_REG(source, dest);
    emitter.PAND_XMM_FROM_MEM(REG_64::RAX, dest);
}

void VU_JIT64::sse_div_check(REG_64 num, REG_64 denom, VU_R& dest)
{
    //Division by zero check
    emitter.MOVD_FROM_XMM(denom, REG_64::RAX);
    emitter.TEST32_EAX(0x7FFFFFFF);

    uint8_t* normal_div = emitter.JCC_NEAR_DEFERRED(ConditionCode::NE);

    //If negative, load MIN_FLT.
    emitter.TEST32_EAX(0x80000000);
    uint8_t* load_min = emitter.JCC_NEAR_DEFERRED(ConditionCode::NE);

    //Load MAX_FLT to destination
    emitter.load_addr((uint64_t)&max_flt_constant, REG_64::R15);
    emitter.MOVAPS_FROM_MEM(REG_64::R15, num);
    emitter.load_addr((uint64_t)&dest, REG_64::RAX);
    emitter.MOVAPS_TO_MEM(num, REG_64::RAX);

    uint8_t* div_by_zero_end = emitter.JMP_NEAR_DEFERRED();

    emitter.set_jump_dest(load_min);

    //Load MIN_FLT to destination
    emitter.load_addr((uint64_t)&min_flt_constant, REG_64::R15);
    emitter.MOVAPS_FROM_MEM(REG_64::R15, num);
    emitter.load_addr((uint64_t)&dest, REG_64::RAX);
    emitter.MOVAPS_TO_MEM(num, REG_64::RAX);

    uint8_t* div_by_zero_end2 = emitter.JMP_NEAR_DEFERRED();

    emitter.set_jump_dest(normal_div);

    emitter.DIVPS(denom, num);

    emitter.load_addr((uint64_t)&dest, REG_64::RAX);
    emitter.MOVAPS_TO_MEM(num, REG_64::RAX);

    emitter.set_jump_dest(div_by_zero_end);
    emitter.set_jump_dest(div_by_zero_end2);
}

void VU_JIT64::handle_branch(VectorUnit& vu)
{
    //Load the "branch happened" variable
    emitter.load_addr((uint64_t)&vu.branch_on, REG_64::RAX);
    emitter.MOV8_FROM_MEM(REG_64::RAX, REG_64::RAX);

    //The branch will point to two locations: one where the PC is set to the branch destination,
    //and another where PC = PC + 16. Our generated code will jump if the branch *fails*.
    //This is to allow for future optimizations where the recompiler may generate code beyond
    //the branch (assuming that the branch fails).
    emitter.TEST8_REG(REG_64::RAX, REG_64::RAX);

    //Because we don't know where the branch will jump to, we defer it.
    //Once the "branch succeeded" case has been finished recompiling, we can rewrite the branch offset...
    uint8_t* offset_addr = emitter.JCC_NEAR_DEFERRED(ConditionCode::E);
    emitter.load_addr((uint64_t)&vu.PC, REG_64::RAX);
    emitter.load_addr((uint64_t)&vu_branch_dest, REG_64::R15);
    emitter.MOV16_FROM_MEM(REG_64::R15, REG_64::R15);
    emitter.MOV16_TO_MEM(REG_64::R15, REG_64::RAX);
    cleanup_recompiler(vu, end_of_program);

    //...Which we do here.
    emitter.set_jump_dest(offset_addr);

    //And here we recompile the "branch failed" case.
    emitter.load_addr((uint64_t)&vu.PC, REG_64::RAX);
    emitter.load_addr((uint64_t)&vu_branch_fail_dest, REG_64::R15);
    emitter.MOV16_FROM_MEM(REG_64::R15, REG_64::R15);
    emitter.MOV16_TO_MEM(REG_64::R15, REG_64::RAX);
    cleanup_recompiler(vu, true);
}

void VU_JIT64::update_mac_flags(VectorUnit &vu, REG_64 xmm_reg, uint8_t field)
{
    field = convert_field(field);
    REG_64 temp = REG_64::XMM0;
    REG_64 temp2 = REG_64::XMM1;

    //Shuffle the vector from WZYX to XYZW
    if (xmm_reg != temp)
        emitter.MOVAPS_REG(xmm_reg, temp);
    emitter.PSHUFD(0x1B, temp, temp);

    //Get the sign bits
    emitter.MOVMSKPS(temp, REG_64::RAX);
    emitter.SHL32_REG_IMM(4, REG_64::RAX);

    //Get zero bits
    //First remove sign bit
    emitter.load_addr((uint64_t)&abs_constant, REG_64::R15);
    emitter.MOVAPS_FROM_MEM(REG_64::R15, temp2);
    emitter.PAND_XMM(temp2, temp);

    //Then compare with 0
    emitter.XORPS(temp2, temp2);
    emitter.CMPEQPS(temp2, temp);
    emitter.MOVMSKPS(temp, REG_64::R15);
    emitter.OR32_REG(REG_64::R15, REG_64::RAX);
    emitter.AND32_EAX((field << 4) | field);

    emitter.load_addr((uint64_t)&vu.new_MAC_flags, REG_64::R15);
    emitter.MOV16_TO_MEM(REG_64::RAX, REG_64::R15);

    should_update_mac = false;
}

void VU_JIT64::load_const(VectorUnit &vu, IR::Instruction &instr)
{
    REG_64 dest = alloc_int_reg(vu, instr.get_dest(), REG_STATE::WRITE);
    emitter.MOV32_REG_IMM(instr.get_source() & 0xFFFF, dest);
}

void VU_JIT64::load_float_const(VectorUnit &vu, IR::Instruction &instr)
{
    if (instr.get_dest() < 33)
        Errors::die("[VU_JIT64] Unrecognized vf%d in load_float_const", instr.get_dest());

    //Load 32-bit constant into a special register
    REG_64 dest = alloc_sse_reg(vu, instr.get_dest(), REG_STATE::WRITE);
    emitter.MOV64_OI(instr.get_source(), REG_64::RAX);
    emitter.MOVD_TO_XMM(REG_64::RAX, dest);
    set_clamping(dest, true, 0xf);
    clamp_vfreg(0xF, dest);
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
        REG_64 base = alloc_int_reg(vu, instr.get_base(), REG_STATE::READ);
        REG_64 dest = alloc_int_reg(vu, instr.get_dest(), REG_STATE::WRITE);
        emitter.MOVZX16_TO_64(base, REG_64::RAX);
        emitter.SHL16_REG_IMM(4, REG_64::RAX);

        if (instr.get_source())
            emitter.ADD16_REG_IMM(instr.get_source(), REG_64::RAX);
        emitter.AND16_AX(vu.mem_mask);

        emitter.load_addr((uint64_t)&vu.data_mem.m[field_offset], REG_64::R15);
        emitter.ADD64_REG(REG_64::RAX, REG_64::R15);
        emitter.MOV16_FROM_MEM(REG_64::R15, dest);
    }
    else
    {
        REG_64 dest = alloc_int_reg(vu, instr.get_dest(), REG_STATE::WRITE);
        uint16_t offset = (instr.get_source() + field_offset) & 0x3FFF;
        emitter.load_addr((uint64_t)&vu.data_mem.m[offset], REG_64::R15);
        emitter.MOV16_FROM_MEM(REG_64::R15, dest);
    }
}

void VU_JIT64::store_int(VectorUnit &vu, IR::Instruction &instr)
{
    uint8_t field = convert_field(instr.get_field());
    REG_64 source = alloc_int_reg(vu, instr.get_source(), REG_STATE::READ);

    if (instr.get_base())
    {
        REG_64 base = alloc_int_reg(vu, instr.get_base(), REG_STATE::READ);
        emitter.MOVZX16_TO_64(base, REG_64::RAX);
        emitter.SHL16_REG_IMM(4, REG_64::RAX);

        if (instr.get_source2())
            emitter.ADD16_REG_IMM(instr.get_source2(), REG_64::RAX);
        emitter.AND16_AX(vu.mem_mask);

        emitter.load_addr((uint64_t)&vu.data_mem.m, REG_64::R15);
        emitter.ADD64_REG(REG_64::RAX, REG_64::R15);
    }
    else
    {
        uint16_t offset = instr.get_source2() & vu.mem_mask;
        emitter.load_addr((uint64_t)&vu.data_mem.m[offset], REG_64::R15);
    }

    REG_64 temp = REG_64::XMM0;
    REG_64 temp2 = REG_64::XMM1;
    
    //move all four vector positions from memory
    emitter.MOVAPS_FROM_MEM(REG_64::R15, temp);
    //Zero-extend the upper 16 bits of the vi source
    emitter.MOVZX16_TO_64(source, REG_64::RAX);
    //Move to XMM and populate all 4 vectors
    emitter.MOVD_TO_XMM(REG_64::RAX, temp2);
    emitter.SHUFPS(0, temp2, temp2);
    //Combine it with the original memory and put it back
    emitter.BLENDPS(field, temp2, temp);
    emitter.MOVAPS_TO_MEM(temp, REG_64::R15);
}

void VU_JIT64::load_quad(VectorUnit &vu, IR::Instruction &instr)
{
    uint8_t field = convert_field(instr.get_field());

    if (instr.get_base())
    {
        REG_64 base = alloc_int_reg(vu, instr.get_base(), REG_STATE::READ);
        emitter.MOVZX16_TO_64(base, REG_64::RAX);
        emitter.SHL16_REG_IMM(4, REG_64::RAX);
        if (instr.get_source())
            emitter.ADD16_REG_IMM(instr.get_source(), REG_64::RAX);

        emitter.AND16_AX(vu.mem_mask);

        emitter.load_addr((uint64_t)&vu.data_mem.m, REG_64::R15);
        emitter.ADD64_REG(REG_64::RAX, REG_64::R15);
    }
    else
    {
        uint16_t offset = instr.get_source() & 0x3FFF;
        emitter.load_addr((uint64_t)&vu.data_mem.m[offset], REG_64::R15);
    }

    if (field == 0xF)
    {
        REG_64 dest = alloc_sse_reg(vu, instr.get_dest(), REG_STATE::WRITE);
        emitter.MOVAPS_FROM_MEM(REG_64::R15, dest);
        set_clamping(dest, true, 0xf);
    }
    else
    {
        REG_64 temp = REG_64::XMM0;
        REG_64 dest = alloc_sse_reg(vu, instr.get_dest(), REG_STATE::READ_WRITE);
        emitter.MOVAPS_FROM_MEM(REG_64::R15, temp);
        emitter.BLENDPS(field, temp, dest);
        set_clamping(dest, true, field);
    }
}

void VU_JIT64::store_quad(VectorUnit &vu, IR::Instruction &instr)
{
    uint8_t field = convert_field(instr.get_field());

    REG_64 source = alloc_sse_reg(vu, instr.get_source(), REG_STATE::READ);

    if (instr.get_base())
    {
        REG_64 base = alloc_int_reg(vu, instr.get_base(), REG_STATE::READ);
        emitter.MOVZX16_TO_64(base, REG_64::RAX);
        emitter.SHL16_REG_IMM(4, REG_64::RAX);
        if (instr.get_source2())
            emitter.ADD16_REG_IMM(instr.get_source2(), REG_64::RAX);

        emitter.AND16_AX(vu.mem_mask);

        emitter.load_addr((uint64_t)&vu.data_mem.m, REG_64::R15);
        emitter.ADD64_REG(REG_64::RAX, REG_64::R15);
    }
    else
    {
        uint16_t offset = instr.get_source2() & 0x3FFF;
        emitter.load_addr((uint64_t)&vu.data_mem.m[offset], REG_64::R15);
    }

    if (field == 0xF)
    {

        emitter.MOVAPS_TO_MEM(source, REG_64::R15);
    }
    else
    {
        REG_64 temp = REG_64::XMM0;
        emitter.MOVAPS_FROM_MEM(REG_64::R15, temp);
        emitter.BLENDPS(field, source, temp);
        emitter.MOVAPS_TO_MEM(temp, REG_64::R15);
    }
}

void VU_JIT64::load_quad_inc(VectorUnit &vu, IR::Instruction &instr)
{
    uint8_t field = convert_field(instr.get_field());
    REG_64 base = alloc_int_reg(vu, instr.get_base(), REG_STATE::READ_WRITE);

    //addr = (int_reg * 16) & mem_mask
    emitter.MOVZX16_TO_64(base, REG_64::RAX);
    emitter.SHL32_REG_IMM(4, REG_64::RAX);
    emitter.AND32_EAX(vu.mem_mask);

    //vf_reg.field = *(_XMM*)&vu.data_mem.m[addr]
    emitter.load_addr((uint64_t)&vu.data_mem.m, REG_64::R15);
    emitter.ADD64_REG(REG_64::RAX, REG_64::R15);

    if (field == 0xF)
    {
        REG_64 dest = alloc_sse_reg(vu, instr.get_dest(), REG_STATE::WRITE);
        emitter.MOVAPS_FROM_MEM(REG_64::R15, dest);
        set_clamping(dest, true, 0xf);
    }
    else
    {
        REG_64 temp = REG_64::XMM0;
        REG_64 dest = alloc_sse_reg(vu, instr.get_dest(), REG_STATE::READ_WRITE);
        emitter.MOVAPS_FROM_MEM(REG_64::R15, temp);
        emitter.BLENDPS(field, temp, dest);
        set_clamping(dest, true, field);
    }

    if (instr.get_base())
        emitter.INC16(base);
}

void VU_JIT64::store_quad_inc(VectorUnit &vu, IR::Instruction &instr)
{
    uint8_t field = convert_field(instr.get_field());
    REG_64 temp = REG_64::XMM0;
    REG_64 source = alloc_sse_reg(vu, instr.get_source(), REG_STATE::READ);
    REG_64 base = alloc_int_reg(vu, instr.get_base(), REG_STATE::READ_WRITE);

    //addr = (int_reg * 16) & mem_mask
    emitter.MOVZX16_TO_64(base, REG_64::RAX);
    emitter.SHL16_REG_IMM(4, REG_64::RAX);
    emitter.AND16_AX(vu.mem_mask);

    emitter.load_addr((uint64_t)&vu.data_mem.m, REG_64::R15);
    emitter.ADD64_REG(REG_64::RAX, REG_64::R15);

    if (field == 0xF)
    {
        emitter.MOVAPS_TO_MEM(source, REG_64::R15);
    }
    else
    {
        emitter.MOVAPS_FROM_MEM(REG_64::R15, temp);
        emitter.BLENDPS(field, source, temp);
        emitter.MOVAPS_TO_MEM(temp, REG_64::R15);
    }

    if (instr.get_base())
        emitter.INC16(base);
}

void VU_JIT64::load_quad_dec(VectorUnit &vu, IR::Instruction &instr)
{
    uint8_t field = convert_field(instr.get_field());
    REG_64 base = alloc_int_reg(vu, instr.get_base(), REG_STATE::READ_WRITE);

    if (instr.get_base())
        emitter.DEC16(base);

    //addr = (int_reg * 16) & mem_mask
    emitter.MOVZX16_TO_64(base, REG_64::RAX);
    emitter.SHL32_REG_IMM(4, REG_64::RAX);
    emitter.AND32_EAX(vu.mem_mask);

    //vf_reg.field = *(_XMM*)&vu.data_mem.m[addr]
    emitter.load_addr((uint64_t)&vu.data_mem.m, REG_64::R15);
    emitter.ADD64_REG(REG_64::RAX, REG_64::R15);

    if (field == 0xF)
    {
        REG_64 dest = alloc_sse_reg(vu, instr.get_dest(), REG_STATE::WRITE);
        emitter.MOVAPS_FROM_MEM(REG_64::R15, dest);
        set_clamping(dest, true, 0xf);
    }
    else
    {
        REG_64 temp = REG_64::XMM0;
        REG_64 dest = alloc_sse_reg(vu, instr.get_dest(), REG_STATE::READ_WRITE);
        emitter.MOVAPS_FROM_MEM(REG_64::R15, temp);
        emitter.BLENDPS(field, temp, dest);
        set_clamping(dest, true, field);
    }
}

void VU_JIT64::store_quad_dec(VectorUnit &vu, IR::Instruction &instr)
{
    uint8_t field = convert_field(instr.get_field());
    REG_64 temp = REG_64::XMM0;
    REG_64 source = alloc_sse_reg(vu, instr.get_source(), REG_STATE::READ);
    REG_64 base = alloc_int_reg(vu, instr.get_base(), REG_STATE::READ_WRITE);

    if (instr.get_base())
        emitter.DEC16(base);

    //addr = (int_reg * 16) & mem_mask
    emitter.MOVZX16_TO_64(base, REG_64::RAX);
    emitter.SHL16_REG_IMM(4, REG_64::RAX);
    emitter.AND16_AX(vu.mem_mask);

    emitter.load_addr((uint64_t)&vu.data_mem.m, REG_64::R15);
    emitter.ADD64_REG(REG_64::RAX, REG_64::R15);

    if (field == 0xF)
    {
        emitter.MOVAPS_TO_MEM(source, REG_64::R15);
    }
    else
    {
        emitter.MOVAPS_FROM_MEM(REG_64::R15, temp);
        emitter.BLENDPS(field, source, temp);
        emitter.MOVAPS_TO_MEM(temp, REG_64::R15);
    }
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
    emitter.load_addr((uint64_t)&vu_branch_delay_dest, REG_64::RAX);
    emitter.MOV16_IMM_MEM(instr.get_jump_dest(), REG_64::RAX);

    emitter.load_addr((uint64_t)&vu.branch_on_delay, REG_64::RAX);
    emitter.MOV8_IMM_MEM(1, REG_64::RAX);
    vu_branch = true;
}

void VU_JIT64::jump_and_link(VectorUnit& vu, IR::Instruction& instr)
{
    //First set the PC
    emitter.load_addr((uint64_t)&vu_branch_delay_dest, REG_64::RAX);
    emitter.MOV16_IMM_MEM(instr.get_jump_dest(), REG_64::RAX);

    emitter.load_addr((uint64_t)&vu.branch_on_delay, REG_64::RAX);
    emitter.MOV8_IMM_MEM(1, REG_64::RAX);

    //Then set the link register
    REG_64 link = alloc_int_reg(vu, instr.get_dest(), REG_STATE::WRITE);
    if (instr.get_bc())
    {
        emitter.load_addr((uint64_t)&vu.branch_on, REG_64::RAX);
        emitter.MOV8_FROM_MEM(REG_64::RAX, REG_64::RAX);

        emitter.TEST8_REG(REG_64::RAX, REG_64::RAX);
        uint8_t* offset_addr = emitter.JCC_NEAR_DEFERRED(ConditionCode::E);
        //First branch is taken, so we set the link according to that destination
        emitter.load_addr((uint64_t)&vu_branch_dest, REG_64::RAX);
        emitter.MOV16_FROM_MEM(REG_64::RAX, link);
        emitter.ADD16_REG_IMM(8, link);
        emitter.SHR16_REG_IMM(3, link);

        uint8_t* skip_addr = emitter.JMP_NEAR_DEFERRED();
        
        emitter.set_jump_dest(offset_addr);
        //First branch was not taken, so the destination is as normal
        emitter.MOV16_REG_IMM(instr.get_return_addr(), link);

        emitter.set_jump_dest(skip_addr);
        
    }
    else
    {
        emitter.MOV16_REG_IMM(instr.get_return_addr(), link);
    }
    vu_branch = true;
}

void VU_JIT64::jump_indirect(VectorUnit &vu, IR::Instruction &instr)
{
    REG_64 return_reg = alloc_int_reg(vu, instr.get_source(), REG_STATE::READ);

    emitter.MOV16_REG(return_reg, REG_64::RAX);

    //Multiply the address by eight
    emitter.SHL16_REG_IMM(3, REG_64::RAX);
    emitter.AND16_AX(vu.mem_mask);

    emitter.load_addr((uint64_t)&vu_branch_delay_dest, REG_64::R15);
    emitter.MOV16_TO_MEM(REG_64::RAX, REG_64::R15);

    emitter.load_addr((uint64_t)&vu.branch_on_delay, REG_64::RAX);
    emitter.MOV8_IMM_MEM(1, REG_64::RAX);
    vu_branch = true;
}

void VU_JIT64::jump_and_link_indirect(VectorUnit &vu, IR::Instruction &instr)
{
    REG_64 jump_reg = alloc_int_reg(vu, instr.get_source(), REG_STATE::READ);
    emitter.MOV16_REG(jump_reg, REG_64::RAX);
    //Multiply the address by eight
    emitter.SHL16_REG_IMM(3, REG_64::RAX);
    emitter.AND16_AX(vu.mem_mask);

    emitter.load_addr((uint64_t)&vu_branch_delay_dest, REG_64::R15);
    emitter.MOV16_TO_MEM(REG_64::RAX, REG_64::R15);

    emitter.load_addr((uint64_t)&vu.branch_on_delay, REG_64::RAX);
    emitter.MOV8_IMM_MEM(1, REG_64::RAX);

    //Then set the link register
    REG_64 link = alloc_int_reg(vu, instr.get_dest(), REG_STATE::WRITE);
    if (instr.get_bc())
    {
        emitter.load_addr((uint64_t)&vu.branch_on, REG_64::RAX);
        emitter.MOV8_FROM_MEM(REG_64::RAX, REG_64::RAX);

        emitter.TEST8_REG(REG_64::RAX, REG_64::RAX);
        uint8_t* offset_addr = emitter.JCC_NEAR_DEFERRED(ConditionCode::E);
        //First branch is taken, so we set the link according to that destination
        emitter.load_addr((uint64_t)&vu_branch_dest, REG_64::RAX);
        emitter.MOV16_FROM_MEM(REG_64::RAX, link);
        emitter.ADD16_REG_IMM(8, link);
        emitter.SHR16_REG_IMM(3, link);

        uint8_t* skip_addr = emitter.JMP_NEAR_DEFERRED();

        emitter.set_jump_dest(offset_addr);
        //First branch was not taken, so the destination is as normal
        emitter.MOV16_REG_IMM(instr.get_return_addr(), link);

        emitter.set_jump_dest(skip_addr);
    }
    else
    {
        emitter.MOV16_REG_IMM(instr.get_return_addr(), link);
    }
    vu_branch = true;
}

void VU_JIT64::handle_branch_destinations(VectorUnit& vu, IR::Instruction& instr)
{
    emitter.load_addr((uint64_t)&vu_branch_delay_dest, REG_64::RAX);
    emitter.MOV16_IMM_MEM(instr.get_jump_dest(), REG_64::RAX);

    if (instr.get_bc())
    {
        emitter.load_addr((uint64_t)&vu.branch_on, REG_64::RAX);
        emitter.MOV8_FROM_MEM(REG_64::RAX, REG_64::RAX);

        emitter.TEST8_REG(REG_64::RAX, REG_64::RAX);
        uint8_t* offset_addr = emitter.JCC_NEAR_DEFERRED(ConditionCode::E);
        //First branch is taken, so we set the fail destination according to that destination
        emitter.load_addr((uint64_t)&vu_branch_dest, REG_64::RAX);
        emitter.MOV16_FROM_MEM(REG_64::RAX, REG_64::R15);
        emitter.ADD16_REG_IMM(8, REG_64::R15);
        emitter.load_addr((uint64_t)&vu_branch_delay_fail_dest, REG_64::RAX);
        emitter.MOV16_TO_MEM(REG_64::R15, REG_64::RAX);

        uint8_t* skip_addr = emitter.JMP_NEAR_DEFERRED();

        emitter.set_jump_dest(offset_addr);
        //First branch was not taken, so the destination is as normal
        emitter.load_addr((uint64_t)&vu_branch_delay_fail_dest, REG_64::RAX);
        emitter.MOV16_IMM_MEM(instr.get_jump_fail_dest(), REG_64::RAX);

        emitter.set_jump_dest(skip_addr);
    }
    else
    {
        emitter.load_addr((uint64_t)&vu_branch_delay_fail_dest, REG_64::RAX);
        emitter.MOV16_IMM_MEM(instr.get_jump_fail_dest(), REG_64::RAX);
    }
}

void VU_JIT64::branch_equal(VectorUnit &vu, IR::Instruction &instr)
{
    REG_64 op1;
    REG_64 op2;

    if (!instr.get_field())
    {
        op1 = alloc_int_reg(vu, instr.get_source(), REG_STATE::READ);
        op2 = alloc_int_reg(vu, instr.get_source2(), REG_STATE::READ);
    }
    else
    {
        if (instr.get_source() == vu.int_backup_id_rec)
        {
            op1 = REG_64::R15;
            op2 = alloc_int_reg(vu, instr.get_source2(), REG_STATE::READ);
        }
        else
        {
            op1 = alloc_int_reg(vu, instr.get_source(), REG_STATE::READ);
            op2 = REG_64::R15;
        }
        emitter.load_addr((uint64_t)&vu.int_backup_reg, REG_64::RAX);
        emitter.MOV16_FROM_MEM(REG_64::RAX, REG_64::R15);
        emitter.load_addr((uint64_t)&vu.int_backup_id, REG_64::RAX);
        emitter.MOV16_IMM_MEM(0, REG_64::RAX);
        clear_int_delay(vu, instr);
    }

    emitter.load_addr((uint64_t)&vu.branch_on_delay, REG_64::RAX);
    emitter.CMP16_REG(op2, op1);
    emitter.SETCC_MEM(ConditionCode::E, REG_64::RAX);

    handle_branch_destinations(vu, instr);

    vu_branch = true;
}

void VU_JIT64::branch_not_equal(VectorUnit &vu, IR::Instruction &instr)
{
    REG_64 op1;
    REG_64 op2;

    if (!instr.get_field())
    {
        op1 = alloc_int_reg(vu, instr.get_source(), REG_STATE::READ);
        op2 = alloc_int_reg(vu, instr.get_source2(), REG_STATE::READ);
    }
    else
    {
        if (instr.get_source() == vu.int_backup_id_rec)
        {
            op1 = REG_64::R15;
            op2 = alloc_int_reg(vu, instr.get_source2(), REG_STATE::READ);
        }
        else
        {
            op1 = alloc_int_reg(vu, instr.get_source(), REG_STATE::READ);
            op2 = REG_64::R15;
        }
        emitter.load_addr((uint64_t)&vu.int_backup_reg, REG_64::RAX);
        emitter.MOV16_FROM_MEM(REG_64::RAX, REG_64::R15);
        emitter.load_addr((uint64_t)&vu.int_backup_id, REG_64::RAX);
        emitter.MOV16_IMM_MEM(0, REG_64::RAX);
        clear_int_delay(vu, instr);
    }

    emitter.load_addr((uint64_t)&vu.branch_on_delay, REG_64::RAX);
    emitter.CMP16_REG(op2, op1);
    emitter.SETCC_MEM(ConditionCode::NE, REG_64::RAX);

    handle_branch_destinations(vu, instr);

    vu_branch = true;
}

void VU_JIT64::branch_less_than_zero(VectorUnit &vu, IR::Instruction &instr)
{
    REG_64 op;

    if (!instr.get_field())
    {
        op = alloc_int_reg(vu, instr.get_source(), REG_STATE::READ);
    }
    else
    {
        op = REG_64::R15;
        emitter.load_addr((uint64_t)&vu.int_backup_reg, REG_64::RAX);
        emitter.MOV16_FROM_MEM(REG_64::RAX, REG_64::R15);
        emitter.load_addr((uint64_t)&vu.int_backup_id, REG_64::RAX);
        emitter.MOV16_IMM_MEM(0, REG_64::RAX);
        clear_int_delay(vu, instr);
    }

    emitter.load_addr((uint64_t)&vu.branch_on_delay, REG_64::RAX);
    emitter.CMP16_IMM(0, op);
    emitter.SETCC_MEM(ConditionCode::L, REG_64::RAX);

    handle_branch_destinations(vu, instr);

    vu_branch = true;
}

void VU_JIT64::branch_greater_than_zero(VectorUnit &vu, IR::Instruction &instr)
{
    REG_64 op;

    if (!instr.get_field())
    {
        op = alloc_int_reg(vu, instr.get_source(), REG_STATE::READ);
    }
    else
    {
        op = REG_64::R15;
        emitter.load_addr((uint64_t)&vu.int_backup_reg, REG_64::RAX);
        emitter.MOV16_FROM_MEM(REG_64::RAX, REG_64::R15);
        emitter.load_addr((uint64_t)&vu.int_backup_id, REG_64::RAX);
        emitter.MOV16_IMM_MEM(0, REG_64::RAX);
        clear_int_delay(vu, instr);
    }

    emitter.load_addr((uint64_t)&vu.branch_on_delay, REG_64::RAX);
    emitter.CMP16_IMM(0, op);
    emitter.SETCC_MEM(ConditionCode::G, REG_64::RAX);

    handle_branch_destinations(vu, instr);

    vu_branch = true;
}

void VU_JIT64::branch_less_or_equal_than_zero(VectorUnit &vu, IR::Instruction &instr)
{
    REG_64 op;

    if (!instr.get_field())
    {
        op = alloc_int_reg(vu, instr.get_source(), REG_STATE::READ);
    }
    else
    {
        op = REG_64::R15;
        emitter.load_addr((uint64_t)&vu.int_backup_reg, REG_64::RAX);
        emitter.MOV16_FROM_MEM(REG_64::RAX, REG_64::R15);
        emitter.load_addr((uint64_t)&vu.int_backup_id, REG_64::RAX);
        emitter.MOV16_IMM_MEM(0, REG_64::RAX);
        clear_int_delay(vu, instr);
    }

    emitter.load_addr((uint64_t)&vu.branch_on_delay, REG_64::RAX);
    emitter.CMP16_IMM(0, op);
    emitter.SETCC_MEM(ConditionCode::LE, REG_64::RAX);

    handle_branch_destinations(vu, instr);

    vu_branch = true;
}

void VU_JIT64::branch_greater_or_equal_than_zero(VectorUnit &vu, IR::Instruction &instr)
{
    REG_64 op;

    if (!instr.get_field())
    {
        op = alloc_int_reg(vu, instr.get_source(), REG_STATE::READ);
    }
    else
    {
        op = REG_64::R15;
        emitter.load_addr((uint64_t)&vu.int_backup_reg, REG_64::RAX);
        emitter.MOV16_FROM_MEM(REG_64::RAX, REG_64::R15);
        emitter.load_addr((uint64_t)&vu.int_backup_id, REG_64::RAX);
        emitter.MOV16_IMM_MEM(0, REG_64::RAX);
        clear_int_delay(vu, instr);
    }

    emitter.load_addr((uint64_t)&vu.branch_on_delay, REG_64::RAX);
    emitter.CMP16_IMM(0, op);
    emitter.SETCC_MEM(ConditionCode::GE, REG_64::RAX);

    handle_branch_destinations(vu, instr);

    vu_branch = true;
}

void VU_JIT64::and_int(VectorUnit &vu, IR::Instruction &instr)
{
    //If RD == RS, RD &= RT
    if (instr.get_dest() == instr.get_source())
    {
        REG_64 op2 = alloc_int_reg(vu, instr.get_source2(), REG_STATE::READ);
        REG_64 dest = alloc_int_reg(vu, instr.get_dest(), REG_STATE::READ_WRITE);

        emitter.AND16_REG(op2, dest);
    }
    //If RD == RT, RD &= RS
    else if (instr.get_dest() == instr.get_source2())
    {
        REG_64 op1 = alloc_int_reg(vu, instr.get_source(), REG_STATE::READ);
        REG_64 dest = alloc_int_reg(vu, instr.get_dest(), REG_STATE::READ_WRITE);

        emitter.AND16_REG(op1, dest);
    }
    else
    {
        REG_64 op1 = alloc_int_reg(vu, instr.get_source(), REG_STATE::READ);
        REG_64 op2 = alloc_int_reg(vu, instr.get_source2(), REG_STATE::READ);
        REG_64 dest = alloc_int_reg(vu, instr.get_dest(), REG_STATE::WRITE);
        emitter.MOV16_REG(op1, dest);
        emitter.AND16_REG(op2, dest);
    }
}

void VU_JIT64::or_int(VectorUnit &vu, IR::Instruction &instr)
{
    //If RD == RS, RD |= RT
    if (instr.get_dest() == instr.get_source())
    {
        REG_64 op2 = alloc_int_reg(vu, instr.get_source2(), REG_STATE::READ);
        REG_64 dest = alloc_int_reg(vu, instr.get_dest(), REG_STATE::READ_WRITE);

        emitter.OR16_REG(op2, dest);
    }
    //If RD == RT, RD |= RS
    else if (instr.get_dest() == instr.get_source2())
    {
        REG_64 op1 = alloc_int_reg(vu, instr.get_source(), REG_STATE::READ);
        REG_64 dest = alloc_int_reg(vu, instr.get_dest(), REG_STATE::READ_WRITE);

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
            REG_64 dest = alloc_int_reg(vu, instr.get_dest(), REG_STATE::READ_WRITE);
            REG_64 op2 = alloc_int_reg(vu, instr.get_source2(), REG_STATE::READ);

            emitter.ADD16_REG(op2, dest);
        }
    }
    //If RD == RT, RD += RS
    else if (instr.get_dest() == instr.get_source2())
    {
        REG_64 dest = alloc_int_reg(vu, instr.get_dest(), REG_STATE::READ_WRITE);
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

void VU_JIT64::sub_int_reg(VectorUnit &vu, IR::Instruction &instr)
{
    if (instr.get_source() == instr.get_source2())
    {
        //Zero out the register
        REG_64 dest = alloc_int_reg(vu, instr.get_dest(), REG_STATE::WRITE);
        emitter.XOR32_REG(dest, dest);
    }
    else if (instr.get_dest() == instr.get_source())
    {
        REG_64 dest = alloc_int_reg(vu, instr.get_dest(), REG_STATE::READ_WRITE);
        REG_64 source2 = alloc_int_reg(vu, instr.get_source2(), REG_STATE::READ);
        emitter.SUB32_REG(source2, dest);
    }
    else if (instr.get_dest() == instr.get_source2())
    {
        REG_64 dest = alloc_int_reg(vu, instr.get_dest(), REG_STATE::READ_WRITE);
        REG_64 source = alloc_int_reg(vu, instr.get_source(), REG_STATE::READ);

        emitter.MOV32_REG(source, REG_64::RAX);
        emitter.SUB32_REG(dest, REG_64::RAX);
        emitter.MOV32_REG(REG_64::RAX, dest);
    }
    else
    {
        REG_64 dest = alloc_int_reg(vu, instr.get_dest(), REG_STATE::WRITE);
        REG_64 source = alloc_int_reg(vu, instr.get_source(), REG_STATE::READ);
        REG_64 source2 = alloc_int_reg(vu, instr.get_source2(), REG_STATE::READ);

        emitter.MOV32_REG(source, dest);
        emitter.SUB32_REG(source2, dest);
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

void VU_JIT64::sub_unsigned_imm(VectorUnit &vu, IR::Instruction &instr)
{
    uint16_t imm = instr.get_source2();
    if (instr.get_dest() == instr.get_source())
    {
        REG_64 dest = alloc_int_reg(vu, instr.get_dest(), REG_STATE::READ_WRITE);
        emitter.SUB16_REG_IMM(imm, dest);
    }
    else
    {
        REG_64 dest = alloc_int_reg(vu, instr.get_dest(), REG_STATE::WRITE);
        REG_64 source = alloc_int_reg(vu, instr.get_source(), REG_STATE::READ);

        emitter.MOV16_REG(source, dest);
        emitter.SUB16_REG_IMM(imm, dest);
    }
}

void VU_JIT64::abs(VectorUnit &vu, IR::Instruction &instr)
{
    uint8_t field = convert_field(instr.get_field());
    REG_64 source = alloc_sse_reg(vu, instr.get_source(), REG_STATE::READ_WRITE);
    REG_64 dest = alloc_sse_reg(vu, instr.get_dest(), REG_STATE::READ_WRITE);

    REG_64 temp = REG_64::XMM0;

    sse_abs(source, temp);
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
    emitter.MAXPS(source, temp);
    emitter.BLENDPS(field, temp, dest);
}

void VU_JIT64::max_vectors(VectorUnit &vu, IR::Instruction &instr)
{
    uint8_t field = convert_field(instr.get_field());

    REG_64 op1 = alloc_sse_reg(vu, instr.get_source(), REG_STATE::READ_WRITE);
    REG_64 op2 = alloc_sse_reg(vu, instr.get_source2(), REG_STATE::READ_WRITE);
    REG_64 dest = alloc_sse_reg(vu, instr.get_dest(), REG_STATE::READ_WRITE);
    REG_64 temp = REG_64::XMM0;

    //GT4 has black screens during 3D if it isn't this way around
    emitter.MOVAPS_REG(op2, temp);
    emitter.MAXPS(op1, temp);
    emitter.BLENDPS(field, temp, dest);
}

void VU_JIT64::min_vector_by_scalar(VectorUnit &vu, IR::Instruction &instr)
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
    emitter.MINPS(source, temp);
    emitter.BLENDPS(field, temp, dest);
}

void VU_JIT64::min_vectors(VectorUnit &vu, IR::Instruction &instr)
{
    uint8_t field = convert_field(instr.get_field());

    REG_64 op1 = alloc_sse_reg(vu, instr.get_source(), REG_STATE::READ_WRITE);
    REG_64 op2 = alloc_sse_reg(vu, instr.get_source2(), REG_STATE::READ_WRITE);
    REG_64 dest = alloc_sse_reg(vu, instr.get_dest(), REG_STATE::READ_WRITE);
    REG_64 temp = REG_64::XMM0;

    //GT4 has black screens during 3D if it isn't this way around
    emitter.MOVAPS_REG(op2, temp);
    emitter.MINPS(op1, temp);
    emitter.BLENDPS(field, temp, dest);
}

void VU_JIT64::add_vectors(VectorUnit &vu, IR::Instruction &instr)
{
    uint8_t field = convert_field(instr.get_field());

    REG_64 op1 = alloc_sse_reg(vu, instr.get_source(), REG_STATE::READ_WRITE);
    REG_64 op2 = alloc_sse_reg(vu, instr.get_source2(), REG_STATE::READ_WRITE);
    REG_64 dest = alloc_sse_reg(vu, instr.get_dest(), REG_STATE::READ_WRITE);
    REG_64 temp = REG_64::XMM0;

    clamp_vfreg(field, op1);
    clamp_vfreg(field, op2);

    emitter.MOVAPS_REG(op1, temp);
    emitter.ADDPS(op2, temp);

    set_clamping(temp, true, field);
    clamp_vfreg(field, temp);

    if (instr.get_dest())
    {
        set_clamping(dest, false, field);
        emitter.BLENDPS(field, temp, dest);
    }

    if (should_update_mac)
        update_mac_flags(vu, temp, field);
}

void VU_JIT64::add_vector_by_scalar(VectorUnit &vu, IR::Instruction &instr)
{
    uint8_t field = convert_field(instr.get_field());
    REG_64 source = alloc_sse_reg(vu, instr.get_source(), REG_STATE::READ_WRITE);
    REG_64 bc_reg = alloc_sse_reg(vu, instr.get_source2(), REG_STATE::READ_WRITE);
    REG_64 dest = alloc_sse_reg(vu, instr.get_dest(), REG_STATE::READ_WRITE);


    uint8_t bc = instr.get_bc();

    clamp_vfreg(field, source);
    bc |= (bc << 6) | (bc << 4) | (bc << 2);

    REG_64 temp = REG_64::XMM0;
    emitter.MOVAPS_REG(bc_reg, temp);
    emitter.SHUFPS(bc, temp, temp);
    set_clamping(temp, true, field);
    clamp_vfreg(field, temp);

    emitter.ADDPS(source, temp);
    set_clamping(temp, true, field);
    clamp_vfreg(field, temp);

    if (instr.get_dest())
    {
        set_clamping(dest, false, field);
        emitter.BLENDPS(field, temp, dest);
    }

    if (should_update_mac)
        update_mac_flags(vu, temp, field);
}

void VU_JIT64::sub_vectors(VectorUnit &vu, IR::Instruction &instr)
{
    uint8_t field = convert_field(instr.get_field());

    REG_64 op1 = alloc_sse_reg(vu, instr.get_source(), REG_STATE::READ_WRITE);
    REG_64 op2 = alloc_sse_reg(vu, instr.get_source2(), REG_STATE::READ_WRITE);
    REG_64 dest = alloc_sse_reg(vu, instr.get_dest(), REG_STATE::READ_WRITE);
    REG_64 temp = REG_64::XMM0;

    clamp_vfreg(field, op1);
    clamp_vfreg(field, op2);

    emitter.MOVAPS_REG(op1, temp);
    emitter.SUBPS(op2, temp);
    set_clamping(temp, true, field);
    clamp_vfreg(field, temp);

    if (instr.get_dest())
    {
        set_clamping(dest, false, field);
        emitter.BLENDPS(field, temp, dest);
    }

    if (should_update_mac)
        update_mac_flags(vu, temp, field);
}

void VU_JIT64::sub_vector_by_scalar(VectorUnit &vu, IR::Instruction &instr)
{
    uint8_t field = convert_field(instr.get_field());
    REG_64 source = alloc_sse_reg(vu, instr.get_source(), REG_STATE::READ_WRITE);
    REG_64 bc_reg = alloc_sse_reg(vu, instr.get_source2(), REG_STATE::READ_WRITE);
    REG_64 dest = alloc_sse_reg(vu, instr.get_dest(), REG_STATE::READ_WRITE);

    uint8_t bc = instr.get_bc();

    clamp_vfreg(field, source);

    bc |= (bc << 6) | (bc << 4) | (bc << 2);

    REG_64 temp = REG_64::XMM0;
    REG_64 temp2 = REG_64::XMM1;
    emitter.MOVAPS_REG(bc_reg, temp);
    emitter.MOVAPS_REG(source, temp2);
    emitter.SHUFPS(bc, temp, temp);
    set_clamping(temp, true, field);
    clamp_vfreg(field, temp);

    emitter.SUBPS(temp, temp2);
    set_clamping(temp2, true, field);
    clamp_vfreg(field, temp2);

    if (instr.get_dest())
    {
        set_clamping(dest, false, field);
        emitter.BLENDPS(field, temp2, dest);
    }

    if (should_update_mac)
        update_mac_flags(vu, temp2, field);
}

void VU_JIT64::mul_vectors(VectorUnit &vu, IR::Instruction &instr)
{
    uint8_t field = convert_field(instr.get_field());

    REG_64 op1 = alloc_sse_reg(vu, instr.get_source(), REG_STATE::READ_WRITE);
    REG_64 op2 = alloc_sse_reg(vu, instr.get_source2(), REG_STATE::READ_WRITE);
    REG_64 dest = alloc_sse_reg(vu, instr.get_dest(), REG_STATE::READ_WRITE);
    REG_64 temp = REG_64::XMM0;

    clamp_vfreg(field, op1);
    clamp_vfreg(field, op2);

    emitter.MOVAPS_REG(op1, temp);
    emitter.MULPS(op2, temp);
    set_clamping(temp, true, field);
    clamp_vfreg(field, temp);

    if (instr.get_dest())
    {
        set_clamping(dest, false, field);
        emitter.BLENDPS(field, temp, dest);
    }

    if (should_update_mac)
        update_mac_flags(vu, temp, field);
}

void VU_JIT64::mul_vector_by_scalar(VectorUnit &vu, IR::Instruction &instr)
{
    uint8_t field = convert_field(instr.get_field());
    REG_64 source = alloc_sse_reg(vu, instr.get_source(), REG_STATE::READ_WRITE);
    REG_64 bc_reg = alloc_sse_reg(vu, instr.get_source2(), REG_STATE::READ_WRITE);
    REG_64 dest = alloc_sse_reg(vu, instr.get_dest(), REG_STATE::READ_WRITE);

    uint8_t bc = instr.get_bc();

    clamp_vfreg(field, source);

    bc |= (bc << 6) | (bc << 4) | (bc << 2);

    REG_64 temp = REG_64::XMM0;
    emitter.MOVAPS_REG(bc_reg, temp);
    emitter.SHUFPS(bc, temp, temp);
    set_clamping(temp, true, field);
    clamp_vfreg(field, temp);

    emitter.MULPS(source, temp);
    set_clamping(temp, true, field);
    clamp_vfreg(field, temp);

    if (instr.get_dest())
    {
        set_clamping(dest, false, field);
        emitter.BLENDPS(field, temp, dest);
    }

    if (should_update_mac)
        update_mac_flags(vu, temp, field);
}

void VU_JIT64::madd_vectors(VectorUnit &vu, IR::Instruction &instr)
{
    uint8_t field = convert_field(instr.get_field());

    REG_64 op1 = alloc_sse_reg(vu, instr.get_source(), REG_STATE::READ_WRITE);
    REG_64 op2 = alloc_sse_reg(vu, instr.get_source2(), REG_STATE::READ_WRITE);
    REG_64 acc = alloc_sse_reg(vu, VU_SpecialReg::ACC, REG_STATE::READ_WRITE);
    REG_64 dest = alloc_sse_reg(vu, instr.get_dest(), REG_STATE::READ_WRITE);
    REG_64 temp = REG_64::XMM0;

    clamp_vfreg(field, op1);
    clamp_vfreg(field, op2);
    clamp_vfreg(field, acc);

    emitter.MOVAPS_REG(op1, temp);
    emitter.MULPS(op2, temp);
    emitter.ADDPS(acc, temp);
    set_clamping(temp, true, field);
    clamp_vfreg(field, temp);

    if (instr.get_dest())
    {
        set_clamping(dest, false, field);
        emitter.BLENDPS(field, temp, dest);
    }

    if (should_update_mac)
        update_mac_flags(vu, temp, field);
}

void VU_JIT64::madd_acc_and_vectors(VectorUnit &vu, IR::Instruction &instr)
{
    uint8_t field = convert_field(instr.get_field());

    REG_64 op1 = alloc_sse_reg(vu, instr.get_source(), REG_STATE::READ_WRITE);
    REG_64 op2 = alloc_sse_reg(vu, instr.get_source2(), REG_STATE::READ_WRITE);
    REG_64 dest = alloc_sse_reg(vu, VU_SpecialReg::ACC, REG_STATE::READ_WRITE);
    REG_64 temp = REG_64::XMM0;

    clamp_vfreg(field, op1);
    clamp_vfreg(field, op2);
    clamp_vfreg(field, dest);

    emitter.MOVAPS_REG(op1, temp);
    emitter.MULPS(op2, temp);
    if (field == 0xF)
    {
        emitter.ADDPS(temp, dest);
        set_clamping(dest, true, field);
        clamp_vfreg(field, dest);

        if (should_update_mac)
            update_mac_flags(vu, dest, field);
    }
    else
    {
        emitter.ADDPS(dest, temp);
        set_clamping(temp, true, field);
        clamp_vfreg(field, temp);
        set_clamping(dest, false, field);
        emitter.BLENDPS(field, temp, dest);

        if (should_update_mac)
            update_mac_flags(vu, temp, field);
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

    clamp_vfreg(field, source);
    clamp_vfreg(field, acc);

    bc |= (bc << 6) | (bc << 4) | (bc << 2);

    emitter.MOVAPS_REG(bc_reg, temp);
    emitter.SHUFPS(bc, temp, temp);
    set_clamping(temp, true, field);
    clamp_vfreg(field, temp);

    emitter.MULPS(source, temp);
    emitter.ADDPS(acc, temp);
    set_clamping(temp, true, field);
    clamp_vfreg(field, temp);

    if (instr.get_dest())
    {
        set_clamping(dest, false, field);
        emitter.BLENDPS(field, temp, dest);
    }

    if (should_update_mac)
        update_mac_flags(vu, temp, field);
}

void VU_JIT64::madd_acc_by_scalar(VectorUnit &vu, IR::Instruction &instr)
{
    uint8_t field = convert_field(instr.get_field());

    REG_64 temp = REG_64::XMM0;
    REG_64 bc_reg = alloc_sse_reg(vu, instr.get_source2(), REG_STATE::READ);
    REG_64 source = alloc_sse_reg(vu, instr.get_source(), REG_STATE::READ);
    REG_64 dest = alloc_sse_reg(vu, VU_SpecialReg::ACC, REG_STATE::READ_WRITE);

    uint8_t bc = instr.get_bc();

    clamp_vfreg(field, source);
    clamp_vfreg(field, dest);

    bc |= (bc << 6) | (bc << 4) | (bc << 2);

    emitter.MOVAPS_REG(bc_reg, temp);
    emitter.SHUFPS(bc, temp, temp);
    set_clamping(temp, true, field);
    clamp_vfreg(field, temp);

    emitter.MULPS(source, temp);

    if (field == 0xF)
    {
        emitter.ADDPS(temp, dest);
        set_clamping(dest, true, field);
        clamp_vfreg(field, dest);
    }
    else
    {
        emitter.ADDPS(dest, temp);
        set_clamping(temp, true, field);
        clamp_vfreg(field, temp);
        set_clamping(dest, false, field);
        emitter.BLENDPS(field, temp, dest);
    }

    if (should_update_mac)
        update_mac_flags(vu, dest, field);
}

void VU_JIT64::msub_vectors(VectorUnit &vu, IR::Instruction &instr)
{
    uint8_t field = convert_field(instr.get_field());

    REG_64 op1 = alloc_sse_reg(vu, instr.get_source(), REG_STATE::READ_WRITE);
    REG_64 op2 = alloc_sse_reg(vu, instr.get_source2(), REG_STATE::READ_WRITE);
    REG_64 acc = alloc_sse_reg(vu, VU_SpecialReg::ACC, REG_STATE::READ_WRITE);
    REG_64 dest = alloc_sse_reg(vu, instr.get_dest(), REG_STATE::READ_WRITE);
    REG_64 temp = REG_64::XMM0;
    REG_64 temp2 = REG_64::XMM1;

    clamp_vfreg(field, op1);
    clamp_vfreg(field, op2);
    clamp_vfreg(field, acc);

    emitter.MOVAPS_REG(acc, temp2);
    emitter.MOVAPS_REG(op1, temp);
    emitter.MULPS(op2, temp);
    emitter.SUBPS(temp, temp2);
    set_clamping(temp2, true, field);
    clamp_vfreg(field, temp2);

    if (instr.get_dest())
    {
        set_clamping(dest, false, field);
        emitter.BLENDPS(field, temp2, dest);
    }

    if (should_update_mac)
        update_mac_flags(vu, temp2, field);
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

    clamp_vfreg(field, source);
    clamp_vfreg(field, acc);

    bc |= (bc << 6) | (bc << 4) | (bc << 2);

    emitter.MOVAPS_REG(bc_reg, temp);
    emitter.MOVAPS_REG(acc, temp2);

    emitter.SHUFPS(bc, temp, temp);
    set_clamping(temp, true, field);
    clamp_vfreg(field, temp);

    emitter.MULPS(source, temp);
    emitter.SUBPS(temp, temp2);
    set_clamping(temp2, true, field);
    clamp_vfreg(field, temp2);

    if (instr.get_dest())
    {
        set_clamping(dest, false, field);
        emitter.BLENDPS(field, temp2, dest);
    }

    if (should_update_mac)
        update_mac_flags(vu, temp2, field);
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

    clamp_vfreg(field, source);
    clamp_vfreg(field, dest);

    bc |= (bc << 6) | (bc << 4) | (bc << 2);

    emitter.MOVAPS_REG(bc_reg, temp);
    emitter.SHUFPS(bc, temp, temp);
    set_clamping(temp, true, field);
    clamp_vfreg(field, temp);

    emitter.MULPS(source, temp);

    if (field == 0xF)
    {
        emitter.SUBPS(temp, dest);
        set_clamping(dest, true, field);
        clamp_vfreg(field, dest);
    }
    else
    {
        emitter.MOVAPS_REG(dest, temp2);
        emitter.SUBPS(temp, temp2);
        set_clamping(temp2, true, field);
        clamp_vfreg(field, temp2);
        set_clamping(dest, false, field);
        emitter.BLENDPS(field, temp2, dest);
    }

    if (should_update_mac)
        update_mac_flags(vu, dest, field);
}

void VU_JIT64::opmula(VectorUnit &vu, IR::Instruction &instr)
{
    REG_64 reg1 = alloc_sse_reg(vu, instr.get_source(), REG_STATE::READ);
    REG_64 reg2 = alloc_sse_reg(vu, instr.get_source2(), REG_STATE::READ);
    REG_64 dest = alloc_sse_reg(vu, VU_SpecialReg::ACC, REG_STATE::READ_WRITE);
    REG_64 temp = REG_64::XMM0;
    REG_64 temp2 = REG_64::XMM1;

    clamp_vfreg(0x7, reg1);
    clamp_vfreg(0x7, reg2);
    clamp_vfreg(0x7, dest);

    //xyz = yzx
    emitter.PSHUFD(0x1 | (0x2 << 2), reg1, temp);

    //xyz = zxy
    emitter.PSHUFD(0x2 | (0x1 << 4), reg2, temp2);

    emitter.MULPS(temp2, temp);

    set_clamping(temp, true, 0x7);
    clamp_vfreg(0x7, temp);

    set_clamping(dest, false, 0x7);
    emitter.BLENDPS(0x7, temp, dest);

    if (should_update_mac)
        update_mac_flags(vu, temp, 0x7);
}

void VU_JIT64::opmsub(VectorUnit &vu, IR::Instruction &instr)
{
    REG_64 reg1 = alloc_sse_reg(vu, instr.get_source(), REG_STATE::READ);
    REG_64 reg2 = alloc_sse_reg(vu, instr.get_source2(), REG_STATE::READ);
    REG_64 dest = alloc_sse_reg(vu, instr.get_dest(), REG_STATE::READ_WRITE);
    REG_64 acc = alloc_sse_reg(vu, VU_SpecialReg::ACC, REG_STATE::READ);

    REG_64 temp = REG_64::XMM0;
    REG_64 temp2 = REG_64::XMM1;

    clamp_vfreg(0x7, reg1);
    clamp_vfreg(0x7, reg2);
    clamp_vfreg(0x7, acc);

    //xyz = yzx
    emitter.PSHUFD(0x1 | (0x2 << 2), reg1, temp);

    //xyz = zxy
    emitter.PSHUFD(0x2 | (0x1 << 4), reg2, temp2);

    emitter.MULPS(temp2, temp);
    emitter.MOVAPS_REG(acc, temp2);
    emitter.SUBPS(temp, temp2);

    set_clamping(temp2, true, 0x7);
    clamp_vfreg(0x7, temp2);

    if (instr.get_dest())
    {
        set_clamping(dest, false, 0x7);
        emitter.BLENDPS(0x7, temp2, dest);
    }

    if (should_update_mac)
        update_mac_flags(vu, temp2, 0x7);
}

void VU_JIT64::clip(VectorUnit &vu, IR::Instruction &instr)
{
    flush_regs(vu);
    for (int i = 0; i < 16; i++)
    {
        xmm_regs[i].used = false;
        xmm_regs[i].age = 0;
        int_regs[i].used = false;
        int_regs[i].age = 0;
    }

    uint32_t imm = instr.get_source() << 11;
    imm |= instr.get_source2() << 16;

    prepare_abi(vu, (uint64_t)&vu);
    prepare_abi(vu, imm);
    call_abi_func((uint64_t)vu_clip);
}

void VU_JIT64::div(VectorUnit &vu, IR::Instruction &instr)
{
    REG_64 num = alloc_sse_reg(vu, instr.get_source(), REG_STATE::READ);
    REG_64 denom = alloc_sse_reg(vu, instr.get_source2(), REG_STATE::READ);
    REG_64 temp = REG_64::XMM0;
    REG_64 temp2 = REG_64::XMM1;

    uint8_t num_field = instr.get_field();
    uint8_t denom_field = instr.get_field2();

    clamp_vfreg(1 << num_field, num);
    clamp_vfreg(1 << denom_field, denom);

    //Insert the float value in num/denom into bits 0-31 in temp/temp2, then zero out bits 32-127
    emitter.INSERTPS(num_field, 0, 0b1110, num, temp);
    emitter.INSERTPS(denom_field, 0, 0b1110, denom, temp2);

    sse_div_check(temp, temp2, vu.new_Q_instance);
}

void VU_JIT64::rsqrt(VectorUnit &vu, IR::Instruction &instr)
{
    REG_64 num = alloc_sse_reg(vu, instr.get_source(), REG_STATE::READ);
    REG_64 denom = alloc_sse_reg(vu, instr.get_source2(), REG_STATE::READ);
    REG_64 temp = REG_64::XMM0;
    REG_64 temp2 = REG_64::XMM1;

    uint8_t num_field = instr.get_field();
    uint8_t denom_field = instr.get_field2();

    clamp_vfreg(1 << num_field, num);
    clamp_vfreg(1 << denom_field, denom);

    //Clear the negative bit in the denominator
    sse_abs(denom, temp2);

    //Insert the float value in num/denom into bits 0-31 in temp/temp2, then zero out bits 32-127
    emitter.INSERTPS(num_field, 0, 0b1110, num, temp);
    emitter.INSERTPS(denom_field, 0, 0b1110, temp2, temp2);

    //denom = sqrt(denom)
    emitter.SQRTPS(temp2, temp2);

    sse_div_check(temp, temp2, vu.new_Q_instance);
}

void VU_JIT64::fixed_to_float(VectorUnit &vu, IR::Instruction &instr, int table_entry)
{
    uint8_t field = convert_field(instr.get_field());
    REG_64 source = alloc_sse_reg(vu, instr.get_source(), REG_STATE::READ);
    REG_64 dest = alloc_sse_reg(vu, instr.get_dest(), REG_STATE::READ_WRITE);
    REG_64 temp = REG_64::XMM0;

    if (field == 0xF)
    {
        emitter.CVTDQ2PS(source, dest);
        if (table_entry)
        {
            uint64_t addr = (uint64_t)&itof_table[table_entry];

            emitter.load_addr(addr, REG_64::RAX);
            emitter.MOVAPS_FROM_MEM(REG_64::RAX, temp);

            emitter.MULPS(temp, dest);
            set_clamping(dest, true, field);
        }
    }
    else
    {
        emitter.CVTDQ2PS(source, temp);

        if (table_entry)
        {
            REG_64 temp2 = REG_64::XMM1;
            uint64_t addr = (uint64_t)&itof_table[table_entry];

            emitter.load_addr(addr, REG_64::RAX);
            emitter.MOVAPS_FROM_MEM(REG_64::RAX, temp2);

            emitter.MULPS(temp2, temp);
        }
        emitter.BLENDPS(field, temp, dest);
        set_clamping(dest, true, field);
    }
}

void VU_JIT64::float_to_fixed(VectorUnit &vu, IR::Instruction &instr, int table_entry)
{
    uint8_t field = convert_field(instr.get_field());
    REG_64 source = alloc_sse_reg(vu, instr.get_source(), REG_STATE::READ_WRITE);
    REG_64 dest = alloc_sse_reg(vu, instr.get_dest(), REG_STATE::READ_WRITE);
    REG_64 temp = REG_64::XMM0;

    clamp_vfreg(field, source);

    emitter.MOVAPS_REG(source, temp);

    if (table_entry)
    {
        REG_64 temp2 = REG_64::XMM1;

        uint64_t addr = (uint64_t)&ftoi_table[table_entry].u[0];

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
        emitter.INSERTPS(field, 0, 0, source, temp);
        emitter.MOVD_FROM_XMM(temp, dest);
    }

    //Unsure if this is necessary
    emitter.AND32_REG_IMM(0xFFFF, dest);
}

void VU_JIT64::move_from_int(VectorUnit &vu, IR::Instruction &instr)
{
    uint8_t field = convert_field(instr.get_field());
    REG_64 source = alloc_int_reg(vu, instr.get_source(), REG_STATE::READ);
    REG_64 dest = alloc_sse_reg(vu, instr.get_dest(), REG_STATE::READ_WRITE);
    REG_64 temp = REG_64::XMM0;

    //The 16-bit integer must be sign extended
    emitter.MOVSX16_TO_64(source, REG_64::RAX);
    emitter.MOVD_TO_XMM(REG_64::RAX, temp);
    emitter.SHUFPS(0, temp, temp);
    emitter.BLENDPS(field, temp, dest);
    set_clamping(dest, true, field);
}

void VU_JIT64::move_float(VectorUnit &vu, IR::Instruction &instr)
{
    uint8_t field = convert_field(instr.get_field());
    REG_64 source = alloc_sse_reg(vu, instr.get_source(), REG_STATE::READ);

    if (field == 0xF)
    {
        REG_64 dest = alloc_sse_reg(vu, instr.get_dest(), REG_STATE::WRITE);

        emitter.MOVAPS_REG(source, dest);
        set_clamping(dest, needs_clamping(source, field), field);
    }
    else
    {
        REG_64 dest = alloc_sse_reg(vu, instr.get_dest(), REG_STATE::READ_WRITE);

        emitter.BLENDPS(field, source, dest);
        set_clamping(dest, true, field);
    }
}

void VU_JIT64::move_rotated_float(VectorUnit &vu, IR::Instruction &instr)
{
    uint8_t field = convert_field(instr.get_field());
    REG_64 source = alloc_sse_reg(vu, instr.get_source(), REG_STATE::READ_WRITE);
    REG_64 dest = alloc_sse_reg(vu, instr.get_dest(), REG_STATE::READ_WRITE);

    //xyzw = yzwx
    uint8_t rot = (1 << 0) | (2 << 2) | (3 << 4) | (0 << 6);
    if (field == 0xF)
    {
        emitter.PSHUFD(rot, source, dest);
        set_clamping(dest, needs_clamping(source, 0xF), field);
    }
    else
    {
        REG_64 temp = REG_64::XMM0;
        emitter.PSHUFD(rot, source, temp);
        emitter.BLENDPS(field, temp, dest);
        set_clamping(dest, needs_clamping(source, (field << 1) | ((field & 0x8) >> 3)), field);
    }
}

void VU_JIT64::mac_eq(VectorUnit &vu, IR::Instruction &instr)
{
    REG_64 source = alloc_int_reg(vu, instr.get_source(), REG_STATE::READ_WRITE);
    REG_64 dest = alloc_int_reg(vu, instr.get_dest(), REG_STATE::READ_WRITE);

    //dest = mac == source
    emitter.load_addr((uint64_t)vu.MAC_flags, REG_64::RAX);
    emitter.MOV32_FROM_MEM(REG_64::RAX, REG_64::RAX);
    emitter.AND32_EAX(0xFFFF);
    emitter.CMP16_REG(REG_64::RAX, source);
    emitter.SETCC_REG(ConditionCode::E, REG_64::RAX);
    emitter.AND32_EAX(0x1);
    emitter.MOV32_REG(REG_64::RAX, dest);
}

void VU_JIT64::mac_and(VectorUnit &vu, IR::Instruction &instr)
{
    emitter.load_addr((uint64_t)vu.MAC_flags, REG_64::RAX);
    emitter.MOV16_FROM_MEM(REG_64::RAX, REG_64::R15);

    //dest = mac & source
    if (instr.get_dest() == instr.get_source())
    {
        REG_64 dest = alloc_int_reg(vu, instr.get_dest(), REG_STATE::READ_WRITE);
        emitter.AND16_REG(REG_64::R15, dest);
    }
    else
    {
        REG_64 source = alloc_int_reg(vu, instr.get_source(), REG_STATE::READ);
        REG_64 dest = alloc_int_reg(vu, instr.get_dest(), REG_STATE::WRITE);

        emitter.AND16_REG(source, REG_64::R15);
        emitter.MOV16_REG(REG_64::R15, dest);
    }
}

void VU_JIT64::set_clip_flags(VectorUnit &vu, IR::Instruction &instr)
{
    emitter.load_addr((uint64_t)&vu.clip_flags, REG_64::RAX);
    emitter.MOV32_IMM_MEM(instr.get_source(), REG_64::RAX);
}

void VU_JIT64::get_clip_flags(VectorUnit &vu, IR::Instruction &instr)
{
    REG_64 dest = alloc_int_reg(vu, instr.get_dest(), REG_STATE::WRITE);

    //dest = clip & 0xFFF
    emitter.load_addr((uint64_t)vu.CLIP_flags, REG_64::RAX);
    emitter.MOV32_FROM_MEM(REG_64::RAX, dest);
    emitter.AND32_REG_IMM(0xFFF, dest);
}

void VU_JIT64::and_clip_flags(VectorUnit &vu, IR::Instruction &instr)
{
    REG_64 vi1 = alloc_int_reg(vu, 1, REG_STATE::WRITE);

    //vi1 = (clip & imm) != 0
    emitter.load_addr((uint64_t)vu.CLIP_flags, REG_64::RAX);
    emitter.MOV32_FROM_MEM(REG_64::RAX, REG_64::RAX);
    emitter.TEST32_EAX(instr.get_source());
    emitter.SETCC_REG(ConditionCode::NE, REG_64::RAX);
    emitter.AND32_EAX(0x1);
    emitter.MOV32_REG(REG_64::RAX, vi1);
}

void VU_JIT64::or_clip_flags(VectorUnit &vu, IR::Instruction &instr)
{
    REG_64 vi1 = alloc_int_reg(vu, 1, REG_STATE::WRITE);

    //vi1 = (clip | imm) == 0xFFFFFF
    emitter.load_addr((uint64_t)vu.CLIP_flags, REG_64::RAX);
    emitter.MOV32_FROM_MEM(REG_64::RAX, REG_64::RAX);
    emitter.OR32_EAX(instr.get_source());
    emitter.AND32_EAX(0xFFFFFF);
    emitter.CMP32_EAX(0xFFFFFF);
    emitter.SETCC_REG(ConditionCode::E, REG_64::RAX);
    emitter.AND32_EAX(0x1);
    emitter.MOV32_REG(REG_64::RAX, vi1);
}

void VU_JIT64::and_stat_flags(VectorUnit &vu, IR::Instruction &instr)
{
    REG_64 dest = alloc_int_reg(vu, instr.get_dest(), REG_STATE::WRITE);

    //dest = status & imm
    emitter.load_addr((uint64_t)&vu.status, REG_64::RAX);
    emitter.MOV32_FROM_MEM(REG_64::RAX, REG_64::RAX);
    emitter.AND32_EAX(instr.get_source());
    emitter.MOV32_REG(REG_64::RAX, dest);
}

void VU_JIT64::move_from_p(VectorUnit &vu, IR::Instruction &instr)
{
    uint8_t field = convert_field(instr.get_field());
    REG_64 dest = alloc_sse_reg(vu, instr.get_dest(), REG_STATE::READ_WRITE);
    REG_64 p_reg = alloc_sse_reg(vu, VU_SpecialReg::P, REG_STATE::READ);

    REG_64 temp = REG_64::XMM0;
    emitter.MOVAPS_REG(p_reg, temp);
    emitter.SHUFPS(0, temp, temp);
    emitter.BLENDPS(field, temp, dest);
    set_clamping(dest, true, field);
}

void VU_JIT64::eleng(VectorUnit &vu, IR::Instruction &instr)
{
    REG_64 source = alloc_sse_reg(vu, instr.get_source(), REG_STATE::READ);
    REG_64 temp = REG_64::XMM0;

    clamp_vfreg(0xE, source);

    emitter.MOVAPS_REG(source, temp);

    //(x^2 + y^2 + z^2) -> P
    emitter.DPPS(0x71, temp, temp);

    //sqrt(P)
    emitter.SQRTPS(temp, temp);

    emitter.load_addr((uint64_t)&vu.new_P_instance, REG_64::RAX);
    emitter.MOVAPS_TO_MEM(temp, REG_64::RAX);
}

void VU_JIT64::erleng(VectorUnit &vu, IR::Instruction &instr)
{
    REG_64 source = alloc_sse_reg(vu, instr.get_source(), REG_STATE::READ);
    REG_64 temp = REG_64::XMM0;
    REG_64 temp2 = REG_64::XMM1;

    clamp_vfreg(0xE, source);

    emitter.MOVAPS_REG(source, temp);

    //sqrt(x^2 + y^2 + z^2) -> P
    emitter.DPPS(0x71, temp, temp);
    emitter.SQRTPS(temp, temp);

    //1.0f / P -> P
    emitter.MOV32_REG_IMM(0x3F800000, REG_64::RAX);
    emitter.MOVD_TO_XMM(REG_64::RAX, temp2);
    emitter.DIVPS(temp, temp2);

    emitter.load_addr((uint64_t)&vu.new_P_instance, REG_64::RAX);
    emitter.MOVAPS_TO_MEM(temp2, REG_64::RAX);
}

void VU_JIT64::esqrt(VectorUnit &vu, IR::Instruction &instr)
{
    uint8_t field = instr.get_field();
    REG_64 source = alloc_sse_reg(vu, instr.get_source(), REG_STATE::READ);
    REG_64 temp = REG_64::XMM0;

    clamp_vfreg(1 << field, source);

    emitter.SQRTPS(source, temp);
    emitter.INSERTPS(field, 0, 0, temp, temp);

    emitter.load_addr((uint64_t)&vu.new_P_instance, REG_64::RAX);
    emitter.MOVAPS_TO_MEM(temp, REG_64::RAX);
}

void VU_JIT64::ersqrt(VectorUnit &vu, IR::Instruction &instr)
{
    uint8_t field = instr.get_field();
    REG_64 source = alloc_sse_reg(vu, instr.get_source(), REG_STATE::READ);
    REG_64 denom = REG_64::XMM0;
    REG_64 num = REG_64::XMM1;

    clamp_vfreg(1 << field, source);

    emitter.SQRTPS(source, denom);
    emitter.INSERTPS(field, 0, 0, denom, denom);

    //Divide 1.0 by sqrt(source)
    emitter.MOV32_REG_IMM(0x3F800000, REG_64::RAX);
    emitter.MOVD_TO_XMM(REG_64::RAX, num);

    emitter.DIVPS(denom, num);

    emitter.load_addr((uint64_t)&vu.new_P_instance, REG_64::RAX);
    emitter.MOVAPS_TO_MEM(num, REG_64::RAX);
}

void VU_JIT64::rinit(VectorUnit &vu, IR::Instruction &instr)
{
    uint8_t field = instr.get_field();
    REG_64 source = alloc_sse_reg(vu, instr.get_source(), REG_STATE::READ);
    REG_64 temp = REG_64::XMM0;

    clamp_vfreg(field, source);

    //R = 0x3F800000 | (reg[field] & 0x007FFFFF)
    emitter.INSERTPS(field, 0, 0, source, temp);
    emitter.MOVD_FROM_XMM(temp, REG_64::RAX);
    emitter.AND32_EAX(0x007FFFFF);
    emitter.OR32_EAX(0x3F800000);

    emitter.load_addr((uint64_t)&vu.R, REG_64::R15);
    emitter.MOV32_TO_MEM(REG_64::RAX, REG_64::R15);
}

void VU_JIT64::backup_vf(VectorUnit& vu, IR::Instruction& instr)
{
    REG_64 vf_reg = alloc_sse_reg(vu, instr.get_source(), REG_STATE::READ_WRITE);

    if (!instr.get_dest())
    {
        emitter.load_addr((uint64_t)&vu.backup_oldgpr, REG_64::RAX);
    }
    else
    {
        emitter.load_addr((uint64_t)&vu.backup_newgpr, REG_64::RAX);
    }

    emitter.MOVAPS_TO_MEM(vf_reg, REG_64::RAX);
}

void VU_JIT64::restore_vf(VectorUnit& vu, IR::Instruction& instr)
{
    REG_64 vf_reg = alloc_sse_reg(vu, instr.get_source(), REG_STATE::READ_WRITE);

    if (!instr.get_dest())
    {
        emitter.load_addr((uint64_t)&vu.backup_oldgpr, REG_64::RAX);
    }
    else
    {
        emitter.load_addr((uint64_t)&vu.backup_newgpr, REG_64::RAX);
    }

    emitter.MOVAPS_FROM_MEM(REG_64::RAX, vf_reg);
}

void VU_JIT64::backup_vi(VectorUnit& vu, IR::Instruction& instr)
{
    REG_64 int_reg = alloc_int_reg(vu, instr.get_source(), REG_STATE::READ);
    emitter.load_addr((uint64_t)&vu.int_backup_reg, REG_64::RAX);
    emitter.MOV16_TO_MEM(int_reg, REG_64::RAX);
    emitter.load_addr((uint64_t)&vu.int_backup_id, REG_64::RAX);
    emitter.MOV8_IMM_MEM(instr.get_source(), REG_64::RAX);
    emitter.load_addr((uint64_t)&vu.int_branch_delay, REG_64::RAX);
    emitter.MOV8_IMM_MEM(1, REG_64::RAX);
    

    vu.int_backup_id_rec = instr.get_source();
}

void VU_JIT64::clear_int_delay(VectorUnit& vu, IR::Instruction& instr)
{
    emitter.load_addr((uint64_t)&vu.int_branch_delay, REG_64::RAX);
    emitter.MOV8_IMM_MEM(0, REG_64::RAX);
}

void VU_JIT64::update_q(VectorUnit &vu, IR::Instruction &instr)
{
    //Store the pipelined Q inside the Q available to the program
    REG_64 q_reg = alloc_sse_reg(vu, VU_SpecialReg::Q, REG_STATE::WRITE);
    emitter.load_addr((uint64_t)&vu.new_Q_instance, REG_64::RAX);
    emitter.MOVAPS_FROM_MEM(REG_64::RAX, q_reg);
    set_clamping(q_reg, true, 0xF);
}

void VU_JIT64::update_p(VectorUnit &vu, IR::Instruction &instr)
{
    //Store the pipelined P inside the P available to the program
    REG_64 p_reg = alloc_sse_reg(vu, VU_SpecialReg::P, REG_STATE::WRITE);
    emitter.load_addr((uint64_t)&vu.new_P_instance, REG_64::RAX);
    emitter.MOVAPS_FROM_MEM(REG_64::RAX, p_reg);
    set_clamping(p_reg, true, 0xF);
}

void VU_JIT64::update_mac_pipeline(VectorUnit &vu, IR::Instruction &instr)
{
    prepare_abi(vu, (uint64_t)&vu);
    prepare_abi(vu, instr.get_source());
    call_abi_func((uint64_t)vu_update_pipelines);
}

void VU_JIT64::move_xtop(VectorUnit &vu, IR::Instruction &instr)
{
    REG_64 dest = alloc_int_reg(vu, instr.get_dest(), REG_STATE::WRITE);

    emitter.load_addr((uint64_t)vu.VIF_TOP, REG_64::RAX);
    emitter.MOV16_FROM_MEM(REG_64::RAX, dest);
}

void VU_JIT64::move_xitop(VectorUnit &vu, IR::Instruction &instr)
{
    REG_64 dest = alloc_int_reg(vu, instr.get_dest(), REG_STATE::WRITE);

    emitter.load_addr((uint64_t)vu.VIF_ITOP, REG_64::RAX);
    emitter.MOV16_FROM_MEM(REG_64::RAX, dest);
}

void VU_JIT64::xgkick(VectorUnit &vu, IR::Instruction &instr)
{
    REG_64 base = alloc_int_reg(vu, instr.get_base(), REG_STATE::READ);

    //Check if we're already transferring, and stall the VU if we are
    emitter.load_addr((uint64_t)&vu.transferring_GIF, REG_64::R15);
    emitter.MOV32_FROM_MEM(REG_64::R15, REG_64::RAX);
    emitter.TEST32_EAX(0x1);

    //Jump if no stall is necessary
    uint8_t* no_stall_addr = emitter.JCC_NEAR_DEFERRED(ConditionCode::E);

    //Stall
    emitter.load_addr((uint64_t)&vu.XGKICK_stall, REG_64::RAX);
    emitter.MOV8_IMM_MEM(1, REG_64::RAX);

    emitter.MOV32_REG(base, REG_64::RAX);
    emitter.SHL32_REG_IMM(4, REG_64::RAX);
    emitter.AND32_EAX(vu.mem_mask);

    emitter.load_addr((uint64_t)&vu.stalled_GIF_addr, REG_64::R15);
    emitter.MOV16_TO_MEM(REG_64::RAX, REG_64::R15);

    //If we're in a delay slot, there's no more XGKicks
    if (!instr.get_dest() && !instr.get_jump_dest())
    {
        emitter.load_addr((uint64_t)&vu.PC, REG_64::RAX);
        emitter.MOV16_IMM_MEM(instr.get_return_addr() + 8, REG_64::RAX);

        emitter.load_addr((uint64_t)&prev_pc, REG_64::RAX);
        emitter.MOV32_IMM_MEM(instr.get_return_addr(), REG_64::RAX);

        emitter.load_addr((uint64_t)&vu.pipeline_state[0], REG_64::RAX);
        emitter.MOV64_OI(instr.get_source(), REG_64::R15);
        emitter.MOV64_TO_MEM(REG_64::R15, REG_64::RAX);

        emitter.load_addr((uint64_t)&vu.pipeline_state[1], REG_64::RAX);
        emitter.MOV64_OI(instr.get_source2(), REG_64::R15);
        emitter.MOV64_TO_MEM(REG_64::R15, REG_64::RAX);

        cleanup_recompiler(vu, false);
    }

    //Jump after the stall handling code is done
    uint8_t* stall_addr = emitter.JMP_NEAR_DEFERRED();

    //No stall
    emitter.set_jump_dest(no_stall_addr);

    //Set transferring_GIF to true
    emitter.MOV8_IMM_MEM(1, REG_64::R15);

    emitter.MOV32_REG(base, REG_64::RAX);
    emitter.SHL32_REG_IMM(4, REG_64::RAX);
    emitter.AND32_EAX(vu.mem_mask);

    emitter.load_addr((uint64_t)&vu.GIF_addr, REG_64::R15);
    emitter.MOV16_TO_MEM(REG_64::RAX, REG_64::R15);

    emitter.set_jump_dest(stall_addr);
}

void VU_JIT64::update_xgkick(VectorUnit &vu, IR::Instruction &instr)
{
    flush_regs(vu);
    for (int i = 0; i < 16; i++)
    {
        xmm_regs[i].used = false;
        xmm_regs[i].age = 0;
        int_regs[i].used = false;
        int_regs[i].age = 0;
    }
    prepare_abi(vu, (uint64_t)&vu);
    prepare_abi(vu, instr.get_source());
    call_abi_func((uint64_t)vu_update_xgkick);
}

void VU_JIT64::stop(VectorUnit &vu, IR::Instruction &instr)
{
    prepare_abi(vu, (uint64_t)&vu);
    call_abi_func((uint64_t)vu_stop_execution);

    emitter.load_addr((uint64_t)&vu.PC, REG_64::RAX);
    emitter.MOV16_IMM_MEM(instr.get_jump_dest(), REG_64::RAX);

    //We're at the end of a program, so the next call doesn't need to know the previous state
    emitter.load_addr((uint64_t)&prev_pc, REG_64::RAX);
    emitter.MOV32_IMM_MEM(0xFFFFFFFF, REG_64::RAX);

    end_of_program = true;
}

void VU_JIT64::stop_by_tbit(VectorUnit &vu, IR::Instruction &instr)
{
    prepare_abi(vu, (uint64_t)&vu);
    call_abi_func((uint64_t)vu_tbit_stop_execution);

    emitter.load_addr((uint64_t)&vu.PC, REG_64::RAX);
    emitter.MOV16_IMM_MEM(instr.get_jump_dest(), REG_64::RAX);

    //We're at the end of a program, so the next call doesn't need to know the previous state
    emitter.load_addr((uint64_t)&prev_pc, REG_64::RAX);
    emitter.MOV32_IMM_MEM(0xFFFFFFFF, REG_64::RAX);

    end_of_program = true;
}

void VU_JIT64::save_pc(VectorUnit &vu, IR::Instruction &instr)
{
    emitter.load_addr((uint64_t)&prev_pc, REG_64::RAX);
    emitter.MOV32_IMM_MEM(instr.get_jump_dest(), REG_64::RAX);
}

void VU_JIT64::save_pipeline_state(VectorUnit &vu, IR::Instruction &instr)
{
    emitter.load_addr((uint64_t)&vu.pipeline_state[0], REG_64::RAX);
    emitter.MOV64_OI(instr.get_source(), REG_64::R15);
    emitter.MOV64_TO_MEM(REG_64::R15, REG_64::RAX);

    emitter.load_addr((uint64_t)&vu.pipeline_state[1], REG_64::RAX);
    emitter.MOV64_OI(instr.get_source2(), REG_64::R15);
    emitter.MOV64_TO_MEM(REG_64::R15, REG_64::RAX);
}

void VU_JIT64::move_delayed_branch(VectorUnit &vu, IR::Instruction &instr)
{
    //Copy over delayed branch information in to immediate branch information
    //The values should be available at compile time, at least the delay ones
    //We will need to check the actual branch ones at execution time
    emitter.load_addr((uint64_t)&vu_branch_dest, REG_64::RAX);
    emitter.load_addr((uint64_t)&vu_branch_delay_dest, REG_64::R15);
    emitter.MOV16_FROM_MEM(REG_64::R15, REG_64::R15);
    emitter.MOV16_TO_MEM(REG_64::R15, REG_64::RAX);

    emitter.load_addr((uint64_t)&vu_branch_fail_dest, REG_64::RAX);
    emitter.load_addr((uint64_t)&vu_branch_delay_fail_dest, REG_64::R15);
    emitter.MOV16_FROM_MEM(REG_64::R15, REG_64::R15);
    emitter.MOV16_TO_MEM(REG_64::R15, REG_64::RAX);

    emitter.load_addr((uint64_t)&vu.branch_on, REG_64::RAX);
    emitter.load_addr((uint64_t)&vu.branch_on_delay, REG_64::R15);
    emitter.MOV8_FROM_MEM(REG_64::R15, REG_64::R15);
    emitter.MOV8_TO_MEM(REG_64::R15, REG_64::RAX);

    vu_branch = true;
}

int VU_JIT64::search_for_register(AllocReg *regs, int vu_reg)
{
    //Returns the index of either a free register or the oldest allocated register, depending on availability
    int reg = -1;
    int age = 0;
    for (int i = 0; i < 16; i++)
    {
        if (regs[i].locked)
            continue;

        if (!regs[i].used)
            return i;

        if (regs[i].age > age)
        {
            reg = i;
            age = regs[i].age;
        }
    }
    return reg;
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

    int reg = search_for_register(int_regs, vi_reg);

    if (int_regs[reg].used && int_regs[reg].modified && int_regs[reg].vu_reg)
    {
        //printf("[VU_JIT64] Flushing int reg %d! (old int reg: %d)\n", reg, int_regs[reg].vu_reg);
        int old_vi_reg = int_regs[reg].vu_reg;
        emitter.load_addr((uint64_t)&vu.int_gpr[old_vi_reg], REG_64::RAX);
        emitter.MOV64_TO_MEM((REG_64)reg, REG_64::RAX);
    }

    //printf("[VU_JIT64] Allocating int reg %d (vi%d)\n", reg, vi_reg);

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
    if (state == REG_STATE::SCRATCHPAD)
        return alloc_sse_scratchpad(vu, vf_reg);

    //If the register is already used, return it
    for (int i = 0; i < 16; i++)
    {
        if (xmm_regs[i].used && xmm_regs[i].vu_reg == vf_reg)
        {
            if (state == REG_STATE::WRITE || state == REG_STATE::READ_WRITE)
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

    int xmm = search_for_register(xmm_regs, vf_reg);

    //If the chosen register is used, flush it back to the VU state.
    if (xmm_regs[xmm].used && xmm_regs[xmm].modified && xmm_regs[xmm].vu_reg)
    {
        //printf("[VU_JIT64] Flushing xmm reg %d! (vf%d)\n", xmm, xmm_regs[xmm].vu_reg);
        int old_vf_reg = xmm_regs[xmm].vu_reg;
        emitter.load_addr(get_vf_addr(vu, old_vf_reg), REG_64::RAX);
        emitter.MOVAPS_TO_MEM((REG_64)xmm, REG_64::RAX);
    }

    //printf("[VU_JIT64] Allocating xmm reg %d (vf%d)\n", xmm, vf_reg);

    if (state != REG_STATE::WRITE)
    {
        //Store the VU state register inside the newly allocated XMM register.
        //printf("[VU_JIT64] Loading xmm reg with VU state\n");
        emitter.load_addr(get_vf_addr(vu, vf_reg), REG_64::RAX);
        emitter.MOVAPS_FROM_MEM(REG_64::RAX, (REG_64)xmm);
    }

    xmm_regs[xmm].modified = state != REG_STATE::READ;

    xmm_regs[xmm].vu_reg = vf_reg;
    xmm_regs[xmm].used = true;
    xmm_regs[xmm].age = 0;
    set_clamping(xmm, true, 0xF);

    return (REG_64)xmm;
}

REG_64 VU_JIT64::alloc_sse_scratchpad(VectorUnit &vu, int vf_reg)
{
    //Get a free register
    int xmm = search_for_register(xmm_regs, vf_reg);
    if (xmm_regs[xmm].used && xmm_regs[xmm].modified && xmm_regs[xmm].vu_reg)
    {
        //printf("[VU_JIT64] Flushing xmm reg %d! (vf%d)\n", xmm, xmm_regs[xmm].vu_reg);
        int old_vf_reg = xmm_regs[xmm].vu_reg;
        emitter.load_addr(get_vf_addr(vu, old_vf_reg), REG_64::RAX);
        emitter.MOVAPS_TO_MEM((REG_64)xmm, REG_64::RAX);
        xmm_regs[xmm].used = false;
    }

    bool found = false;
    for (int i = 0; i < 16; i++)
    {
        if (xmm_regs[i].used && xmm_regs[i].vu_reg == vf_reg)
        {
            found = true;
            break;
        }
    }

    if (found)
        emitter.MOVAPS_REG((REG_64)vf_reg, (REG_64)xmm);
    else
    {
        emitter.load_addr(get_vf_addr(vu, vf_reg), REG_64::RAX);
        emitter.MOVAPS_FROM_MEM(REG_64::RAX, (REG_64)xmm);
    }

    xmm_regs[xmm].modified = false;
    xmm_regs[xmm].vu_reg = vf_reg;
    xmm_regs[xmm].used = true;
    xmm_regs[xmm].age = 0;

    return (REG_64)xmm;
}


void VU_JIT64::set_clamping(int xmmreg, bool value, uint8_t field)
{
    if (xmm_regs[xmmreg].vu_reg)
    {
        if (value == false)
            xmm_regs[xmmreg].needs_clamping &= ~field;
        else
            xmm_regs[xmmreg].needs_clamping |= field;
    }
    else
        xmm_regs[xmmreg].needs_clamping = false;
}

bool VU_JIT64::needs_clamping(int xmmreg, uint8_t field)
{
    return xmm_regs[xmmreg].needs_clamping & field;
}


void VU_JIT64::flush_regs(VectorUnit &vu)
{
    //Store the contents of all allocated x64 registers into the VU state.
    //printf("[VU_JIT64] Flushing regs\n");
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
        if (xmm_regs[i].used && xmm_regs[i].vu_reg == vf_reg && vf_reg)
        {
            emitter.load_addr(get_vf_addr(vu, vf_reg), REG_64::RAX);
            emitter.MOVAPS_TO_MEM((REG_64)i, REG_64::RAX);

            xmm_regs[i].used = false;
            xmm_regs[i].age = 0;
            break;
        }
    }
}

void VU_JIT64::create_prologue_block()
{
    jit_block.clear();

    emit_prologue();

    emitter.SUB64_REG_IMM(0x28, REG_64::RSP);
    emitter.load_addr((uint64_t)&exec_block_vu, REG_64::RAX);
    emitter.CALL_INDIR(REG_64::RAX);
    emitter.CALL_INDIR(REG_64::RAX);
    emitter.ADD64_REG_IMM(0x28, REG_64::RSP);

    emit_epilogue();
    emitter.RET();

    VUBlockState state;
    state.pc = 0xFFFF;
    state.prev_pc = 0xFFFE;
    state.param1 = 0;
    state.param2 = 0;
    state.program = 0;

    jit_block.print_block();

    prologue_block = (VUJitPrologue)jit_heap.insert_block(state, &jit_block)->code_start;
}

void VU_JIT64::emit_prologue()
{
    emitter.PUSH(REG_64::RBP);
    emitter.MOV64_MR(REG_64::RSP, REG_64::RBP);
    emitter.PUSH(REG_64::RBX);
    emitter.PUSH(REG_64::R12);
    emitter.PUSH(REG_64::R13);
    emitter.PUSH(REG_64::R14);
    emitter.PUSH(REG_64::R15);
#ifdef _WIN32
    emitter.PUSH(REG_64::RDI);
    emitter.PUSH(REG_64::RSI);
#endif
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
        case IR::Opcode::StoreQuad:
            store_quad(vu, instr);
            break;
        case IR::Opcode::LoadQuadInc:
            load_quad_inc(vu, instr);
            break;
        case IR::Opcode::StoreQuadInc:
            store_quad_inc(vu, instr);
            break;
        case IR::Opcode::LoadQuadDec:
            load_quad_dec(vu, instr);
            break;
        case IR::Opcode::StoreQuadDec:
            store_quad_dec(vu, instr);
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
        case IR::Opcode::JumpAndLinkIndirect:
            jump_and_link_indirect(vu, instr);
            break;
        case IR::Opcode::BranchEqual:
            branch_equal(vu, instr);
            break;
        case IR::Opcode::BranchNotEqual:
            branch_not_equal(vu, instr);
            break;
        case IR::Opcode::BranchLessThanZero:
            branch_less_than_zero(vu, instr);
            break;
        case IR::Opcode::BranchGreaterThanZero:
            branch_greater_than_zero(vu, instr);
            break;
        case IR::Opcode::BranchLessOrEqualThanZero:
            branch_less_or_equal_than_zero(vu, instr);
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
        case IR::Opcode::VMaxVectors:
            max_vectors(vu, instr);
            break;
        case IR::Opcode::VMinVectorByScalar:
            min_vector_by_scalar(vu, instr);
            break;
        case IR::Opcode::VMinVectors:
            min_vectors(vu, instr);
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
        case IR::Opcode::VMsubVectors:
            msub_vectors(vu, instr);
            break;
        case IR::Opcode::VMsubVectorByScalar:
            msub_vector_by_scalar(vu, instr);
            break;
        case IR::Opcode::VMsubAccByScalar:
            msub_acc_by_scalar(vu, instr);
            break;
        case IR::Opcode::VOpMsub:
            opmsub(vu, instr);
            break;
        case IR::Opcode::VOpMula:
            opmula(vu, instr);
            break;
        case IR::Opcode::VClip:
            clip(vu, instr);
            break;
        case IR::Opcode::VDiv:
            div(vu, instr);
            break;
        case IR::Opcode::VRsqrt:
            rsqrt(vu, instr);
            break;
        case IR::Opcode::VFixedToFloat0:
            fixed_to_float(vu, instr, 0);
            break;
        case IR::Opcode::VFixedToFloat4:
            fixed_to_float(vu, instr, 1);
            break;
        case IR::Opcode::VFixedToFloat12:
            fixed_to_float(vu, instr, 2);
            break;
        case IR::Opcode::VFixedToFloat15:
            fixed_to_float(vu, instr, 3);
            break;
        case IR::Opcode::VFloatToFixed0:
            float_to_fixed(vu, instr, 0);
            break;
        case IR::Opcode::VFloatToFixed4:
            float_to_fixed(vu, instr, 1);
            break;
        case IR::Opcode::VFloatToFixed12:
            float_to_fixed(vu, instr, 2);
            break;
        case IR::Opcode::VFloatToFixed15:
            float_to_fixed(vu, instr, 3);
            break;
        case IR::Opcode::AndInt:
            and_int(vu, instr);
            break;
        case IR::Opcode::OrInt:
            or_int(vu, instr);
            break;
        case IR::Opcode::AddIntReg:
            add_int_reg(vu, instr);
            break;
        case IR::Opcode::SubIntReg:
            sub_int_reg(vu, instr);
            break;
        case IR::Opcode::AddUnsignedImm:
            add_unsigned_imm(vu, instr);
            break;
        case IR::Opcode::SubUnsignedImm:
            sub_unsigned_imm(vu, instr);
            break;
        case IR::Opcode::VMoveToInt:
            move_to_int(vu, instr);
            break;
        case IR::Opcode::VMoveFromInt:
            move_from_int(vu, instr);
            break;
        case IR::Opcode::VMoveFloat:
            move_float(vu, instr);
            break;
        case IR::Opcode::VMoveRotatedFloat:
            move_rotated_float(vu, instr);
            break;
        case IR::Opcode::VMacEq:
            mac_eq(vu, instr);
            break;
        case IR::Opcode::VMacAnd:
            mac_and(vu, instr);
            break;
        case IR::Opcode::SetClipFlags:
            set_clip_flags(vu, instr);
            break;
        case IR::Opcode::GetClipFlags:
            get_clip_flags(vu, instr);
            break;
        case IR::Opcode::AndClipFlags:
            and_clip_flags(vu, instr);
            break;
        case IR::Opcode::OrClipFlags:
            or_clip_flags(vu, instr);
            break;
        case IR::Opcode::AndStatFlags:
            and_stat_flags(vu, instr);
            break;
        case IR::Opcode::VEleng:
            eleng(vu, instr);
            break;
        case IR::Opcode::VErleng:
            erleng(vu, instr);
            break;
        case IR::Opcode::VESqrt:
            esqrt(vu, instr);
            break;
        case IR::Opcode::VERsqrt:
            ersqrt(vu, instr);
            break;
        case IR::Opcode::VRInit:
            rinit(vu, instr);
            break;
        case IR::Opcode::VMoveFromP:
            move_from_p(vu, instr);
            break;
        case IR::Opcode::BackupVF:
            backup_vf(vu, instr);
            break;
        case IR::Opcode::RestoreVF:
            restore_vf(vu, instr);
            break;
        case IR::Opcode::BackupVI:
            backup_vi(vu, instr);
            break;
        case IR::Opcode::ClearIntDelay:
            clear_int_delay(vu, instr);
            break;
        case IR::Opcode::UpdateQ:
            update_q(vu, instr);
            break;
        case IR::Opcode::UpdateP:
            update_p(vu, instr);
            break;
        case IR::Opcode::UpdateMacFlags:
            should_update_mac = true;
            break;
        case IR::Opcode::UpdateMacPipeline:
            update_mac_pipeline(vu, instr);
            break;
        case IR::Opcode::MoveXTOP:
            move_xtop(vu, instr);
            break;
        case IR::Opcode::MoveXITOP:
            move_xitop(vu, instr);
            break;
        case IR::Opcode::Xgkick:
            xgkick(vu, instr);
            break;
        case IR::Opcode::UpdateXgkick:
            update_xgkick(vu, instr);
            break;
        case IR::Opcode::Stop:
            stop(vu, instr);
            break;
        case IR::Opcode::StopTBit:
            stop_by_tbit(vu, instr);
            break;
        case IR::Opcode::SavePC:
            save_pc(vu, instr);
            break;
        case IR::Opcode::SavePipelineState:
            save_pipeline_state(vu, instr);
            break;
        case IR::Opcode::MoveDelayedBranch:
            move_delayed_branch(vu, instr);
            break;
        case IR::Opcode::FallbackInterpreter:
            fallback_interpreter(vu, instr);
            break;
        default:
            Errors::die("[VU_JIT64] Unknown IR instruction %d", instr.op);
    }
}

VUJitBlockRecord* VU_JIT64::recompile_block(VectorUnit& vu, IR::Block& block)
{
    jit_block.clear();

    vu_branch = false;
    end_of_program = false;
    cycle_count = block.get_cycle_count();

    //Prologue
    emitter.PUSH(REG_64::RBP);
    emitter.MOV64_MR(REG_64::RSP, REG_64::RBP);

    while (block.get_instruction_count() > 0)
    {
        IR::Instruction instr = block.get_next_instr();
        emit_instruction(vu, instr);
    }

    if (vu_branch)
        handle_branch(vu);
    else
        cleanup_recompiler(vu, true);

    return jit_heap.insert_block(VUBlockState{vu.get_PC(), prev_pc, current_program, vu.pipeline_state[0], vu.pipeline_state[1]}, &jit_block);
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

    //Mask PC because devs like to branch outside of memory...
    emitter.load_addr((uint64_t)&vu.PC, REG_64::R15);
    emitter.MOV16_FROM_MEM(REG_64::R15, REG_64::RAX);
    emitter.AND32_EAX(vu.mem_mask);
    emitter.MOV16_TO_MEM(REG_64::RAX, REG_64::R15);

    //Return the amount of cycles to update the VUs with
    emitter.MOV16_REG_IMM(cycle_count, REG_64::RAX);
    emitter.load_addr((uint64_t)&cycle_count, REG_64::R15);
    emitter.MOV16_TO_MEM(REG_64::RAX, REG_64::R15);

    //Epilogue
    emitter.POP(REG_64::RBP);
    emitter.RET();
}

void VU_JIT64::emit_epilogue()
{
#ifdef _WIN32
    emitter.POP(REG_64::RSI);
    emitter.POP(REG_64::RDI);
#endif
    emitter.POP(REG_64::R15);
    emitter.POP(REG_64::R14);
    emitter.POP(REG_64::R13);
    emitter.POP(REG_64::R12);
    emitter.POP(REG_64::RBX);
    emitter.POP(REG_64::RBP);
}

void VU_JIT64::prepare_abi(VectorUnit& vu, uint64_t value)
{
#ifdef _WIN32
    const static REG_64 regs[] = { RCX, RDX, R8, R9 };

    if (abi_int_count >= 4)
        Errors::die("[VU_JIT64] ABI integer arguments exceeded 4!");
#else
    const static REG_64 regs[] = {RDI, RSI, RDX, RCX, R8, R9};

    if (abi_int_count >= 6)
        Errors::die("[VU_JIT64] ABI integer arguments exceeded 6!");
#endif    

    REG_64 arg = regs[abi_int_count];

    //If the chosen integer argument is being used, flush it back to the VU state
    if (int_regs[arg].used)
    {
        int vi_reg = int_regs[arg].vu_reg;
        if (int_regs[arg].modified)
        {
            emitter.load_addr((uint64_t)&vu.int_gpr[vi_reg], REG_64::RAX);
            emitter.MOV64_TO_MEM(arg, REG_64::RAX);
        }
        int_regs[arg].used = false;
        int_regs[arg].age = 0;
    }
    emitter.load_addr(value, regs[abi_int_count]);
    abi_int_count++;
}

void VU_JIT64::call_abi_func(uint64_t addr)
{
    const static REG_64 saved_regs[] = {RCX, RDX, R8, R9, R10, R11};
    int total_regs = sizeof(saved_regs) / sizeof(REG_64);
    int regs_used = 0;
    int sp_offset = 0;

    for (int i = 0; i < total_regs; i++)
    {
        if (int_regs[saved_regs[i]].used)
        {
            emitter.PUSH(saved_regs[i]);
            regs_used++;
        }
    }
  
    //Align stack pointer on 16-byte boundary
    if (regs_used & 1)
        sp_offset += 8;
#ifdef _WIN32
    //x64 Windows requires a 32-byte "shadow region" to store the four argument registers, even if not all are used
    sp_offset += 32;
#endif
    emitter.MOV64_OI(addr, REG_64::RAX);
  
    if (sp_offset)
        emitter.SUB64_REG_IMM(sp_offset, REG_64::RSP);
    emitter.CALL_INDIR(REG_64::RAX);
    if (sp_offset)
        emitter.ADD64_REG_IMM(sp_offset, REG_64::RSP);

    for (int i = total_regs - 1; i >= 0; i--)
    {
        if (int_regs[saved_regs[i]].used)
            emitter.POP(saved_regs[i]);
    }
    abi_int_count = 0;
    abi_xmm_count = 0;
}

void VU_JIT64::fallback_interpreter(VectorUnit &vu, IR::Instruction &instr)
{
    flush_regs(vu);
    for (int i = 0; i < 16; i++)
    {
        int_regs[i].age = 0;
        int_regs[i].used = false;
        xmm_regs[i].age = 0;
        xmm_regs[i].used = false;
    }

    uint32_t instr_word = instr.get_source();

    //VU_Interpreter::upper/lower
    prepare_abi(vu, (uint64_t)&vu);
    prepare_abi(vu, instr_word);

    bool is_upper = instr.get_field();

    if (is_upper)
        call_abi_func((uint64_t)&interpreter_upper);
    else
        call_abi_func((uint64_t)&interpreter_lower);
}

extern "C"
uint8_t* exec_block_vu(VU_JIT64& jit, VectorUnit& vu)
{
    //fprintf(stderr, "[VU_JIT64] Executing block at $%04X, Prev PC $%04X Current Program %08X: recompiling\n", vu.PC, jit.prev_pc, jit.current_program);
    VUJitBlockRecord* found_block = jit.jit_heap.find_block(VUBlockState
        { vu.get_PC(), jit.prev_pc, jit.current_program, vu.pipeline_state[0], vu.pipeline_state[1] });

    if (!found_block)
    {
        //fprintf(stderr, "[VU_JIT64] Block not found at $%04X, Prev PC $%04X Current Program %08X: recompiling\n", vu.PC, jit.prev_pc, jit.current_program);
        IR::Block block = jit.ir.translate(vu, vu.get_instr_mem(), jit.prev_pc);
        found_block = jit.recompile_block(vu, block);
    }
    return (uint8_t*)found_block->code_start;
}

uint16_t VU_JIT64::run(VectorUnit& vu)
{
    //Need to check we have space on the heap before running any recompiled code
    //Resetting the heap during recompilation wipes out the prologue/epilogue!
    //Checks for maximum 5mb block size of space before continuing
    if (jit_heap.heap_is_full())
    {
        printf("[VU JIT] Not enough room for new blocks, clearing cache\n");
        jit_heap.flush_all_blocks();
        create_prologue_block();
    }

    prologue_block(*this, vu);
    
    return cycle_count;
}
