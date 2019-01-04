#include "vu_jit.hpp"
#include "vu.hpp"

#include "vu_jittrans.hpp"
#include "../jitcommon/jitcache.hpp"
#include "../jitcommon/emitter64.hpp"

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

namespace VU_JIT
{

JitCache cache;
Emitter64 emitter(&cache);

void vu_set_pc(VectorUnit* vu, uint32_t PC)
{
    vu->set_PC(PC);
}

void vu_set_int(VectorUnit* vu, int index, uint16_t value)
{
    vu->set_int(index, value);
}

void x64_jump(VectorUnit* vu, IR::Instruction instr)
{
    //We just need to set the PC.
    emitter.MOV64_OI((uint64_t)vu, REG_64::RDI);
    emitter.MOV64_OI(instr.get_jump_dest(), REG_64::RSI);
    emitter.CALL((uint64_t)vu_set_pc);
}

void x64_jump_and_link(VectorUnit* vu, IR::Instruction instr)
{
    //First set the PC
    emitter.MOV64_OI((uint64_t)vu, REG_64::RDI);
    emitter.MOV64_OI(instr.get_jump_dest(), REG_64::RSI);
    emitter.CALL((uint64_t)vu_set_pc);

    //Then set the link register
    emitter.MOV64_OI(instr.get_link_register(), REG_64::RSI);
    emitter.MOV64_OI(instr.get_return_addr(), REG_64::RDX);
    emitter.CALL((uint64_t)vu_set_int);
}

void recompile_block_x64(IR::Block& block, VectorUnit* vu)
{
    cache.alloc_block(vu->get_PC());

    //Prologue
    emitter.PUSH(REG_64::RBP);
    emitter.MOV64_MR(REG_64::RSP, REG_64::RBP);

    while (block.get_instruction_count() > 0)
    {
        IR::Instruction instr = block.get_next_instr();

        switch (instr.op)
        {
            case IR::Opcode::Jump:
                x64_jump(vu, instr);
                break;
            case IR::Opcode::JumpAndLink:
                x64_jump_and_link(vu, instr);
                break;
            default:
                Errors::die("[VU_JIT] Unknown IR instruction");
        }
    }

    //Return the amount of cycles to update the VUs with
    emitter.MOV64_OI(block.get_cycle_count(), REG_64::RAX);

    //Epilogue
    emitter.POP(REG_64::RBP);
    emitter.RET();

    //Switch the block's privileges from RW to RX.
    cache.set_current_block_rx();
}

uint8_t* exec_block(VectorUnit* vu)
{
    if (cache.find_block(vu->get_PC()) == -1)
    {
        printf("[VU_JIT] Recompiling block at $%04X\n", vu->get_PC());
        IR::Block block = VU_JitTranslator::translate(vu->get_PC(), vu->get_instr_mem());
        recompile_block_x64(block, vu);
    }
    return cache.get_current_block_start();
}

int run(VectorUnit *vu)
{
    __asm__ (
        "pushq %rsi\n"
        "pushq %rdi\n"

        "callq __ZN6VU_JIT10exec_blockEP10VectorUnit\n"
        "callq *%rax\n"

        "popq %rdi\n"
        "popq %rsi\n"
    );
    return 1;
}

void reset()
{
    cache.flush_all_blocks();
}

};
