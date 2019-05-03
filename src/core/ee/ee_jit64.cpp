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
extern "C" void run_ee_jit(EE_JIT64& jit, EmotionEngine& ee);
#endif

EE_JIT64::EE_JIT64() : emitter(&cache) 
{
    // VU0 initialization
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
        int_regs[i].reg = -1;
    }

    // Lock special registers to prevent them from being used
    int_regs[REG_64::RSP].locked = true;

    // Scratchpad Registers
    int_regs[REG_64::RAX].locked = true;

    if (clear_cache)
        cache.flush_all_blocks();
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
    run_ee_jit(*this, ee);
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
        case IR::Opcode::Null:
            break;
        case IR::Opcode::AddDoublewordImm:
            add_doubleword_imm(ee, instr);
            break;
        case IR::Opcode::AddWordImm:
            add_word_imm(ee, instr);
            break;
        case IR::Opcode::AddDoublewordReg:
            add_doubleword_reg(ee, instr);
            break;
        case IR::Opcode::AddWordReg:
            add_word_reg(ee, instr);
            break;
        case IR::Opcode::AndImm:
            and_imm(ee, instr);
            break;
        case IR::Opcode::AndReg:
            and_reg(ee, instr);
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
        case IR::Opcode::BranchEqualZero:
            branch_equal_zero(ee, instr);
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
        case IR::Opcode::BranchNotEqualZero:
            branch_not_equal_zero(ee, instr);
            break;
        case IR::Opcode::ClearDoublewordReg:
            clear_doubleword_reg(ee, instr);
            break;
        case IR::Opcode::ClearWordReg:
            clear_word_reg(ee, instr);
            break;
        case IR::Opcode::DivideWord:
            divide_word(ee, instr);
            break;
        case IR::Opcode::DivideUnsignedWord:
            divide_unsigned_word(ee, instr);
            break;
        case IR::Opcode::DoublewordShiftLeftLogical:
            doubleword_shift_left_logical(ee, instr);
            break;
        case IR::Opcode::DoublewordShiftLeftLogicalVariable:
            doubleword_shift_left_logical_variable(ee, instr);
            break;
        case IR::Opcode::DoublewordShiftRightArithmetic:
            doubleword_shift_right_arithmetic(ee, instr);
            break;
        case IR::Opcode::DoublewordShiftRightArithmeticVariable:
            doubleword_shift_right_arithmetic_variable(ee, instr);
            break;
        case IR::Opcode::DoublewordShiftRightLogical:
            doubleword_shift_right_logical(ee, instr);
            break;
        case IR::Opcode::DoublewordShiftRightLogicalVariable:
            doubleword_shift_right_logical_variable(ee, instr);
            break;
        case IR::Opcode::ExceptionReturn:
            exception_return(ee, instr);
            break;
        case IR::Opcode::FloatingPointAbsoluteValue:
            floating_point_absolute_value(ee, instr);
            break;
        case IR::Opcode::FloatingPointAdd:
            floating_point_add(ee, instr);
            break;
        case IR::Opcode::FloatingPointClearControl:
            floating_point_clear_control(ee, instr);
            break;
        case IR::Opcode::FloatingPointCompareEqual:
            floating_point_compare_equal(ee, instr);
            break;
        case IR::Opcode::FloatingPointCompareLessThan:
            floating_point_compare_less_than(ee, instr);
            break;
        case IR::Opcode::FloatingPointCompareLessThanOrEqual:
            floating_point_compare_less_than_or_equal(ee, instr);
            break;
        case IR::Opcode::FloatingPointNegate:
            floating_point_negate(ee, instr);
            break;
        case IR::Opcode::FloatingPointMaximum:
            floating_point_maximum(ee, instr);
            break;
        case IR::Opcode::FloatingPointMinimum:
            floating_point_minimum(ee, instr);
            break;
        case IR::Opcode::FloatingPointMultiply:
            floating_point_multiply(ee, instr);
            break;
        case IR::Opcode::FloatingPointSquareRoot:
            floating_point_square_root(ee, instr);
            break;
        case IR::Opcode::FloatingPointSubtract:
            floating_point_subtract(ee, instr);
            break;
        case IR::Opcode::Jump:
            jump(ee, instr);
            break;
        case IR::Opcode::JumpIndirect:
            jump_indirect(ee, instr);
            break;
        case IR::Opcode::LoadByte:
            load_byte(ee, instr);
            break;
        case IR::Opcode::LoadByteUnsigned:
            load_byte_unsigned(ee, instr);
            break;
        case IR::Opcode::LoadConst:
            load_const(ee, instr);
            break;
        case IR::Opcode::LoadDoubleword:
            load_doubleword(ee, instr);
            break;
        case IR::Opcode::LoadHalfword:
            load_halfword(ee, instr);
            break;
        case IR::Opcode::LoadHalfwordUnsigned:
            load_halfword_unsigned(ee, instr);
            break;
        case IR::Opcode::LoadWord:
            load_word(ee, instr);
            break;
        case IR::Opcode::LoadWordUnsigned:
            load_word_unsigned(ee, instr);
            break;
        case IR::Opcode::MoveConditionalOnNotZero:
            move_conditional_on_not_zero(ee, instr);
            break;
        case IR::Opcode::MoveConditionalOnZero:
            move_conditional_on_zero(ee, instr);
            break;
        case IR::Opcode::MoveDoublewordReg:
            move_doubleword_reg(ee, instr);
            break;
        case IR::Opcode::MoveFromCoprocessor1:
            move_from_coprocessor1(ee, instr);
            break;
        case IR::Opcode::MoveToCoprocessor1:
            move_to_coprocessor1(ee, instr);
            break;
        case IR::Opcode::MoveToLOHI:
            move_to_lo_hi(ee, instr);
            break;
        case IR::Opcode::MoveXmmReg:
            move_xmm_reg(ee, instr);
            break;
        case IR::Opcode::MoveWordReg:
            move_word_reg(ee, instr);
            break;
        case IR::Opcode::MultiplyUnsignedWord:
            multiply_unsigned_word(ee, instr);
            break;
        case IR::Opcode::MultiplyWord:
            multiply_word(ee, instr);
            break;
        case IR::Opcode::NegateDoublewordReg:
            negate_doubleword_reg(ee, instr);
            break;
        case IR::Opcode::NegateWordReg:
            negate_word_reg(ee, instr);
            break;
        case IR::Opcode::NorReg:
            nor_reg(ee, instr);
            break;
        case IR::Opcode::OrImm:
            or_imm(ee, instr);
            break;
        case IR::Opcode::OrReg:
            or_reg(ee, instr);
            break;
        case IR::Opcode::SetOnLessThan:
            set_on_less_than(ee, instr);
            break;
        case IR::Opcode::SetOnLessThanUnsigned:
            set_on_less_than_unsigned(ee, instr);
            break;
        case IR::Opcode::SetOnLessThanImmediate:
            set_on_less_than_immediate(ee, instr);
            break;
        case IR::Opcode::SetOnLessThanImmediateUnsigned:
            set_on_less_than_immediate_unsigned(ee, instr);
            break;
        case IR::Opcode::ShiftLeftLogical:
            shift_left_logical(ee, instr);
            break;
        case IR::Opcode::ShiftLeftLogicalVariable:
            shift_left_logical_variable(ee, instr);
            break;
        case IR::Opcode::ShiftRightArithmetic:
            shift_right_arithmetic(ee, instr);
            break;
        case IR::Opcode::ShiftRightArithmeticVariable:
            shift_right_arithmetic_variable(ee, instr);
            break;
        case IR::Opcode::ShiftRightLogical:
            shift_right_logical(ee, instr);
            break;
        case IR::Opcode::ShiftRightLogicalVariable:
            shift_right_logical_variable(ee, instr);
            break;
        case IR::Opcode::StoreByte:
            store_byte(ee, instr);
            break;
        case IR::Opcode::StoreDoubleword:
            store_doubleword(ee, instr);
            break;
        case IR::Opcode::StoreHalfword:
            store_halfword(ee, instr);
            break;
        case IR::Opcode::StoreWord:
            store_word(ee, instr);
            break;
        case IR::Opcode::SubDoublewordReg:
            sub_doubleword_reg(ee, instr);
            break;
        case IR::Opcode::SubWordReg:
            sub_word_reg(ee, instr);
            break;
        case IR::Opcode::SystemCall:
            system_call(ee, instr);
            break;
        case IR::Opcode::VAddVectorByScalar:
            add_vector_by_scalar(ee, instr);
            break;
        case IR::Opcode::VCallMS:
            vcall_ms(ee, instr);
            break;
        case IR::Opcode::VCallMSR:
            vcall_msr(ee, instr);
            break;
        case IR::Opcode::XorImm:
            xor_imm(ee, instr);
            break;
        case IR::Opcode::XorReg:
            xor_reg(ee, instr);
            break;
        case IR::Opcode::FallbackInterpreter:
            fallback_interpreter(ee, instr);
            break;
        default:
            Errors::die("[EE_JIT64] Unknown IR instruction %d", instr.op);
    }
}

void EE_JIT64::alloc_abi_regs(int count)
{
#ifdef _WIN32
    const static REG_64 regs[] = { RCX, RDX, R8, R9 };

    if (abi_int_count + count > 4)
        Errors::die("[EE_JIT64] ABI integer arguments exceeded 4!");
#else
    const static REG_64 regs[] = { RDI, RSI, RDX, RCX, R8, R9 };

    if (abi_int_count + count > 6)
        Errors::die("[EE_JIT64] ABI integer arguments exceeded 6!");
#endif

    for (int i = abi_int_count; i < abi_int_count + count; ++i)
    {
        int_regs[regs[i]].locked = true;
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
    int_regs[arg].locked = true;

    //If the chosen integer argument is being used, flush it back to the EE state
    flush_int_reg(ee, arg);
    int_regs[arg].used = false;
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
    int_regs[arg].locked = true;

    //If the chosen integer argument is being used, flush it back to the EE state
    flush_int_reg(ee, arg);
    int_regs[arg].used = false;
    if (reg != regs[abi_int_count])
        emitter.MOV64_MR(reg, regs[abi_int_count]);
    abi_int_count++;
}

void EE_JIT64::call_abi_func(EmotionEngine& ee, uint64_t addr)
{
    const static REG_64 saved_regs[] = { RCX, RDX, R8, R9, R10, R11 };
#ifdef _WIN32
    const static REG_64 abi_regs[] = { RCX, RDX, R8, R9 };
#else
    const static REG_64 abi_regs[] = { RDI, RSI, RDX, RCX, R8, R9 };
#endif    
    int total_regs = sizeof(saved_regs) / sizeof(REG_64);
    int regs_used = 0;
    int sp_offset = 0;

    for (int i = 0; i < 16; ++i)
    {
        flush_xmm_reg(ee, i);
        xmm_regs[i].used = false;
    }

    REG_64 addrReg = REG_64::RAX;
    emitter.MOV64_OI(addr, addrReg);

    for (int i = 0; i < abi_int_count; ++i)
        int_regs[abi_regs[i]].locked = false;

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

    if (sp_offset)
        emitter.SUB64_REG_IMM(sp_offset, REG_64::RSP);
    emitter.CALL_INDIR(addrReg);
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

int EE_JIT64::search_for_register_scratchpad(AllocReg *regs)
{
    // Returns the index of either a free register or the oldest allocated register, depending on availability
    // Will prioritize volatile (caller-saved) registers
#ifdef _WIN32
    REG_64 favored_registers[] = { RAX, RCX, RDX, R8, R9, R10, R11 };
#else
    REG_64 favored_registers[] = { RAX, RCX, RDX, RDI, RSI, R8, R9, R10, R11 };
#endif

    for (REG_64 favreg : favored_registers)
    {
        if (regs[favreg].locked)
            continue;

        if (!regs[favreg].used)
            return favreg;
    }

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

int EE_JIT64::search_for_register_priority(AllocReg *regs)
{
    // Returns the index of either a free register or the oldest allocated register, depending on availability
    // Will prioritize non-volatile (callee-saved) registers
#ifdef _WIN32
    REG_64 favored_registers[] = { RBX, RBP, RDI, RSI, R12, R13, R14, R15 };
#else
    REG_64 favored_registers[] = { RBX, RBP, R12, R13, R14, R15 };
#endif
    for (REG_64 favreg : favored_registers)
    {
        if (regs[favreg].locked)
            continue;

        if (!regs[favreg].used)
            return favreg;
    }

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

int EE_JIT64::search_for_register_xmm(AllocReg *regs)
{
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

REG_64 EE_JIT64::alloc_reg(EmotionEngine& ee, int reg, REG_TYPE type, REG_STATE state, REG_64 destination)
{
    // An explicit destination is not provided, so we find one ourselves here.
    if (destination < 0)
    {
        switch (type)
        {
            case REG_TYPE::INTSCRATCHPAD:
                destination = (REG_64)search_for_register_scratchpad(int_regs);
                break;
            case REG_TYPE::XMMSCRATCHPAD:
                destination = (REG_64)search_for_register_xmm(xmm_regs);
                break;
            case REG_TYPE::GPR:
            case REG_TYPE::VI:
                for (int i = 0; i < 16; ++i)
                {
                    if (!int_regs[i].locked && int_regs[i].used && int_regs[i].reg == reg && int_regs[i].type == type)
                    {
                        destination = (REG_64)i;
                        break;
                    }
                }

                // If our EE register isn't already in a register
                if (destination < 0)
                {
                    destination = (REG_64)search_for_register_priority(int_regs);
                }
                break;
            case REG_TYPE::FPU:
            case REG_TYPE::GPREXTENDED:
            case REG_TYPE::VF:
                for (int i = 0; i < 16; ++i)
                {
                    // Check if our register is already allocated
                    if (!xmm_regs[i].locked && xmm_regs[i].used && xmm_regs[i].reg == reg && xmm_regs[i].type == type)
                    {
                        destination = (REG_64)i;
                        break;
                    }
                }

                // If our EE register isn't already in a register
                if (destination < 0)
                {
                    destination = (REG_64)search_for_register_xmm(xmm_regs);
                }
                break;
            default:
                break;
        }
    }

    switch (type)
    {
        case REG_TYPE::GPR:
        case REG_TYPE::VI:
        case REG_TYPE::INTSCRATCHPAD:
            if (int_regs[destination].locked)
                Errors::die("[EE_JIT64] Alloc Int error: Attempted to allocate locked x64 register %d", destination);
            break;
        case REG_TYPE::FPU:
        case REG_TYPE::GPREXTENDED:
        case REG_TYPE::VF:
        case REG_TYPE::XMMSCRATCHPAD:
            if (xmm_regs[destination].locked)
                Errors::die("[EE_JIT64] Alloc Xmm error: Attempted to allocate locked x64 register %d", destination);
            break;
        default:
            break;
    }

    switch (type)
    {
        case REG_TYPE::INTSCRATCHPAD:
            flush_int_reg(ee, destination);
            int_regs[destination].modified = false;
            int_regs[destination].used = true;
            int_regs[destination].age = 0;
            int_regs[destination].reg = -1;
            int_regs[destination].type = type;
            return (REG_64)destination;
        case REG_TYPE::XMMSCRATCHPAD:
            flush_xmm_reg(ee, destination);
            xmm_regs[destination].modified = false;
            xmm_regs[destination].used = true;
            xmm_regs[destination].age = 0;
            xmm_regs[destination].reg = -1;
            xmm_regs[destination].type = type;
            return (REG_64)destination;
        case REG_TYPE::GPR:
        case REG_TYPE::VI:
            // Increment age of every used register
            for (int i = 0; i < 16; ++i)
            {
                if (int_regs[i].used)
                    ++int_regs[i].age;
            }

            // Do nothing if the EE register is already inside our x86 register
            if (int_regs[destination].used && int_regs[destination].type == type && int_regs[destination].reg == reg)
            {
                if (state != REG_STATE::READ && !(reg == 0 && type == REG_TYPE::GPR))
                    int_regs[destination].modified = true;
                int_regs[destination].age = 0;
                return (REG_64)destination;
            }

            // Flush new register's contents back to EE state
            flush_int_reg(ee, destination);
            int_regs[destination].used = false;

            if (state != REG_STATE::WRITE)
            {
                int reg_to_find = -1;

                for (int i = 0; i < 16; ++i)
                {
                    if (int_regs[i].used && int_regs[i].reg == reg && int_regs[i].type == type)
                    {
                        reg_to_find = i;
                        break;
                    }
                }

                if (reg_to_find >= 0)
                {
                    // This case is hit if we want a register in a specific destination but
                    // we already have the contents in another register.
                    emitter.MOV64_MR((REG_64)reg_to_find, destination);
                }
                else
                {
                    switch (type)
                    {
                        case REG_TYPE::GPR:
                            emitter.load_addr((uint64_t)get_gpr_addr(ee, (EE_NormalReg)reg), (REG_64)destination);
                            break;
                        case REG_TYPE::VI:
                            // TODO
                            break;
                        default:
                            break;
                    }
                    emitter.MOV64_FROM_MEM((REG_64)destination, (REG_64)destination);
                }
            }

            int_regs[destination].modified = state != REG_STATE::READ && !(reg == 0 && type == REG_TYPE::GPR);
            int_regs[destination].reg = reg;
            int_regs[destination].type = type;
            int_regs[destination].used = true;
            int_regs[destination].age = 0;
            return (REG_64)destination;
        case REG_TYPE::FPU:
        case REG_TYPE::GPREXTENDED:
        case REG_TYPE::VF:
            // Increment age of every used register
            for (int i = 0; i < 16; ++i)
            {
                if (xmm_regs[i].used)
                    ++xmm_regs[i].age;
            }

            // Do nothing if the EE register is already inside our x86 register
            if (xmm_regs[destination].used && xmm_regs[destination].type == type && xmm_regs[destination].reg == reg)
            {
                if (state != REG_STATE::READ && !(reg == 0 && type == REG_TYPE::VF))
                    xmm_regs[destination].modified = true;
                xmm_regs[destination].age = 0;
                return (REG_64)destination;
            }

            // Flush new register's contents back to EE state
            flush_xmm_reg(ee, destination);
            xmm_regs[destination].used = false;

            if (state != REG_STATE::WRITE)
            {
                int reg_to_find = -1;

                for (int i = 0; i < 16; ++i)
                {
                    if (xmm_regs[i].used && xmm_regs[i].reg == reg && xmm_regs[i].type == type)
                    {
                        reg_to_find = i;
                        break;
                    }
                }

                if (reg_to_find >= 0)
                {
                    // This case is hit if we want a register in a specific destination but
                    // we already have the contents in another register.
                    emitter.MOVAPS_REG((REG_64)reg_to_find, destination);
                }
                else
                {
                    switch (type)
                    {
                        case REG_TYPE::FPU:
                            emitter.load_addr((uint64_t)get_fpu_addr(ee, reg), REG_64::RAX);
                            emitter.MOVD_FROM_MEM(REG_64::RAX, (REG_64)destination);
                            break;
                        case REG_TYPE::VF:
                            emitter.load_addr((uint64_t)get_vf_addr(ee, reg), REG_64::RAX);
                            emitter.MOVAPS_FROM_MEM(REG_64::RAX, (REG_64)destination);
                            break;
                        case REG_TYPE::GPREXTENDED:
                            emitter.load_addr((uint64_t)get_gpr_addr(ee, (EE_NormalReg)reg), (REG_64)destination);
                            emitter.MOVAPS_FROM_MEM(REG_64::RAX, (REG_64)destination);
                            break;
                        default:
                            break;
                    }
                    
                }
            }

            xmm_regs[destination].modified = state != REG_STATE::READ && !(reg == 0 && type == REG_TYPE::VF);
            xmm_regs[destination].reg = reg;
            xmm_regs[destination].type = type;
            xmm_regs[destination].used = true;
            xmm_regs[destination].age = 0;
            return (REG_64)destination;
        default:
            Errors::die("[EE_JIT64] Register allocation error! Attempted to allocate a register with an unknown type %d!", type);
            return (REG_64)-1;
    }
}

REG_64 EE_JIT64::lalloc_int_reg(EmotionEngine& ee, int gpr_reg, REG_TYPE type, REG_STATE state, REG_64 destination)
{
    REG_64 result = alloc_reg(ee, gpr_reg, type, state, destination);
    int_regs[result].locked = true;
    return result;
}

REG_64 EE_JIT64::lalloc_xmm_reg(EmotionEngine& ee, int gpr_reg, REG_TYPE type, REG_STATE state, REG_64 destination)
{
    REG_64 result = alloc_reg(ee, gpr_reg, type, state, destination);
    xmm_regs[result].locked = true;
    return result;
}

void EE_JIT64::free_int_reg(EmotionEngine& ee, REG_64 reg)
{
    int_regs[reg].locked = false;
    int_regs[reg].used = false;
    flush_int_reg(ee, reg);
}

void EE_JIT64::free_xmm_reg(EmotionEngine& ee, REG_64 reg)
{
    xmm_regs[reg].locked = false;
    xmm_regs[reg].used = false;
    flush_xmm_reg(ee, reg);
}

void EE_JIT64::flush_int_reg(EmotionEngine& ee, int reg)
{
    int old_int_reg = int_regs[reg].reg;
    if (int_regs[reg].used && int_regs[reg].modified)
    {
        //printf("[EE_JIT64] Flushing int reg %d! (old int reg: %d, reg type: %d)\n", reg, int_regs[reg].reg, int_regs[reg].type);
        switch (int_regs[reg].type)
        {
            case REG_TYPE::GPR:
                // old_int_reg = $zero, which should never be flushed
                if (!old_int_reg)
                    break;
                emitter.load_addr((uint64_t)get_gpr_addr(ee, old_int_reg), REG_64::RAX);
                emitter.MOV64_TO_MEM((REG_64)reg, REG_64::RAX);
                break;
            case REG_TYPE::VI:
                emitter.load_addr((uint64_t)get_vi_addr(ee, old_int_reg), REG_64::RAX);
                emitter.MOV16_TO_MEM((REG_64)reg, REG_64::RAX);
                break;
            default:
                break;
        }
    }
}

void EE_JIT64::flush_xmm_reg(EmotionEngine& ee, int reg)
{
    int old_xmm_reg = xmm_regs[reg].reg;
    if (xmm_regs[reg].used && xmm_regs[reg].modified)
    {
        //printf("[EE_JIT64] Flushing xmm reg %d! (old float reg: %d, reg type: %d)\n", reg, xmm_regs[reg].reg, xmm_regs[reg].type);
        switch (xmm_regs[reg].type)
        {
            case REG_TYPE::FPU:
                emitter.load_addr((uint64_t)get_fpu_addr(ee, old_xmm_reg), REG_64::RAX);
                emitter.MOVD_TO_MEM((REG_64)reg, REG_64::RAX);
                break;
            case REG_TYPE::GPREXTENDED:
                // old_xmm_reg = $zero, which should never be flushed
                if (!old_xmm_reg)
                    break;
                emitter.load_addr((uint64_t)get_gpr_addr(ee, old_xmm_reg), REG_64::RAX);
                emitter.MOVAPS_TO_MEM((REG_64)reg, REG_64::RAX);
                break;
            case REG_TYPE::VF:
                emitter.load_addr((uint64_t)get_vf_addr(ee, old_xmm_reg), REG_64::RAX);
                emitter.MOVAPS_TO_MEM((REG_64)reg, REG_64::RAX);
                break;
            default:
                break;
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
    if (index < 32)
        return (uint64_t)&ee.vu0->gpr[index];

    switch (index)
    {
        case VU_SpecialReg::ACC:
            return (uint64_t)&ee.vu0->ACC;
        case VU_SpecialReg::I:
            return (uint64_t)&ee.vu0->I;
        case VU_SpecialReg::Q:
            return (uint64_t)&ee.vu0->Q;
        case VU_SpecialReg::P:
            return (uint64_t)&ee.vu0->P;
        case VU_SpecialReg::R:
            return (uint64_t)&ee.vu0->R;
        default:
            Errors::die("[EE_JIT64] get_vf_addr error: Unrecognized reg %d", index);
    }
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

    switch (static_cast<EE_SpecialReg>(index))
    {
        case EE_SpecialReg::LO:
            return (uint64_t)&ee.LO;
        case EE_SpecialReg::HI:
            return (uint64_t)&ee.HI;
        case EE_SpecialReg::SA:
            return (uint64_t)&ee.SA;
        default:
            Errors::die("[EE_JIT64] get_gpr_addr error: Unrecognized reg %d", index);
    }
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
    for (int i = 0; i < 16; i++)
    {
        int_regs[i].age = 0;
        int_regs[i].used = false;
        xmm_regs[i].age = 0;
        xmm_regs[i].used = false;
    }

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

    call_abi_func(ee, reinterpret_cast<uint64_t>(&interpreter));
}