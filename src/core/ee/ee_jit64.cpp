#include <cmath>
#include <algorithm>

#include "ee_jit64.hpp"
#include "emotioninterpreter.hpp"
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

    cycle_count = block.get_cycle_count();

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
    else if (ee_branch)
        handle_branch(ee);
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
        case IR::Opcode::JumpAndLink:
            jump_and_link(ee, instr);
            break;
        case IR::Opcode::JumpAndLinkIndirect:
            jump_and_link_indirect(ee, instr);
            break;
        case IR::Opcode::JumpIndirect:
            jump_indirect(ee, instr);
            break;
        case IR::Opcode::SystemCall:
            system_call(ee, instr);
            break;
        case IR::Opcode::FallbackInterpreter:
            fallback_interpreter(ee, instr);
            break;
        default:
            Errors::die("[EE_JIT64] Unknown IR instruction %d", instr.op);
    }
}

void EE_JIT64::prepare_abi(const EmotionEngine& ee, uint64_t value)
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
        int vi_reg = int_regs[arg].vu_reg;
        if (int_regs[arg].modified)
        {
            emitter.load_addr((uint64_t)&ee.gpr[vi_reg], REG_64::RAX);
            emitter.MOV64_TO_MEM(arg, REG_64::RAX);
        }
        int_regs[arg].used = false;
        int_regs[arg].age = 0;
    }
    emitter.load_addr(value, regs[abi_int_count]);
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

int EE_JIT64::search_for_register(AllocReg *regs, int vu_reg)
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

REG_64 EE_JIT64::alloc_int_reg(EmotionEngine& ee, int vi_reg, REG_STATE state)
{
    if (vi_reg >= 32)
        Errors::die("[EE_JIT64] Alloc Int error: vi_reg == %d", vi_reg);

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
        emitter.load_addr((uint64_t)&ee.gpr[old_vi_reg * sizeof(uint128_t)], REG_64::RAX);
        emitter.MOV64_TO_MEM((REG_64)reg, REG_64::RAX);
    }

    //printf("[VU_JIT64] Allocating int reg %d (vi%d)\n", reg, vi_reg);

    if (state != REG_STATE::WRITE)
    {
        emitter.load_addr((uint64_t)&ee.gpr[vi_reg * sizeof(uint128_t)], REG_64::RAX);
        emitter.MOV64_FROM_MEM(REG_64::RAX, (REG_64)reg);
    }

    int_regs[reg].modified = state != REG_STATE::READ;

    int_regs[reg].vu_reg = vi_reg;
    int_regs[reg].used = true;
    int_regs[reg].age = 0;

    return (REG_64)reg;
}

void EE_JIT64::handle_branch(EmotionEngine& ee)
{
    //Load the "branch happened" variable
    emitter.load_addr(reinterpret_cast<uint64_t>(&ee.branch_on), REG_64::RAX);
    emitter.MOV16_FROM_MEM(REG_64::RAX, REG_64::RAX);

    //The branch will point to two locations: one where the PC is set to the branch destination,
    //and another where PC = PC + 8. Our generated code will jump if the branch *fails*.
    //This is to allow for future optimizations where the recompiler may generate code beyond
    //the branch (assuming that the branch fails).
    emitter.TEST16_REG(REG_64::RAX, REG_64::RAX);

    //Because we don't know where the branch will jump to, we defer it.
    //Once the "branch succeeded" case has been finished recompiling, we can rewrite the branch offset...
    uint8_t* offset_addr = emitter.JE_NEAR_DEFERRED();
    emitter.load_addr(reinterpret_cast<uint64_t>(&ee.PC), REG_64::RAX);
    emitter.load_addr(reinterpret_cast<uint64_t>(&ee_branch_dest), REG_64::R15);
    emitter.MOV32_FROM_MEM(REG_64::R15, REG_64::R15);
    emitter.MOV32_TO_MEM(REG_64::R15, REG_64::RAX);
    cleanup_recompiler(ee, false);

    //...Which we do here.
    emitter.set_jump_dest(offset_addr);

    //And here we recompile the "branch failed" case.
    emitter.load_addr(reinterpret_cast<uint64_t>(&ee.PC), REG_64::RAX);
    emitter.load_addr(reinterpret_cast<uint64_t>(&ee_branch_fail_dest), REG_64::R15);
    emitter.MOV32_FROM_MEM(REG_64::R15, REG_64::R15);
    emitter.MOV32_TO_MEM(REG_64::R15, REG_64::RAX);
    cleanup_recompiler(ee, true);
}

void EE_JIT64::handle_branch_likely(EmotionEngine& ee, IR::Block& block)
{
    //Load the "branch happened" variable
    emitter.load_addr(reinterpret_cast<uint64_t>(&ee.branch_on), REG_64::RAX);
    emitter.MOV16_FROM_MEM(REG_64::RAX, REG_64::RAX);

    //The branch will point to two locations: one where the PC is set to the branch destination,
    //and another where PC = PC + 8. Our generated code will jump if the branch *fails*.
    //This is to allow for future optimizations where the recompiler may generate code beyond
    //the branch (assuming that the branch fails).
    //Additionally, by emitting the last instruction in the success case, we can have it "nullified"
    //if the branch were to be a failure instead.
    emitter.TEST16_REG(REG_64::RAX, REG_64::RAX);

    //Because we don't know where the branch will jump to, we defer it.
    //Once the "branch succeeded" case has been finished recompiling, we can rewrite the branch offset...
    uint8_t* offset_addr = emitter.JE_NEAR_DEFERRED();
    IR::Instruction instr = block.get_next_instr();
    emit_instruction(ee, instr);
    emitter.load_addr(reinterpret_cast<uint64_t>(&ee.PC), REG_64::RAX);
    emitter.load_addr(reinterpret_cast<uint64_t>(&ee_branch_dest), REG_64::R15);
    emitter.MOV32_FROM_MEM(REG_64::R15, REG_64::R15);
    emitter.MOV32_TO_MEM(REG_64::R15, REG_64::RAX);
    cleanup_recompiler(ee, false);

    //...Which we do here.
    emitter.set_jump_dest(offset_addr);

    //And here we recompile the "branch failed" case.
    emitter.load_addr(reinterpret_cast<uint64_t>(&ee.PC), REG_64::RAX);
    emitter.load_addr(reinterpret_cast<uint64_t>(&ee_branch_fail_dest), REG_64::R15);
    emitter.MOV32_FROM_MEM(REG_64::R15, REG_64::R15);
    emitter.MOV32_TO_MEM(REG_64::R15, REG_64::RAX);
    cleanup_recompiler(ee, true);
}

void EE_JIT64::handle_branch_destinations(EmotionEngine& ee, IR::Instruction& instr)
{
    emitter.load_addr((uint64_t)&ee_branch_delay_dest, REG_64::RAX);
    emitter.MOV32_IMM_MEM(instr.get_jump_dest(), REG_64::RAX);

    emitter.load_addr((uint64_t)&ee_branch_fail_dest, REG_64::RAX);
    emitter.MOV32_IMM_MEM(instr.get_jump_fail_dest(), REG_64::RAX);
}

bool cop0_get_condition(Cop0& cop0)
{
    return cop0.get_condition();
}

void EE_JIT64::branch_cop0(EmotionEngine& ee, IR::Instruction &instr)
{
    // Set success destination
    emitter.MOV32_REG_IMM(instr.get_jump_dest(), REG_64::RAX);
    emitter.load_addr((uint64_t)&ee_branch_dest, REG_64::R15);
    emitter.MOV32_TO_MEM(REG_64::RAX, REG_64::R15);

    // Set failure destination
    emitter.MOV32_REG_IMM(instr.get_jump_fail_dest(), REG_64::RAX);
    emitter.load_addr((uint64_t)&ee_branch_fail_dest, REG_64::R15);
    emitter.MOV32_TO_MEM(REG_64::RAX, REG_64::R15);

    // Call cop0.get_condition to get value to compare
    prepare_abi(ee, reinterpret_cast<uint64_t>(&ee.cp0));

    call_abi_func((uint64_t)cop0_get_condition);

    // Compare the condition to see which branch is taken
    emitter.MOV16_REG_IMM(instr.get_field(), REG_64::R15);
    emitter.CMP16_REG(REG_64::RAX, REG_64::R15);
    emitter.load_addr((uint64_t)&ee.branch_on, REG_64::RAX);
    emitter.SETE_MEM(REG_64::RAX);

    handle_branch_destinations(ee, instr);

    if (instr.get_is_likely())
        likely_branch = true;
    else
        ee_branch = true;
}

void EE_JIT64::branch_cop1(EmotionEngine& ee, IR::Instruction &instr)
{
    // Set success destination
    emitter.MOV32_REG_IMM(instr.get_jump_dest(), REG_64::RAX);
    emitter.load_addr((uint64_t)&ee_branch_dest, REG_64::R15);
    emitter.MOV32_TO_MEM(REG_64::RAX, REG_64::R15);

    // Set failure destination
    emitter.MOV32_REG_IMM(instr.get_jump_fail_dest(), REG_64::RAX);
    emitter.load_addr((uint64_t)&ee_branch_fail_dest, REG_64::R15);
    emitter.MOV32_TO_MEM(REG_64::RAX, REG_64::R15);

    // Compare the FPU condition to see which branch is taken
    emitter.load_addr((uint64_t)&ee.fpu->control.condition, REG_64::RAX);
    emitter.MOV16_FROM_MEM(REG_64::RAX, REG_64::RAX);
    emitter.MOV16_REG_IMM(instr.get_field(), REG_64::R15);
    emitter.CMP16_REG(REG_64::RAX, REG_64::R15);
    emitter.load_addr((uint64_t)&ee.branch_on, REG_64::RAX);
    emitter.SETE_MEM(REG_64::RAX);

    handle_branch_destinations(ee, instr);

    if (instr.get_is_likely())
        likely_branch = true;
    else
        ee_branch = true;
}

void EE_JIT64::branch_cop2(EmotionEngine& ee, IR::Instruction &instr)
{
    Errors::die("[EE_JIT64] Branch COP2 not implemented!");
}

void EE_JIT64::branch_equal(EmotionEngine& ee, IR::Instruction &instr)
{
    REG_64 op1;
    REG_64 op2;

    op1 = alloc_int_reg(ee, instr.get_source(), REG_STATE::READ);
    op2 = alloc_int_reg(ee, instr.get_source2(), REG_STATE::READ);

    // Set success destination
    emitter.MOV32_REG_IMM(instr.get_jump_dest(), REG_64::RAX);
    emitter.load_addr((uint64_t)&ee_branch_dest, REG_64::R15);
    emitter.MOV32_TO_MEM(REG_64::RAX, REG_64::R15);

    // Set failure destination
    emitter.MOV32_REG_IMM(instr.get_jump_fail_dest(), REG_64::RAX);
    emitter.load_addr((uint64_t)&ee_branch_fail_dest, REG_64::R15);
    emitter.MOV32_TO_MEM(REG_64::RAX, REG_64::R15);

    // Compare the two registers to see which jump we take
    emitter.CMP64_REG(op2, op1);
    emitter.load_addr((uint64_t)&ee.branch_on, REG_64::RAX);
    emitter.SETE_MEM(REG_64::RAX);

    handle_branch_destinations(ee, instr);

    if (instr.get_is_likely())
        likely_branch = true;
    else
        ee_branch = true;
}

void EE_JIT64::branch_greater_than_or_equal_zero(EmotionEngine& ee, IR::Instruction &instr)
{
    REG_64 op1;
    op1 = alloc_int_reg(ee, instr.get_source(), REG_STATE::READ);

    // Set success destination
    emitter.MOV32_REG_IMM(instr.get_jump_dest(), REG_64::RAX);
    emitter.load_addr((uint64_t)&ee_branch_dest, REG_64::R15);
    emitter.MOV32_TO_MEM(REG_64::RAX, REG_64::R15);

    // Set failure destination
    emitter.MOV32_REG_IMM(instr.get_jump_fail_dest(), REG_64::RAX);
    emitter.load_addr((uint64_t)&ee_branch_fail_dest, REG_64::R15);
    emitter.MOV32_TO_MEM(REG_64::RAX, REG_64::R15);

    // Compare the two registers to see which jump we take
    emitter.MOV64_OI(0, REG_64::RAX);
    emitter.CMP64_REG(REG_64::RAX, op1);
    emitter.load_addr((uint64_t)&ee.branch_on, REG_64::RAX);
    emitter.SETGE_MEM(REG_64::RAX);

    if (instr.get_is_link())
    {
        // Set the link register
        emitter.load_addr((uint64_t)&ee.gpr[EE_NormalReg::ra * sizeof(uint128_t)], REG_64::RAX);
        emitter.MOV32_REG_IMM(instr.get_return_addr(), REG_64::R15);
        emitter.MOV32_TO_MEM(REG_64::R15, REG_64::RAX);
    }

    handle_branch_destinations(ee, instr);

    if (instr.get_is_likely())
        likely_branch = true;
    else
        ee_branch = true;
}

void EE_JIT64::branch_greater_than_zero(EmotionEngine& ee, IR::Instruction &instr)
{
    REG_64 op1;
    op1 = alloc_int_reg(ee, instr.get_source(), REG_STATE::READ);

    // Set success destination
    emitter.MOV32_REG_IMM(instr.get_jump_dest(), REG_64::RAX);
    emitter.load_addr((uint64_t)&ee_branch_dest, REG_64::R15);
    emitter.MOV32_TO_MEM(REG_64::RAX, REG_64::R15);

    // Set failure destination
    emitter.MOV32_REG_IMM(instr.get_jump_fail_dest(), REG_64::RAX);
    emitter.load_addr((uint64_t)&ee_branch_fail_dest, REG_64::R15);
    emitter.MOV32_TO_MEM(REG_64::RAX, REG_64::R15);

    // Compare the two registers to see which jump we take
    emitter.MOV64_OI(0, REG_64::RAX);
    emitter.CMP64_REG(REG_64::RAX, op1);
    emitter.load_addr((uint64_t)&ee.branch_on, REG_64::RAX);
    emitter.SETG_MEM(REG_64::RAX);

    handle_branch_destinations(ee, instr);

    if (instr.get_is_likely())
        likely_branch = true;
    else
        ee_branch = true;
}

void EE_JIT64::branch_less_than_or_equal_zero(EmotionEngine& ee, IR::Instruction &instr)
{
    REG_64 op1;
    op1 = alloc_int_reg(ee, instr.get_source(), REG_STATE::READ);

    // Set success destination
    emitter.MOV32_REG_IMM(instr.get_jump_dest(), REG_64::RAX);
    emitter.load_addr((uint64_t)&ee_branch_dest, REG_64::R15);
    emitter.MOV32_TO_MEM(REG_64::RAX, REG_64::R15);

    // Set failure destination
    emitter.MOV32_REG_IMM(instr.get_jump_fail_dest(), REG_64::RAX);
    emitter.load_addr((uint64_t)&ee_branch_fail_dest, REG_64::R15);
    emitter.MOV32_TO_MEM(REG_64::RAX, REG_64::R15);

    // Compare the two registers to see which jump we take
    emitter.MOV64_OI(0, REG_64::RAX);
    emitter.CMP64_REG(REG_64::RAX, op1);
    emitter.load_addr((uint64_t)&ee.branch_on, REG_64::RAX);
    emitter.SETLE_MEM(REG_64::RAX);

    handle_branch_destinations(ee, instr);

    if (instr.get_is_likely())
        likely_branch = true;
    else
        ee_branch = true;
}

void EE_JIT64::branch_less_than_zero(EmotionEngine& ee, IR::Instruction &instr)
{
    REG_64 op1;
    op1 = alloc_int_reg(ee, instr.get_source(), REG_STATE::READ);

    // Set success destination
    emitter.MOV32_REG_IMM(instr.get_jump_dest(), REG_64::RAX);
    emitter.load_addr((uint64_t)&ee_branch_dest, REG_64::R15);
    emitter.MOV32_TO_MEM(REG_64::RAX, REG_64::R15);

    // Set failure destination
    emitter.MOV32_REG_IMM(instr.get_jump_fail_dest(), REG_64::RAX);
    emitter.load_addr((uint64_t)&ee_branch_fail_dest, REG_64::R15);
    emitter.MOV32_TO_MEM(REG_64::RAX, REG_64::R15);

    // Compare the two registers to see which jump we take
    emitter.MOV64_OI(0, REG_64::RAX);
    emitter.CMP64_REG(REG_64::RAX, op1);
    emitter.load_addr((uint64_t)&ee.branch_on, REG_64::RAX);
    emitter.SETL_MEM(REG_64::RAX);

    if (instr.get_is_link())
    {
        // Set the link register
        emitter.load_addr((uint64_t)&ee.gpr[EE_NormalReg::ra * sizeof(uint128_t)], REG_64::RAX);
        emitter.MOV32_REG_IMM(instr.get_return_addr(), REG_64::R15);
        emitter.MOV32_TO_MEM(REG_64::R15, REG_64::RAX);
    }

    handle_branch_destinations(ee, instr);

    if (instr.get_is_likely())
        likely_branch = true;
    else
        ee_branch = true;
}


void EE_JIT64::branch_not_equal(EmotionEngine& ee, IR::Instruction &instr)
{
    REG_64 op1;
    REG_64 op2;

    op1 = alloc_int_reg(ee, instr.get_source(), REG_STATE::READ);
    op2 = alloc_int_reg(ee, instr.get_source2(), REG_STATE::READ);

    // Set success destination
    emitter.MOV32_REG_IMM(instr.get_jump_dest(), REG_64::RAX);
    emitter.load_addr((uint64_t)&ee_branch_dest, REG_64::R15);
    emitter.MOV32_TO_MEM(REG_64::RAX, REG_64::R15);

    // Set failure destination
    emitter.MOV32_REG_IMM(instr.get_jump_fail_dest(), REG_64::RAX);
    emitter.load_addr((uint64_t)&ee_branch_fail_dest, REG_64::R15);
    emitter.MOV32_TO_MEM(REG_64::RAX, REG_64::R15);

    // Compare the two registers to see which jump we take
    emitter.CMP64_REG(op2, op1);
    emitter.load_addr((uint64_t)&ee.branch_on, REG_64::RAX);
    emitter.SETNE_MEM(REG_64::RAX);

    handle_branch_destinations(ee, instr);

    if (instr.get_is_likely())
        likely_branch = true;
    else
        ee_branch = true;
}

void EE_JIT64::exception_return(EmotionEngine& ee, IR::Instruction& instr)
{
    fallback_interpreter(ee, instr);

    // Since the interpreter decrements PC by 4, we reset it here to account for that.
    emitter.load_addr((uint64_t)&ee.PC, REG_64::RAX);
    emitter.MOV32_FROM_MEM(REG_64::RAX, REG_64::R15);
    emitter.ADD32_REG_IMM(4, REG_64::R15);
    emitter.MOV32_TO_MEM(REG_64::R15, REG_64::RAX);
}

void EE_JIT64::jump(EmotionEngine& ee, IR::Instruction& instr)
{
    // We just need to set the PC.
    emitter.load_addr((uint64_t)&ee_branch_dest, REG_64::RAX);
    emitter.MOV32_IMM_MEM(instr.get_jump_dest(), REG_64::RAX);

    emitter.load_addr((uint64_t)&ee.branch_on, REG_64::RAX);
    emitter.MOV16_IMM_MEM(true, REG_64::RAX);
    ee_branch = true;
}

void EE_JIT64::jump_and_link(EmotionEngine& ee, IR::Instruction& instr)
{
    //First set the PC
    emitter.load_addr((uint64_t)&ee_branch_dest, REG_64::RAX);
    emitter.MOV32_IMM_MEM(instr.get_jump_dest(), REG_64::RAX);

    emitter.load_addr((uint64_t)&ee.branch_on, REG_64::RAX);
    emitter.MOV16_IMM_MEM(true, REG_64::RAX);

    //Then set the link register
    emitter.load_addr((uint64_t)&ee.gpr[EE_NormalReg::ra * sizeof(uint128_t)], REG_64::RAX);
    emitter.MOV32_REG_IMM(instr.get_return_addr(), REG_64::R15);
    emitter.MOV32_TO_MEM(REG_64::R15, REG_64::RAX);

    ee_branch = true;
}

void EE_JIT64::jump_and_link_indirect(EmotionEngine& ee, IR::Instruction &instr)
{
    // Load and set jump address
    REG_64 jump_reg = alloc_int_reg(ee, instr.get_source(), REG_STATE::READ);
    emitter.MOV32_REG(jump_reg, REG_64::RAX);
    emitter.load_addr((uint64_t)&ee_branch_dest, REG_64::R15);
    emitter.MOV32_TO_MEM(REG_64::RAX, REG_64::R15);

    // Prime JIT to jump
    emitter.load_addr((uint64_t)&ee.branch_on, REG_64::RAX);
    emitter.MOV16_IMM_MEM(true, REG_64::RAX);

    //Then set the link register
    emitter.load_addr((uint64_t)&ee.gpr[EE_NormalReg::ra * sizeof(uint128_t)], REG_64::RAX);
    emitter.MOV32_REG_IMM(instr.get_return_addr(), REG_64::R15);
    emitter.MOV32_TO_MEM(REG_64::R15, REG_64::RAX);

    ee_branch = true;
}

void EE_JIT64::jump_indirect(EmotionEngine& ee, IR::Instruction& instr)
{
    REG_64 jump_dest = alloc_int_reg(ee, instr.get_source(), REG_STATE::READ);

    // Set destination
    emitter.load_addr((uint64_t)&ee_branch_dest, REG_64::RAX);
    emitter.MOV32_TO_MEM(jump_dest, REG_64::RAX);

    // Prime JIT to jump to success destination
    emitter.load_addr((uint64_t)&ee.branch_on, REG_64::RAX);
    emitter.MOV16_IMM_MEM(true, REG_64::RAX);

    handle_branch_destinations(ee, instr);

    ee_branch = true;
}

void EE_JIT64::ee_handle_exception(EmotionEngine& ee, uint32_t new_addr, uint8_t code)
{
    ee.handle_exception(new_addr, code);
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
    emitter.MOV32_REG_IMM(false, REG_64::R15);
    emitter.MOV32_TO_MEM(REG_64::R15, REG_64::RAX);

    prepare_abi(ee, reinterpret_cast<uint64_t>(&ee));
    prepare_abi(ee, 0x80000180);
    prepare_abi(ee, 0x08);

    call_abi_func((uint64_t)ee_handle_exception);
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

void EE_JIT64::flush_regs(EmotionEngine& ee)
{
    //Store the contents of all allocated x64 registers into the EE state.
    printf("[EE_JIT64] Flushing regs\n");
    for (int i = 0; i < 16; i++)
    {
        int vf_reg = xmm_regs[i].vu_reg;
        int vi_reg = int_regs[i].vu_reg;
        if (xmm_regs[i].used && vf_reg && xmm_regs[i].modified)
        {
            emitter.load_addr(get_fpu_addr(ee, static_cast<FPU_SpecialReg>(vf_reg)) , REG_64::RAX);
            emitter.MOVAPS_TO_MEM((REG_64)i, REG_64::RAX);
        }

        if (int_regs[i].used && vi_reg && int_regs[i].modified)
        {
            emitter.load_addr((uint64_t)&ee.gpr[vi_reg], REG_64::RAX);
            emitter.MOV16_TO_MEM((REG_64)i, REG_64::RAX);
        }
    }
}

uint64_t EE_JIT64::get_fpu_addr(EmotionEngine &ee, FPU_SpecialReg index)
{
    int i = static_cast<int>(index);
    if (i < 32)
        return reinterpret_cast<uint64_t>(&ee.fpu->gpr[i]);

    switch (index)
    {
    case FPU_SpecialReg::ACC:
        return (uint64_t)&ee.fpu->accumulator;
    default:
        Errors::die("[EE_JIT64] get_fpu_addr error: Unrecognized reg %d", index);
    }
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

    //Return the amount of cycles to update the EE with
    emitter.MOV16_REG_IMM(cycle_count, REG_64::RAX);
    emitter.load_addr(reinterpret_cast<uint64_t>(&cycle_count), REG_64::R15);
    emitter.MOV16_TO_MEM(REG_64::RAX, REG_64::R15);

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
    Errors::die("[VU_JIT64] emit_prologue not implemented for _WIN32");
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