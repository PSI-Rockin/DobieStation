#include <cmath>
#include <algorithm>

#include "ee_jit64.hpp"
#include "emotioninterpreter.hpp"
#include "vu.hpp"
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

#ifdef _WIN32
extern "C" void run_ee_jit();
#endif

EE_JIT64::EE_JIT64() : emitter(&cache)
{
    //TODO
}


void EE_JIT64::reset(bool clear_cache)
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

    // NOTE: Some emitted operations don't like using certain modes with RBP, so tentatively locking this register
    // for debugging purposes
    int_regs[REG_64::RBP].locked = true;

    if (clear_cache)
        cache.flush_all_blocks();

    //ir.reset_instr_info();

    //prev_pc = 0xFFFFFFFF;
    //current_program = 0;
}

extern "C"
uint8_t* exec_block_ee(EE_JIT64& jit, EmotionEngine& ee)
{
    //printf("[EE_JIT64] Executing block at $%08X\n", ee.PC);
    if (jit.cache.find_block(BlockState{ ee.PC, 0, 0, 0, 0 }) == nullptr)
    {
        printf("[EE_JIT64] Block not found at $%08X: recompiling\n", ee.PC);
        IR::Block block = jit.ir.translate(ee);
        jit.recompile_block(ee, block);
    }
    return jit.cache.get_current_block_start();
}

void interpreter(EmotionEngine& ee, uint32_t instr)
{
    EmotionInterpreter::interpret(ee, instr);
}

uint16_t EE_JIT64::run(EmotionEngine& ee)
{
#ifdef _MSC_VER
    run_ee_jit();
#else
    uint8_t* block = exec_block_ee(*this, ee);

    __asm__ volatile (
        "callq *%0"
        :
    : "r" (block)
        );
#endif

    return cycle_count;
}

void EE_JIT64::recompile_block(EmotionEngine& ee, IR::Block& block)
{
    ee_branch = false;
    likely_branch = false;
    cache.alloc_block(BlockState{ ee.get_PC(), 0, 0, 0, 0 });

    //Return the amount of cycles to update the EE with
    emitter.load_addr((uint64_t)&cycle_count, REG_64::RAX);
    emitter.MOV16_IMM_MEM(block.get_cycle_count(), REG_64::RAX);

    //Prologue
#ifndef _MSC_VER
    emit_prologue();
#endif
    emitter.PUSH(REG_64::RBP);
    emitter.MOV64_MR(REG_64::RSP, REG_64::RBP);

    while (block.get_instruction_count() > 0 && !likely_branch)
    {
        IR::Instruction instr = block.get_next_instr();
        emit_instruction(ee, instr);
    }

    if (likely_branch)
        handle_branch_likely(ee, block);
    else
        cleanup_recompiler(ee, true);

    //Switch the block's privileges from RW to RX.
    cache.set_current_block_rx();
    //cache.print_current_block();
    //cache.print_literal_pool();
}

void EE_JIT64::emit_instruction(EmotionEngine &ee, IR::Instruction &instr)
{
    switch (instr.op)
    {
        case IR::Opcode::AddUnsignedImm:
            add_unsigned_imm(ee, instr);
            break;
        case IR::Opcode::AndInt:
            and_int(ee, instr);
            break;
        case IR::Opcode::BranchCop0:
            branch_cop0(ee, instr);
            break;
        case IR::Opcode::BranchCop1:
            branch_cop1(ee, instr);
            break;
        case IR::Opcode::BranchCop2:
            branch_cop2(ee, instr);
            break;
        case IR::Opcode::BranchEqual:
            branch_equal(ee, instr);
            break;
        case IR::Opcode::BranchGreaterThanOrEqualZero:
            branch_greater_than_or_equal_zero(ee, instr);
            break;
        case IR::Opcode::BranchGreaterThanZero:
            branch_greater_than_zero(ee, instr);
            break;
        case IR::Opcode::BranchLessThanOrEqualZero:
            branch_less_than_or_equal_zero(ee, instr);
            break;
        case IR::Opcode::BranchLessThanZero:
            branch_less_than_zero(ee, instr);
            break;
        case IR::Opcode::BranchNotEqual:
            branch_not_equal(ee, instr);
            break;
        case IR::Opcode::ExceptionReturn:
            exception_return(ee, instr);
            break;
        case IR::Opcode::Jump:
            jump(ee, instr);
            break;
        case IR::Opcode::JumpIndirect:
            jump_indirect(ee, instr);
            break;
        case IR::Opcode::LoadUpperImmediate:
            load_upper_immediate(ee, instr);
            break;
        case IR::Opcode::OrInt:
            or_int(ee, instr);
            break;
        case IR::Opcode::SetOnLessThanImmediate:
            set_on_less_than_immediate(ee, instr);
            break;
        case IR::Opcode::SetOnLessThanImmediateUnsigned:
            set_on_less_than_immediate_unsigned(ee, instr);
            break;
        case IR::ShiftLeftLogical:
            shift_left_logical(ee, instr);
            break;
        case IR::ShiftLeftLogicalVariable:
            shift_left_logical_variable(ee, instr);
            break;
        case IR::ShiftRightArithmetic:
            shift_right_arithmetic(ee, instr);
            break;
        case IR::ShiftRightArithmeticVariable:
            shift_right_arithmetic_variable(ee, instr);
            break;
        case IR::ShiftRightLogical:
            shift_right_logical(ee, instr);
            break;
        case IR::ShiftRightLogicalVariable:
            shift_right_logical_variable(ee, instr);
            break;
        case IR::Opcode::SystemCall:
            system_call(ee, instr);
            break;
        case IR::Opcode::VCallMS:
            vcall_ms(ee, instr);
            break;
        case IR::Opcode::VCallMSR:
            vcall_msr(ee, instr);
            break;
        case IR::Opcode::XorInt:
            xor_int(ee, instr);
            break;
        case IR::Opcode::FallbackInterpreter:
            fallback_interpreter(ee, instr);
            break;
        default:
            Errors::die("[EE_JIT64] Unknown IR instruction %d", instr.op);
    }
}

void EE_JIT64::prepare_abi(EmotionEngine& ee, uint64_t value)
{
#ifdef _WIN32
    const static REG_64 regs[] = { RCX, RDX, R8, R9 };

    if (abi_int_count >= 4)
        Errors::die("[EE_JIT64] ABI integer arguments exceeded 4!");
#else
    const static REG_64 regs[] = { RDI, RSI, RDX, RCX, R8, R9 };

    if (abi_int_count >= 6)
        Errors::die("[EE_JIT64] ABI integer arguments exceeded 6!");
#endif    

    REG_64 arg = regs[abi_int_count];

    //If the chosen integer argument is being used, flush it back to the EE state
    if (int_regs[arg].used)
    {
        int vi_reg = int_regs[arg].reg;
        flush_int_reg(ee, vi_reg);
        int_regs[arg].used = false;
        int_regs[arg].age = 0;
    }
    emitter.load_addr(value, regs[abi_int_count]);
    abi_int_count++;
}

void EE_JIT64::prepare_abi_reg(EmotionEngine& ee, REG_64 reg)
{
#ifdef _WIN32
    const static REG_64 regs[] = { RCX, RDX, R8, R9 };

    if (abi_int_count >= 4)
        Errors::die("[EE_JIT64] ABI integer arguments exceeded 4!");
#else
    const static REG_64 regs[] = { RDI, RSI, RDX, RCX, R8, R9 };

    if (abi_int_count >= 6)
        Errors::die("[EE_JIT64] ABI integer arguments exceeded 6!");
#endif    

    REG_64 arg = regs[abi_int_count];

    //If the chosen integer argument is being used, flush it back to the EE state
    if (int_regs[arg].used)
    {
        int vi_reg = int_regs[arg].reg;
        flush_int_reg(ee, vi_reg);
        int_regs[arg].used = false;
        int_regs[arg].age = 0;
    }
    emitter.MOV64_MR(reg, regs[abi_int_count]);
    abi_int_count++;
}

void EE_JIT64::call_abi_func(uint64_t addr)
{
    const static REG_64 saved_regs[] = { RCX, RDX, R8, R9, R10, R11 };
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

int EE_JIT64::search_for_register(AllocReg *regs)
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

void EE_JIT64::alloc_gpr_reg(EmotionEngine& ee, int gpr_reg, REG_64 destination, REG_STATE state)
{
    if (gpr_reg >= 32)
        Errors::die("[EE_JIT64] Alloc Int error: gpr_reg == %d", gpr_reg);

    if (int_regs[destination].locked)
        Errors::die("[EE_JIT64] Alloc Int error: Attempted to allocate locked x64 register %d", destination);

    // Do nothing if the EE register is already inside our x86 register
    if (int_regs[destination].used && int_regs[destination].is_gpr_reg && int_regs[destination].reg == gpr_reg)
    {
        int_regs[destination].age = 0;
        return;
    }

    flush_int_reg(ee, destination);

    if (state == REG_STATE::SCRATCHPAD)
    {
        int_regs[destination].modified = false;
        int_regs[destination].used = true;
        int_regs[destination].age = 0;
        int_regs[destination].reg = -1;
        return;
    }

    if (state != REG_STATE::WRITE)
    {
        int reg = -1;

        for (int i = 0; i < 16; ++i)
        {
            if (int_regs[i].used && int_regs[i].reg == gpr_reg && int_regs[i].is_gpr_reg)
            {
                reg = i;
                break;
            }
        }

        if (reg >= 0)
        {
            emitter.MOV64_MR((REG_64)reg, destination);
        }
        else
        {
            emitter.load_addr((uint64_t)get_gpr_addr(ee, (EE_NormalReg)gpr_reg), REG_64::RAX);
            emitter.MOV64_FROM_MEM(REG_64::RAX, (REG_64)destination);
        }
    }

    int_regs[destination].modified = state != REG_STATE::READ;
    int_regs[destination].reg = gpr_reg;
    int_regs[destination].is_gpr_reg = true;
    int_regs[destination].used = true;
    int_regs[destination].age = 0;
}

REG_64 EE_JIT64::alloc_gpr_reg(EmotionEngine& ee, int gpr_reg, REG_STATE state)
{
    if (gpr_reg >= 32)
        Errors::die("[EE_JIT64] Alloc Int error: gpr_reg == %d", gpr_reg);

    for (int i = 0; i < 16; i++)
    {
        if (int_regs[i].used && int_regs[i].reg == gpr_reg && int_regs[i].is_gpr_reg)
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

    int reg = search_for_register(int_regs);
    flush_int_reg(ee, reg);

    //printf("[EE_JIT64] Allocating GPR reg %d (EEREG %02d)\n", reg, gpr_reg);

    if (state == REG_STATE::SCRATCHPAD)
    {
        int_regs[reg].modified = false;
        int_regs[reg].used = true;
        int_regs[reg].age = 0;
        int_regs[reg].reg = -1;
        return (REG_64)reg;
    }

    if (state != REG_STATE::WRITE)
    {
        emitter.load_addr((uint64_t)get_gpr_addr(ee, (EE_NormalReg)gpr_reg), REG_64::RAX);
        emitter.MOV64_FROM_MEM(REG_64::RAX, (REG_64)reg);
    }

    int_regs[reg].modified = state != REG_STATE::READ;
    int_regs[reg].reg = gpr_reg;
    int_regs[reg].is_gpr_reg = true;
    int_regs[reg].used = true;
    int_regs[reg].age = 0;

    return (REG_64)reg;
}

REG_64 EE_JIT64::alloc_vi_reg(EmotionEngine& ee, int vi_reg, REG_STATE state)
{
    if (vi_reg >= 16)
        Errors::die("[EE_JIT64] Alloc Int error: vi_reg == %d", vi_reg);

    for (int i = 0; i < 16; i++)
    {
        if (int_regs[i].used && int_regs[i].reg == vi_reg && !int_regs[i].is_gpr_reg)
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

    int reg = search_for_register(int_regs);

    if (int_regs[reg].used && int_regs[reg].modified && int_regs[reg].reg)
    {
        flush_int_reg(ee, reg);
    }

    //printf("[EE_JIT64] Allocating vu0i reg %d (vi%02d)\n", reg, vi_reg);

    if (state != REG_STATE::WRITE)
    {
        emitter.load_addr((uint64_t)get_vi_addr(ee, (EE_NormalReg)vi_reg), REG_64::RAX);
        emitter.MOV64_FROM_MEM(REG_64::RAX, (REG_64)reg);
    }

    int_regs[reg].modified = state != REG_STATE::READ;

    int_regs[reg].reg = vi_reg;
    int_regs[reg].is_gpr_reg = false;
    int_regs[reg].used = true;
    int_regs[reg].age = 0;

    return (REG_64)reg;
}

REG_64 EE_JIT64::alloc_fpu_reg(EmotionEngine& ee, int fpu_reg, REG_STATE state)
{
    if (fpu_reg >= 32)
        Errors::die("[EE_JIT64] Alloc Float error: fpu_reg == %d", fpu_reg);

    for (int i = 0; i < 16; i++)
    {
        if (xmm_regs[i].used && xmm_regs[i].reg == fpu_reg && xmm_regs[i].is_fpu_reg)
        {
            if (state != REG_STATE::READ)
                xmm_regs[i].modified = true;
            xmm_regs[i].age = 0;
            return (REG_64)i;
        }
    }

    for (int i = 0; i < 16; i++)
    {
        if (xmm_regs[i].used)
            xmm_regs[i].age++;
    }

    int reg = search_for_register(xmm_regs);

    if (xmm_regs[reg].used && xmm_regs[reg].modified)
    {
        flush_xmm_reg(ee, reg);
    }

    //printf("[EE_JIT64] Allocating FPU reg %d (f%02d)\n", reg, fpu_reg);

    if (state != REG_STATE::WRITE)
    {
        emitter.load_addr((uint64_t)get_fpu_addr(ee, fpu_reg), REG_64::RAX);
        emitter.MOV32_FROM_MEM(REG_64::RAX, REG_64::RAX);
        emitter.MOVD_TO_XMM(REG_64::RAX, (REG_64)reg);
    }

    int_regs[reg].modified = state != REG_STATE::READ;

    int_regs[reg].reg = fpu_reg;
    int_regs[reg].is_fpu_reg = true;
    int_regs[reg].used = true;
    int_regs[reg].age = 0;

    return (REG_64)reg;
}

REG_64 EE_JIT64::alloc_vf_reg(EmotionEngine& ee, int vf_reg, REG_STATE state)
{
    if (vf_reg >= 32)
        Errors::die("[EE_JIT64] Alloc Float error: vf_reg == %d", vf_reg);

    for (int i = 0; i < 16; i++)
    {
        if (xmm_regs[i].used && xmm_regs[i].reg == vf_reg && !xmm_regs[i].is_fpu_reg)
        {
            if (state != REG_STATE::READ)
                xmm_regs[i].modified = true;
            xmm_regs[i].age = 0;
            return (REG_64)i;
        }
    }

    for (int i = 0; i < 16; i++)
    {
        if (xmm_regs[i].used)
            xmm_regs[i].age++;
    }

    int reg = search_for_register(xmm_regs);

    if (xmm_regs[reg].used && xmm_regs[reg].modified)
    {
        flush_xmm_reg(ee, reg);
    }

    //printf("[EE_JIT64] Allocating vu0f reg %d (f%02d)\n", reg, vf_reg);

    if (state != REG_STATE::WRITE)
    {
        // TODO: Broadcast VU0f state to XMM register
        Errors::die("[EE_JIT64] alloc_vf_reg: Allocating vu0f register not supported!");
    }

    int_regs[reg].modified = state != REG_STATE::READ;

    int_regs[reg].reg = vf_reg;
    int_regs[reg].is_fpu_reg = false;
    int_regs[reg].used = true;
    int_regs[reg].age = 0;

    return (REG_64)reg;
}

void EE_JIT64::flush_int_reg(EmotionEngine& ee, int reg)
{
    int old_int_reg = int_regs[reg].reg;
    if (int_regs[reg].used && int_regs[reg].modified)
    {
        //printf("[EE_JIT64] Flushing int reg %d! (old int reg: %d, was GPR reg?: %d)\n", reg, int_regs[reg].reg, int_regs[reg].is_gpr_reg);
        if (int_regs[reg].is_gpr_reg && old_int_reg)
        {
            emitter.load_addr((uint64_t)get_gpr_addr(ee, old_int_reg), REG_64::RAX);
            emitter.MOV64_TO_MEM((REG_64)reg, REG_64::RAX);
        }
        else if (!int_regs[reg].is_gpr_reg)
        {
            emitter.load_addr((uint64_t)get_vi_addr(ee, old_int_reg), REG_64::RAX);
            emitter.MOV16_TO_MEM((REG_64)reg, REG_64::RAX);
        }
    }
    int_regs[reg].used = false;
}

void EE_JIT64::flush_xmm_reg(EmotionEngine& ee, int reg)
{
    int old_xmm_reg = xmm_regs[reg].reg;
    if (xmm_regs[reg].used && xmm_regs[reg].modified)
    {
        //printf("[EE_JIT64] Flushing float reg %d! (old float reg: %d, was FPU reg?: %d)\n", reg, xmm_regs[reg].reg, xmm_regs[reg].is_fpu_reg);
        if (xmm_regs[reg].is_fpu_reg)
        {
            emitter.load_addr((uint64_t)get_fpu_addr(ee, old_xmm_reg), REG_64::RAX);
            emitter.MOVD_FROM_XMM((REG_64)reg, REG_64::R15);
            emitter.MOV32_TO_MEM(REG_64::R15, REG_64::RAX);
        }
        else if (!xmm_regs[reg].is_fpu_reg && old_xmm_reg)
        {
            emitter.load_addr((uint64_t)get_vf_addr(ee, old_xmm_reg), REG_64::RAX);
            emitter.MOVAPS_TO_MEM((REG_64)reg, REG_64::RAX);
        }
    }
}

void EE_JIT64::flush_regs(EmotionEngine& ee)
{
    //Store the contents of all allocated x64 registers into the EE state.
    //printf("[EE_JIT64] Flushing regs\n");
    for (int i = 0; i < 16; i++)
    {
        flush_xmm_reg(ee, i);
        flush_int_reg(ee, i);
    }
}

uint64_t EE_JIT64::get_vi_addr(const EmotionEngine&ee, int index) const
{
    //TODO
    return 0;
}

uint64_t EE_JIT64::get_vf_addr(const EmotionEngine&ee, int index) const
{
    //TODO
    return 0;
}

uint64_t EE_JIT64::get_fpu_addr(const EmotionEngine &ee, int index) const
{
    if (index < 32)
        return (uint64_t)(&ee.fpu->gpr[index]);

    switch (static_cast<FPU_SpecialReg>(index))
    {
        case FPU_SpecialReg::ACC:
            return (uint64_t)&ee.fpu->accumulator;
        default:
            Errors::die("[EE_JIT64] get_fpu_addr error: Unrecognized reg %d", index);
    }
    return 0;
}

uint64_t EE_JIT64::get_gpr_addr(const EmotionEngine &ee, int index) const
{
    if (index < 32)
        return (uint64_t)&ee.gpr[index * sizeof(uint128_t)];

    Errors::die("[EE_JIT64] get_gpr_addr error: Unrecognized reg %d", index);
    return 0;
}

void EE_JIT64::cleanup_recompiler(EmotionEngine& ee, bool clear_regs)
{
    flush_regs(ee);

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

    //Epilogue
    emitter.POP(REG_64::RBP);
#ifndef _MSC_VER
    emit_epilogue();
#endif
    emitter.RET();
}

void EE_JIT64::emit_prologue()
{
#ifdef _WIN32
    Errors::die("[EE_JIT64] emit_prologue not implemented for _WIN32");
#else
    emitter.PUSH(REG_64::RBX);
    emitter.PUSH(REG_64::R12);
    emitter.PUSH(REG_64::R13);
    emitter.PUSH(REG_64::R14);
    emitter.PUSH(REG_64::R15);
    emitter.PUSH(REG_64::RDI);
#endif
}


void EE_JIT64::emit_epilogue()
{
#ifdef _WIN32
    Errors::die("[EE_JIT64] emit_epilogue not implemented for _WIN32");
#else
    emitter.POP(REG_64::RDI);
    emitter.POP(REG_64::R15);
    emitter.POP(REG_64::R14);
    emitter.POP(REG_64::R13);
    emitter.POP(REG_64::R12);
    emitter.POP(REG_64::RBX);
#endif
}

void EE_JIT64::handle_branch_likely(EmotionEngine& ee, IR::Block& block)
{
    // Load the "branch happened" variable
    emitter.load_addr(reinterpret_cast<uint64_t>(&ee.branch_on), REG_64::RAX);
    emitter.MOV8_FROM_MEM(REG_64::RAX, REG_64::RAX);

    // Test variable. In the case that the variable is true,
    // we jump over the last instruction in the block in order
    // to "nullify" it before returning from the recompiled block.
    emitter.TEST8_REG(REG_64::RAX, REG_64::RAX);

    // Flush registers before nondeterministically executing the delay slot.
    flush_regs(ee);

    // Because we don't know where the branch will jump to, we defer it.
    // Once the "branch failure" case has been finished recompiling, we can rewrite the branch offset...
    uint8_t* offset_addr = emitter.JCC_NEAR_DEFERRED(ConditionCode::NZ);

    // flush the EE state back to the EE and return, not executing the delay slot
    cleanup_recompiler(ee, true);

    // ...Which we do here.
    emitter.set_jump_dest(offset_addr);

    // execute delay slot and flush EE state back to EE
    IR::Instruction instr = block.get_next_instr();
    emit_instruction(ee, instr);
    cleanup_recompiler(ee, true);
}

bool cop0_get_condition(Cop0& cop0)
{
    return cop0.get_condition();
}

void EE_JIT64::add_unsigned_imm(EmotionEngine& ee, IR::Instruction &instr)
{
    if (instr.get_dest() == instr.get_source())
    {
        REG_64 dest = alloc_gpr_reg(ee, instr.get_dest(), REG_STATE::READ_WRITE);

        // temp = (int32_t)((int16_t)immediate)
        // dest = (int64_t)(temp + dest)
        emitter.MOV16_REG_IMM(instr.get_source2(), REG_64::RAX);
        emitter.MOVSX16_TO_32(REG_64::RAX, REG_64::RAX);
        emitter.ADD32_REG(REG_64::RAX, dest);
        emitter.MOVSX32_TO_64(dest, dest);
    }
    else
    {
        REG_64 source = alloc_gpr_reg(ee, instr.get_source(), REG_STATE::READ);
        REG_64 dest = alloc_gpr_reg(ee, instr.get_dest(), REG_STATE::WRITE);

        // temp = (int32_t)((int16_t)immediate)
        // temp += source
        // dest = (int64_t)temp
        emitter.MOV16_REG_IMM(instr.get_source2(), REG_64::RAX);
        emitter.MOVSX16_TO_32(REG_64::RAX, REG_64::RAX);
        emitter.ADD32_REG(source, REG_64::RAX);
        emitter.MOV32_REG(REG_64::RAX, dest);
        emitter.MOVSX32_TO_64(dest, dest);
    }
}

void EE_JIT64::and_int(EmotionEngine& ee, IR::Instruction &instr)
{
    if (instr.get_dest() == instr.get_source())
    {
        REG_64 dest = alloc_gpr_reg(ee, instr.get_dest(), REG_STATE::READ_WRITE);

        // temp = (uint64_t)immediate
        // dest &= temp
        emitter.MOV64_OI(instr.get_source2(), REG_64::RAX);
        emitter.AND64_REG(REG_64::RAX, dest);
    }
    else
    {
        REG_64 source = alloc_gpr_reg(ee, instr.get_source(), REG_STATE::READ);
        REG_64 dest = alloc_gpr_reg(ee, instr.get_dest(), REG_STATE::WRITE);

        // dest = (uint64_t)immediate
        // dest &= source
        emitter.MOV64_OI(instr.get_source2(), dest);
        emitter.AND64_REG(source, dest);
    }
}

void EE_JIT64::branch_cop0(EmotionEngine& ee, IR::Instruction &instr)
{
    // Call cop0.get_condition to get value to compare
    prepare_abi(ee, reinterpret_cast<uint64_t>(&ee.cp0));

    call_abi_func((uint64_t)cop0_get_condition);

    // Conditionally move the success or failure destination into ee.PC
    emitter.MOV8_REG_IMM(instr.get_field(), REG_64::R15);
    emitter.CMP8_REG(REG_64::RAX, REG_64::R15);
    emitter.MOV32_REG_IMM(instr.get_jump_fail_dest(), REG_64::RAX);
    emitter.MOV32_REG_IMM(instr.get_jump_dest(), REG_64::R15);
    emitter.CMOVCC32_REG(ConditionCode::E, REG_64::R15, REG_64::RAX);
    emitter.load_addr((uint64_t)&ee.PC, REG_64::R15);
    emitter.MOV32_TO_MEM(REG_64::RAX, REG_64::R15);

    if (instr.get_is_likely())
    {
        likely_branch = true;

        // Use the result of the FLAGS register from the last compare to set branch_on,
        // this boolean is used in handle_likely_branch
        emitter.load_addr((uint64_t)&ee.branch_on, REG_64::RAX);
        emitter.SETCC_MEM(ConditionCode::E, REG_64::RAX);
    }
}

void EE_JIT64::branch_cop1(EmotionEngine& ee, IR::Instruction &instr)
{
    // Conditionally move the success or failure destination into ee.PC
    emitter.load_addr((uint64_t)&ee.fpu->control.condition, REG_64::RAX);
    emitter.MOV8_FROM_MEM(REG_64::RAX, REG_64::RAX);
    emitter.MOV8_REG_IMM(instr.get_field(), REG_64::R15);
    emitter.CMP8_REG(REG_64::RAX, REG_64::R15);
    emitter.MOV32_REG_IMM(instr.get_jump_fail_dest(), REG_64::RAX);
    emitter.MOV32_REG_IMM(instr.get_jump_dest(), REG_64::R15);
    emitter.CMOVCC32_REG(ConditionCode::E, REG_64::R15, REG_64::RAX);
    emitter.load_addr((uint64_t)&ee.PC, REG_64::R15);
    emitter.MOV32_TO_MEM(REG_64::RAX, REG_64::R15);

    if (instr.get_is_likely())
    {
        likely_branch = true;

        // Use the result of the FLAGS register from the last compare to set branch_on,
        // this boolean is used in handle_likely_branch
        emitter.load_addr((uint64_t)&ee.branch_on, REG_64::RAX);
        emitter.SETCC_MEM(ConditionCode::E, REG_64::RAX);
    }
}

void EE_JIT64::branch_cop2(EmotionEngine& ee, IR::Instruction &instr)
{
    Errors::die("[EE_JIT64] Branch COP2 not implemented!");
}

void EE_JIT64::branch_equal(EmotionEngine& ee, IR::Instruction &instr)
{
    REG_64 op1;
    REG_64 op2;

    op1 = alloc_gpr_reg(ee, instr.get_source(), REG_STATE::READ);
    op2 = alloc_gpr_reg(ee, instr.get_source2(), REG_STATE::READ);

    // Conditionally move the success or failure destination into ee.PC
    emitter.MOV32_REG_IMM(instr.get_jump_fail_dest(), REG_64::RAX);
    emitter.MOV32_REG_IMM(instr.get_jump_dest(), REG_64::R15);
    emitter.CMP64_REG(op2, op1);
    emitter.CMOVCC32_REG(ConditionCode::E, REG_64::R15, REG_64::RAX);
    emitter.load_addr((uint64_t)&ee.PC, REG_64::R15);
    emitter.MOV32_TO_MEM(REG_64::RAX, REG_64::R15);

    if (instr.get_is_likely())
    {
        likely_branch = true;

        // Use the result of the FLAGS register from the last compare to set branch_on,
        // this boolean is used in handle_likely_branch
        emitter.load_addr((uint64_t)&ee.branch_on, REG_64::RAX);
        emitter.SETCC_MEM(ConditionCode::E, REG_64::RAX);
    }
}

void EE_JIT64::branch_greater_than_or_equal_zero(EmotionEngine& ee, IR::Instruction &instr)
{
    REG_64 op1;
    op1 = alloc_gpr_reg(ee, instr.get_source(), REG_STATE::READ);

    if (instr.get_is_link())
    {
        // Set the link register
        emitter.load_addr((uint64_t)get_gpr_addr(ee, EE_NormalReg::ra), REG_64::RAX);
        emitter.MOV32_IMM_MEM(instr.get_return_addr(), REG_64::RAX);
    }

    // Conditionally move the success or failure destination into ee.PC
    emitter.MOV32_REG_IMM(instr.get_jump_fail_dest(), REG_64::RAX);
    emitter.MOV32_REG_IMM(instr.get_jump_dest(), REG_64::R15);
    emitter.TEST64_REG(op1, op1);
    emitter.CMOVCC32_REG(ConditionCode::NS, REG_64::R15, REG_64::RAX);
    emitter.load_addr((uint64_t)&ee.PC, REG_64::R15);
    emitter.MOV32_TO_MEM(REG_64::RAX, REG_64::R15);

    if (instr.get_is_likely())
    {
        likely_branch = true;

        // Use the result of the FLAGS register from the last compare to set branch_on,
        // this boolean is used in handle_likely_branch
        emitter.load_addr((uint64_t)&ee.branch_on, REG_64::RAX);
        emitter.SETCC_MEM(ConditionCode::NS, REG_64::RAX);
    }
}

void EE_JIT64::branch_greater_than_zero(EmotionEngine& ee, IR::Instruction &instr)
{
    REG_64 op1;
    op1 = alloc_gpr_reg(ee, instr.get_source(), REG_STATE::READ);

    // Conditionally move the success or failure destination into ee.PC
    emitter.MOV64_OI(0, REG_64::RAX);
    emitter.CMP64_REG(REG_64::RAX, op1);
    emitter.MOV32_REG_IMM(instr.get_jump_fail_dest(), REG_64::RAX);
    emitter.MOV32_REG_IMM(instr.get_jump_dest(), REG_64::R15);
    emitter.CMOVCC32_REG(ConditionCode::G, REG_64::R15, REG_64::RAX);
    emitter.load_addr((uint64_t)&ee.PC, REG_64::R15);
    emitter.MOV32_TO_MEM(REG_64::RAX, REG_64::R15);

    if (instr.get_is_likely())
    {
        likely_branch = true;

        // Use the result of the FLAGS register from the last compare to set branch_on,
        // this boolean is used in handle_likely_branch
        emitter.load_addr((uint64_t)&ee.branch_on, REG_64::RAX);
        emitter.SETCC_MEM(ConditionCode::G, REG_64::RAX);
    }
}

void EE_JIT64::branch_less_than_or_equal_zero(EmotionEngine& ee, IR::Instruction &instr)
{
    REG_64 op1;
    op1 = alloc_gpr_reg(ee, instr.get_source(), REG_STATE::READ);

    // Conditionally move the success or failure destination into ee.PC
    emitter.MOV64_OI(0, REG_64::RAX);
    emitter.CMP64_REG(REG_64::RAX, op1);
    emitter.MOV32_REG_IMM(instr.get_jump_fail_dest(), REG_64::RAX);
    emitter.MOV32_REG_IMM(instr.get_jump_dest(), REG_64::R15);
    emitter.CMOVCC32_REG(ConditionCode::LE, REG_64::R15, REG_64::RAX);
    emitter.load_addr((uint64_t)&ee.PC, REG_64::R15);
    emitter.MOV32_TO_MEM(REG_64::RAX, REG_64::R15);

    if (instr.get_is_likely())
    {
        likely_branch = true;

        // Use the result of the FLAGS register from the last compare to set branch_on,
        // this boolean is used in handle_likely_branch
        emitter.load_addr((uint64_t)&ee.branch_on, REG_64::RAX);
        emitter.SETCC_MEM(ConditionCode::LE, REG_64::RAX);
    }
}

void EE_JIT64::branch_less_than_zero(EmotionEngine& ee, IR::Instruction &instr)
{
    REG_64 op1;
    op1 = alloc_gpr_reg(ee, instr.get_source(), REG_STATE::READ);

    if (instr.get_is_link())
    {
        // Set the link register
        emitter.load_addr((uint64_t)get_gpr_addr(ee, EE_NormalReg::ra), REG_64::RAX);
        emitter.MOV32_IMM_MEM(instr.get_return_addr(), REG_64::RAX);
    }

    // Conditionally move the success or failure destination into ee.PC
    emitter.MOV32_REG_IMM(instr.get_jump_fail_dest(), REG_64::RAX);
    emitter.MOV32_REG_IMM(instr.get_jump_dest(), REG_64::R15);
    emitter.TEST64_REG(op1, op1);
    emitter.CMOVCC32_REG(ConditionCode::S, REG_64::R15, REG_64::RAX);
    emitter.load_addr((uint64_t)&ee.PC, REG_64::R15);
    emitter.MOV32_TO_MEM(REG_64::RAX, REG_64::R15);

    if (instr.get_is_likely())
    {
        likely_branch = true;

        // Use the result of the FLAGS register from the last compare to set branch_on,
        // this boolean is used in handle_likely_branch
        emitter.load_addr((uint64_t)&ee.branch_on, REG_64::RAX);
        emitter.SETCC_MEM(ConditionCode::S, REG_64::RAX);
    }
}


void EE_JIT64::branch_not_equal(EmotionEngine& ee, IR::Instruction &instr)
{
    REG_64 op1;
    REG_64 op2;

    op1 = alloc_gpr_reg(ee, instr.get_source(), REG_STATE::READ);
    op2 = alloc_gpr_reg(ee, instr.get_source2(), REG_STATE::READ);

    // Conditionally move the success or failure destination into ee.PC
    emitter.MOV32_REG_IMM(instr.get_jump_fail_dest(), REG_64::RAX);
    emitter.MOV32_REG_IMM(instr.get_jump_dest(), REG_64::R15);
    emitter.CMP64_REG(op2, op1);
    emitter.CMOVCC32_REG(ConditionCode::NE, REG_64::R15, REG_64::RAX);
    emitter.load_addr((uint64_t)&ee.PC, REG_64::R15);
    emitter.MOV32_TO_MEM(REG_64::RAX, REG_64::R15);

    if (instr.get_is_likely())
    {
        likely_branch = true;

        // Use the result of the FLAGS register from the last compare to set branch_on,
        // this boolean is used in handle_likely_branch
        emitter.load_addr((uint64_t)&ee.branch_on, REG_64::RAX);
        emitter.SETCC_MEM(ConditionCode::NE, REG_64::RAX);
    }
}

void EE_JIT64::exception_return(EmotionEngine& ee, IR::Instruction& instr)
{
    // Cleanup branch_on, exception handler expects this to be false
    emitter.load_addr((uint64_t)&ee.branch_on, REG_64::RAX);
    emitter.MOV8_REG_IMM(false, REG_64::R15);
    emitter.MOV8_TO_MEM(REG_64::R15, REG_64::RAX);

    fallback_interpreter(ee, instr);

    // Since the interpreter decrements PC by 4, we reset it here to account for that.
    emitter.load_addr((uint64_t)&ee.PC, REG_64::RAX);
    emitter.MOV32_FROM_MEM(REG_64::RAX, REG_64::R15);
    emitter.ADD32_REG_IMM(4, REG_64::R15);
    emitter.MOV32_TO_MEM(REG_64::R15, REG_64::RAX);
}

void EE_JIT64::jump(EmotionEngine& ee, IR::Instruction& instr)
{
    // Simply set the PC
    emitter.load_addr((uint64_t)&ee.PC, REG_64::RAX);
    emitter.MOV32_IMM_MEM(instr.get_jump_dest(), REG_64::RAX);

    if (instr.get_is_link())
    {
        // Set the link register
        emitter.load_addr((uint64_t)get_gpr_addr(ee, EE_NormalReg::ra), REG_64::RAX);
        emitter.MOV32_IMM_MEM(instr.get_return_addr(), REG_64::RAX);
    }
}

void EE_JIT64::jump_indirect(EmotionEngine& ee, IR::Instruction& instr)
{
    REG_64 jump_dest = alloc_gpr_reg(ee, instr.get_source(), REG_STATE::READ);

    // Simply set the PC
    emitter.load_addr((uint64_t)&ee.PC, REG_64::RAX);
    emitter.MOV32_TO_MEM(jump_dest, REG_64::RAX);

    if (instr.get_is_link())
    {
        // Set the link register
        emitter.load_addr((uint64_t)get_gpr_addr(ee, (EE_NormalReg)instr.get_dest()), REG_64::RAX);
        emitter.MOV32_IMM_MEM(instr.get_return_addr(), REG_64::RAX);
    }
}

void EE_JIT64::load_upper_immediate(EmotionEngine& ee, IR::Instruction &instr)
{
    REG_64 dest = alloc_gpr_reg(ee, instr.get_dest(), REG_STATE::WRITE);
    int64_t imm = (int32_t)(instr.get_source() << 16);

    // dest = (int64_t)((int32_t)(imm << 16))
    emitter.MOV64_OI(imm, dest);
}

void EE_JIT64::or_int(EmotionEngine& ee, IR::Instruction &instr)
{
    if (instr.get_dest() == instr.get_source())
    {
        REG_64 dest = alloc_gpr_reg(ee, instr.get_dest(), REG_STATE::READ_WRITE);

        // temp = (uint64_t)immediate
        // dest |= temp
        emitter.MOV64_OI(instr.get_source2(), REG_64::RAX);
        emitter.OR64_REG(REG_64::RAX, dest);
    }
    else
    {
        REG_64 source = alloc_gpr_reg(ee, instr.get_source(), REG_STATE::READ);
        REG_64 dest = alloc_gpr_reg(ee, instr.get_dest(), REG_STATE::WRITE);

        // dest = (uint64_t)immediate
        // dest |= source
        emitter.MOV64_OI(instr.get_source2(), dest);
        emitter.OR64_REG(source, dest);
    }
}

void EE_JIT64::ee_syscall_exception(EmotionEngine& ee)
{
    ee.syscall_exception();
}


void EE_JIT64::set_on_less_than_immediate(EmotionEngine& ee, IR::Instruction& instr)
{
    int64_t imm = instr.get_source2();

    if (instr.get_source() == instr.get_dest())
    {
        REG_64 dest = alloc_gpr_reg(ee, instr.get_dest(), REG_STATE::READ_WRITE);
        emitter.MOV64_OI(imm, REG_64::RAX);
        emitter.CMP64_REG(REG_64::RAX, dest);
        emitter.MOV64_OI(0, dest);
        emitter.SETCC_REG(ConditionCode::L, dest);
    }
    else
    {
        REG_64 source = alloc_gpr_reg(ee, instr.get_source(), REG_STATE::READ);
        REG_64 dest = alloc_gpr_reg(ee, instr.get_dest(), REG_STATE::WRITE);
        emitter.XOR64_REG(dest, dest);
        emitter.MOV64_OI(imm, REG_64::RAX);
        emitter.CMP64_REG(REG_64::RAX, source);
        emitter.SETCC_REG(ConditionCode::L, dest);
    }
}

void EE_JIT64::set_on_less_than_immediate_unsigned(EmotionEngine& ee, IR::Instruction& instr)
{
    int64_t imm = instr.get_source2();

    if (instr.get_source() == instr.get_dest())
    {
        REG_64 dest = alloc_gpr_reg(ee, instr.get_dest(), REG_STATE::READ_WRITE);
        emitter.MOV64_OI(imm, REG_64::RAX);
        emitter.CMP64_REG(REG_64::RAX, dest);
        emitter.MOV64_OI(0, dest);
        emitter.SETCC_REG(ConditionCode::B, dest);
    }
    else
    {
        REG_64 source = alloc_gpr_reg(ee, instr.get_source(), REG_STATE::READ);
        REG_64 dest = alloc_gpr_reg(ee, instr.get_dest(), REG_STATE::WRITE);
        emitter.XOR64_REG(dest, dest);
        emitter.MOV64_OI(imm, REG_64::RAX);
        emitter.CMP64_REG(REG_64::RAX, source);
        emitter.SETCC_REG(ConditionCode::B, dest);
    }
}

void EE_JIT64::shift_left_logical(EmotionEngine& ee, IR::Instruction& instr)
{
    if (instr.get_source() == instr.get_dest())
    {
        REG_64 dest = alloc_gpr_reg(ee, instr.get_dest(), REG_STATE::READ_WRITE);

        // dest <<= immediate
        // dest = (int64_t)dest
        emitter.SHL32_REG_IMM(instr.get_source2(), dest);
        emitter.MOVSX32_TO_64(dest, dest);
    }
    else
    {
        REG_64 source = alloc_gpr_reg(ee, instr.get_source(), REG_STATE::READ);
        REG_64 dest = alloc_gpr_reg(ee, instr.get_dest(), REG_STATE::WRITE);
        
        // source = dest
        // dest <<= immediate
        // dest = (int64_t)dest
        emitter.MOV32_REG(source, dest);
        emitter.SHL32_REG_IMM(instr.get_source2(), dest);
        emitter.MOVSX32_TO_64(dest, dest);
    }
}

void EE_JIT64::shift_left_logical_variable(EmotionEngine& ee, IR::Instruction& instr)
{
    // Alloc variable into RCX
    alloc_gpr_reg(ee, instr.get_source2(), REG_64::RCX, REG_STATE::SCRATCHPAD);
    REG_64 variable = alloc_gpr_reg(ee, instr.get_source2(), REG_STATE::READ);
    emitter.MOV8_REG(variable, REG_64::CL);
    emitter.AND8_REG_IMM(0x1F, REG_64::CL);
    
    REG_64 source = alloc_gpr_reg(ee, instr.get_source(), REG_STATE::READ);
    REG_64 dest = alloc_gpr_reg(ee, instr.get_dest(), REG_STATE::WRITE);

    if (source != dest)
        emitter.MOV32_REG(source, dest);

    emitter.SHL32_CL(dest);
    emitter.MOVSX32_TO_64(dest, dest);
}

void EE_JIT64::shift_right_arithmetic(EmotionEngine& ee, IR::Instruction& instr)
{
    if (instr.get_source() == instr.get_dest())
    {
        REG_64 dest = alloc_gpr_reg(ee, instr.get_dest(), REG_STATE::READ_WRITE);

        // dest >>= immediate
        // dest = (int64_t)dest
        emitter.SAR32_REG_IMM(instr.get_source2(), dest);
        emitter.MOVSX32_TO_64(dest, dest);
    }
    else
    {
        REG_64 source = alloc_gpr_reg(ee, instr.get_source(), REG_STATE::READ);
        REG_64 dest = alloc_gpr_reg(ee, instr.get_dest(), REG_STATE::WRITE);

        // source = dest
        // dest >>= immediate
        // dest = (int64_t)dest
        emitter.MOV32_REG(source, dest);
        emitter.SAR32_REG_IMM(instr.get_source2(), dest);
        emitter.MOVSX32_TO_64(dest, dest);
    }
}

void EE_JIT64::shift_right_arithmetic_variable(EmotionEngine& ee, IR::Instruction& instr)
{
    // Alloc variable into RCX
    alloc_gpr_reg(ee, instr.get_source2(), REG_64::RCX, REG_STATE::SCRATCHPAD);
    REG_64 variable = alloc_gpr_reg(ee, instr.get_source2(), REG_STATE::READ);
    emitter.MOV8_REG(variable, REG_64::CL);
    emitter.AND8_REG_IMM(0x1F, REG_64::CL);

    REG_64 source = alloc_gpr_reg(ee, instr.get_source(), REG_STATE::READ);
    REG_64 dest = alloc_gpr_reg(ee, instr.get_dest(), REG_STATE::WRITE);

    if (source != dest)
        emitter.MOV32_REG(source, dest);

    emitter.SAR32_CL(dest);
    emitter.MOVSX32_TO_64(dest, dest);
}

void EE_JIT64::shift_right_logical(EmotionEngine& ee, IR::Instruction& instr)
{
    if (instr.get_source() == instr.get_dest())
    {
        REG_64 dest = alloc_gpr_reg(ee, instr.get_dest(), REG_STATE::READ_WRITE);

        // dest >>= immediate
        // dest = (int64_t)dest
        emitter.SHR32_REG_IMM(instr.get_source2(), dest);
        emitter.MOVSX32_TO_64(dest, dest);
    }
    else
    {
        REG_64 source = alloc_gpr_reg(ee, instr.get_source(), REG_STATE::READ);
        REG_64 dest = alloc_gpr_reg(ee, instr.get_dest(), REG_STATE::WRITE);

        // source = dest
        // dest >>= immediate
        // dest = (int64_t)dest
        emitter.MOV32_REG(source, dest);
        emitter.SHR32_REG_IMM(instr.get_source2(), dest);
        emitter.MOVSX32_TO_64(dest, dest);
    }
}

void EE_JIT64::shift_right_logical_variable(EmotionEngine& ee, IR::Instruction& instr)
{
    // Alloc variable into RCX
    alloc_gpr_reg(ee, instr.get_source2(), REG_64::RCX, REG_STATE::SCRATCHPAD);
    REG_64 variable = alloc_gpr_reg(ee, instr.get_source2(), REG_STATE::READ);
    emitter.MOV8_REG(variable, REG_64::CL);
    emitter.AND8_REG_IMM(0x1F, REG_64::CL);

    REG_64 source = alloc_gpr_reg(ee, instr.get_source(), REG_STATE::READ);
    REG_64 dest = alloc_gpr_reg(ee, instr.get_dest(), REG_STATE::WRITE);

    if (source != dest)
        emitter.MOV32_REG(source, dest);

    emitter.SHR32_CL(dest);
    emitter.MOVSX32_TO_64(dest, dest);
}

void EE_JIT64::system_call(EmotionEngine& ee, IR::Instruction& instr)
{
    flush_regs(ee);
    for (int i = 0; i < 16; i++)
    {
        int_regs[i].age = 0;
        int_regs[i].used = false;
        xmm_regs[i].age = 0;
        xmm_regs[i].used = false;
    }

    // Update PC before calling exception handler
    emitter.load_addr((uint64_t)&ee.PC, REG_64::RAX);
    emitter.MOV32_REG_IMM(instr.get_return_addr(), REG_64::R15);
    emitter.MOV32_TO_MEM(REG_64::R15, REG_64::RAX);

    // Cleanup branch_on, exception handler expects this to be false
    emitter.load_addr((uint64_t)&ee.branch_on, REG_64::RAX);
    emitter.MOV8_REG_IMM(false, REG_64::R15);
    emitter.MOV8_TO_MEM(REG_64::R15, REG_64::RAX);

    prepare_abi(ee, reinterpret_cast<uint64_t>(&ee));

    call_abi_func((uint64_t)ee_syscall_exception);

    // Since the interpreter decrements PC by 4, we reset it here to account for that.
    emitter.load_addr((uint64_t)&ee.PC, REG_64::RAX);
    emitter.MOV32_FROM_MEM(REG_64::RAX, REG_64::R15);
    emitter.ADD32_REG_IMM(4, REG_64::R15);
    emitter.MOV32_TO_MEM(REG_64::R15, REG_64::RAX);
}

void vu0_start_program(VectorUnit& vu0, uint32_t addr)
{
    vu0.start_program(addr);
}

uint32_t vu0_read_CMSAR0_shl3(VectorUnit& vu0)
{
    return vu0.read_CMSAR0() * 8;
}

bool ee_vu0_wait(EmotionEngine& ee)
{
    return ee.vu0_wait();
}

void EE_JIT64::vcall_ms(EmotionEngine& ee, IR::Instruction& instr)
{
    prepare_abi(ee, (uint64_t)&ee);
    call_abi_func((uint64_t)ee_vu0_wait);
    emitter.TEST8_REG(REG_64::AL, REG_64::AL);

    uint8_t* offset_addr = emitter.JCC_NEAR_DEFERRED(ConditionCode::Z);

    // If ee.vu0_wait(), set PC to the current operation's address and abort
    emitter.load_addr((uint64_t)&ee.PC, REG_64::RAX);
    emitter.MOV32_REG_IMM(instr.get_return_addr(), REG_64::R15);
    emitter.MOV32_TO_MEM(REG_64::R15, REG_64::RAX);

    // Set wait for VU0 flag
    emitter.load_addr((uint64_t)&ee.wait_for_VU0, REG_64::RAX);
    emitter.MOV8_IMM_MEM(true, REG_64::RAX);

    // Set cycle count to number of cycles executed
    emitter.load_addr((uint64_t)&cycle_count, REG_64::RAX);
    emitter.MOV16_IMM_MEM(instr.get_cycle_count(), REG_64::RAX);

    cleanup_recompiler(ee, true);

    // Otherwise continue execution of block otherwise
    emitter.set_jump_dest(offset_addr);
    prepare_abi(ee, (uint64_t)ee.vu0);
    prepare_abi(ee, instr.get_source());
    call_abi_func((uint64_t)vu0_start_program);
}

void EE_JIT64::vcall_msr(EmotionEngine& ee, IR::Instruction& instr)
{
    prepare_abi(ee, (uint64_t)&ee);
    call_abi_func((uint64_t)ee_vu0_wait);
    emitter.TEST8_REG(REG_64::AL, REG_64::AL);

    uint8_t* offset_addr = emitter.JCC_NEAR_DEFERRED(ConditionCode::Z);

    // If ee.vu0_wait(), set PC to the current operation's address and abort
    emitter.load_addr((uint64_t)&ee.PC, REG_64::RAX);
    emitter.MOV32_REG_IMM(instr.get_return_addr(), REG_64::R15);
    emitter.MOV32_TO_MEM(REG_64::R15, REG_64::RAX);

    // Set wait for VU0 flag
    emitter.load_addr((uint64_t)&ee.wait_for_VU0, REG_64::RAX);
    emitter.MOV8_IMM_MEM(true, REG_64::RAX);

    // Set cycle count to number of cycles executed
    emitter.load_addr((uint64_t)&cycle_count, REG_64::RAX);
    emitter.MOV16_IMM_MEM(instr.get_cycle_count(), REG_64::RAX);

    cleanup_recompiler(ee, true);

    // Otherwise continue execution of block otherwise
    emitter.set_jump_dest(offset_addr);

    prepare_abi(ee, (uint64_t)ee.vu0);
    call_abi_func((uint64_t)vu0_read_CMSAR0_shl3);
    
    prepare_abi_reg(ee, REG_64::RAX);
    call_abi_func((uint64_t)vu0_start_program);
}

void EE_JIT64::xor_int(EmotionEngine& ee, IR::Instruction &instr)
{
    if (instr.get_dest() == instr.get_source())
    {
        REG_64 dest = alloc_gpr_reg(ee, instr.get_dest(), REG_STATE::READ_WRITE);

        // temp = (uint64_t)immediate
        // dest ^= temp
        emitter.MOV64_OI(instr.get_source2(), REG_64::RAX);
        emitter.XOR64_REG(REG_64::RAX, dest);
    }
    else
    {
        REG_64 source = alloc_gpr_reg(ee, instr.get_source(), REG_STATE::READ);
        REG_64 dest = alloc_gpr_reg(ee, instr.get_dest(), REG_STATE::WRITE);

        // dest = (uint64_t)immediate
        // dest ^= source
        emitter.MOV64_OI(instr.get_source2(), dest);
        emitter.XOR64_REG(source, dest);
    }
}

void EE_JIT64::fallback_interpreter(EmotionEngine& ee, const IR::Instruction &instr)
{
    flush_regs(ee);
    for (int i = 0; i < 16; i++)
    {
        int_regs[i].age = 0;
        int_regs[i].used = false;
        xmm_regs[i].age = 0;
        xmm_regs[i].used = false;
    }

    uint32_t instr_word = instr.get_source();

    prepare_abi(ee, reinterpret_cast<uint64_t>(&ee));
    prepare_abi(ee, instr_word);

    call_abi_func(reinterpret_cast<uint64_t>(&interpreter));
}