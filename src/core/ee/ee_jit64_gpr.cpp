#include <cmath>
#include <algorithm>
#include "ee_jit64.hpp"
#include "vu.hpp"

bool cop0_get_condition(Cop0& cop0)
{
    return cop0.get_condition();
}

void EE_JIT64::add_doubleword_imm(EmotionEngine& ee, IR::Instruction &instr)
{
    REG_64 source = alloc_reg(ee, instr.get_source(), REG_TYPE::GPR, REG_STATE::READ);
    REG_64 dest = alloc_reg(ee, instr.get_dest(), REG_TYPE::GPR, REG_STATE::WRITE);

    emitter.LEA64_M(source, dest, instr.get_source2());
}

void EE_JIT64::add_doubleword_reg(EmotionEngine& ee, IR::Instruction &instr)
{
    REG_64 source = alloc_reg(ee, instr.get_source(), REG_TYPE::GPR, REG_STATE::READ);
    REG_64 source2 = alloc_reg(ee, instr.get_source2(), REG_TYPE::GPR, REG_STATE::READ);
    REG_64 dest = alloc_reg(ee, instr.get_dest(), REG_TYPE::GPR, REG_STATE::WRITE);

    emitter.LEA64_REG(source, source2, dest);
}

void EE_JIT64::add_word_imm(EmotionEngine& ee, IR::Instruction &instr)
{
    REG_64 source = alloc_reg(ee, instr.get_source(), REG_TYPE::GPR, REG_STATE::READ);
    REG_64 dest = alloc_reg(ee, instr.get_dest(), REG_TYPE::GPR, REG_STATE::WRITE);

    emitter.LEA32_M(source, dest, instr.get_source2());
    emitter.MOVSX32_TO_64(dest, dest);
}

void EE_JIT64::add_word_reg(EmotionEngine& ee, IR::Instruction &instr)
{
    REG_64 source = alloc_reg(ee, instr.get_source(), REG_TYPE::GPR, REG_STATE::READ);
    REG_64 source2 = alloc_reg(ee, instr.get_source2(), REG_TYPE::GPR, REG_STATE::READ);
    REG_64 dest = alloc_reg(ee, instr.get_dest(), REG_TYPE::GPR, REG_STATE::WRITE);

    emitter.LEA32_REG(source, source2, dest);
    emitter.MOVSX32_TO_64(dest, dest);
}

void EE_JIT64::and_imm(EmotionEngine& ee, IR::Instruction &instr)
{
    REG_64 source = alloc_reg(ee, instr.get_source(), REG_TYPE::GPR, REG_STATE::READ);
    REG_64 dest = alloc_reg(ee, instr.get_dest(), REG_TYPE::GPR, REG_STATE::WRITE);
    uint16_t imm = instr.get_source2();

    if (source != dest)
        emitter.MOV64_MR(source, dest);
    emitter.AND16_REG_IMM(imm, dest);
    emitter.MOVZX16_TO_64(dest, dest);
}

void EE_JIT64::and_reg(EmotionEngine& ee, IR::Instruction &instr)
{
    REG_64 source = alloc_reg(ee, instr.get_source(), REG_TYPE::GPR, REG_STATE::READ);
    REG_64 source2 = alloc_reg(ee, instr.get_source2(), REG_TYPE::GPR, REG_STATE::READ);
    REG_64 dest = alloc_reg(ee, instr.get_dest(), REG_TYPE::GPR, REG_STATE::WRITE);

    if (dest == source)
    {
        emitter.AND64_REG(source2, dest);
    }
    else if (dest == source2)
    {
        emitter.AND64_REG(source, dest);
    }
    else
    {
        emitter.MOV64_MR(source, dest);
        emitter.AND64_REG(source2, dest);
    }
}

void EE_JIT64::branch_cop0(EmotionEngine& ee, IR::Instruction &instr)
{
    // Call cop0.get_condition to get value to compare
    prepare_abi(ee, reinterpret_cast<uint64_t>(&ee.cp0));

    call_abi_func(ee, (uint64_t)cop0_get_condition);

    // Alloc scratchpad register
    REG_64 R15 = lalloc_reg(ee, 0, REG_TYPE::INTSCRATCHPAD, REG_STATE::SCRATCHPAD);

    // Conditionally move the success or failure destination into ee.PC
    emitter.MOV8_REG_IMM(instr.get_field(), R15);
    emitter.CMP8_REG(REG_64::RAX, R15);
    emitter.MOV32_REG_IMM(instr.get_jump_fail_dest(), REG_64::RAX);
    emitter.MOV32_REG_IMM(instr.get_jump_dest(), R15);
    emitter.CMOVCC32_REG(ConditionCode::E, R15, REG_64::RAX);
    emitter.load_addr((uint64_t)&ee.PC, R15);
    emitter.MOV32_TO_MEM(REG_64::RAX, R15);

    if (instr.get_is_likely())
    {
        likely_branch = true;

        // Use the result of the FLAGS register from the last compare to set branch_on,
        // this boolean is used in handle_likely_branch
        emitter.load_addr((uint64_t)&ee.branch_on, REG_64::RAX);
        emitter.SETCC_MEM(ConditionCode::E, REG_64::RAX);
    }
    free_int_reg(ee, R15);
}

void EE_JIT64::branch_cop1(EmotionEngine& ee, IR::Instruction &instr)
{
    REG_64 R15 = lalloc_reg(ee, 0, REG_TYPE::INTSCRATCHPAD, REG_STATE::SCRATCHPAD);

    // Conditionally move the success or failure destination into ee.PC
    emitter.load_addr((uint64_t)&ee.fpu->control.condition, REG_64::RAX);
    emitter.MOV8_FROM_MEM(REG_64::RAX, REG_64::RAX);
    emitter.MOV8_REG_IMM(instr.get_field(), R15);
    emitter.CMP8_REG(REG_64::RAX, R15);
    emitter.MOV32_REG_IMM(instr.get_jump_fail_dest(), REG_64::RAX);
    emitter.MOV32_REG_IMM(instr.get_jump_dest(), R15);
    emitter.CMOVCC32_REG(ConditionCode::E, R15, REG_64::RAX);
    emitter.load_addr((uint64_t)&ee.PC, R15);
    emitter.MOV32_TO_MEM(REG_64::RAX, R15);

    if (instr.get_is_likely())
    {
        likely_branch = true;

        // Use the result of the FLAGS register from the last compare to set branch_on,
        // this boolean is used in handle_likely_branch
        emitter.load_addr((uint64_t)&ee.branch_on, REG_64::RAX);
        emitter.SETCC_MEM(ConditionCode::E, REG_64::RAX);
    }
    free_int_reg(ee, R15);
}

void EE_JIT64::branch_cop2(EmotionEngine& ee, IR::Instruction &instr)
{
    Errors::die("[EE_JIT64] Branch COP2 not implemented!");
}

void EE_JIT64::branch_equal(EmotionEngine& ee, IR::Instruction &instr)
{
    // Alloc registers to compare
    REG_64 op1 = alloc_reg(ee, instr.get_source(), REG_TYPE::GPR, REG_STATE::READ);
    REG_64 op2 = alloc_reg(ee, instr.get_source2(), REG_TYPE::GPR, REG_STATE::READ);

    // Alloc scratchpad registers
    REG_64 R15 = lalloc_reg(ee, 0, REG_TYPE::INTSCRATCHPAD, REG_STATE::SCRATCHPAD);

    // Conditionally move the success or failure destination into ee.PC
    emitter.MOV32_REG_IMM(instr.get_jump_fail_dest(), REG_64::RAX);
    emitter.MOV32_REG_IMM(instr.get_jump_dest(), R15);
    emitter.CMP64_REG(op2, op1);
    emitter.CMOVCC32_REG(ConditionCode::E, R15, REG_64::RAX);
    emitter.load_addr((uint64_t)&ee.PC, R15);
    emitter.MOV32_TO_MEM(REG_64::RAX, R15);

    if (instr.get_is_likely())
    {
        likely_branch = true;

        // Use the result of the FLAGS register from the last compare to set branch_on,
        // this boolean is used in handle_likely_branch
        emitter.load_addr((uint64_t)&ee.branch_on, REG_64::RAX);
        emitter.SETCC_MEM(ConditionCode::E, REG_64::RAX);
    }

    // Free scratchpad registers
    free_int_reg(ee, R15);
}

void EE_JIT64::branch_equal_zero(EmotionEngine& ee, IR::Instruction &instr)
{
    // Alloc registers to compare
    REG_64 op1 = alloc_reg(ee, instr.get_source(), REG_TYPE::GPR, REG_STATE::READ);

    // Alloc scratchpad registers
    REG_64 R15 = lalloc_reg(ee, 0, REG_TYPE::INTSCRATCHPAD, REG_STATE::SCRATCHPAD);

    // Conditionally move the success or failure destination into ee.PC
    emitter.MOV32_REG_IMM(instr.get_jump_fail_dest(), REG_64::RAX);
    emitter.MOV32_REG_IMM(instr.get_jump_dest(), R15);
    emitter.TEST64_REG(op1, op1);
    emitter.CMOVCC32_REG(ConditionCode::Z, R15, REG_64::RAX);
    emitter.load_addr((uint64_t)&ee.PC, R15);
    emitter.MOV32_TO_MEM(REG_64::RAX, R15);

    if (instr.get_is_likely())
    {
        likely_branch = true;

        // Use the result of the FLAGS register from the last compare to set branch_on,
        // this boolean is used in handle_likely_branch
        emitter.load_addr((uint64_t)&ee.branch_on, REG_64::RAX);
        emitter.SETCC_MEM(ConditionCode::Z, REG_64::RAX);
    }

    // Free scratchpad registers
    free_int_reg(ee, R15);
}

void EE_JIT64::branch_greater_than_or_equal_zero(EmotionEngine& ee, IR::Instruction &instr)
{
    // Alloc registers to compare
    REG_64 op1 = alloc_reg(ee, instr.get_source(), REG_TYPE::GPR, REG_STATE::READ);

    // Alloc scratchpad registers
    REG_64 R15 = lalloc_reg(ee, 0, REG_TYPE::INTSCRATCHPAD, REG_STATE::SCRATCHPAD);

    if (instr.get_is_link())
    {
        // Set the link register
        emitter.load_addr((uint64_t)get_gpr_addr(ee, EE_NormalReg::ra), REG_64::RAX);
        emitter.MOV32_IMM_MEM(instr.get_return_addr(), REG_64::RAX);
    }

    // Conditionally move the success or failure destination into ee.PC
    emitter.MOV32_REG_IMM(instr.get_jump_fail_dest(), REG_64::RAX);
    emitter.MOV32_REG_IMM(instr.get_jump_dest(), R15);
    emitter.TEST64_REG(op1, op1);
    emitter.CMOVCC32_REG(ConditionCode::NS, R15, REG_64::RAX);
    emitter.load_addr((uint64_t)&ee.PC, R15);
    emitter.MOV32_TO_MEM(REG_64::RAX, R15);

    if (instr.get_is_likely())
    {
        likely_branch = true;

        // Use the result of the FLAGS register from the last compare to set branch_on,
        // this boolean is used in handle_likely_branch
        emitter.load_addr((uint64_t)&ee.branch_on, REG_64::RAX);
        emitter.SETCC_MEM(ConditionCode::NS, REG_64::RAX);
    }

    // Free scratchpad registers
    free_int_reg(ee, R15);
}

void EE_JIT64::branch_greater_than_zero(EmotionEngine& ee, IR::Instruction &instr)
{
    // Alloc registers to compare
    REG_64 op1 = alloc_reg(ee, instr.get_source(), REG_TYPE::GPR, REG_STATE::READ);

    // Alloc scratchpad registers
    REG_64 R15 = lalloc_reg(ee, 0, REG_TYPE::INTSCRATCHPAD, REG_STATE::SCRATCHPAD);

    // Conditionally move the success or failure destination into ee.PC
    emitter.TEST64_REG(op1, op1);
    emitter.MOV32_REG_IMM(instr.get_jump_fail_dest(), REG_64::RAX);
    emitter.MOV32_REG_IMM(instr.get_jump_dest(), R15);
    emitter.CMOVCC32_REG(ConditionCode::G, R15, REG_64::RAX);
    emitter.load_addr((uint64_t)&ee.PC, R15);
    emitter.MOV32_TO_MEM(REG_64::RAX, R15);

    if (instr.get_is_likely())
    {
        likely_branch = true;

        // Use the result of the FLAGS register from the last compare to set branch_on,
        // this boolean is used in handle_likely_branch
        emitter.load_addr((uint64_t)&ee.branch_on, REG_64::RAX);
        emitter.SETCC_MEM(ConditionCode::G, REG_64::RAX);
    }

    // Free scratchpad registers
    free_int_reg(ee, R15);
}

void EE_JIT64::branch_less_than_or_equal_zero(EmotionEngine& ee, IR::Instruction &instr)
{
    // Alloc registers to compare
    REG_64 op1 = alloc_reg(ee, instr.get_source(), REG_TYPE::GPR, REG_STATE::READ);

    // Alloc scratchpad registers
    REG_64 R15 = lalloc_reg(ee, 0, REG_TYPE::INTSCRATCHPAD, REG_STATE::SCRATCHPAD);

    // Conditionally move the success or failure destination into ee.PC
    emitter.TEST64_REG(op1, op1);
    emitter.MOV32_REG_IMM(instr.get_jump_fail_dest(), REG_64::RAX);
    emitter.MOV32_REG_IMM(instr.get_jump_dest(), R15);
    emitter.CMOVCC32_REG(ConditionCode::LE, R15, REG_64::RAX);
    emitter.load_addr((uint64_t)&ee.PC, R15);
    emitter.MOV32_TO_MEM(REG_64::RAX, R15);

    if (instr.get_is_likely())
    {
        likely_branch = true;

        // Use the result of the FLAGS register from the last compare to set branch_on,
        // this boolean is used in handle_likely_branch
        emitter.load_addr((uint64_t)&ee.branch_on, REG_64::RAX);
        emitter.SETCC_MEM(ConditionCode::LE, REG_64::RAX);
    }

    // Free scratchpad registers
    free_int_reg(ee, R15);
}

void EE_JIT64::branch_less_than_zero(EmotionEngine& ee, IR::Instruction &instr)
{
    // Alloc registers to compare
    REG_64 op1 = alloc_reg(ee, instr.get_source(), REG_TYPE::GPR, REG_STATE::READ);

    // Alloc scratchpad registers
    REG_64 R15 = lalloc_reg(ee, 0, REG_TYPE::INTSCRATCHPAD, REG_STATE::SCRATCHPAD);

    if (instr.get_is_link())
    {
        // Set the link register
        emitter.load_addr((uint64_t)get_gpr_addr(ee, EE_NormalReg::ra), REG_64::RAX);
        emitter.MOV32_IMM_MEM(instr.get_return_addr(), REG_64::RAX);
    }

    // Conditionally move the success or failure destination into ee.PC
    emitter.MOV32_REG_IMM(instr.get_jump_fail_dest(), REG_64::RAX);
    emitter.MOV32_REG_IMM(instr.get_jump_dest(), R15);
    emitter.TEST64_REG(op1, op1);
    emitter.CMOVCC32_REG(ConditionCode::S, R15, REG_64::RAX);
    emitter.load_addr((uint64_t)&ee.PC, R15);
    emitter.MOV32_TO_MEM(REG_64::RAX, R15);

    if (instr.get_is_likely())
    {
        likely_branch = true;

        // Use the result of the FLAGS register from the last compare to set branch_on,
        // this boolean is used in handle_likely_branch
        emitter.load_addr((uint64_t)&ee.branch_on, REG_64::RAX);
        emitter.SETCC_MEM(ConditionCode::S, REG_64::RAX);
    }

    // Free scratchpad registers
    free_int_reg(ee, R15);
}


void EE_JIT64::branch_not_equal(EmotionEngine& ee, IR::Instruction &instr)
{
    // Alloc registers to compare
    REG_64 op1 = alloc_reg(ee, instr.get_source(), REG_TYPE::GPR, REG_STATE::READ);
    REG_64 op2 = alloc_reg(ee, instr.get_source2(), REG_TYPE::GPR, REG_STATE::READ);

    // Alloc scratchpad registers
    REG_64 R15 = lalloc_reg(ee, 0, REG_TYPE::INTSCRATCHPAD, REG_STATE::SCRATCHPAD);

    // Conditionally move the success or failure destination into ee.PC
    emitter.MOV32_REG_IMM(instr.get_jump_fail_dest(), REG_64::RAX);
    emitter.MOV32_REG_IMM(instr.get_jump_dest(), R15);
    emitter.CMP64_REG(op2, op1);
    emitter.CMOVCC32_REG(ConditionCode::NE, R15, REG_64::RAX);
    emitter.load_addr((uint64_t)&ee.PC, R15);
    emitter.MOV32_TO_MEM(REG_64::RAX, R15);

    if (instr.get_is_likely())
    {
        likely_branch = true;

        // Use the result of the FLAGS register from the last compare to set branch_on,
        // this boolean is used in handle_likely_branch
        emitter.load_addr((uint64_t)&ee.branch_on, REG_64::RAX);
        emitter.SETCC_MEM(ConditionCode::NE, REG_64::RAX);
    }

    // Free scratchpad registers
    free_int_reg(ee, R15);
}

void EE_JIT64::branch_not_equal_zero(EmotionEngine& ee, IR::Instruction &instr)
{
    // Alloc registers to compare
    REG_64 op1 = alloc_reg(ee, instr.get_source(), REG_TYPE::GPR, REG_STATE::READ);

    // Alloc scratchpad registers
    REG_64 R15 = lalloc_reg(ee, 0, REG_TYPE::INTSCRATCHPAD, REG_STATE::SCRATCHPAD);

    // Conditionally move the success or failure destination into ee.PC
    emitter.MOV32_REG_IMM(instr.get_jump_fail_dest(), REG_64::RAX);
    emitter.MOV32_REG_IMM(instr.get_jump_dest(), R15);
    emitter.TEST64_REG(op1, op1);
    emitter.CMOVCC32_REG(ConditionCode::NZ, R15, REG_64::RAX);
    emitter.load_addr((uint64_t)&ee.PC, R15);
    emitter.MOV32_TO_MEM(REG_64::RAX, R15);

    if (instr.get_is_likely())
    {
        likely_branch = true;

        // Use the result of the FLAGS register from the last compare to set branch_on,
        // this boolean is used in handle_likely_branch
        emitter.load_addr((uint64_t)&ee.branch_on, REG_64::RAX);
        emitter.SETCC_MEM(ConditionCode::NZ, REG_64::RAX);
    }

    // Free scratchpad registers
    free_int_reg(ee, R15);
}

void EE_JIT64::clear_doubleword_reg(EmotionEngine& ee, IR::Instruction& instr)
{
    REG_64 dest = alloc_reg(ee, instr.get_dest(), REG_TYPE::GPR, REG_STATE::WRITE);
    emitter.XOR64_REG(dest, dest);
}

void EE_JIT64::clear_word_reg(EmotionEngine& ee, IR::Instruction& instr)
{
    REG_64 dest = alloc_reg(ee, instr.get_dest(), REG_TYPE::GPR, REG_STATE::WRITE);
    emitter.XOR32_REG(dest, dest);
}

void EE_JIT64::divide_unsigned_word(EmotionEngine& ee, IR::Instruction& instr)
{
    // idiv result is stored in RAX:RDX, so we allocate those registers first.
    REG_64 RDX = lalloc_reg(ee, 0, REG_TYPE::GPR, REG_STATE::SCRATCHPAD, REG_64::RDX);
    REG_64 dividend = alloc_reg(ee, instr.get_source(), REG_TYPE::GPR, REG_STATE::READ);
    REG_64 divisor = alloc_reg(ee, instr.get_source2(), REG_TYPE::GPR, REG_STATE::READ);
    REG_64 LO = alloc_reg(ee, (int)EE_SpecialReg::LO, REG_TYPE::GPR, REG_STATE::WRITE);
    REG_64 HI = alloc_reg(ee, (int)EE_SpecialReg::HI, REG_TYPE::GPR, REG_STATE::WRITE);

    emitter.TEST32_REG(divisor, divisor);
    uint8_t *label1 = emitter.JCC_NEAR_DEFERRED(ConditionCode::Z);
    emitter.MOV32_REG(dividend, REG_64::RAX);
    emitter.XOR32_REG(RDX, RDX);
    emitter.DIV32(divisor);
    emitter.MOVSX32_TO_64(REG_64::RAX, REG_64::RAX);
    emitter.MOVSX32_TO_64(RDX, RDX);
    move_to_lo_hi(ee, REG_64::RAX, RDX, LO, HI);
    uint8_t *end = emitter.JMP_NEAR_DEFERRED();
    free_int_reg(ee, RDX);

    emitter.set_jump_dest(label1);
    emitter.MOVSX32_TO_64(dividend, HI);
    emitter.MOV64_OI(-1, LO);

    emitter.set_jump_dest(end);
}

void EE_JIT64::divide_word(EmotionEngine& ee, IR::Instruction &instr)
{
    /*
    NOTE:
    This operation is kind of a mess due to all the possible edge cases.
    I have included an example of what this operation may look like in x64 assembly.
    You may also wish to refer to the implementation in emotioninterpreter.cpp.
    -Souzooka

        ; allocated regs - RDX, RAX, source (RCX), source2 (RBX), HI/LO (N/A)
        cmp ECX, 0x80000000
        jne label1
        cmp EBX, 0xFFFFFFFF
        jne label1
        ; move 0x80000000 to LO
        ; move 0 to HI
        jmp end
    label1:
        test EBX, EBX
        jz label2
        mov EAX, ECX
        cdq       ; sign extend EAX into EDX
        idiv EBX  ; divide ECX by EBX, division is stored in EAX, mod is stored in EDX
        movsx RAX, EAX
        movsx RDX, EDX
        ; move RAX to LO
        ; move RDX to HI
        jmp end
    label2:
        test ECX, ECX
        setl AL
        shl AL, 1 ; AL will be 0 if op1 >= 0, 2 otherwise
        dec AL    ; AL will be -1 if op1 >= 0, 1 otherwise
        movzx RAX, AL
        movsx RCX, ECX
        ; move RAX to LO
        ; move RCX to HI
    end:
        ; continue block
    */

    // idiv result is stored in RAX:RDX, so we allocate those registers first.
    REG_64 RDX = lalloc_reg(ee, 0, REG_TYPE::GPR, REG_STATE::SCRATCHPAD, REG_64::RDX);
    REG_64 dividend = alloc_reg(ee, instr.get_source(), REG_TYPE::GPR, REG_STATE::READ);
    REG_64 divisor = alloc_reg(ee, instr.get_source2(), REG_TYPE::GPR, REG_STATE::READ);
    REG_64 LO = alloc_reg(ee, (int)EE_SpecialReg::LO, REG_TYPE::GPR, REG_STATE::WRITE);
    REG_64 HI = alloc_reg(ee, (int)EE_SpecialReg::HI, REG_TYPE::GPR, REG_STATE::WRITE);

    emitter.CMP32_IMM(0x80000000, dividend);
    uint8_t *label1_1 = emitter.JCC_NEAR_DEFERRED(ConditionCode::NE);
    emitter.CMP32_IMM(0xFFFFFFFF, divisor);
    uint8_t *label1_2 = emitter.JCC_NEAR_DEFERRED(ConditionCode::NE);
    move_to_lo_hi_imm(ee, (int64_t)(int32_t)0x80000000, 0);
    uint8_t *end_1 = emitter.JMP_NEAR_DEFERRED();

    emitter.set_jump_dest(label1_1);
    emitter.set_jump_dest(label1_2);
    emitter.TEST32_REG(divisor, divisor);
    uint8_t* label2 = emitter.JCC_NEAR_DEFERRED(ConditionCode::Z);
    emitter.MOV32_REG(dividend, REG_64::RAX);
    emitter.CDQ();
    emitter.IDIV32(divisor);
    emitter.MOVSX32_TO_64(REG_64::RAX, REG_64::RAX);
    emitter.MOVSX32_TO_64(RDX, RDX);
    move_to_lo_hi(ee, REG_64::RAX, RDX, LO, HI);
    uint8_t *end_2 = emitter.JMP_NEAR_DEFERRED();

    emitter.set_jump_dest(label2);
    emitter.TEST32_REG(dividend, dividend);
    emitter.SETCC_REG(ConditionCode::L, REG_64::RAX);
    emitter.SHL8_REG_1(REG_64::RAX);
    emitter.DEC8(REG_64::RAX);
    emitter.MOVSX8_TO_64(REG_64::RAX, REG_64::RAX);
    emitter.MOVSX32_TO_64(dividend, dividend);
    move_to_lo_hi(ee, REG_64::RAX, dividend, LO, HI);

    emitter.set_jump_dest(end_1);
    emitter.set_jump_dest(end_2);

    free_int_reg(ee, RDX);
}

void EE_JIT64::doubleword_shift_left_logical(EmotionEngine& ee, IR::Instruction& instr)
{
    REG_64 source = alloc_reg(ee, instr.get_source(), REG_TYPE::GPR, REG_STATE::READ);
    REG_64 dest = alloc_reg(ee, instr.get_dest(), REG_TYPE::GPR, REG_STATE::WRITE);

    if (source != dest)
        emitter.MOV64_MR(source, dest);
    emitter.SHL64_REG_IMM(instr.get_source2(), dest);
}

void EE_JIT64::doubleword_shift_left_logical_variable(EmotionEngine& ee, IR::Instruction& instr)
{
    // Alloc variable into RCX
    REG_64 RCX = lalloc_reg(ee, 0, REG_TYPE::GPR, REG_STATE::SCRATCHPAD, REG_64::RCX);
    REG_64 variable = alloc_reg(ee, instr.get_source2(), REG_TYPE::GPR, REG_STATE::READ);
    emitter.MOV8_REG(variable, RCX);
    emitter.AND8_REG_IMM(0x3F, RCX);

    REG_64 source = alloc_reg(ee, instr.get_source(), REG_TYPE::GPR, REG_STATE::READ);
    REG_64 dest = alloc_reg(ee, instr.get_dest(), REG_TYPE::GPR, REG_STATE::WRITE);

    if (source != dest)
        emitter.MOV64_MR(source, dest);

    emitter.SHL64_CL(dest);
    free_int_reg(ee, RCX);
}

void EE_JIT64::doubleword_shift_right_arithmetic(EmotionEngine& ee, IR::Instruction& instr)
{
    REG_64 source = alloc_reg(ee, instr.get_source(), REG_TYPE::GPR, REG_STATE::READ);
    REG_64 dest = alloc_reg(ee, instr.get_dest(), REG_TYPE::GPR, REG_STATE::WRITE);

    if (source != dest)
        emitter.MOV64_MR(source, dest);
    emitter.SAR64_REG_IMM(instr.get_source2(), dest);
}

void EE_JIT64::doubleword_shift_right_arithmetic_variable(EmotionEngine& ee, IR::Instruction& instr)
{
    // Alloc variable into RCX
    REG_64 RCX = lalloc_reg(ee, 0, REG_TYPE::GPR, REG_STATE::SCRATCHPAD, REG_64::RCX);
    REG_64 variable = alloc_reg(ee, instr.get_source2(), REG_TYPE::GPR, REG_STATE::READ);
    emitter.MOV8_REG(variable, RCX);
    emitter.AND8_REG_IMM(0x3F, RCX);

    REG_64 source = alloc_reg(ee, instr.get_source(), REG_TYPE::GPR, REG_STATE::READ);
    REG_64 dest = alloc_reg(ee, instr.get_dest(), REG_TYPE::GPR, REG_STATE::WRITE);

    if (source != dest)
        emitter.MOV64_MR(source, dest);

    emitter.SAR64_CL(dest);

    free_int_reg(ee, RCX);
}

void EE_JIT64::doubleword_shift_right_logical(EmotionEngine& ee, IR::Instruction& instr)
{
    REG_64 source = alloc_reg(ee, instr.get_source(), REG_TYPE::GPR, REG_STATE::READ);
    REG_64 dest = alloc_reg(ee, instr.get_dest(), REG_TYPE::GPR, REG_STATE::WRITE);

    if (source != dest)
        emitter.MOV64_MR(source, dest);
    emitter.SHR64_REG_IMM(instr.get_source2(), dest);
}

void EE_JIT64::doubleword_shift_right_logical_variable(EmotionEngine& ee, IR::Instruction& instr)
{
    // Alloc variable into RCX
    REG_64 RCX = lalloc_reg(ee, 0, REG_TYPE::GPR, REG_STATE::SCRATCHPAD, REG_64::RCX);
    REG_64 variable = alloc_reg(ee, instr.get_source2(), REG_TYPE::GPR, REG_STATE::READ);
    emitter.MOV8_REG(variable, RCX);
    emitter.AND8_REG_IMM(0x3F, RCX);

    REG_64 source = alloc_reg(ee, instr.get_source(), REG_TYPE::GPR, REG_STATE::READ);
    REG_64 dest = alloc_reg(ee, instr.get_dest(), REG_TYPE::GPR, REG_STATE::WRITE);

    if (source != dest)
        emitter.MOV64_MR(source, dest);

    emitter.SHR64_CL(dest);
    free_int_reg(ee, RCX);
}

void EE_JIT64::exception_return(EmotionEngine& ee, IR::Instruction& instr)
{
    // Alloc scratchpad registers
    REG_64 R15 = lalloc_reg(ee, 0, REG_TYPE::INTSCRATCHPAD, REG_STATE::SCRATCHPAD);

    // Cleanup branch_on, exception handler expects this to be false
    emitter.load_addr((uint64_t)&ee.branch_on, REG_64::RAX);
    emitter.MOV8_REG_IMM(false, R15);
    emitter.MOV8_TO_MEM(R15, REG_64::RAX);

    fallback_interpreter(ee, instr);

    // Since the interpreter decrements PC by 4, we reset it here to account for that.
    emitter.load_addr((uint64_t)&ee.PC, REG_64::RAX);
    emitter.MOV32_FROM_MEM(REG_64::RAX, R15);
    emitter.ADD32_REG_IMM(4, R15);
    emitter.MOV32_TO_MEM(R15, REG_64::RAX);

    // Free scratchpad registers
    free_int_reg(ee, R15);
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
    REG_64 jump_dest = alloc_reg(ee, instr.get_source(), REG_TYPE::GPR, REG_STATE::READ);

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


void EE_JIT64::load_byte(EmotionEngine& ee, IR::Instruction &instr)
{
    alloc_abi_regs(2);
    REG_64 source = alloc_reg(ee, instr.get_source(), REG_TYPE::GPR, REG_STATE::READ);
    REG_64 addr = lalloc_reg(ee, 0, REG_TYPE::INTSCRATCHPAD, REG_STATE::SCRATCHPAD);

    int64_t offset = instr.get_source2();

    if (offset)
        emitter.LEA32_M(source, addr, offset);
    else
        emitter.MOV32_REG(source, addr);
    prepare_abi(ee, (uint64_t)&ee);
    prepare_abi_reg(ee, addr);
    free_int_reg(ee, addr);
    call_abi_func(ee, (uint64_t)ee_read8);

    REG_64 dest = alloc_reg(ee, instr.get_dest(), REG_TYPE::GPR, REG_STATE::WRITE);
    emitter.MOVSX8_TO_64(REG_64::RAX, dest);
}

void EE_JIT64::load_byte_unsigned(EmotionEngine& ee, IR::Instruction &instr)
{
    alloc_abi_regs(2);
    REG_64 source = alloc_reg(ee, instr.get_source(), REG_TYPE::GPR, REG_STATE::READ);
    REG_64 addr = lalloc_reg(ee, 0, REG_TYPE::INTSCRATCHPAD, REG_STATE::SCRATCHPAD);

    int64_t offset = instr.get_source2();

    if (offset)
        emitter.LEA32_M(source, addr, offset);
    else
        emitter.MOV32_REG(source, addr);
    prepare_abi(ee, (uint64_t)&ee);
    prepare_abi_reg(ee, addr);
    free_int_reg(ee, addr);
    call_abi_func(ee, (uint64_t)ee_read8);

    REG_64 dest = alloc_reg(ee, instr.get_dest(), REG_TYPE::GPR, REG_STATE::WRITE);
    emitter.MOVZX8_TO_64(REG_64::RAX, dest);
}

void EE_JIT64::load_doubleword(EmotionEngine& ee, IR::Instruction &instr)
{
    alloc_abi_regs(2);
    REG_64 source = alloc_reg(ee, instr.get_source(), REG_TYPE::GPR, REG_STATE::READ);
    REG_64 addr = lalloc_reg(ee, 0, REG_TYPE::INTSCRATCHPAD, REG_STATE::SCRATCHPAD);

    int64_t offset = instr.get_source2();

    if (offset)
        emitter.LEA32_M(source, addr, offset);
    else
        emitter.MOV32_REG(source, addr);
    prepare_abi(ee, (uint64_t)&ee);
    prepare_abi_reg(ee, addr);
    free_int_reg(ee, addr);
    call_abi_func(ee, (uint64_t)ee_read64);

    REG_64 dest = alloc_reg(ee, instr.get_dest(), REG_TYPE::GPR, REG_STATE::WRITE);
    emitter.MOV64_MR(REG_64::RAX, dest);
}

void EE_JIT64::load_halfword(EmotionEngine& ee, IR::Instruction &instr)
{
    alloc_abi_regs(2);
    REG_64 source = alloc_reg(ee, instr.get_source(), REG_TYPE::GPR, REG_STATE::READ);
    REG_64 addr = lalloc_reg(ee, 0, REG_TYPE::INTSCRATCHPAD, REG_STATE::SCRATCHPAD);

    int64_t offset = instr.get_source2();

    if (offset)
        emitter.LEA32_M(source, addr, offset);
    else
        emitter.MOV32_REG(source, addr);
    prepare_abi(ee, (uint64_t)&ee);
    prepare_abi_reg(ee, addr);
    free_int_reg(ee, addr);
    call_abi_func(ee, (uint64_t)ee_read16);

    REG_64 dest = alloc_reg(ee, instr.get_dest(), REG_TYPE::GPR, REG_STATE::WRITE);
    emitter.MOVSX16_TO_64(REG_64::RAX, dest);
}

void EE_JIT64::load_halfword_unsigned(EmotionEngine& ee, IR::Instruction &instr)
{
    alloc_abi_regs(2);
    REG_64 source = alloc_reg(ee, instr.get_source(), REG_TYPE::GPR, REG_STATE::READ);
    REG_64 addr = lalloc_reg(ee, 0, REG_TYPE::INTSCRATCHPAD, REG_STATE::SCRATCHPAD);

    int64_t offset = instr.get_source2();

    if (offset)
        emitter.LEA32_M(source, addr, offset);
    else
        emitter.MOV32_REG(source, addr);
    prepare_abi(ee, (uint64_t)&ee);
    prepare_abi_reg(ee, addr);
    free_int_reg(ee, addr);
    call_abi_func(ee, (uint64_t)ee_read16);

    REG_64 dest = alloc_reg(ee, instr.get_dest(), REG_TYPE::GPR, REG_STATE::WRITE);
    emitter.MOVZX16_TO_64(REG_64::RAX, dest);
}

void EE_JIT64::load_const(EmotionEngine& ee, IR::Instruction &instr)
{
    REG_64 dest = alloc_reg(ee, instr.get_dest(), REG_TYPE::GPR, REG_STATE::WRITE);
    int64_t imm = instr.get_source();

    // dest = immediate
    emitter.MOV64_OI(imm, dest);
}

void EE_JIT64::load_word(EmotionEngine& ee, IR::Instruction &instr)
{
    alloc_abi_regs(2);
    REG_64 source = alloc_reg(ee, instr.get_source(), REG_TYPE::GPR, REG_STATE::READ);
    REG_64 addr = lalloc_reg(ee, 0, REG_TYPE::INTSCRATCHPAD, REG_STATE::SCRATCHPAD);

    int64_t offset = instr.get_source2();

    if (offset)
        emitter.LEA32_M(source, addr, offset);
    else
        emitter.MOV32_REG(source, addr);
    prepare_abi(ee, (uint64_t)&ee);
    prepare_abi_reg(ee, addr);
    free_int_reg(ee, addr);
    call_abi_func(ee, (uint64_t)ee_read32);

    REG_64 dest = alloc_reg(ee, instr.get_dest(), REG_TYPE::GPR, REG_STATE::WRITE);
    emitter.MOVSX32_TO_64(REG_64::RAX, dest);
}

void EE_JIT64::load_word_unsigned(EmotionEngine& ee, IR::Instruction &instr)
{
    alloc_abi_regs(2);
    REG_64 source = alloc_reg(ee, instr.get_source(), REG_TYPE::GPR, REG_STATE::READ);
    REG_64 addr = lalloc_reg(ee, 0, REG_TYPE::INTSCRATCHPAD, REG_STATE::SCRATCHPAD);

    int64_t offset = instr.get_source2();

    if (offset)
        emitter.LEA32_M(source, addr, offset);
    else
        emitter.MOV32_REG(source, addr);
    prepare_abi(ee, (uint64_t)&ee);
    prepare_abi_reg(ee, addr);
    free_int_reg(ee, addr);
    call_abi_func(ee, (uint64_t)ee_read32);

    REG_64 dest = alloc_reg(ee, instr.get_dest(), REG_TYPE::GPR, REG_STATE::WRITE);
    emitter.MOV32_REG(REG_64::RAX, dest);
}

void EE_JIT64::move_conditional_on_not_zero(EmotionEngine& ee, IR::Instruction& instr)
{
    REG_64 source = alloc_reg(ee, instr.get_source(), REG_TYPE::GPR, REG_STATE::READ);
    REG_64 source2 = alloc_reg(ee, instr.get_source2(), REG_TYPE::GPR, REG_STATE::READ);
    REG_64 dest = alloc_reg(ee, instr.get_dest(), REG_TYPE::GPR, REG_STATE::READ_WRITE);

    emitter.TEST64_REG(source, source);
    emitter.CMOVCC64_REG(ConditionCode::NZ, source2, dest);
}

void EE_JIT64::move_conditional_on_zero(EmotionEngine& ee, IR::Instruction& instr)
{
    REG_64 source = alloc_reg(ee, instr.get_source(), REG_TYPE::GPR, REG_STATE::READ);
    REG_64 source2 = alloc_reg(ee, instr.get_source2(), REG_TYPE::GPR, REG_STATE::READ);
    REG_64 dest = alloc_reg(ee, instr.get_dest(), REG_TYPE::GPR, REG_STATE::READ_WRITE);

    emitter.TEST64_REG(source, source);
    emitter.CMOVCC64_REG(ConditionCode::Z, source2, dest);
}

void EE_JIT64::move_doubleword_reg(EmotionEngine& ee, IR::Instruction& instr)
{
    REG_64 source = alloc_reg(ee, instr.get_source(), REG_TYPE::GPR, REG_STATE::READ);
    REG_64 dest = alloc_reg(ee, instr.get_dest(), REG_TYPE::GPR, REG_STATE::WRITE);

    emitter.MOV64_MR(source, dest);
}

void EE_JIT64::move_to_lo_hi(EmotionEngine& ee, IR::Instruction &instr)
{
    REG_64 source = alloc_reg(ee, instr.get_source(), REG_TYPE::GPR, REG_STATE::READ);
    REG_64 source2 = alloc_reg(ee, instr.get_source2(), REG_TYPE::GPR, REG_STATE::READ);
    REG_64 LO = alloc_reg(ee, (int)EE_SpecialReg::LO, REG_TYPE::GPR, REG_STATE::WRITE);
    REG_64 HI = alloc_reg(ee, (int)EE_SpecialReg::HI, REG_TYPE::GPR, REG_STATE::WRITE);
    move_to_lo_hi(ee, source, source2, LO, HI);
}

void EE_JIT64::move_to_lo_hi(EmotionEngine& ee, REG_64 loSource, REG_64 hiSource, REG_64 loDest, REG_64 hiDest)
{
    emitter.MOV32_REG(loSource, loDest);
    emitter.MOV32_REG(hiSource, hiDest);
}

void EE_JIT64::move_to_lo_hi_imm(EmotionEngine& ee, int64_t loValue, int64_t hiValue)
{
    REG_64 LO = alloc_reg(ee, (int)EE_SpecialReg::LO, REG_TYPE::GPR, REG_STATE::WRITE);
    REG_64 HI = alloc_reg(ee, (int)EE_SpecialReg::HI, REG_TYPE::GPR, REG_STATE::WRITE);
    emitter.MOV32_REG_IMM(loValue, LO);
    emitter.MOV32_REG_IMM(hiValue, HI);
}

void EE_JIT64::move_word_reg(EmotionEngine& ee, IR::Instruction& instr)
{
    REG_64 source = alloc_reg(ee, instr.get_source(), REG_TYPE::GPR, REG_STATE::READ);
    REG_64 dest = alloc_reg(ee, instr.get_dest(), REG_TYPE::GPR, REG_STATE::WRITE);

    emitter.MOV32_REG(source, dest);
}

void EE_JIT64::multiply_unsigned_word(EmotionEngine& ee, IR::Instruction& instr)
{
    REG_64 RDX = lalloc_reg(ee, 0, REG_TYPE::INTSCRATCHPAD, REG_STATE::SCRATCHPAD, REG_64::RDX);
    REG_64 source = alloc_reg(ee, instr.get_source(), REG_TYPE::GPR, REG_STATE::READ);
    REG_64 source2 = alloc_reg(ee, instr.get_source2(), REG_TYPE::GPR, REG_STATE::READ);
    REG_64 LO = alloc_reg(ee, (int)EE_SpecialReg::LO, REG_TYPE::GPR, REG_STATE::WRITE);
    REG_64 HI = alloc_reg(ee, (int)EE_SpecialReg::HI, REG_TYPE::GPR, REG_STATE::WRITE);

    emitter.MOV32_REG(source, REG_64::RAX);
    emitter.MUL32(source2);
    move_to_lo_hi(ee, REG_64::RAX, RDX, LO, HI);

    if (instr.get_dest())
    {
        REG_64 dest = alloc_reg(ee, instr.get_dest(), REG_TYPE::GPR, REG_STATE::WRITE);
        emitter.MOV32_REG(LO, dest);
    }

    free_int_reg(ee, RDX);
}

void EE_JIT64::multiply_word(EmotionEngine& ee, IR::Instruction& instr)
{
    REG_64 RDX = lalloc_reg(ee, 0, REG_TYPE::INTSCRATCHPAD, REG_STATE::SCRATCHPAD, REG_64::RDX);
    REG_64 source = alloc_reg(ee, instr.get_source(), REG_TYPE::GPR, REG_STATE::READ);
    REG_64 source2 = alloc_reg(ee, instr.get_source2(), REG_TYPE::GPR, REG_STATE::READ);
    REG_64 LO = alloc_reg(ee, (int)EE_SpecialReg::LO, REG_TYPE::GPR, REG_STATE::WRITE);
    REG_64 HI = alloc_reg(ee, (int)EE_SpecialReg::HI, REG_TYPE::GPR, REG_STATE::WRITE);

    emitter.MOV32_REG(source, REG_64::RAX);
    emitter.IMUL32(source2);
    move_to_lo_hi(ee, REG_64::RAX, RDX, LO, HI);

    if (instr.get_dest())
    {
        REG_64 dest = alloc_reg(ee, instr.get_dest(), REG_TYPE::GPR, REG_STATE::WRITE);
        emitter.MOV32_REG(LO, dest);
    }

    free_int_reg(ee, RDX);
}

void EE_JIT64::negate_doubleword_reg(EmotionEngine& ee, IR::Instruction &instr)
{
    REG_64 source = alloc_reg(ee, instr.get_source(), REG_TYPE::GPR, REG_STATE::READ);
    REG_64 dest = alloc_reg(ee, instr.get_dest(), REG_TYPE::GPR, REG_STATE::WRITE);

    if (source != dest)
        emitter.MOV64_MR(source, dest);
    emitter.NEG64(dest);
}

void EE_JIT64::negate_word_reg(EmotionEngine& ee, IR::Instruction &instr)
{
    REG_64 source = alloc_reg(ee, instr.get_source(), REG_TYPE::GPR, REG_STATE::READ);
    REG_64 dest = alloc_reg(ee, instr.get_dest(), REG_TYPE::GPR, REG_STATE::WRITE);

    if (source != dest)
        emitter.MOV32_REG(source, dest);
    emitter.NEG32(dest);
}

void EE_JIT64::nor_reg(EmotionEngine& ee, IR::Instruction &instr)
{
    REG_64 source = alloc_reg(ee, instr.get_source(), REG_TYPE::GPR, REG_STATE::READ);
    REG_64 source2 = alloc_reg(ee, instr.get_source2(), REG_TYPE::GPR, REG_STATE::READ);
    REG_64 dest = alloc_reg(ee, instr.get_dest(), REG_TYPE::GPR, REG_STATE::WRITE);

    if (dest == source)
    {
        emitter.OR64_REG(source2, dest);
    }
    else if (dest == source2)
    {
        emitter.OR64_REG(source, dest);
    }
    else
    {
        emitter.MOV64_MR(source, dest);
        emitter.OR64_REG(source2, dest);
    }

    emitter.NOT64(dest);
}

void EE_JIT64::or_imm(EmotionEngine& ee, IR::Instruction &instr)
{
    REG_64 source = alloc_reg(ee, instr.get_source(), REG_TYPE::GPR, REG_STATE::READ);
    REG_64 dest = alloc_reg(ee, instr.get_dest(), REG_TYPE::GPR, REG_STATE::WRITE);
    uint16_t imm = instr.get_source2();

    if (source != dest)
        emitter.MOV64_MR(source, dest);
    emitter.OR16_REG_IMM(imm, dest);
}

void EE_JIT64::or_reg(EmotionEngine& ee, IR::Instruction &instr)
{
    REG_64 source = alloc_reg(ee, instr.get_source(), REG_TYPE::GPR, REG_STATE::READ);
    REG_64 source2 = alloc_reg(ee, instr.get_source2(), REG_TYPE::GPR, REG_STATE::READ);
    REG_64 dest = alloc_reg(ee, instr.get_dest(), REG_TYPE::GPR, REG_STATE::WRITE);

    if (dest == source)
    {
        emitter.OR64_REG(source2, dest);
    }
    else if (dest == source2)
    {
        emitter.OR64_REG(source, dest);
    }
    else
    {
        emitter.MOV64_MR(source, dest);
        emitter.OR64_REG(source2, dest);
    }
}

void EE_JIT64::set_on_less_than(EmotionEngine& ee, IR::Instruction& instr)
{
    REG_64 source = alloc_reg(ee, instr.get_source(), REG_TYPE::GPR, REG_STATE::READ);
    REG_64 source2 = alloc_reg(ee, instr.get_source2(), REG_TYPE::GPR, REG_STATE::READ);
    REG_64 dest = alloc_reg(ee, instr.get_dest(), REG_TYPE::GPR, REG_STATE::WRITE);

    emitter.CMP64_REG(source2, source);
    emitter.SETCC_REG(ConditionCode::L, dest);
    emitter.MOVZX8_TO_64(dest, dest);
}

void EE_JIT64::set_on_less_than_unsigned(EmotionEngine& ee, IR::Instruction& instr)
{
    REG_64 source = alloc_reg(ee, instr.get_source(), REG_TYPE::GPR, REG_STATE::READ);
    REG_64 source2 = alloc_reg(ee, instr.get_source2(), REG_TYPE::GPR, REG_STATE::READ);
    REG_64 dest = alloc_reg(ee, instr.get_dest(), REG_TYPE::GPR, REG_STATE::WRITE);

    emitter.CMP64_REG(source2, source);
    emitter.SETCC_REG(ConditionCode::B, dest);
    emitter.MOVZX8_TO_64(dest, dest);
}

void EE_JIT64::set_on_less_than_immediate(EmotionEngine& ee, IR::Instruction& instr)
{
    int64_t imm = instr.get_source2();
    REG_64 source = alloc_reg(ee, instr.get_source(), REG_TYPE::GPR, REG_STATE::READ);
    REG_64 dest = alloc_reg(ee, instr.get_dest(), REG_TYPE::GPR, REG_STATE::WRITE);

    emitter.CMP64_IMM(imm, source);
    emitter.SETCC_REG(ConditionCode::L, dest);
    emitter.MOVZX8_TO_64(dest, dest);
}

void EE_JIT64::set_on_less_than_immediate_unsigned(EmotionEngine& ee, IR::Instruction& instr)
{
    int64_t imm = instr.get_source2();
    REG_64 source = alloc_reg(ee, instr.get_source(), REG_TYPE::GPR, REG_STATE::READ);
    REG_64 dest = alloc_reg(ee, instr.get_dest(), REG_TYPE::GPR, REG_STATE::WRITE);

    emitter.CMP64_IMM(imm, source);
    emitter.SETCC_REG(ConditionCode::B, dest);
    emitter.MOVZX8_TO_64(dest, dest);
}

void EE_JIT64::shift_left_logical(EmotionEngine& ee, IR::Instruction& instr)
{
    REG_64 source = alloc_reg(ee, instr.get_source(), REG_TYPE::GPR, REG_STATE::READ);
    REG_64 dest = alloc_reg(ee, instr.get_dest(), REG_TYPE::GPR, REG_STATE::WRITE);

    if (source != dest)
        emitter.MOV32_REG(source, dest);
    emitter.SHL32_REG_IMM(instr.get_source2(), dest);
    emitter.MOVSX32_TO_64(dest, dest);
}

void EE_JIT64::shift_left_logical_variable(EmotionEngine& ee, IR::Instruction& instr)
{
    // Alloc variable into RCX
    REG_64 RCX = lalloc_reg(ee, 0, REG_TYPE::INTSCRATCHPAD, REG_STATE::SCRATCHPAD, REG_64::RCX);
    REG_64 variable = alloc_reg(ee, instr.get_source2(), REG_TYPE::GPR, REG_STATE::READ);
    emitter.MOV8_REG(variable, RCX);
    emitter.AND8_REG_IMM(0x1F, RCX);

    REG_64 source = alloc_reg(ee, instr.get_source(), REG_TYPE::GPR, REG_STATE::READ);
    REG_64 dest = alloc_reg(ee, instr.get_dest(), REG_TYPE::GPR, REG_STATE::WRITE);

    if (source != dest)
        emitter.MOV32_REG(source, dest);

    emitter.SHL32_CL(dest);
    emitter.MOVSX32_TO_64(dest, dest);

    free_int_reg(ee, RCX);
}

void EE_JIT64::shift_right_arithmetic(EmotionEngine& ee, IR::Instruction& instr)
{
    REG_64 source = alloc_reg(ee, instr.get_source(), REG_TYPE::GPR, REG_STATE::READ);
    REG_64 dest = alloc_reg(ee, instr.get_dest(), REG_TYPE::GPR, REG_STATE::WRITE);

    if (source != dest)
        emitter.MOV32_REG(source, dest);
    emitter.SAR32_REG_IMM(instr.get_source2(), dest);
    emitter.MOVSX32_TO_64(dest, dest);
}

void EE_JIT64::shift_right_arithmetic_variable(EmotionEngine& ee, IR::Instruction& instr)
{
    // Alloc variable into RCX
    REG_64 RCX = lalloc_reg(ee, 0, REG_TYPE::INTSCRATCHPAD, REG_STATE::SCRATCHPAD, REG_64::RCX);
    REG_64 variable = alloc_reg(ee, instr.get_source2(), REG_TYPE::GPR, REG_STATE::READ);
    emitter.MOV8_REG(variable, RCX);
    emitter.AND8_REG_IMM(0x1F, RCX);

    REG_64 source = alloc_reg(ee, instr.get_source(), REG_TYPE::GPR, REG_STATE::READ);
    REG_64 dest = alloc_reg(ee, instr.get_dest(), REG_TYPE::GPR, REG_STATE::WRITE);

    if (source != dest)
        emitter.MOV32_REG(source, dest);

    emitter.SAR32_CL(dest);
    emitter.MOVSX32_TO_64(dest, dest);

    free_int_reg(ee, RCX);
}

void EE_JIT64::shift_right_logical(EmotionEngine& ee, IR::Instruction& instr)
{
    REG_64 source = alloc_reg(ee, instr.get_source(), REG_TYPE::GPR, REG_STATE::READ);
    REG_64 dest = alloc_reg(ee, instr.get_dest(), REG_TYPE::GPR, REG_STATE::WRITE);

    if (source != dest)
        emitter.MOV32_REG(source, dest);
    emitter.SHR32_REG_IMM(instr.get_source2(), dest);
    emitter.MOVSX32_TO_64(dest, dest);
}

void EE_JIT64::shift_right_logical_variable(EmotionEngine& ee, IR::Instruction& instr)
{
    // Alloc variable into RCX
    REG_64 RCX = lalloc_reg(ee, 0, REG_TYPE::INTSCRATCHPAD, REG_STATE::SCRATCHPAD, REG_64::RCX);
    REG_64 variable = alloc_reg(ee, instr.get_source2(), REG_TYPE::GPR, REG_STATE::READ);
    emitter.MOV8_REG(variable, RCX);
    emitter.AND8_REG_IMM(0x1F, RCX);

    REG_64 source = alloc_reg(ee, instr.get_source(), REG_TYPE::GPR, REG_STATE::READ);
    REG_64 dest = alloc_reg(ee, instr.get_dest(), REG_TYPE::GPR, REG_STATE::WRITE);

    if (source != dest)
        emitter.MOV32_REG(source, dest);

    emitter.SHR32_CL(dest);
    emitter.MOVSX32_TO_64(dest, dest);

    free_int_reg(ee, RCX);
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

void EE_JIT64::store_byte(EmotionEngine& ee, IR::Instruction& instr)
{
    alloc_abi_regs(3);
    REG_64 source = alloc_reg(ee, instr.get_source(), REG_TYPE::GPR, REG_STATE::READ);
    REG_64 dest = alloc_reg(ee, instr.get_dest(), REG_TYPE::GPR, REG_STATE::READ);
    REG_64 addr = lalloc_reg(ee, 0, REG_TYPE::INTSCRATCHPAD, REG_STATE::SCRATCHPAD);
    int64_t offset = instr.get_source2();

    if (offset)
        emitter.LEA32_M(dest, addr, offset);
    else
        emitter.MOV32_REG(dest, addr);
    prepare_abi(ee, (uint64_t)&ee);
    prepare_abi_reg(ee, addr);
    prepare_abi_reg(ee, source);
    call_abi_func(ee, (uint64_t)ee_write8);
    free_int_reg(ee, addr);
}

void EE_JIT64::store_doubleword(EmotionEngine& ee, IR::Instruction& instr)
{
    alloc_abi_regs(3);
    REG_64 source = alloc_reg(ee, instr.get_source(), REG_TYPE::GPR, REG_STATE::READ);
    REG_64 dest = alloc_reg(ee, instr.get_dest(), REG_TYPE::GPR, REG_STATE::READ);
    REG_64 addr = lalloc_reg(ee, 0, REG_TYPE::INTSCRATCHPAD, REG_STATE::SCRATCHPAD);
    int64_t offset = instr.get_source2();

    if (offset)
        emitter.LEA32_M(dest, addr, offset);
    else
        emitter.MOV32_REG(dest, addr);
    prepare_abi(ee, (uint64_t)&ee);
    prepare_abi_reg(ee, addr);
    prepare_abi_reg(ee, source);
    call_abi_func(ee, (uint64_t)ee_write64);
    free_int_reg(ee, addr);
}

void EE_JIT64::store_halfword(EmotionEngine& ee, IR::Instruction& instr)
{
    alloc_abi_regs(3);
    REG_64 source = alloc_reg(ee, instr.get_source(), REG_TYPE::GPR, REG_STATE::READ);
    REG_64 dest = alloc_reg(ee, instr.get_dest(), REG_TYPE::GPR, REG_STATE::READ);
    REG_64 addr = lalloc_reg(ee, 0, REG_TYPE::INTSCRATCHPAD, REG_STATE::SCRATCHPAD);
    int64_t offset = instr.get_source2();

    if (offset)
        emitter.LEA32_M(dest, addr, offset);
    else
        emitter.MOV32_REG(dest, addr);
    prepare_abi(ee, (uint64_t)&ee);
    prepare_abi_reg(ee, addr);
    prepare_abi_reg(ee, source);
    call_abi_func(ee, (uint64_t)ee_write16);
    free_int_reg(ee, addr);
}

void EE_JIT64::store_word(EmotionEngine& ee, IR::Instruction& instr)
{
    alloc_abi_regs(3);
    REG_64 source = alloc_reg(ee, instr.get_source(), REG_TYPE::GPR, REG_STATE::READ);
    REG_64 dest = alloc_reg(ee, instr.get_dest(), REG_TYPE::GPR, REG_STATE::READ);
    REG_64 addr = lalloc_reg(ee, 0, REG_TYPE::INTSCRATCHPAD, REG_STATE::SCRATCHPAD);
    int64_t offset = instr.get_source2();

    if (offset)
        emitter.LEA32_M(dest, addr, offset);
    else
        emitter.MOV32_REG(dest, addr);
    prepare_abi(ee, (uint64_t)&ee);
    prepare_abi_reg(ee, addr);
    prepare_abi_reg(ee, source);
    call_abi_func(ee, (uint64_t)ee_write32);
    free_int_reg(ee, addr);
}

void EE_JIT64::sub_doubleword_reg(EmotionEngine& ee, IR::Instruction &instr)
{
    REG_64 source = alloc_reg(ee, instr.get_source(), REG_TYPE::GPR, REG_STATE::READ);
    REG_64 source2 = alloc_reg(ee, instr.get_source2(), REG_TYPE::GPR, REG_STATE::READ);
    REG_64 dest = alloc_reg(ee, instr.get_dest(), REG_TYPE::GPR, REG_STATE::WRITE);

    if (dest == source)
    {
        emitter.SUB64_REG(source2, dest);
    }
    else if (dest == source2)
    {
        emitter.SUB64_REG(source, dest);
        emitter.NEG64(dest);
    }
    else
    {
        emitter.MOV64_MR(source, dest);
        emitter.SUB64_REG(source2, dest);
    }
}

void EE_JIT64::sub_word_reg(EmotionEngine& ee, IR::Instruction &instr)
{
    REG_64 source = alloc_reg(ee, instr.get_source(), REG_TYPE::GPR, REG_STATE::READ);
    REG_64 source2 = alloc_reg(ee, instr.get_source2(), REG_TYPE::GPR, REG_STATE::READ);
    REG_64 dest = alloc_reg(ee, instr.get_dest(), REG_TYPE::GPR, REG_STATE::WRITE);

    if (dest == source)
    {
        emitter.SUB32_REG(source2, dest);
        emitter.MOVSX32_TO_64(dest, dest);
    }
    else if (dest == source2)
    {
        emitter.SUB32_REG(source, dest);
        emitter.NEG32(dest);
        emitter.MOVSX32_TO_64(dest, dest);
    }
    else
    {
        emitter.MOV32_REG(source, dest);
        emitter.SUB32_REG(source2, dest);
        emitter.MOVSX32_TO_64(dest, dest);
    }
}

void EE_JIT64::ee_syscall_exception(EmotionEngine& ee)
{
    ee.syscall_exception();
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

    call_abi_func(ee, (uint64_t)ee_syscall_exception);

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
    // Alloc scratchpad registers
    REG_64 R15 = lalloc_reg(ee, 0, REG_TYPE::INTSCRATCHPAD, REG_STATE::SCRATCHPAD);

    prepare_abi(ee, (uint64_t)&ee);
    call_abi_func(ee, (uint64_t)ee_vu0_wait);
    emitter.TEST8_REG(REG_64::AL, REG_64::AL);

    uint8_t* offset_addr = emitter.JCC_NEAR_DEFERRED(ConditionCode::Z);

    // If ee.vu0_wait(), set PC to the current operation's address and abort
    emitter.load_addr((uint64_t)&ee.PC, REG_64::RAX);
    emitter.MOV32_REG_IMM(instr.get_return_addr(), R15);
    emitter.MOV32_TO_MEM(R15, REG_64::RAX);

    // Set wait for VU0 flag
    emitter.load_addr((uint64_t)&ee.wait_for_VU0, REG_64::RAX);
    emitter.MOV8_IMM_MEM(true, REG_64::RAX);

    // Set cycle count to number of cycles executed
    emitter.load_addr((uint64_t)&cycle_count, REG_64::RAX);
    emitter.MOV16_IMM_MEM(instr.get_cycle_count(), REG_64::RAX);

    cleanup_recompiler(ee, false);

    // Otherwise continue execution of block otherwise
    emitter.set_jump_dest(offset_addr);
    prepare_abi(ee, (uint64_t)ee.vu0);
    prepare_abi(ee, instr.get_source());
    call_abi_func(ee, (uint64_t)vu0_start_program);

    free_int_reg(ee, R15);
}

void EE_JIT64::vcall_msr(EmotionEngine& ee, IR::Instruction& instr)
{
    // Alloc scratchpad registers
    REG_64 R15 = lalloc_reg(ee, 0, REG_TYPE::INTSCRATCHPAD, REG_STATE::SCRATCHPAD);

    prepare_abi(ee, (uint64_t)&ee);
    call_abi_func(ee, (uint64_t)ee_vu0_wait);
    emitter.TEST8_REG(REG_64::AL, REG_64::AL);

    uint8_t* offset_addr = emitter.JCC_NEAR_DEFERRED(ConditionCode::Z);

    // If ee.vu0_wait(), set PC to the current operation's address and abort
    emitter.load_addr((uint64_t)&ee.PC, REG_64::RAX);
    emitter.MOV32_REG_IMM(instr.get_return_addr(), R15);
    emitter.MOV32_TO_MEM(R15, REG_64::RAX);

    // Set wait for VU0 flag
    emitter.load_addr((uint64_t)&ee.wait_for_VU0, REG_64::RAX);
    emitter.MOV8_IMM_MEM(true, REG_64::RAX);

    // Set cycle count to number of cycles executed
    emitter.load_addr((uint64_t)&cycle_count, REG_64::RAX);
    emitter.MOV16_IMM_MEM(instr.get_cycle_count(), REG_64::RAX);

    cleanup_recompiler(ee, false);

    // Otherwise continue execution of block otherwise
    emitter.set_jump_dest(offset_addr);

    prepare_abi(ee, (uint64_t)ee.vu0);
    call_abi_func(ee, (uint64_t)vu0_read_CMSAR0_shl3);

    prepare_abi_reg(ee, REG_64::RAX);
    call_abi_func(ee, (uint64_t)vu0_start_program);

    // Free scratchpad registers
    free_int_reg(ee, R15);
}

void EE_JIT64::xor_imm(EmotionEngine& ee, IR::Instruction &instr)
{
    REG_64 source = alloc_reg(ee, instr.get_source(), REG_TYPE::GPR, REG_STATE::READ);
    REG_64 dest = alloc_reg(ee, instr.get_dest(), REG_TYPE::GPR, REG_STATE::WRITE);
    uint16_t imm = instr.get_source2();

    if (source != dest)
        emitter.MOV64_MR(source, dest);
    emitter.XOR16_REG_IMM(imm, dest);
}

void EE_JIT64::xor_reg(EmotionEngine& ee, IR::Instruction &instr)
{
    REG_64 source = alloc_reg(ee, instr.get_source(), REG_TYPE::GPR, REG_STATE::READ);
    REG_64 source2 = alloc_reg(ee, instr.get_source2(), REG_TYPE::GPR, REG_STATE::READ);
    REG_64 dest = alloc_reg(ee, instr.get_dest(), REG_TYPE::GPR, REG_STATE::WRITE);

    if (dest == source)
    {
        emitter.XOR64_REG(source2, dest);
    }
    else if (dest == source2)
    {
        emitter.XOR64_REG(source, dest);
    }
    else
    {
        emitter.MOV64_MR(source, dest);
        emitter.XOR64_REG(source2, dest);
    }
}