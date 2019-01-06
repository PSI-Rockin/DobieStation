#include "vu_jittrans.hpp"
#include "vu_jit64.hpp"
#include "vu.hpp"

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

}

void vu_set_int(VectorUnit& vu, int dest, uint16_t value)
{
    vu.set_int(dest, value);
}

void VU_JIT64::reset()
{
    cache.flush_all_blocks();
}

void VU_JIT64::jump(VectorUnit& vu, IR::Instruction& instr)
{
    //We just need to set the PC.
    emitter.MOV64_OI((uint64_t)&vu.PC, REG_64::RAX);
    emitter.MOV32_MI_MEM(instr.get_jump_dest(), REG_64::RAX);
}

void VU_JIT64::jump_and_link(VectorUnit& vu, IR::Instruction& instr)
{
    //First set the PC
    emitter.MOV64_OI((uint64_t)&vu.PC, REG_64::RAX);
    emitter.MOV32_MI_MEM(instr.get_jump_dest(), REG_64::RAX);

    //Then set the link register
    emitter.MOV64_OI((uint64_t)&vu, REG_64::RDI);
    emitter.MOV64_OI(instr.get_dest(), REG_64::RSI);
    emitter.MOV64_OI(instr.get_return_addr(), REG_64::RDX);
    emitter.CALL((uint64_t)vu_set_int);
}

void VU_JIT64::mul_vector_by_scalar(VectorUnit &vu, IR::Instruction &instr)
{
    //TODO
}

REG_64_SSE VU_JIT64::alloc_sse_reg()
{
    //TODO
    return REG_64_SSE::XMM0;
}

void VU_JIT64::recompile_block(VectorUnit& vu, IR::Block& block)
{
    cache.alloc_block(vu.get_PC());

    //Prologue
    emitter.PUSH(REG_64::RBP);
    emitter.MOV64_MR(REG_64::RSP, REG_64::RBP);

    while (block.get_instruction_count() > 0)
    {
        IR::Instruction instr = block.get_next_instr();

        switch (instr.op)
        {
            case IR::Opcode::Jump:
                jump(vu, instr);
                break;
            case IR::Opcode::JumpAndLink:
                jump_and_link(vu, instr);
                break;
            case IR::Opcode::VMulVectorByScalar:
                mul_vector_by_scalar(vu, instr);
                break;
            default:
                Errors::die("[VU_JIT64] Unknown IR instruction");
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

uint8_t* VU_JIT64::exec_block(VectorUnit& vu)
{
    printf("[VU_JIT64] Executing block at $%04X\n", vu.PC);
    if (cache.find_block(vu.PC) == -1)
    {
        printf("[VU_JIT64] Block not found: recompiling\n");
        IR::Block block = VU_JitTranslator::translate(vu.PC, vu.get_instr_mem());
        recompile_block(vu, block);
    }
    return cache.get_current_block_start();
}

int VU_JIT64::run(VectorUnit& vu)
{
    __asm__ (
        "pushq %rsi\n"
        "pushq %rdi\n"

        "callq __ZN8VU_JIT6410exec_blockER10VectorUnit\n"
        "callq *%rax\n"

        "popq %rdi\n"
        "popq %rsi\n"
    );
}
