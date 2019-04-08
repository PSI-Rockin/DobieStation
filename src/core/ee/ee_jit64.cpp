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
    //TODO

    if (clear_cache)
        cache.flush_all_blocks();

    //ir.reset_instr_info();

    //prev_pc = 0xFFFFFFFF;
    //current_program = 0;
}

extern "C"
uint8_t* exec_block_ee(EE_JIT64& jit, EmotionEngine& ee)
{
    printf("[EE_JIT64] Executing block at $%04X: recompiling\n", ee.PC);
    if (jit.cache.find_block(BlockState{ ee.PC, 0, 0, 0, 0 }) == nullptr)
    {
        printf("[EE_JIT64] Block not found at $%04X: recompiling\n", ee.PC);
        IR::Block block = jit.ir.translate(ee, ee.PC);
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
    cache.alloc_block(BlockState{ ee.get_PC(), 0, 0, 0, 0 });

    cycle_count = block.get_cycle_count();

    //Prologue
#ifndef _MSC_VER
    emit_prologue();
#endif
    emitter.PUSH(REG_64::RBP);
    emitter.MOV64_MR(REG_64::RSP, REG_64::RBP);

    while (block.get_instruction_count() > 0)
    {
        IR::Instruction instr = block.get_next_instr();
        emit_instruction(ee, instr);
    }

    if (ee_branch)
        handle_branch(ee);
    else
        cleanup_recompiler(ee, true);

    //Switch the block's privileges from RW to RX.
    cache.set_current_block_rx();
    //cache.print_current_block();
    //cache.print_literal_pool();
}

void EE_JIT64::emit_instruction(EmotionEngine &ee, const IR::Instruction &instr)
{
    switch (instr.op)
    {
    case IR::Opcode::FallbackInterpreter:
        fallback_interpreter(ee, instr);
        break;
    default:
        Errors::die("[EE_JIT64] Unknown IR instruction %d", instr.op);
    }
}

void EE_JIT64::handle_branch(EmotionEngine& ee)
{
    //Load the "branch happened" variable
    emitter.load_addr(reinterpret_cast<uint64_t>(&ee.branch_on), REG_64::RAX);
    emitter.MOV16_FROM_MEM(REG_64::RAX, REG_64::RAX);

    //The branch will point to two locations: one where the PC is set to the branch destination,
    //and another where PC = PC + 16. Our generated code will jump if the branch *fails*.
    //This is to allow for future optimizations where the recompiler may generate code beyond
    //the branch (assuming that the branch fails).
    emitter.TEST16_REG(REG_64::RAX, REG_64::RAX);

    //Because we don't know where the branch will jump to, we defer it.
    //Once the "branch succeeded" case has been finished recompiling, we can rewrite the branch offset...
    uint8_t* offset_addr = emitter.JE_NEAR_DEFERRED();
    emitter.load_addr(reinterpret_cast<uint64_t>(&ee.PC), REG_64::RAX);
    emitter.load_addr(reinterpret_cast<uint64_t>(&ee_branch_dest), REG_64::R15);
    emitter.MOV16_FROM_MEM(REG_64::R15, REG_64::R15);
    emitter.MOV16_TO_MEM(REG_64::R15, REG_64::RAX);
    cleanup_recompiler(ee, false);

    //...Which we do here.
    emitter.set_jump_dest(offset_addr);

    //And here we recompile the "branch failed" case.
    emitter.load_addr(reinterpret_cast<uint64_t>(&ee.PC), REG_64::RAX);
    emitter.load_addr(reinterpret_cast<uint64_t>(&ee_branch_fail_dest), REG_64::R15);
    emitter.MOV16_FROM_MEM(REG_64::R15, REG_64::R15);
    emitter.MOV16_TO_MEM(REG_64::R15, REG_64::RAX);
    cleanup_recompiler(ee, true);
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
        for (int i = 0; i < 16; i++)
        {
            int vf_reg = xmm_regs[i].vu_reg;
            int vi_reg = int_regs[i].vu_reg;
            if (xmm_regs[i].used && vf_reg && xmm_regs[i].modified)
            {
                emitter.load_addr(get_fpu_addr(ee, vf_reg) , REG_64::RAX);
                emitter.MOVAPS_TO_MEM((REG_64)i, REG_64::RAX);
            }

            if (int_regs[i].used && vi_reg && int_regs[i].modified)
            {
                emitter.load_addr((uint64_t)&ee.gpr[vi_reg], REG_64::RAX);
                emitter.MOV16_TO_MEM((REG_64)i, REG_64::RAX);
            }
        }
    }
}

uint64_t EE_JIT64::get_fpu_addr(EmotionEngine &ee, int index)
{
    if (index < 32)
        return reinterpret_cast<uint64_t>(&ee.fpu->gpr[index]);

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