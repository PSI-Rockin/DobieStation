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

uint16_t EE_JIT64::run(EmotionEngine& ee)
{
    //TODO
}