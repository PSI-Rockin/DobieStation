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

EE_JIT64::EE_JIT64() : jit_block("EE"), emitter(&jit_block), prologue_block(nullptr)
{
}

void EE_JIT64::reset(bool clear_cache)
{
    ee_mxcsr = 0xFFC0;

    abi_int_count = 0;
    abi_xmm_count = 0;
    saved_int_regs = std::vector<REG_64>();
    saved_xmm_regs = std::vector<REG_64>();
    cycles_added = 0;
    for (int i = 0; i < 16; i++)
    {
        xmm_regs[i].used = false;
        xmm_regs[i].locked = false;
        xmm_regs[i].stored = false;
        xmm_regs[i].age = 0;
        xmm_regs[i].needs_clamping = false;

        int_regs[i].used = false;
        int_regs[i].locked = false;
        int_regs[i].stored = false;
        int_regs[i].age = 0;
        int_regs[i].reg = -1;
    }

    //Current register allocation
    //RSP: reserved for stack
    //R13: Fast lookup cache
    //R14: JIT64 object
    //R15: EE object
    //RAX: generic scratchpad register
    //Any register not on this list may be used freely by the recompiler.

    int_regs[REG_64::RSP].locked = true;
    int_regs[REG_64::R13].locked = true;
    int_regs[REG_64::R14].locked = true;
    int_regs[REG_64::R15].locked = true;
    int_regs[REG_64::RAX].locked = true;

    if (clear_cache)
    {
        jit_heap.flush_all_blocks();
        prologue_block = create_prologue_block();
    }
}

extern "C"
uint8_t* exec_block_ee(EE_JIT64& jit, EmotionEngine& ee)
{
    bool is_modified = false;
    EEJitBlockRecord *recompiledBlock = jit.jit_heap.find_block(ee.PC);

    uint32_t ee_page = ee.PC >> 12;

    if (ee.cp0->get_tlb_modified(ee_page))
    {
        if (ee_page < (0x80000000ULL >> 12ULL) && ee_page >= (0x80040000ULL >> 12ULL))
        {
            jit.jit_heap.invalidate_ee_page(ee_page);
            is_modified = true;
            ee.cp0->clear_tlb_modified(ee_page);
        }
    }

    if (is_modified || recompiledBlock == nullptr)
    {
        printf("[EE_JIT64] Block not found at $%08X: recompiling\n", ee.PC);
        IR::Block block = jit.ir.translate(ee);
        recompiledBlock = jit.recompile_block(ee, block);
    }
    jit.jit_heap.lookup_cache[(ee.PC >> 2) & 0x7FFF] = recompiledBlock;
    return (uint8_t*)recompiledBlock->code_start;
}

uint16_t EE_JIT64::run(EmotionEngine& ee)
{
    prologue_block(*this, ee, &jit_heap.lookup_cache[0]);

    return cycle_count;
}

EEJitPrologue EE_JIT64::create_prologue_block()
{
    jit_block.clear();

    emit_prologue();

    //Store the JIT64/EE objects in R14/R15 respectively
#ifdef _WIN32
    emitter.MOV64_MR(REG_64::RDX, REG_64::R15);
    emitter.MOV64_MR(REG_64::RCX, REG_64::R14);
    emitter.MOV64_MR(REG_64::R8, REG_64::R13);
#else
    emitter.MOV64_MR(REG_64::RSI, REG_64::R15);
    emitter.MOV64_MR(REG_64::RDI, REG_64::R14);
    emitter.MOV64_MR(REG_64::RDX, REG_64::R13);
#endif

    emit_dispatcher();

    //Reserve 0xFFFFFFFF as the PC.
    //Because this is an invalid address, it doesn't matter that the prologue block has this.
    return (EEJitPrologue)jit_heap.insert_block(0xFFFFFFFF, &jit_block)->code_start;
}

void EE_JIT64::emit_dispatcher()
{
    //Check if cycles_to_run > 0 and VU0 wait and check interlock is false. When both are true, we execute another block.
    //Otherwise, we return.
    emitter.CMP32_IMM_MEM(0, REG_64::R15, offsetof(EmotionEngine, cycles_to_run));
    uint8_t* exit_cyclecount = emitter.JCC_NEAR_DEFERRED(ConditionCode::LE);

    //Fetch pointer to index in cache
    //ptr = lookup_cache[(PC >> 2) & 0x7FFF]
    //lookup_cache is an array of size 8 elements, so we can skip shifting PC to the right
    //by doing PC & (0x7FFF << 2), which would provide the correct array offset if the array consisted of size 4 elements.
    //We can complete the lookup by adding that result * 2 to R13, which we do in a LEA operation.
    emitter.MOV32_FROM_MEM(REG_64::R15, REG_64::RCX, offsetof(EmotionEngine, PC));
    emitter.MOV32_REG(REG_64::RCX, REG_64::RAX);
    emitter.AND32_EAX(0x7FFF << 2);
    emitter.LEA64_REG(REG_64::RAX, REG_64::R13, REG_64::RAX, 0, 1);
    emitter.MOV64_FROM_MEM(REG_64::RAX, REG_64::RAX);

    //First we need to make sure the pointer isn't NULL, which means no block has been recompiled.
    //If it is NULL, we go to the slow path.
    emitter.CMP64_IMM(0, REG_64::RAX);
    uint8_t* slow_path_dest1 = emitter.JCC_NEAR_DEFERRED(ConditionCode::E);

    //Compare current PC with the PC of the block
    //If equal, fetch the block's start and jump to it.
    //Otherwise, we have to go to the slow path.
    emitter.MOV32_FROM_MEM(REG_64::RAX, REG_64::RDX,
                           offsetof(EEJitBlockRecord, block_data) + offsetof(EEJitBlockRecordData, pc));
    emitter.CMP32_REG(REG_64::RCX, REG_64::RDX);
    uint8_t* slow_path_dest2 = emitter.JCC_NEAR_DEFERRED(ConditionCode::NE);

    emitter.MOV64_FROM_MEM(REG_64::RAX, REG_64::RAX, offsetof(EEJitBlockRecord, code_start));

    //Tail-call optimization
    emitter.JMP_INDIR(REG_64::RAX);

    //Find a block the slow way.
    //This entails calling a C++ function that finds a block by looking in an unordered_map.
    //If no block is found, this function will also recompile a new block.
    emitter.set_jump_dest(slow_path_dest1);
    emitter.set_jump_dest(slow_path_dest2);
#ifdef _WIN32
    emitter.MOV64_MR(REG_64::R14, REG_64::RCX);
    emitter.MOV64_MR(REG_64::R15, REG_64::RDX);
    emitter.SUB64_REG_IMM(0x20, REG_64::RSP);
#else
    emitter.MOV64_MR(REG_64::R14, REG_64::RDI);
    emitter.MOV64_MR(REG_64::R15, REG_64::RSI);
#endif
    emitter.load_addr((uint64_t)exec_block_ee, REG_64::RAX);
    emitter.CALL_INDIR(REG_64::RAX);

#ifdef _WIN32
    emitter.ADD64_REG_IMM(0x20, REG_64::RSP);
#endif

    //Tail-call optimization
    emitter.JMP_INDIR(REG_64::RAX);

    // Exit the block (in case of waiting for COP2 sync, or if we've met the cycles goal)
    emitter.set_jump_dest(exit_cyclecount);

    emit_epilogue();
}

EEJitBlockRecord* EE_JIT64::recompile_block(EmotionEngine& ee, IR::Block& block)
{
    cycles_added = 0;
    ee_branch = false;
    likely_branch = false;
    saved_int_regs = std::vector<REG_64>();
    saved_xmm_regs = std::vector<REG_64>();

    jit_block.clear();

    //Create new stack frame
    emitter.PUSH(REG_64::RBP);
    emitter.MOV64_MR(REG_64::RSP, REG_64::RBP);

    // Preallocate a chunk of stack space for storing registers during function calls, + some other utility
    // RSP + 0h: 32 bytes of argument spillage for Windows
    // RSP + 20h: uint64_t[16] for integer registers
    // RSP + A0h: uint128_t[16] for xmm registers
    // RSP + 1A0h: uint128_t for storing the argument/result of store/load quad
    // An extra 0x8 is needed so that functions we call can have a 16-byte aligned stack pointer.
    emitter.SUB64_REG_IMM(0x1B8, REG_64::RSP);

    while (block.get_instruction_count() > 0 && !likely_branch)
    {
        IR::Instruction instr = block.get_next_instr();
        emit_instruction(ee, instr);
    }

    if (likely_branch)
        handle_branch_likely(ee, block);
    else
        cleanup_recompiler(ee, true, true, block.get_cycle_count());

    return jit_heap.insert_block(ee.get_PC(), &jit_block);
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
        case IR::Opcode::CheckInterlockVU0:
            check_interlock_vu0(ee, instr);
            break;
        case IR::Opcode::ClearDoublewordReg:
            clear_doubleword_reg(ee, instr);
            break;
        case IR::Opcode::ClearWordReg:
            clear_word_reg(ee, instr);
            break;
        case IR::Opcode::DivideWord:
            divide_word(ee, instr, false);
            break;
        case IR::Opcode::DivideWord1:
            divide_word(ee, instr, true);
            break;
        case IR::Opcode::DivideUnsignedWord:
            divide_unsigned_word(ee, instr, false);
            break;
        case IR::Opcode::DivideUnsignedWord1:
            divide_unsigned_word(ee, instr, true);
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
        case IR::Opcode::FixedPointConvertToFloatingPoint:
            fixed_point_convert_to_floating_point(ee, instr);
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
        case IR::Opcode::FloatingPointConvertToFixedPoint:
            floating_point_convert_to_fixed_point(ee, instr);
            break;
        case IR::Opcode::FloatingPointDivide:
            floating_point_divide(ee, instr);
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
        case IR::Opcode::FloatingPointMultiplyAdd:
            floating_point_multiply_add(ee, instr);
            break;
        case IR::Opcode::FloatingPointMultiplySubtract:
            floating_point_multiply_subtract(ee, instr);
            break;
        case IR::Opcode::FloatingPointNegate:
            floating_point_negate(ee, instr);
            break;
        case IR::Opcode::FloatingPointSquareRoot:
            floating_point_square_root(ee, instr);
            break;
        case IR::Opcode::FloatingPointSubtract:
            floating_point_subtract(ee, instr);
            break;
        case IR::Opcode::FloatingPointReciprocalSquareRoot:
            floating_point_reciprocal_square_root(ee, instr);
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
        case IR::Opcode::LoadDoublewordLeft:
            load_doubleword_left(ee, instr);
            break;
        case IR::Opcode::LoadDoublewordRight:
            load_doubleword_right(ee, instr);
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
        case IR::Opcode::LoadWordCoprocessor1:
            load_word_coprocessor1(ee, instr);
            break;
        case IR::Opcode::LoadWordLeft:
            load_word_left(ee, instr);
            break;
        case IR::Opcode::LoadWordRight:
            load_word_right(ee, instr);
            break;
        case IR::Opcode::LoadWordUnsigned:
            load_word_unsigned(ee, instr);
            break;
        case IR::Opcode::LoadQuadword:
            load_quadword(ee, instr);
            break;
        case IR::Opcode::LoadQuadwordCoprocessor2:
            fallback_interpreter(ee, instr);
            break;
        case IR::Opcode::MoveConditionalOnNotZero:
            move_conditional_on_not_zero(ee, instr);
            break;
        case IR::Opcode::MoveConditionalOnZero:
            move_conditional_on_zero(ee, instr);
            break;
        case IR::Opcode::MoveControlWordFromFloatingPoint:
            move_control_word_from_floating_point(ee, instr);
            break;
        case IR::Opcode::MoveControlWordToFloatingPoint:
            move_control_word_to_floating_point(ee, instr);
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
        case IR::Opcode::MoveXmmReg:
            move_xmm_reg(ee, instr);
            break;
        case IR::Opcode::MoveWordReg:
            move_word_reg(ee, instr);
            break;
        case IR::Opcode::MoveQuadwordReg:
            move_quadword_reg(ee, instr);
            break;
        case IR::Opcode::MultiplyAddUnsignedWord:
            multiply_add_unsigned_word(ee, instr, false);
            break;
        case IR::Opcode::MultiplyAddUnsignedWord1:
            multiply_add_unsigned_word(ee, instr, true);
            break;
        case IR::Opcode::MultiplyAddWord:
            multiply_add_word(ee, instr, false);
            break;
        case IR::Opcode::MultiplyAddWord1:
            multiply_add_word(ee, instr, true);
            break;
        case IR::Opcode::MultiplyUnsignedWord:
            multiply_unsigned_word(ee, instr, false);
            break;
        case IR::Opcode::MultiplyUnsignedWord1:
            multiply_unsigned_word(ee, instr, true);
            break;
        case IR::Opcode::MultiplyWord:
            multiply_word(ee, instr, false);
            break;
        case IR::Opcode::MultiplyWord1:
            multiply_word(ee, instr, true);
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
        case IR::Opcode::ParallelAbsoluteHalfword:
            fallback_interpreter(ee, instr); // TODO, needs testing
            break;
        case IR::Opcode::ParallelAbsoluteWord:
            fallback_interpreter(ee, instr); // TODO, needs testing
            break;
        case IR::Opcode::ParallelAnd:
            fallback_interpreter(ee, instr); // TODO, needs testing
            break;
        case IR::Opcode::ParallelAddByte:
            fallback_interpreter(ee, instr); // TODO, needs testing
            break;
        case IR::Opcode::ParallelAddHalfword:
            fallback_interpreter(ee, instr); // TODO, needs testing
            break;
        case IR::Opcode::ParallelAddWord:
            fallback_interpreter(ee, instr); // TODO, needs testing
            break;
        case IR::Opcode::ParallelAddWithSignedSaturationByte:
            fallback_interpreter(ee, instr); // TODO, needs testing
            break;
        case IR::Opcode::ParallelAddWithSignedSaturationHalfword:
            fallback_interpreter(ee, instr); // TODO, needs testing
            break;
        case IR::Opcode::ParallelAddWithSignedSaturationWord:
            fallback_interpreter(ee, instr); // TODO
            break;
        case IR::Opcode::ParallelAddWithUnsignedSaturationByte:
            fallback_interpreter(ee, instr); // TODO, broken in YuGiOh Duelists of The Roses
            break;
        case IR::Opcode::ParallelAddWithUnsignedSaturationHalfword:
            fallback_interpreter(ee, instr); // TODO, needs testing
            break;
        case IR::Opcode::ParallelAddWithUnsignedSaturationWord:
            fallback_interpreter(ee, instr); // TODO
            break;
        case IR::Opcode::ParallelCompareEqualByte:
            fallback_interpreter(ee, instr); // TODO, needs testing
            break;
        case IR::Opcode::ParallelCompareEqualHalfword:
            fallback_interpreter(ee, instr); // TODO, needs testing
            break;
        case IR::Opcode::ParallelCompareEqualWord:
            fallback_interpreter(ee, instr); // TODO, needs testing
            break;
        case IR::Opcode::ParallelCompareGreaterThanByte:
            fallback_interpreter(ee, instr); // TODO, needs testing
            break;
        case IR::Opcode::ParallelCompareGreaterThanHalfword:
            fallback_interpreter(ee, instr); // TODO, needs testing
            break;
        case IR::Opcode::ParallelCompareGreaterThanWord:
            fallback_interpreter(ee, instr); // TODO, needs testing
            break;
        case IR::Opcode::ParallelDivideWord:
            fallback_interpreter(ee, instr); // TODO, needs testing
            break;
        case IR::Opcode::ParallelExchangeEvenHalfword:
            fallback_interpreter(ee, instr); // TODO, needs testing
            break;
        case IR::Opcode::ParallelExchangeCenterHalfword:
            fallback_interpreter(ee, instr); // TODO, needs testing
            break;
        case IR::Opcode::ParallelExchangeEvenWord:
            fallback_interpreter(ee, instr); // TODO, needs testing
            break;
        case IR::Opcode::ParallelExchangeCenterWord:
            fallback_interpreter(ee, instr); // TODO, needs testing
            break;
        case IR::Opcode::ParallelMaximizeHalfword:
            fallback_interpreter(ee, instr); // TODO, needs testing
            break;
        case IR::Opcode::ParallelMaximizeWord:
            fallback_interpreter(ee, instr); // TODO, needs testing
            break;
        case IR::Opcode::ParallelMinimizeHalfword:
            fallback_interpreter(ee, instr); // TODO, needs testing
            break;
        case IR::Opcode::ParallelMinimizeWord:
            fallback_interpreter(ee, instr); // TODO, needs testing
            break;
        case IR::Opcode::ParallelNor:
            fallback_interpreter(ee, instr); // TODO, needs testing
            break;
        case IR::Opcode::ParallelOr:
            fallback_interpreter(ee, instr); // TODO, needs testing
            break;
        case IR::Opcode::ParallelPackToByte:
            fallback_interpreter(ee, instr); // TODO, needs testing
            break;
        case IR::Opcode::ParallelPackToHalfword:
            fallback_interpreter(ee, instr); // TODO, needs testing
            break;
        case IR::Opcode::ParallelPackToWord:
            fallback_interpreter(ee, instr); // TODO, needs testing
            break;
        case IR::Opcode::ParallelShiftLeftLogicalHalfword:
            fallback_interpreter(ee, instr); // TODO, needs testing
            break;
        case IR::Opcode::ParallelShiftLeftLogicalWord:
            fallback_interpreter(ee, instr); // TODO, needs testing
            break;
        case IR::Opcode::ParallelShiftRightArithmeticHalfword:
            fallback_interpreter(ee, instr); // TODO, needs testing
            break;
        case IR::Opcode::ParallelShiftRightArithmeticWord:
            fallback_interpreter(ee, instr); // TODO, needs testing
            break;
        case IR::Opcode::ParallelShiftRightLogicalHalfword:
            fallback_interpreter(ee, instr); // TODO, needs testing
            break;
        case IR::Opcode::ParallelShiftRightLogicalWord:
            fallback_interpreter(ee, instr); // TODO, needs testing
            break;
        case IR::Opcode::ParallelSubtractByte:
            fallback_interpreter(ee, instr); // TODO, needs testing
            break;
        case IR::Opcode::ParallelSubtractHalfword:
            fallback_interpreter(ee, instr); // TODO, needs testing
            break;
        case IR::Opcode::ParallelSubtractWord:
            fallback_interpreter(ee, instr); // TODO, needs testing
            break;
        case IR::Opcode::ParallelSubtractWithSignedSaturationByte:
            fallback_interpreter(ee, instr); // TODO, needs testing
            break;
        case IR::Opcode::ParallelSubtractWithSignedSaturationHalfword:
            fallback_interpreter(ee, instr); // TODO, needs testing
            break;
        case IR::Opcode::ParallelSubtractWithSignedSaturationWord:
            fallback_interpreter(ee, instr); // TODO
            break;
        case IR::Opcode::ParallelSubtractWithUnsignedSaturationByte:
            fallback_interpreter(ee, instr); // TODO, needs testing
            break;
        case IR::Opcode::ParallelSubtractWithUnsignedSaturationHalfword:
            fallback_interpreter(ee, instr); // TODO, needs testing
            break;
        case IR::Opcode::ParallelReverseHalfword:
            fallback_interpreter(ee, instr); // TODO, needs testing
            break;
        case IR::Opcode::ParallelRotate3WordsLeft:
            fallback_interpreter(ee, instr); // TODO, needs testing
            break;
        case IR::Opcode::ParallelSubtractWithUnsignedSaturationWord:
            fallback_interpreter(ee, instr); // TODO
            break;
        case IR::Opcode::ParallelXor:
            fallback_interpreter(ee, instr); // TODO, needs testing
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
        case IR::Opcode::StoreDoublewordLeft:
            store_doubleword_left(ee, instr);
            break;
        case IR::Opcode::StoreDoublewordRight:
            store_doubleword_right(ee, instr);
            break;
        case IR::Opcode::StoreHalfword:
            store_halfword(ee, instr);
            break;
        case IR::Opcode::StoreWord:
            store_word(ee, instr);
            break;
        case IR::Opcode::StoreWordLeft:
            store_word_left(ee, instr);
            break;
        case IR::Opcode::StoreWordRight:
            store_word_right(ee, instr);
            break;
        case IR::Opcode::StoreWordCoprocessor1:
            store_word_coprocessor1(ee, instr);
            break;
        case IR::Opcode::StoreQuadword:
            store_quadword(ee, instr);
            break;
        case IR::Opcode::StoreQuadwordCoprocessor2:
            fallback_interpreter(ee, instr);
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
        case IR::Opcode::UpdateVU0:
            update_vu0(ee, instr);
            break;
        case IR::Opcode::VCallMS:
            vcall_ms(ee, instr);
            break;
        case IR::Opcode::VCallMSR:
            vcall_msr(ee, instr);
            break;
        case IR::Opcode::WaitVU0:
            wait_for_vu0(ee, instr);
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
            Errors::print_warning("[EE_JIT64] Unknown IR instruction %d\n", instr.op);
            fallback_interpreter(ee, instr);
            break;
    }
}

void EE_JIT64::prepare_abi(uint64_t value)
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

    if (int_regs[arg].used)
    {
        saved_int_regs.push_back(arg);
        // Note: The 0x20 here is the int register array offset noted in recompile_block
        // TODO: Store 0x20 in some sort of constant
        emitter.MOV64_TO_MEM(arg, REG_64::RSP, 0x20 + (int)arg * sizeof(uint64_t));
        // int_regs[arg].stored = true;
    }
    int_regs[arg].used = false;
    emitter.load_addr(value, arg);
    abi_int_count++;
}

void EE_JIT64::prepare_abi_reg(REG_64 reg, uint32_t offset)
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

    if (int_regs[arg].used)
    {
        // Note: The 0x20 here is the int register array offset noted in recompile_block
        // TODO: Store 0x20 in some sort of constant
        saved_int_regs.push_back(arg);
        emitter.MOV64_TO_MEM(arg, REG_64::RSP, 0x20 + (int)arg * sizeof(uint64_t));
        // int_regs[arg].stored = true;
    }
    auto p = std::find(saved_int_regs.begin(), saved_int_regs.end(), reg);
    if (p != saved_int_regs.end())
    {
        // The argument register was previously stored on the stack, so it's probably actually garbage.
        // (e.g. on Unix preparing an immediate as a register (ex: 9 -> RDI), then using RDI as an argument)
        // Here we restore it from the stack.
        // Note: The 0x20 here is the int register array offset noted in recompile_block
        // TODO: Store 0x20 in some sort of constant
        emitter.MOV64_FROM_MEM(REG_64::RSP, arg, 0x20 + (int)reg * sizeof(uint64_t));
    }
    int_regs[arg].used = false;
    if (reg != regs[abi_int_count] && p == saved_int_regs.end())
    {   
        if (offset)
            emitter.LEA64_M(reg, arg, offset);
        else
            emitter.MOV64_MR(reg, arg);
    }
    abi_int_count++;
}

void EE_JIT64::prepare_abi_reg_from_xmm(REG_64 reg)
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

    if (int_regs[arg].used)
    {
        saved_int_regs.push_back(arg);
        // Note: The 0x20 here is the int register array offset noted in recompile_block
        // TODO: Store 0x20 in some sort of constant
        emitter.MOV64_TO_MEM(arg, REG_64::RSP, 0x20 + (int)arg * sizeof(uint64_t));
        // int_regs[arg].stored = true;
    }
    int_regs[arg].used = false;
    emitter.MOVD_FROM_XMM(reg, arg);
    abi_int_count++;
}

void EE_JIT64::call_abi_func(uint64_t addr)
{
#ifdef _WIN32
    const static REG_64 saved_regs[] = { RCX, RDX, R8, R9, R10, R11 };
    const static REG_64 abi_regs[] = { RCX, RDX, R8, R9 };
#else
    const static REG_64 saved_regs[] = { RDI, RSI, RCX, RDX, R8, R9, R10, R11 };
    const static REG_64 abi_regs[] = { RDI, RSI, RDX, RCX, R8, R9 };
#endif    

    for (int i = 0; i < abi_int_count; ++i)
        int_regs[abi_regs[i]].locked = false;

    // Store any used XMM registers into the stack
    for (int i = 0; i < 16; ++i)
    {
        if (xmm_regs[i].used && !xmm_regs[i].stored)
        {
            // Note: The 0xA0 here is the xmm register array offset noted in recompile_block
            // TODO: Store 0xA0 in some sort of constant
            emitter.MOVAPS_TO_MEM((REG_64)i, REG_64::RSP, 0xA0 + i * 16);
            xmm_regs[i].stored = true;
        }
    }

    // Store any volatile INT registers into the stack
    for (REG_64 reg : saved_regs)
    {
        if (int_regs[reg].used /* && !int_regs[reg].stored*/)
        {
            // Note: The 0x20 here is the int register array offset noted in recompile_block
            // TODO: Store 0x20 in some sort of constant
            emitter.MOV64_TO_MEM(reg, REG_64::RSP, 0x20 + (int)reg * sizeof(uint64_t));
            // int_regs[reg].stored = true;
        }
    }

    // Call function
    emitter.MOV64_OI(addr, REG_64::RAX);
    emitter.CALL_INDIR(REG_64::RAX);

    // Restore any used INT registers from the stack (+ those stored in prepare_abi functions)
    for (REG_64 reg : saved_regs)
    {
        if (int_regs[reg].used)
        {
            // Note: The 0x20 here is the int register array offset noted in recompile_block
            // TODO: Store 0x20 in some sort of constant
            emitter.MOV64_FROM_MEM(REG_64::RSP, reg, 0x20 + (int)reg * sizeof(uint64_t));
        }
    }
    for (REG_64 reg : saved_int_regs)
    {
        // Note: The 0x20 here is the int register array offset noted in recompile_block
        // TODO: Store 0x20 in some sort of constant
        emitter.MOV64_FROM_MEM(REG_64::RSP, reg, 0x20 + (int)reg * sizeof(uint64_t));
        int_regs[reg].used = true;
    }
    saved_int_regs.clear();
    abi_int_count = 0;
    abi_xmm_count = 0;
}

// Explicitly restore XMM registers when they are stored on the stack
void EE_JIT64::restore_xmm_regs(const std::vector<REG_64>& regs, bool restore_values)
{
    for (REG_64 reg : regs)
    {
        if (xmm_regs[reg].stored)
        {
            // Note: The 0xA0 here is the xmm register array offset noted in recompile_block
            // TODO: Store 0xA0 in some sort of constant
            if (restore_values)
                emitter.MOVAPS_FROM_MEM(REG_64::RSP, reg, 0xA0 + (int)reg * 0x10);
            xmm_regs[reg].stored = false;
        }
    }
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

    // NOTE: If you attempted to allocate a locked register -1, you probably didn't free your registers after lalloc'ing them.
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
            xmm_regs[destination].stored = false;
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

            if (type == REG_TYPE::GPR)
            {
                for (int i = 0; i < 16; ++i)
                {
                    if (xmm_regs[i].used && xmm_regs[i].reg == reg && xmm_regs[i].type == REG_TYPE::GPREXTENDED)
                    {
                        flush_xmm_reg(ee, i);
                        xmm_regs[i].used = false;
                    }
                }
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

            int_regs[destination].stored = false;
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
                if (xmm_regs[destination].stored)
                {
                    // Move int register from stack
                    // Note: The 0xA0 here is the xmm register array offset noted in recompile_block
                    // TODO: Store 0xA0 in some sort of constant
                    emitter.MOVAPS_FROM_MEM(REG_64::RSP, destination, 0xA0 + (int)destination * 0x10);
                    xmm_regs[destination].stored = false;
                }
                if (state != REG_STATE::READ && !(reg == 0 && (type == REG_TYPE::VF || type == REG_TYPE::GPREXTENDED)))
                    xmm_regs[destination].modified = true;
                xmm_regs[destination].age = 0;
                return (REG_64)destination;
            }

            if (type == REG_TYPE::GPREXTENDED)
            {
                for (int i = 0; i < 16; ++i)
                {
                    if (int_regs[i].used && int_regs[i].reg == reg && int_regs[i].type == REG_TYPE::GPR)
                    {
                        flush_int_reg(ee, i);
                        int_regs[i].used = false;
                    }
                }
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
                    if (xmm_regs[reg_to_find].stored)
                    {
                        // Move int register from stack
                        // Note: The 0xA0 here is the xmm register array offset noted in recompile_block
                        // TODO: Store 0xA0 in some sort of constant
                        emitter.MOVAPS_FROM_MEM(REG_64::RSP, destination, 0xA0 + (int)reg_to_find * 0x10);
                    }
                    else
                    {
                        emitter.MOVAPS_REG((REG_64)reg_to_find, destination);
                    }
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
                            emitter.load_addr((uint64_t)get_gpr_addr(ee, (EE_NormalReg)reg), REG_64::RAX);
                            emitter.MOVAPS_FROM_MEM(REG_64::RAX, (REG_64)destination);
                            break;
                        default:
                            break;
                    }

                }
            }

            xmm_regs[destination].stored = false;
            xmm_regs[destination].modified = state != REG_STATE::READ && !(reg == 0 && (type == REG_TYPE::VF || type == REG_TYPE::GPREXTENDED));
            xmm_regs[destination].reg = reg;
            xmm_regs[destination].type = type;
            xmm_regs[destination].used = true;
            xmm_regs[destination].age = 0;
            return (REG_64)destination;
        default:
            Errors::die("[EE_JIT64] Register allocation error! Attempted to allocate a register with an unknown type %d!", type);
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
    flush_int_reg(ee, reg);
    int_regs[reg].used = false;
    int_regs[reg].stored = false;
}

void EE_JIT64::free_xmm_reg(EmotionEngine& ee, REG_64 reg)
{
    xmm_regs[reg].locked = false;
    flush_xmm_reg(ee, reg);
    xmm_regs[reg].used = false;
    xmm_regs[reg].stored = false;
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
                if (old_int_reg < 32)
                {
                    emitter.MOV64_TO_MEM((REG_64)reg, REG_64::R15, offsetof(EmotionEngine, gpr) + get_gpr_offset(old_int_reg));
                }
                else
                {
                    emitter.load_addr((uint64_t)get_gpr_addr(ee, old_int_reg), REG_64::RAX);
                    emitter.MOV64_TO_MEM((REG_64)reg, REG_64::RAX);
                }
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
        if (xmm_regs[reg].stored)
        {
            // Note: The 0xA0 here is the xmm register array offset noted in recompile_block
            // TODO: Store 0xA0 in some sort of constant
            emitter.MOVAPS_FROM_MEM(REG_64::RSP, (REG_64)reg, 0xA0 + (int)reg * 0x10);
        }

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
                // old_xmm_reg = $vf00, which should never be flushed
                if (!old_xmm_reg)
                    break;
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
            return (uint64_t)&ee.LO.lo;
        case EE_SpecialReg::LO1:
            return (uint64_t)&ee.LO.hi;
        case EE_SpecialReg::HI:
            return (uint64_t)&ee.HI.lo;
        case EE_SpecialReg::HI1:
            return (uint64_t)&ee.HI.hi;
        case EE_SpecialReg::SA:
            return (uint64_t)&ee.SA;
        default:
            Errors::die("[EE_JIT64] get_gpr_addr error: Unrecognized reg %d", index);
    }
    return 0;
}

uint64_t EE_JIT64::get_gpr_offset(int index) const
{
    if (index < 32)
        return index * sizeof(uint128_t);
    else
        Errors::die("EE_JIT64::get_gpr_offset not supported for special registers");
}

void EE_JIT64::cleanup_recompiler(EmotionEngine& ee, bool clear_regs, bool dispatcher, uint64_t cycles)
{
    // FIXME: COP2 should handle incrementing the EE cycle count on its on when spinning on mbit (we'll need to increment it ourself on vuwait)
    // Maybe keep it after COP2 fixes incrementing on stalls and conditionally check for vu0wait?
    // Make sure to remove this line when that feature is implemented in COP2 logic!
    cycles = std::max((uint64_t)1, cycles);

    flush_regs(ee);

    if (clear_regs)
    {
        for (int i = 0; i < 16; i++)
        {
            int_regs[i].age = 0;
            int_regs[i].used = false;
            int_regs[i].stored = false;
            xmm_regs[i].age = 0;
            xmm_regs[i].used = false;
            xmm_regs[i].stored = false;
        }
    }

    // Decrement cycles to run by the cycles argument
    emitter.SUB32_MEM_IMM(cycles, REG_64::R15, offsetof(EmotionEngine, cycles_to_run));

    // Update cycle_count_now for COP2 sync
    // cycle_count_now = cycle_count += cycles - cycles we already added
    emitter.MOV64_FROM_MEM(REG_64::R15, REG_64::RAX, offsetof(EmotionEngine, cycle_count));
    emitter.ADD64_REG_IMM(cycles - cycles_added, REG_64::RAX);
    emitter.MOV64_TO_MEM(REG_64::RAX, REG_64::R15, offsetof(EmotionEngine, cycle_count));

    //Clean up stack, has to be handled before we enter dispatcher
    emitter.ADD64_REG_IMM(0x1B8, REG_64::RSP);
    emitter.POP(REG_64::RBP);

    //Go back to the dispatcher to potentially execute another block
    if (dispatcher)
        emit_dispatcher();
    else
        emit_epilogue();
}

void EE_JIT64::emit_prologue()
{
    emitter.PUSH(REG_64::RBX);
    emitter.PUSH(REG_64::R12);
    emitter.PUSH(REG_64::R13);
    emitter.PUSH(REG_64::R14);
    emitter.PUSH(REG_64::R15);
#ifdef _WIN32
    emitter.PUSH(REG_64::RDI);
    emitter.PUSH(REG_64::RSI);
#endif
    //Set DaZ FTZ and rounding modes etc for Denormal handling and rounding mode
    emitter.load_addr((uint64_t)&saved_mxcsr, REG_64::R14);
    emitter.load_addr((uint64_t)&ee_mxcsr, REG_64::R15);
    emitter.STMXCSR(REG_64::R14);
    emitter.LDMXCSR(REG_64::R15);
}

void EE_JIT64::emit_epilogue()
{
    //Revert rounding modes
    emitter.load_addr((uint64_t)&saved_mxcsr, REG_64::R14);
    emitter.LDMXCSR(REG_64::R14);
#ifdef _WIN32
    emitter.POP(REG_64::RSI);
    emitter.POP(REG_64::RDI);
#endif
    emitter.POP(REG_64::R15);
    emitter.POP(REG_64::R14);
    emitter.POP(REG_64::R13);
    emitter.POP(REG_64::R12);
    emitter.POP(REG_64::RBX);
    emitter.RET();
}

void EE_JIT64::handle_branch_likely(EmotionEngine& ee, IR::Block& block)
{
    // Load the "branch happened" variable
    emitter.MOV8_FROM_MEM(REG_64::R15, REG_64::RAX, offsetof(EmotionEngine, branch_on));

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
    cleanup_recompiler(ee, false, true, block.get_cycle_count());

    // ...Which we do here.
    emitter.set_jump_dest(offset_addr);

    // execute delay slot and flush EE state back to EE
    for (IR::Instruction instr = block.get_next_instr(); instr.op != IR::Opcode::Null; instr = block.get_next_instr())
        emit_instruction(ee, instr);
    cleanup_recompiler(ee, true, true, block.get_cycle_count());
}

void EE_JIT64::fallback_interpreter(EmotionEngine& ee, const IR::Instruction &instr)
{
    flush_regs(ee);
    for (int i = 0; i < 16; i++)
    {
        int_regs[i].age = 0;
        int_regs[i].used = false;
        int_regs[i].stored = false;
        xmm_regs[i].age = 0;
        xmm_regs[i].used = false;
        xmm_regs[i].stored = false;
    }

    uint32_t instr_word = instr.get_opcode();

    prepare_abi((uint64_t)&ee);
    prepare_abi(instr_word);

    call_abi_func((uint64_t)instr.get_interpreter_fallback());
}

void EE_JIT64::wait_for_vu0(EmotionEngine& ee, IR::Instruction& instr)
{
    prepare_abi((uint64_t)&ee);
    call_abi_func((uint64_t)&ee_vu0_wait);
    emitter.TEST8_REG(REG_64::AL, REG_64::AL);

    uint8_t* offset_addr = emitter.JCC_NEAR_DEFERRED(ConditionCode::Z);

    // If ee.vu0_wait(), set PC to the current operation's address and abort
    emitter.MOV32_IMM_MEM(instr.get_return_addr(), REG_64::R15, offsetof(EmotionEngine, PC));

    // Set wait for VU0 flag
    emitter.MOV8_IMM_MEM(true, REG_64::R15, offsetof(EmotionEngine, wait_for_VU0));

    // Set cycle count to number of cycles executed
    // FIXME: This changes the return value of the JIT run function, but the return value of the JIT's run function is not used!
    // What do we want to do here?
    // emitter.MOV16_IMM_MEM(instr.get_cycle_count(), REG_64::R14, offsetof(EE_JIT64, cycle_count));

    cleanup_recompiler(ee, false, false, instr.get_cycle_count());

    // Otherwise continue execution of block otherwise
    emitter.set_jump_dest(offset_addr);
}

void EE_JIT64::check_interlock_vu0(EmotionEngine& ee, IR::Instruction& instr)
{
    prepare_abi((uint64_t)&ee);
    call_abi_func((uint64_t)&ee_check_interlock);
    emitter.TEST8_REG(REG_64::AL, REG_64::AL);

    uint8_t* offset_addr = emitter.JCC_NEAR_DEFERRED(ConditionCode::Z);

    // If ee.ee_check_interlock(), set PC to the current operation's address and abort
    emitter.MOV32_IMM_MEM(instr.get_return_addr(), REG_64::R15, offsetof(EmotionEngine, PC));

    // Set wait for interlock clear flag
    emitter.MOV8_IMM_MEM(true, REG_64::R15, offsetof(EmotionEngine, wait_for_interlock));

    // Set cycle count to number of cycles executed
    // FIXME: This changes the return value of the JIT run function, but the return value of the JIT's run function is not used!
    // What do we want to do here?
    // emitter.MOV16_IMM_MEM(instr.get_cycle_count(), REG_64::R14, offsetof(EE_JIT64, cycle_count));

    cleanup_recompiler(ee, false, false, instr.get_cycle_count());

    // Otherwise clear interlock then continue execution of block otherwise
    emitter.set_jump_dest(offset_addr);
    prepare_abi((uint64_t)&ee);
    call_abi_func((uint64_t)&ee_clear_interlock);
}

void EE_JIT64::update_vu0(EmotionEngine& ee, IR::Instruction& instr)
{
    // Update PC
    emitter.load_addr((uint64_t)&ee, REG_64::RAX);
    emitter.MOV32_IMM_MEM(instr.get_return_addr(), REG_64::RAX, offsetof(EmotionEngine, PC_now));

    // Update cycle_count_now for COP2 sync
    // cycle_count_now = cycle_count += cycles
    emitter.MOV64_FROM_MEM(REG_64::R15, REG_64::RAX, offsetof(EmotionEngine, cycle_count));
    emitter.ADD64_REG_IMM(instr.get_cycle_count() - cycles_added, REG_64::RAX);
    emitter.MOV64_TO_MEM(REG_64::RAX, REG_64::R15, offsetof(EmotionEngine, cycle_count));

    cycles_added = instr.get_cycle_count();
}

uint8_t ee_read8(EmotionEngine& ee, uint32_t addr)
{
    return ee.read8(addr);
}

uint16_t ee_read16(EmotionEngine& ee, uint32_t addr)
{
    return ee.read16(addr);
}

uint32_t ee_read32(EmotionEngine& ee, uint32_t addr)
{
    return ee.read32(addr);
}

uint64_t ee_read64(EmotionEngine& ee, uint32_t addr)
{
    return ee.read64(addr);
}

void ee_read128(EmotionEngine& ee, uint32_t addr, uint128_t& dest)
{
    dest = ee.read128(addr);
}

void ee_write8(EmotionEngine& ee, uint32_t addr, uint8_t value)
{
    ee.write8(addr, value);
}

void ee_write16(EmotionEngine& ee, uint32_t addr, uint16_t value)
{
    ee.write16(addr, value);
}

void ee_write32(EmotionEngine& ee, uint32_t addr, uint32_t value)
{
    ee.write32(addr, value);
}

void ee_write64(EmotionEngine& ee, uint32_t addr, uint64_t value)
{
    ee.write64(addr, value);
}

void ee_write128(EmotionEngine& ee, uint32_t addr, uint128_t& value)
{
    ee.write128(addr, value);
}

void ee_syscall_exception(EmotionEngine& ee)
{
    ee.syscall_exception();
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

bool ee_check_interlock(EmotionEngine& ee)
{
    return ee.check_interlock();
}

void ee_clear_interlock(EmotionEngine& ee)
{
    ee.clear_interlock();
}