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
    prepare_abi((uint64_t)ee.cp0);
    call_abi_func((uint64_t)cop0_get_condition);

    // Alloc scratchpad register
    REG_64 R15 = lalloc_int_reg(ee, 0, REG_TYPE::INTSCRATCHPAD, REG_STATE::SCRATCHPAD);

    // Conditionally move the success or failure destination into ee.PC
    emitter.MOV8_REG_IMM(instr.get_field(), R15);
    emitter.CMP8_REG(REG_64::RAX, R15);
    emitter.MOV32_REG_IMM(instr.get_jump_fail_dest(), REG_64::RAX);
    emitter.MOV32_REG_IMM(instr.get_jump_dest(), R15);
    emitter.CMOVCC32_REG(ConditionCode::E, R15, REG_64::RAX);
    emitter.MOV32_TO_MEM(REG_64::RAX, REG_64::R15, offsetof(EmotionEngine, PC));

    if (instr.get_is_likely())
    {
        likely_branch = true;

        // Use the result of the FLAGS register from the last compare to set branch_on,
        // this boolean is used in handle_likely_branch
        emitter.SETCC_MEM(ConditionCode::E, REG_64::R15, offsetof(EmotionEngine, branch_on));
    }
    free_int_reg(ee, R15);
}

void EE_JIT64::branch_cop1(EmotionEngine& ee, IR::Instruction &instr)
{
    REG_64 R15 = lalloc_int_reg(ee, 0, REG_TYPE::INTSCRATCHPAD, REG_STATE::SCRATCHPAD);

    // Conditionally move the success or failure destination into ee.PC
    emitter.load_addr((uint64_t)&ee.fpu->control.condition, REG_64::RAX);
    emitter.MOV8_FROM_MEM(REG_64::RAX, REG_64::RAX);
    emitter.MOV8_REG_IMM(instr.get_field(), R15);
    emitter.CMP8_REG(REG_64::RAX, R15);
    emitter.MOV32_REG_IMM(instr.get_jump_fail_dest(), REG_64::RAX);
    emitter.MOV32_REG_IMM(instr.get_jump_dest(), R15);
    emitter.CMOVCC32_REG(ConditionCode::E, R15, REG_64::RAX);
    emitter.MOV32_TO_MEM(REG_64::RAX, REG_64::R15, offsetof(EmotionEngine, PC));

    if (instr.get_is_likely())
    {
        likely_branch = true;

        // Use the result of the FLAGS register from the last compare to set branch_on,
        // this boolean is used in handle_likely_branch
        emitter.SETCC_MEM(ConditionCode::E, REG_64::R15, offsetof(EmotionEngine, branch_on));
    }
    free_int_reg(ee, R15);
}

void EE_JIT64::branch_cop2(EmotionEngine& ee, IR::Instruction &instr)
{
    REG_64 R15 = lalloc_int_reg(ee, 0, REG_TYPE::INTSCRATCHPAD, REG_STATE::SCRATCHPAD);

    // Conditionally move the success or failure destination into ee.PC
    emitter.load_addr((uint64_t)&ee.vu1->running, REG_64::RAX);
    emitter.MOV8_FROM_MEM(REG_64::RAX, REG_64::RAX);
    emitter.MOV8_REG_IMM(instr.get_field(), R15);
    emitter.CMP8_REG(REG_64::RAX, R15);
    emitter.MOV32_REG_IMM(instr.get_jump_fail_dest(), REG_64::RAX);
    emitter.MOV32_REG_IMM(instr.get_jump_dest(), R15);
    emitter.CMOVCC32_REG(ConditionCode::E, R15, REG_64::RAX);
    emitter.MOV32_TO_MEM(REG_64::RAX, REG_64::R15, offsetof(EmotionEngine, PC));

    if (instr.get_is_likely())
    {
        likely_branch = true;

        // Use the result of the FLAGS register from the last compare to set branch_on,
        // this boolean is used in handle_likely_branch
        emitter.SETCC_MEM(ConditionCode::E, REG_64::R15, offsetof(EmotionEngine, branch_on));
    }
    free_int_reg(ee, R15);
}

void EE_JIT64::branch_equal(EmotionEngine& ee, IR::Instruction &instr)
{
    // Alloc registers to compare
    REG_64 op1 = alloc_reg(ee, instr.get_source(), REG_TYPE::GPR, REG_STATE::READ);
    REG_64 op2 = alloc_reg(ee, instr.get_source2(), REG_TYPE::GPR, REG_STATE::READ);

    // Alloc scratchpad registers
    REG_64 R15 = lalloc_int_reg(ee, 0, REG_TYPE::INTSCRATCHPAD, REG_STATE::SCRATCHPAD);

    // Conditionally move the success or failure destination into ee.PC
    emitter.MOV32_REG_IMM(instr.get_jump_fail_dest(), REG_64::RAX);
    emitter.MOV32_REG_IMM(instr.get_jump_dest(), R15);
    emitter.CMP64_REG(op2, op1);
    emitter.CMOVCC32_REG(ConditionCode::E, R15, REG_64::RAX);
    emitter.MOV32_TO_MEM(REG_64::RAX, REG_64::R15, offsetof(EmotionEngine, PC));

    if (instr.get_is_likely())
    {
        likely_branch = true;

        // Use the result of the FLAGS register from the last compare to set branch_on,
        // this boolean is used in handle_likely_branch
        emitter.SETCC_MEM(ConditionCode::E, REG_64::R15, offsetof(EmotionEngine, branch_on));
    }

    // Free scratchpad registers
    free_int_reg(ee, R15);
}

void EE_JIT64::branch_equal_zero(EmotionEngine& ee, IR::Instruction &instr)
{
    // Alloc registers to compare
    REG_64 op1 = alloc_reg(ee, instr.get_source(), REG_TYPE::GPR, REG_STATE::READ);

    // Alloc scratchpad registers
    REG_64 R15 = lalloc_int_reg(ee, 0, REG_TYPE::INTSCRATCHPAD, REG_STATE::SCRATCHPAD);

    // Conditionally move the success or failure destination into ee.PC
    emitter.MOV32_REG_IMM(instr.get_jump_fail_dest(), REG_64::RAX);
    emitter.MOV32_REG_IMM(instr.get_jump_dest(), R15);
    emitter.TEST64_REG(op1, op1);
    emitter.CMOVCC32_REG(ConditionCode::Z, R15, REG_64::RAX);
    emitter.MOV32_TO_MEM(REG_64::RAX, REG_64::R15, offsetof(EmotionEngine, PC));

    if (instr.get_is_likely())
    {
        likely_branch = true;

        // Use the result of the FLAGS register from the last compare to set branch_on,
        // this boolean is used in handle_likely_branch
        emitter.SETCC_MEM(ConditionCode::Z, REG_64::R15, offsetof(EmotionEngine, branch_on));
    }

    // Free scratchpad registers
    free_int_reg(ee, R15);
}

void EE_JIT64::branch_greater_than_or_equal_zero(EmotionEngine& ee, IR::Instruction &instr)
{
    // Alloc registers to compare
    REG_64 op1 = alloc_reg(ee, instr.get_source(), REG_TYPE::GPR, REG_STATE::READ);

    // Alloc scratchpad registers
    REG_64 R15 = lalloc_int_reg(ee, 0, REG_TYPE::INTSCRATCHPAD, REG_STATE::SCRATCHPAD);

    if (instr.get_is_link())
    {
        // Set the link register
        emitter.MOV32_IMM_MEM(instr.get_return_addr(), REG_64::R15, offsetof(EmotionEngine, gpr) + get_gpr_offset(EE_NormalReg::ra));
    }

    // Conditionally move the success or failure destination into ee.PC
    emitter.MOV32_REG_IMM(instr.get_jump_fail_dest(), REG_64::RAX);
    emitter.MOV32_REG_IMM(instr.get_jump_dest(), R15);
    emitter.TEST64_REG(op1, op1);
    emitter.CMOVCC32_REG(ConditionCode::NS, R15, REG_64::RAX);
    emitter.MOV32_TO_MEM(REG_64::RAX, REG_64::R15, offsetof(EmotionEngine, PC));

    if (instr.get_is_likely())
    {
        likely_branch = true;

        // Use the result of the FLAGS register from the last compare to set branch_on,
        // this boolean is used in handle_likely_branch
        emitter.SETCC_MEM(ConditionCode::NS, REG_64::R15, offsetof(EmotionEngine, branch_on));
    }

    // Free scratchpad registers
    free_int_reg(ee, R15);
}

void EE_JIT64::branch_greater_than_zero(EmotionEngine& ee, IR::Instruction &instr)
{
    // Alloc registers to compare
    REG_64 op1 = alloc_reg(ee, instr.get_source(), REG_TYPE::GPR, REG_STATE::READ);

    // Alloc scratchpad registers
    REG_64 R15 = lalloc_int_reg(ee, 0, REG_TYPE::INTSCRATCHPAD, REG_STATE::SCRATCHPAD);

    // Conditionally move the success or failure destination into ee.PC
    emitter.TEST64_REG(op1, op1);
    emitter.MOV32_REG_IMM(instr.get_jump_fail_dest(), REG_64::RAX);
    emitter.MOV32_REG_IMM(instr.get_jump_dest(), R15);
    emitter.CMOVCC32_REG(ConditionCode::G, R15, REG_64::RAX);
    emitter.MOV32_TO_MEM(REG_64::RAX, REG_64::R15, offsetof(EmotionEngine, PC));

    if (instr.get_is_likely())
    {
        likely_branch = true;

        // Use the result of the FLAGS register from the last compare to set branch_on,
        // this boolean is used in handle_likely_branch
        emitter.SETCC_MEM(ConditionCode::G, REG_64::R15, offsetof(EmotionEngine, branch_on));
    }

    // Free scratchpad registers
    free_int_reg(ee, R15);
}

void EE_JIT64::branch_less_than_or_equal_zero(EmotionEngine& ee, IR::Instruction &instr)
{
    // Alloc registers to compare
    REG_64 op1 = alloc_reg(ee, instr.get_source(), REG_TYPE::GPR, REG_STATE::READ);

    // Alloc scratchpad registers
    REG_64 R15 = lalloc_int_reg(ee, 0, REG_TYPE::INTSCRATCHPAD, REG_STATE::SCRATCHPAD);

    // Conditionally move the success or failure destination into ee.PC
    emitter.TEST64_REG(op1, op1);
    emitter.MOV32_REG_IMM(instr.get_jump_fail_dest(), REG_64::RAX);
    emitter.MOV32_REG_IMM(instr.get_jump_dest(), R15);
    emitter.CMOVCC32_REG(ConditionCode::LE, R15, REG_64::RAX);
    emitter.MOV32_TO_MEM(REG_64::RAX, REG_64::R15, offsetof(EmotionEngine, PC));

    if (instr.get_is_likely())
    {
        likely_branch = true;

        // Use the result of the FLAGS register from the last compare to set branch_on,
        // this boolean is used in handle_likely_branch
        emitter.SETCC_MEM(ConditionCode::LE, REG_64::R15, offsetof(EmotionEngine, branch_on));
    }

    // Free scratchpad registers
    free_int_reg(ee, R15);
}

void EE_JIT64::branch_less_than_zero(EmotionEngine& ee, IR::Instruction &instr)
{
    // Alloc registers to compare
    REG_64 op1 = alloc_reg(ee, instr.get_source(), REG_TYPE::GPR, REG_STATE::READ);

    // Alloc scratchpad registers
    REG_64 R15 = lalloc_int_reg(ee, 0, REG_TYPE::INTSCRATCHPAD, REG_STATE::SCRATCHPAD);

    if (instr.get_is_link())
    {
        // Set the link register
        emitter.MOV32_IMM_MEM(instr.get_return_addr(), REG_64::R15, offsetof(EmotionEngine, gpr) + get_gpr_offset(EE_NormalReg::ra));
    }

    // Conditionally move the success or failure destination into ee.PC
    emitter.MOV32_REG_IMM(instr.get_jump_fail_dest(), REG_64::RAX);
    emitter.MOV32_REG_IMM(instr.get_jump_dest(), R15);
    emitter.TEST64_REG(op1, op1);
    emitter.CMOVCC32_REG(ConditionCode::S, R15, REG_64::RAX);
    emitter.MOV32_TO_MEM(REG_64::RAX, REG_64::R15, offsetof(EmotionEngine, PC));

    if (instr.get_is_likely())
    {
        likely_branch = true;

        // Use the result of the FLAGS register from the last compare to set branch_on,
        // this boolean is used in handle_likely_branch
        emitter.SETCC_MEM(ConditionCode::S, REG_64::R15, offsetof(EmotionEngine, branch_on));
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
    REG_64 R15 = lalloc_int_reg(ee, 0, REG_TYPE::INTSCRATCHPAD, REG_STATE::SCRATCHPAD);

    // Conditionally move the success or failure destination into ee.PC
    emitter.MOV32_REG_IMM(instr.get_jump_fail_dest(), REG_64::RAX);
    emitter.MOV32_REG_IMM(instr.get_jump_dest(), R15);
    emitter.CMP64_REG(op2, op1);
    emitter.CMOVCC32_REG(ConditionCode::NE, R15, REG_64::RAX);
    emitter.MOV32_TO_MEM(REG_64::RAX, REG_64::R15, offsetof(EmotionEngine, PC));

    if (instr.get_is_likely())
    {
        likely_branch = true;

        // Use the result of the FLAGS register from the last compare to set branch_on,
        // this boolean is used in handle_likely_branch
        emitter.SETCC_MEM(ConditionCode::NE, REG_64::R15, offsetof(EmotionEngine, branch_on));
    }

    // Free scratchpad registers
    free_int_reg(ee, R15);
}

void EE_JIT64::branch_not_equal_zero(EmotionEngine& ee, IR::Instruction &instr)
{
    // Alloc registers to compare
    REG_64 op1 = alloc_reg(ee, instr.get_source(), REG_TYPE::GPR, REG_STATE::READ);

    // Alloc scratchpad registers
    REG_64 R15 = lalloc_int_reg(ee, 0, REG_TYPE::INTSCRATCHPAD, REG_STATE::SCRATCHPAD);

    // Conditionally move the success or failure destination into ee.PC
    emitter.MOV32_REG_IMM(instr.get_jump_fail_dest(), REG_64::RAX);
    emitter.MOV32_REG_IMM(instr.get_jump_dest(), R15);
    emitter.TEST64_REG(op1, op1);
    emitter.CMOVCC32_REG(ConditionCode::NZ, R15, REG_64::RAX);
    emitter.MOV32_TO_MEM(REG_64::RAX, REG_64::R15, offsetof(EmotionEngine, PC));

    if (instr.get_is_likely())
    {
        likely_branch = true;

        // Use the result of the FLAGS register from the last compare to set branch_on,
        // this boolean is used in handle_likely_branch
        emitter.SETCC_MEM(ConditionCode::NZ, REG_64::R15, offsetof(EmotionEngine, branch_on));
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

void EE_JIT64::divide_unsigned_word(EmotionEngine& ee, IR::Instruction& instr, bool hi)
{
    // choose destination registers based on operation pipeline
    EE_SpecialReg lo_reg = hi ? EE_SpecialReg::LO1 : EE_SpecialReg::LO;
    EE_SpecialReg hi_reg = hi ? EE_SpecialReg::HI1 : EE_SpecialReg::HI;

    // idiv result is stored in RAX:RDX, so we allocate those registers first.
    REG_64 RDX = lalloc_int_reg(ee, 0, REG_TYPE::INTSCRATCHPAD, REG_STATE::SCRATCHPAD, REG_64::RDX);
    REG_64 dividend = alloc_reg(ee, instr.get_source(), REG_TYPE::GPR, REG_STATE::READ);
    REG_64 divisor = alloc_reg(ee, instr.get_source2(), REG_TYPE::GPR, REG_STATE::READ);
    REG_64 LO = alloc_reg(ee, (int)lo_reg, REG_TYPE::GPR, REG_STATE::WRITE);
    REG_64 HI = alloc_reg(ee, (int)hi_reg, REG_TYPE::GPR, REG_STATE::WRITE);

    emitter.TEST32_REG(divisor, divisor);
    uint8_t *label1 = emitter.JCC_NEAR_DEFERRED(ConditionCode::Z);
    emitter.MOV32_REG(dividend, REG_64::RAX);
    emitter.XOR32_REG(RDX, RDX);
    emitter.DIV32(divisor);
    emitter.MOVSX32_TO_64(REG_64::RAX, LO);
    emitter.MOVSX32_TO_64(RDX, HI);
    uint8_t *end = emitter.JMP_NEAR_DEFERRED();
    free_int_reg(ee, RDX);

    emitter.set_jump_dest(label1);
    emitter.MOVSX32_TO_64(dividend, HI);
    emitter.MOV64_OI(-1, LO);

    emitter.set_jump_dest(end);
}

void EE_JIT64::divide_word(EmotionEngine& ee, IR::Instruction &instr, bool hi)
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

    // choose destination registers based on operation pipeline
    EE_SpecialReg lo_reg = hi ? EE_SpecialReg::LO1 : EE_SpecialReg::LO;
    EE_SpecialReg hi_reg = hi ? EE_SpecialReg::HI1 : EE_SpecialReg::HI;

    // idiv result is stored in RAX:RDX, so we allocate those registers first.
    REG_64 RDX = lalloc_int_reg(ee, 0, REG_TYPE::INTSCRATCHPAD, REG_STATE::SCRATCHPAD, REG_64::RDX);
    REG_64 dividend = alloc_reg(ee, instr.get_source(), REG_TYPE::GPR, REG_STATE::READ);
    REG_64 divisor = alloc_reg(ee, instr.get_source2(), REG_TYPE::GPR, REG_STATE::READ);
    REG_64 LO = alloc_reg(ee, (int)lo_reg, REG_TYPE::GPR, REG_STATE::WRITE);
    REG_64 HI = alloc_reg(ee, (int)hi_reg, REG_TYPE::GPR, REG_STATE::WRITE);

    emitter.CMP32_IMM(0x80000000, dividend);
    uint8_t *label1_1 = emitter.JCC_NEAR_DEFERRED(ConditionCode::NE);
    emitter.CMP32_IMM(0xFFFFFFFF, divisor);
    uint8_t *label1_2 = emitter.JCC_NEAR_DEFERRED(ConditionCode::NE);
    emitter.MOV64_OI((int64_t)(int32_t)0x80000000, LO);
    emitter.MOV64_OI(0, HI);
    uint8_t *end_1 = emitter.JMP_NEAR_DEFERRED();

    emitter.set_jump_dest(label1_1);
    emitter.set_jump_dest(label1_2);
    emitter.TEST32_REG(divisor, divisor);
    uint8_t* label2 = emitter.JCC_NEAR_DEFERRED(ConditionCode::Z);
    emitter.MOV32_REG(dividend, REG_64::RAX);
    emitter.CDQ();
    emitter.IDIV32(divisor);
    emitter.MOVSX32_TO_64(REG_64::RAX, LO);
    emitter.MOVSX32_TO_64(RDX, HI);
    uint8_t *end_2 = emitter.JMP_NEAR_DEFERRED();

    emitter.set_jump_dest(label2);
    emitter.TEST32_REG(dividend, dividend);
    emitter.SETCC_REG(ConditionCode::L, REG_64::RAX);
    emitter.SHL8_REG_1(REG_64::RAX);
    emitter.DEC8(REG_64::RAX);
    emitter.MOVSX8_TO_64(REG_64::RAX, REG_64::RAX);
    emitter.MOVSX32_TO_64(dividend, dividend);
    emitter.MOVSX8_TO_64(REG_64::RAX, LO);
    emitter.MOVSX32_TO_64(dividend, HI);

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
    REG_64 RCX = lalloc_int_reg(ee, 0, REG_TYPE::GPR, REG_STATE::SCRATCHPAD, REG_64::RCX);
    REG_64 variable = alloc_reg(ee, instr.get_source2(), REG_TYPE::GPR, REG_STATE::READ);
    emitter.MOV8_REG(variable, RCX);

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
    REG_64 RCX = lalloc_int_reg(ee, 0, REG_TYPE::GPR, REG_STATE::SCRATCHPAD, REG_64::RCX);
    REG_64 variable = alloc_reg(ee, instr.get_source2(), REG_TYPE::GPR, REG_STATE::READ);
    emitter.MOV8_REG(variable, RCX);

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
    REG_64 RCX = lalloc_int_reg(ee, 0, REG_TYPE::GPR, REG_STATE::SCRATCHPAD, REG_64::RCX);
    REG_64 variable = alloc_reg(ee, instr.get_source2(), REG_TYPE::GPR, REG_STATE::READ);
    emitter.MOV8_REG(variable, RCX);

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
    REG_64 R15 = lalloc_int_reg(ee, 0, REG_TYPE::INTSCRATCHPAD, REG_STATE::SCRATCHPAD);

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
    emitter.MOV32_IMM_MEM(instr.get_jump_dest(), REG_64::R15, offsetof(EmotionEngine, PC));

    if (instr.get_is_link())
    {
        // Set the link register
        emitter.MOV32_IMM_MEM(instr.get_return_addr(), REG_64::R15, offsetof(EmotionEngine, gpr) + get_gpr_offset(EE_NormalReg::ra));
    }
}

void EE_JIT64::jump_indirect(EmotionEngine& ee, IR::Instruction& instr)
{
    REG_64 jump_dest = alloc_reg(ee, instr.get_source(), REG_TYPE::GPR, REG_STATE::READ);

    // Simply set the PC
    emitter.MOV32_TO_MEM(jump_dest, REG_64::R15, offsetof(EmotionEngine, PC));

    if (instr.get_is_link())
    {
        // Set the link register
        emitter.MOV32_IMM_MEM(instr.get_return_addr(), REG_64::R15, offsetof(EmotionEngine, gpr) + get_gpr_offset(EE_NormalReg::ra));
    }
}

void EE_JIT64::load_byte(EmotionEngine& ee, IR::Instruction &instr)
{
    REG_64 source = alloc_reg(ee, instr.get_source(), REG_TYPE::GPR, REG_STATE::READ);
    REG_64 addr = lalloc_int_reg(ee, 0, REG_TYPE::INTSCRATCHPAD, REG_STATE::SCRATCHPAD);
    REG_64 dest = alloc_reg(ee, instr.get_dest(), REG_TYPE::GPR, REG_STATE::WRITE);

    int64_t offset = instr.get_source2();

    if (offset)
        emitter.LEA32_M(source, addr, offset);
    else
        emitter.MOV32_REG(source, addr);
    prepare_abi((uint64_t)&ee);
    prepare_abi_reg(addr);
    free_int_reg(ee, addr);
    call_abi_func((uint64_t)ee_read8);

    emitter.MOVSX8_TO_64(REG_64::RAX, dest);
}

void EE_JIT64::load_byte_unsigned(EmotionEngine& ee, IR::Instruction &instr)
{
    REG_64 source = alloc_reg(ee, instr.get_source(), REG_TYPE::GPR, REG_STATE::READ);
    REG_64 addr = lalloc_int_reg(ee, 0, REG_TYPE::INTSCRATCHPAD, REG_STATE::SCRATCHPAD);
    REG_64 dest = alloc_reg(ee, instr.get_dest(), REG_TYPE::GPR, REG_STATE::WRITE);

    int64_t offset = instr.get_source2();

    if (offset)
        emitter.LEA32_M(source, addr, offset);
    else
        emitter.MOV32_REG(source, addr);
    prepare_abi((uint64_t)&ee);
    prepare_abi_reg(addr);
    free_int_reg(ee, addr);
    call_abi_func((uint64_t)ee_read8);

    emitter.MOVZX8_TO_64(REG_64::RAX, dest);
}

void EE_JIT64::load_doubleword(EmotionEngine& ee, IR::Instruction &instr)
{
    REG_64 source = alloc_reg(ee, instr.get_source(), REG_TYPE::GPR, REG_STATE::READ);
    REG_64 addr = lalloc_int_reg(ee, 0, REG_TYPE::INTSCRATCHPAD, REG_STATE::SCRATCHPAD);
    REG_64 dest = alloc_reg(ee, instr.get_dest(), REG_TYPE::GPR, REG_STATE::WRITE);

    int64_t offset = instr.get_source2();

    if (offset)
        emitter.LEA32_M(source, addr, offset);
    else
        emitter.MOV32_REG(source, addr);
    prepare_abi((uint64_t)&ee);
    prepare_abi_reg(addr);
    free_int_reg(ee, addr);
    call_abi_func((uint64_t)ee_read64);

    emitter.MOV64_MR(REG_64::RAX, dest);
}

void EE_JIT64::load_doubleword_left(EmotionEngine& ee, IR::Instruction& instr)
{
    REG_64 RCX = lalloc_int_reg(ee, 0, REG_TYPE::INTSCRATCHPAD, REG_STATE::SCRATCHPAD, REG_64::RCX);
    REG_64 addr = lalloc_int_reg(ee, 0, REG_TYPE::INTSCRATCHPAD, REG_STATE::SCRATCHPAD);
    REG_64 source = alloc_reg(ee, instr.get_source(), REG_TYPE::GPR, REG_STATE::READ);
    REG_64 dest = alloc_reg(ee, instr.get_dest(), REG_TYPE::GPR, REG_STATE::READ_WRITE);

    int64_t offset = instr.get_source2();

    if (offset)
        emitter.LEA32_M(source, addr, offset);
    else
        emitter.MOV32_REG(source, addr);
    emitter.MOV32_REG(addr, RCX);
    emitter.AND32_REG_IMM(~0x7, RCX);
    prepare_abi((uint64_t)&ee);
    prepare_abi_reg(RCX);
    call_abi_func((uint64_t)ee_read64);
    emitter.AND32_REG_IMM(0x7, addr);
    emitter.SHL32_REG_IMM(0x3, addr);
    emitter.MOV32_REG_IMM(56, RCX);
    emitter.SUB32_REG(addr, RCX);
    emitter.SHL64_CL(REG_64::RAX);

    emitter.SUB16_REG_IMM(64, REG_64::RCX);
    emitter.NEG16(REG_64::RCX);
    emitter.MOV32_REG_IMM(0x3F, addr);
    emitter.CMP16_IMM(0x40, RCX);
    emitter.CMOVCC16_REG(ConditionCode::E, addr, RCX);
    emitter.SHL64_CL(dest);
    emitter.SHR64_CL(dest);
    emitter.OR64_REG(REG_64::RAX, dest);

    free_int_reg(ee, RCX);
    free_int_reg(ee, addr);
}

void EE_JIT64::load_doubleword_right(EmotionEngine& ee, IR::Instruction& instr)
{
    REG_64 RCX = lalloc_int_reg(ee, 0, REG_TYPE::INTSCRATCHPAD, REG_STATE::SCRATCHPAD, REG_64::RCX);
    REG_64 addr = lalloc_int_reg(ee, 0, REG_TYPE::INTSCRATCHPAD, REG_STATE::SCRATCHPAD);
    REG_64 source = alloc_reg(ee, instr.get_source(), REG_TYPE::GPR, REG_STATE::READ);
    REG_64 dest = alloc_reg(ee, instr.get_dest(), REG_TYPE::GPR, REG_STATE::READ_WRITE);

    int64_t offset = instr.get_source2();

    if (offset)
        emitter.LEA32_M(source, addr, offset);
    else
        emitter.MOV32_REG(source, addr);
    emitter.MOV32_REG(addr, RCX);
    emitter.AND32_REG_IMM(~0x7, RCX);
    prepare_abi((uint64_t)&ee);
    prepare_abi_reg(RCX);
    call_abi_func((uint64_t)ee_read64);
    emitter.AND32_REG_IMM(0x7, addr);
    emitter.SHL32_REG_IMM(0x3, addr);
    emitter.MOV32_REG(addr, RCX);
    emitter.SHR64_CL(REG_64::RAX);

    emitter.SUB16_REG_IMM(64, RCX);
    emitter.NEG16(RCX);
    emitter.MOV32_REG_IMM(0x3F, addr);
    emitter.CMP16_IMM(0x40, RCX);
    emitter.CMOVCC16_REG(ConditionCode::E, addr, RCX);
    emitter.SHR64_CL(dest);
    emitter.SHL64_CL(dest);
    emitter.OR64_REG(REG_64::RAX, dest);

    free_int_reg(ee, RCX);
    free_int_reg(ee, addr);
}

void EE_JIT64::load_halfword(EmotionEngine& ee, IR::Instruction &instr)
{
    REG_64 source = alloc_reg(ee, instr.get_source(), REG_TYPE::GPR, REG_STATE::READ);
    REG_64 addr = lalloc_int_reg(ee, 0, REG_TYPE::INTSCRATCHPAD, REG_STATE::SCRATCHPAD);
    REG_64 dest = alloc_reg(ee, instr.get_dest(), REG_TYPE::GPR, REG_STATE::WRITE);

    int64_t offset = instr.get_source2();

    if (offset)
        emitter.LEA32_M(source, addr, offset);
    else
        emitter.MOV32_REG(source, addr);
    prepare_abi((uint64_t)&ee);
    prepare_abi_reg(addr);
    free_int_reg(ee, addr);
    call_abi_func((uint64_t)ee_read16);

    emitter.MOVSX16_TO_64(REG_64::RAX, dest);
}

void EE_JIT64::load_halfword_unsigned(EmotionEngine& ee, IR::Instruction &instr)
{
    REG_64 source = alloc_reg(ee, instr.get_source(), REG_TYPE::GPR, REG_STATE::READ);
    REG_64 addr = lalloc_int_reg(ee, 0, REG_TYPE::INTSCRATCHPAD, REG_STATE::SCRATCHPAD);
    REG_64 dest = alloc_reg(ee, instr.get_dest(), REG_TYPE::GPR, REG_STATE::WRITE);

    int64_t offset = instr.get_source2();

    if (offset)
        emitter.LEA32_M(source, addr, offset);
    else
        emitter.MOV32_REG(source, addr);
    prepare_abi((uint64_t)&ee);
    prepare_abi_reg(addr);
    free_int_reg(ee, addr);
    call_abi_func((uint64_t)ee_read16);

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
    REG_64 source = alloc_reg(ee, instr.get_source(), REG_TYPE::GPR, REG_STATE::READ);
    REG_64 addr = lalloc_int_reg(ee, 0, REG_TYPE::INTSCRATCHPAD, REG_STATE::SCRATCHPAD);
    REG_64 dest = alloc_reg(ee, instr.get_dest(), REG_TYPE::GPR, REG_STATE::WRITE);

    int64_t offset = instr.get_source2();

    if (offset)
        emitter.LEA32_M(source, addr, offset);
    else
        emitter.MOV32_REG(source, addr);
    prepare_abi((uint64_t)&ee);
    prepare_abi_reg(addr);
    free_int_reg(ee, addr);
    call_abi_func((uint64_t)ee_read32);

    emitter.MOVSX32_TO_64(REG_64::RAX, dest);
}

void EE_JIT64::load_word_left(EmotionEngine& ee, IR::Instruction& instr)
{
    REG_64 RCX = lalloc_int_reg(ee, 0, REG_TYPE::INTSCRATCHPAD, REG_STATE::SCRATCHPAD, REG_64::RCX);
    REG_64 addr = lalloc_int_reg(ee, 0, REG_TYPE::INTSCRATCHPAD, REG_STATE::SCRATCHPAD);
    REG_64 source = alloc_reg(ee, instr.get_source(), REG_TYPE::GPR, REG_STATE::READ);
    REG_64 dest = alloc_reg(ee, instr.get_dest(), REG_TYPE::GPR, REG_STATE::READ_WRITE);

    int64_t offset = instr.get_source2();

    if (offset)
        emitter.LEA32_M(source, addr, offset);
    else
        emitter.MOV32_REG(source, addr);
    emitter.MOV32_REG(addr, RCX);
    emitter.AND32_REG_IMM(~0x3, RCX);
    prepare_abi((uint64_t)&ee);
    prepare_abi_reg(RCX);
    call_abi_func((uint64_t)ee_read32);
    emitter.AND32_REG_IMM(0x3, addr);
    emitter.SHL32_REG_IMM(0x3, addr);
    emitter.MOV32_REG_IMM(24, RCX);
    emitter.SUB32_REG(addr, RCX);
    emitter.SHL32_CL(REG_64::RAX);

    emitter.SUB16_REG_IMM(32, RCX);
    emitter.NEG16(RCX);
    emitter.MOV32_REG_IMM(0x1F, addr);
    emitter.CMP16_IMM(0x20, RCX);
    emitter.CMOVCC16_REG(ConditionCode::E, addr, RCX);
    emitter.SHL32_CL(dest);
    emitter.SHR32_CL(dest);
    emitter.OR32_REG(REG_64::RAX, dest);
    emitter.MOVSX32_TO_64(dest, dest);

    free_int_reg(ee, RCX);
    free_int_reg(ee, addr);
}

void EE_JIT64::load_word_right(EmotionEngine& ee, IR::Instruction& instr)
{
    REG_64 RCX = lalloc_int_reg(ee, 0, REG_TYPE::INTSCRATCHPAD, REG_STATE::SCRATCHPAD, REG_64::RCX);
    REG_64 addr = lalloc_int_reg(ee, 0, REG_TYPE::INTSCRATCHPAD, REG_STATE::SCRATCHPAD);
    REG_64 source = alloc_reg(ee, instr.get_source(), REG_TYPE::GPR, REG_STATE::READ);
    REG_64 dest = alloc_reg(ee, instr.get_dest(), REG_TYPE::GPR, REG_STATE::READ_WRITE);

    int64_t offset = instr.get_source2();

    if (offset)
        emitter.LEA32_M(source, addr, offset);
    else
        emitter.MOV32_REG(source, addr);
    emitter.MOV32_REG(addr, RCX);
    emitter.AND32_REG_IMM(~0x3, RCX);
    prepare_abi((uint64_t)&ee);
    prepare_abi_reg(RCX);
    call_abi_func((uint64_t)ee_read32);
    emitter.AND32_REG_IMM(0x3, addr);
    emitter.SHL32_REG_IMM(0x3, addr);
    emitter.MOV32_REG(addr, RCX);
    emitter.SHR32_CL(REG_64::RAX);

    emitter.SUB16_REG_IMM(32, RCX);
    emitter.NEG16(RCX);
    emitter.MOV32_REG_IMM(0x1F, addr);
    emitter.CMP16_IMM(0x20, RCX);
    emitter.CMOVCC16_REG(ConditionCode::E, addr, RCX);
    emitter.SHR32_CL(dest);
    emitter.SHL32_CL(dest);
    emitter.OR32_REG(REG_64::RAX, dest);
    emitter.MOVSX32_TO_64(dest, dest);

    free_int_reg(ee, RCX);
    free_int_reg(ee, addr);
}

void EE_JIT64::load_word_unsigned(EmotionEngine& ee, IR::Instruction &instr)
{
    REG_64 source = alloc_reg(ee, instr.get_source(), REG_TYPE::GPR, REG_STATE::READ);
    REG_64 addr = lalloc_int_reg(ee, 0, REG_TYPE::INTSCRATCHPAD, REG_STATE::SCRATCHPAD);
    REG_64 dest = alloc_reg(ee, instr.get_dest(), REG_TYPE::GPR, REG_STATE::WRITE);

    int64_t offset = instr.get_source2();

    if (offset)
        emitter.LEA32_M(source, addr, offset);
    else
        emitter.MOV32_REG(source, addr);
    prepare_abi((uint64_t)&ee);
    prepare_abi_reg(addr);
    free_int_reg(ee, addr);
    call_abi_func((uint64_t)ee_read32);


    emitter.MOV32_REG(REG_64::RAX, dest);
}

void EE_JIT64::load_quadword(EmotionEngine& ee, IR::Instruction &instr)
{
    REG_64 source = alloc_reg(ee, instr.get_source(), REG_TYPE::GPR, REG_STATE::READ);
    REG_64 addr = lalloc_int_reg(ee, 0, REG_TYPE::INTSCRATCHPAD, REG_STATE::SCRATCHPAD);
    REG_64 dest = alloc_reg(ee, instr.get_dest(), REG_TYPE::GPREXTENDED, REG_STATE::WRITE);
    
    int64_t offset = instr.get_source2();

    if (offset)
        emitter.LEA32_M(source, addr, offset);
    else
        emitter.MOV32_REG(source, addr);
    emitter.AND32_REG_IMM(0xFFFFFFF0, addr);

    // Due to differences in how the uint128_t struct is returned on different platforms,
    // we simply allocate space for it on the stack, which the wrapper function will store the
    // result into.
    // Note: The 0x1A0 here is the SQ/LQ uint128_t offset noted in recompile_block
    // TODO: Store 0x1A0 in some sort of constant
    prepare_abi((uint64_t)&ee);
    prepare_abi_reg(addr);
    prepare_abi_reg(REG_64::RSP, 0x1A0);
    free_int_reg(ee, addr);
    call_abi_func((uint64_t)ee_read128);
    restore_xmm_regs(std::vector<REG_64> {dest}, false);

    emitter.MOVAPS_FROM_MEM(REG_64::RSP, dest, 0x1A0);
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

void EE_JIT64::move_word_reg(EmotionEngine& ee, IR::Instruction& instr)
{
    REG_64 source = alloc_reg(ee, instr.get_source(), REG_TYPE::GPR, REG_STATE::READ);
    REG_64 dest = alloc_reg(ee, instr.get_dest(), REG_TYPE::GPR, REG_STATE::WRITE);

    emitter.MOV32_REG(source, dest);
}

void EE_JIT64::multiply_add_unsigned_word(EmotionEngine& ee, IR::Instruction& instr, bool hi)
{
    // choose destination registers based on operation pipeline
    EE_SpecialReg lo_reg = hi ? EE_SpecialReg::LO1 : EE_SpecialReg::LO;
    EE_SpecialReg hi_reg = hi ? EE_SpecialReg::HI1 : EE_SpecialReg::HI;

    REG_64 RDX = lalloc_int_reg(ee, 0, REG_TYPE::INTSCRATCHPAD, REG_STATE::SCRATCHPAD, REG_64::RDX);
    REG_64 source = alloc_reg(ee, instr.get_source(), REG_TYPE::GPR, REG_STATE::READ);
    REG_64 source2 = alloc_reg(ee, instr.get_source2(), REG_TYPE::GPR, REG_STATE::READ);
    REG_64 LO = alloc_reg(ee, (int)lo_reg, REG_TYPE::GPR, REG_STATE::READ_WRITE);
    REG_64 HI = alloc_reg(ee, (int)hi_reg, REG_TYPE::GPR, REG_STATE::READ_WRITE);

    emitter.MOVSX32_TO_64(source, REG_64::RAX);
    emitter.MUL32(source2);

    // FIXME: Handle LO overflow which modifies HI (use carry flag?)
    emitter.ADD32_REG(REG_64::RAX, LO);
    emitter.MOVSX32_TO_64(LO, LO);
    emitter.ADD32_REG(RDX, HI);
    emitter.MOVSX32_TO_64(HI, HI);

    if (instr.get_dest())
    {
        REG_64 dest = alloc_reg(ee, instr.get_dest(), REG_TYPE::GPR, REG_STATE::WRITE);
        emitter.MOVSX32_TO_64(LO, dest);
    }

    free_int_reg(ee, RDX);
}

void EE_JIT64::multiply_add_word(EmotionEngine& ee, IR::Instruction& instr, bool hi)
{
    // choose destination registers based on operation pipeline
    EE_SpecialReg lo_reg = hi ? EE_SpecialReg::LO1 : EE_SpecialReg::LO;
    EE_SpecialReg hi_reg = hi ? EE_SpecialReg::HI1 : EE_SpecialReg::HI;

    REG_64 RDX = lalloc_int_reg(ee, 0, REG_TYPE::INTSCRATCHPAD, REG_STATE::SCRATCHPAD, REG_64::RDX);
    REG_64 source = alloc_reg(ee, instr.get_source(), REG_TYPE::GPR, REG_STATE::READ);
    REG_64 source2 = alloc_reg(ee, instr.get_source2(), REG_TYPE::GPR, REG_STATE::READ);

    // FIXME: Handle LO overflow which modifies HI (use carry flag?)
    REG_64 LO = alloc_reg(ee, (int)lo_reg, REG_TYPE::GPR, REG_STATE::READ_WRITE);
    REG_64 HI = alloc_reg(ee, (int)hi_reg, REG_TYPE::GPR, REG_STATE::READ_WRITE);

    emitter.MOVSX32_TO_64(source, REG_64::RAX);
    emitter.IMUL32(source2);
    emitter.ADD32_REG(REG_64::RAX, LO);
    emitter.MOVSX32_TO_64(LO, LO);
    emitter.ADD32_REG(RDX, HI);
    emitter.MOVSX32_TO_64(HI, HI);

    if (instr.get_dest())
    {
        REG_64 dest = alloc_reg(ee, instr.get_dest(), REG_TYPE::GPR, REG_STATE::WRITE);
        emitter.MOVSX32_TO_64(LO, dest);
    }

    free_int_reg(ee, RDX);
}

void EE_JIT64::multiply_unsigned_word(EmotionEngine& ee, IR::Instruction& instr, bool hi)
{
    // choose destination registers based on operation pipeline
    EE_SpecialReg lo_reg = hi ? EE_SpecialReg::LO1 : EE_SpecialReg::LO;
    EE_SpecialReg hi_reg = hi ? EE_SpecialReg::HI1 : EE_SpecialReg::HI;

    REG_64 RDX = lalloc_int_reg(ee, 0, REG_TYPE::INTSCRATCHPAD, REG_STATE::SCRATCHPAD, REG_64::RDX);
    REG_64 source = alloc_reg(ee, instr.get_source(), REG_TYPE::GPR, REG_STATE::READ);
    REG_64 source2 = alloc_reg(ee, instr.get_source2(), REG_TYPE::GPR, REG_STATE::READ);
    REG_64 LO = alloc_reg(ee, (int)lo_reg, REG_TYPE::GPR, REG_STATE::WRITE);
    REG_64 HI = alloc_reg(ee, (int)hi_reg, REG_TYPE::GPR, REG_STATE::WRITE);

    emitter.MOVSX32_TO_64(source, REG_64::RAX);
    emitter.MUL32(source2);
    emitter.MOVSX32_TO_64(REG_64::RAX, LO);
    emitter.MOVSX32_TO_64(RDX, HI);

    if (instr.get_dest())
    {
        REG_64 dest = alloc_reg(ee, instr.get_dest(), REG_TYPE::GPR, REG_STATE::WRITE);
        emitter.MOV64_MR(LO, dest);
    }

    free_int_reg(ee, RDX);
}

void EE_JIT64::multiply_word(EmotionEngine& ee, IR::Instruction& instr, bool hi)
{
    // choose destination registers based on operation pipeline
    EE_SpecialReg lo_reg = hi ? EE_SpecialReg::LO1 : EE_SpecialReg::LO;
    EE_SpecialReg hi_reg = hi ? EE_SpecialReg::HI1 : EE_SpecialReg::HI;

    REG_64 RDX = lalloc_int_reg(ee, 0, REG_TYPE::INTSCRATCHPAD, REG_STATE::SCRATCHPAD, REG_64::RDX);
    REG_64 source = alloc_reg(ee, instr.get_source(), REG_TYPE::GPR, REG_STATE::READ);
    REG_64 source2 = alloc_reg(ee, instr.get_source2(), REG_TYPE::GPR, REG_STATE::READ);
    REG_64 LO = alloc_reg(ee, (int)lo_reg, REG_TYPE::GPR, REG_STATE::WRITE);
    REG_64 HI = alloc_reg(ee, (int)hi_reg, REG_TYPE::GPR, REG_STATE::WRITE);

    emitter.MOVSX32_TO_64(source, REG_64::RAX);
    emitter.IMUL32(source2);
    emitter.MOVSX32_TO_64(REG_64::RAX, LO);
    emitter.MOVSX32_TO_64(RDX, HI);

    if (instr.get_dest())
    {
        REG_64 dest = alloc_reg(ee, instr.get_dest(), REG_TYPE::GPR, REG_STATE::WRITE);
        emitter.MOV64_MR(LO, dest);
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
    REG_64 RCX = lalloc_int_reg(ee, 0, REG_TYPE::INTSCRATCHPAD, REG_STATE::SCRATCHPAD, REG_64::RCX);
    REG_64 variable = alloc_reg(ee, instr.get_source2(), REG_TYPE::GPR, REG_STATE::READ);
    emitter.MOV8_REG(variable, RCX);

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
    REG_64 RCX = lalloc_int_reg(ee, 0, REG_TYPE::INTSCRATCHPAD, REG_STATE::SCRATCHPAD, REG_64::RCX);
    REG_64 variable = alloc_reg(ee, instr.get_source2(), REG_TYPE::GPR, REG_STATE::READ);
    emitter.MOV8_REG(variable, RCX);

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
    REG_64 RCX = lalloc_int_reg(ee, 0, REG_TYPE::INTSCRATCHPAD, REG_STATE::SCRATCHPAD, REG_64::RCX);
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

void EE_JIT64::store_byte(EmotionEngine& ee, IR::Instruction& instr)
{
    REG_64 source = alloc_reg(ee, instr.get_source(), REG_TYPE::GPR, REG_STATE::READ);
    REG_64 dest = alloc_reg(ee, instr.get_dest(), REG_TYPE::GPR, REG_STATE::READ);
    REG_64 addr = lalloc_int_reg(ee, 0, REG_TYPE::INTSCRATCHPAD, REG_STATE::SCRATCHPAD);
    int64_t offset = instr.get_source2();

    if (offset)
        emitter.LEA32_M(dest, addr, offset);
    else
        emitter.MOV32_REG(dest, addr);
    prepare_abi((uint64_t)&ee);
    prepare_abi_reg(addr);
    prepare_abi_reg(source);
    free_int_reg(ee, addr);
    call_abi_func((uint64_t)ee_write8);
}

void EE_JIT64::store_doubleword(EmotionEngine& ee, IR::Instruction& instr)
{
    REG_64 source = alloc_reg(ee, instr.get_source(), REG_TYPE::GPR, REG_STATE::READ);
    REG_64 dest = alloc_reg(ee, instr.get_dest(), REG_TYPE::GPR, REG_STATE::READ);
    REG_64 addr = lalloc_int_reg(ee, 0, REG_TYPE::INTSCRATCHPAD, REG_STATE::SCRATCHPAD);
    int64_t offset = instr.get_source2();

    if (offset)
        emitter.LEA32_M(dest, addr, offset);
    else
        emitter.MOV32_REG(dest, addr);
    prepare_abi((uint64_t)&ee);
    prepare_abi_reg(addr);
    prepare_abi_reg(source);
    free_int_reg(ee, addr);
    call_abi_func((uint64_t)ee_write64);
}

void EE_JIT64::store_doubleword_left(EmotionEngine& ee, IR::Instruction& instr)
{
    REG_64 RCX = lalloc_int_reg(ee, 0, REG_TYPE::INTSCRATCHPAD, REG_STATE::SCRATCHPAD, REG_64::RCX);
    REG_64 addr = lalloc_int_reg(ee, 0, REG_TYPE::INTSCRATCHPAD, REG_STATE::SCRATCHPAD);
    REG_64 addr_backup = lalloc_int_reg(ee, 0, REG_TYPE::INTSCRATCHPAD, REG_STATE::SCRATCHPAD);
    REG_64 source = alloc_reg(ee, instr.get_source(), REG_TYPE::GPR, REG_STATE::READ);
    REG_64 dest = alloc_reg(ee, instr.get_dest(), REG_TYPE::GPR, REG_STATE::READ);
    int64_t offset = instr.get_source2();

    if (offset)
        emitter.LEA32_M(dest, addr, offset);
    else
        emitter.MOV32_REG(dest, addr);
    emitter.MOV32_REG(addr, addr_backup);
    emitter.AND32_REG_IMM(~0x7, addr);
    prepare_abi((uint64_t)&ee);
    prepare_abi_reg(addr);
    call_abi_func((uint64_t)ee_read64);
    emitter.MOV32_REG(addr_backup, RCX);
    emitter.AND32_REG_IMM(0x7, RCX);
    emitter.SHL32_REG_IMM(0x3, RCX);
    emitter.XOR16_REG_IMM(56, RCX);
    emitter.MOV64_MR(source, addr);
    emitter.SHR64_CL(addr);
    emitter.SUB16_REG_IMM(64, RCX);
    emitter.NEG16(RCX);
    emitter.MOV32_REG_IMM(0x3F, addr_backup);
    emitter.CMP16_IMM(0x40, RCX);
    emitter.CMOVCC16_REG(ConditionCode::E, addr_backup, RCX);
    emitter.SHR64_CL(REG_64::RAX);
    emitter.SHL64_CL(REG_64::RAX);
    emitter.OR64_REG(REG_64::RAX, addr);
    if (offset)
        emitter.LEA32_M(dest, addr_backup, offset);
    else
        emitter.MOV32_REG(dest, addr_backup);
    emitter.AND32_REG_IMM(~0x7, addr_backup);
    prepare_abi((uint64_t)&ee);
    prepare_abi_reg(addr_backup);
    prepare_abi_reg(addr);
    free_int_reg(ee, RCX);
    free_int_reg(ee, addr);
    free_int_reg(ee, addr_backup);
    call_abi_func((uint64_t)ee_write64);
}

void EE_JIT64::store_doubleword_right(EmotionEngine& ee, IR::Instruction& instr)
{
    REG_64 RCX = lalloc_int_reg(ee, 0, REG_TYPE::INTSCRATCHPAD, REG_STATE::SCRATCHPAD, REG_64::RCX);
    REG_64 addr = lalloc_int_reg(ee, 0, REG_TYPE::INTSCRATCHPAD, REG_STATE::SCRATCHPAD);
    REG_64 addr_backup = lalloc_int_reg(ee, 0, REG_TYPE::INTSCRATCHPAD, REG_STATE::SCRATCHPAD);
    REG_64 source = alloc_reg(ee, instr.get_source(), REG_TYPE::GPR, REG_STATE::READ);
    REG_64 dest = alloc_reg(ee, instr.get_dest(), REG_TYPE::GPR, REG_STATE::READ);
    int64_t offset = instr.get_source2();

    if (offset)
        emitter.LEA32_M(dest, addr, offset);
    else
        emitter.MOV32_REG(dest, addr);
    emitter.MOV32_REG(addr, addr_backup);
    emitter.AND32_REG_IMM(~0x7, addr);
    prepare_abi((uint64_t)&ee);
    prepare_abi_reg(addr);
    call_abi_func((uint64_t)ee_read64);
    emitter.MOV32_REG(addr_backup, RCX);
    emitter.AND32_REG_IMM(0x7, RCX);
    emitter.SHL32_REG_IMM(0x3, RCX);
    emitter.MOV64_MR(source, addr);
    emitter.SHL64_CL(addr);
    emitter.SUB16_REG_IMM(64, RCX);
    emitter.NEG16(RCX);
    emitter.MOV32_REG_IMM(0x3F, addr_backup);
    emitter.CMP16_IMM(0x40, RCX);
    emitter.CMOVCC16_REG(ConditionCode::E, addr_backup, RCX);
    emitter.SHL64_CL(REG_64::RAX);
    emitter.SHR64_CL(REG_64::RAX);
    emitter.OR64_REG(REG_64::RAX, addr);
    if (offset)
        emitter.LEA32_M(dest, addr_backup, offset);
    else
        emitter.MOV32_REG(dest, addr_backup);
    emitter.AND32_REG_IMM(~0x7, addr_backup);
    prepare_abi((uint64_t)&ee);
    prepare_abi_reg(addr_backup);
    prepare_abi_reg(addr);
    free_int_reg(ee, RCX);
    free_int_reg(ee, addr);
    free_int_reg(ee, addr_backup);
    call_abi_func((uint64_t)ee_write64);
}

void EE_JIT64::store_halfword(EmotionEngine& ee, IR::Instruction& instr)
{
    REG_64 source = alloc_reg(ee, instr.get_source(), REG_TYPE::GPR, REG_STATE::READ);
    REG_64 dest = alloc_reg(ee, instr.get_dest(), REG_TYPE::GPR, REG_STATE::READ);
    REG_64 addr = lalloc_int_reg(ee, 0, REG_TYPE::INTSCRATCHPAD, REG_STATE::SCRATCHPAD);
    int64_t offset = instr.get_source2();

    if (offset)
        emitter.LEA32_M(dest, addr, offset);
    else
        emitter.MOV32_REG(dest, addr);
    prepare_abi((uint64_t)&ee);
    prepare_abi_reg(addr);
    prepare_abi_reg(source);
    free_int_reg(ee, addr);
    call_abi_func((uint64_t)ee_write16);
}

void EE_JIT64::store_word(EmotionEngine& ee, IR::Instruction& instr)
{
    REG_64 source = alloc_reg(ee, instr.get_source(), REG_TYPE::GPR, REG_STATE::READ);
    REG_64 dest = alloc_reg(ee, instr.get_dest(), REG_TYPE::GPR, REG_STATE::READ);
    REG_64 addr = lalloc_int_reg(ee, 0, REG_TYPE::INTSCRATCHPAD, REG_STATE::SCRATCHPAD);
    int64_t offset = instr.get_source2();

    if (offset)
        emitter.LEA32_M(dest, addr, offset);
    else
        emitter.MOV32_REG(dest, addr);
    prepare_abi((uint64_t)&ee);
    prepare_abi_reg(addr);
    prepare_abi_reg(source);
    free_int_reg(ee, addr);
    call_abi_func((uint64_t)ee_write32);
}

void EE_JIT64::store_word_left(EmotionEngine& ee, IR::Instruction& instr)
{
    REG_64 RCX = lalloc_int_reg(ee, 0, REG_TYPE::INTSCRATCHPAD, REG_STATE::SCRATCHPAD, REG_64::RCX);
    REG_64 addr = lalloc_int_reg(ee, 0, REG_TYPE::INTSCRATCHPAD, REG_STATE::SCRATCHPAD);
    REG_64 addr_backup = lalloc_int_reg(ee, 0, REG_TYPE::INTSCRATCHPAD, REG_STATE::SCRATCHPAD);
    REG_64 source = alloc_reg(ee, instr.get_source(), REG_TYPE::GPR, REG_STATE::READ);
    REG_64 dest = alloc_reg(ee, instr.get_dest(), REG_TYPE::GPR, REG_STATE::READ);
    int64_t offset = instr.get_source2();

    if (offset)
        emitter.LEA32_M(dest, addr, offset);
    else
        emitter.MOV32_REG(dest, addr);
    emitter.MOV32_REG(addr, addr_backup);
    emitter.AND32_REG_IMM(~0x3, addr);
    prepare_abi((uint64_t)&ee);
    prepare_abi_reg(addr);
    call_abi_func((uint64_t)ee_read32);
    emitter.MOV32_REG(addr_backup, RCX);
    emitter.AND32_REG_IMM(0x3, RCX);
    emitter.SHL32_REG_IMM(0x3, RCX);
    emitter.XOR16_REG_IMM(24, RCX);
    emitter.MOV32_REG(source, addr);
    emitter.SHR32_CL(addr);
    emitter.SUB16_REG_IMM(32, RCX);
    emitter.NEG16(RCX);
    emitter.MOV32_REG_IMM(0x1F, addr_backup);
    emitter.CMP16_IMM(0x20, RCX);
    emitter.CMOVCC16_REG(ConditionCode::E, addr_backup, RCX);
    emitter.SHR32_CL(REG_64::RAX);
    emitter.SHL32_CL(REG_64::RAX);
    emitter.OR32_REG(REG_64::RAX, addr);
    if (offset)
        emitter.LEA32_M(dest, addr_backup, offset);
    else
        emitter.MOV32_REG(dest, addr_backup);
    emitter.AND32_REG_IMM(~0x3, addr_backup);
    prepare_abi((uint64_t)&ee);
    prepare_abi_reg(addr_backup);
    prepare_abi_reg(addr);
    free_int_reg(ee, RCX);
    free_int_reg(ee, addr);
    free_int_reg(ee, addr_backup);
    call_abi_func((uint64_t)ee_write32);
}

void EE_JIT64::store_word_right(EmotionEngine& ee, IR::Instruction& instr)
{
    REG_64 RCX = lalloc_int_reg(ee, 0, REG_TYPE::INTSCRATCHPAD, REG_STATE::SCRATCHPAD, REG_64::RCX);
    REG_64 addr = lalloc_int_reg(ee, 0, REG_TYPE::INTSCRATCHPAD, REG_STATE::SCRATCHPAD);
    REG_64 addr_backup = lalloc_int_reg(ee, 0, REG_TYPE::INTSCRATCHPAD, REG_STATE::SCRATCHPAD);
    REG_64 source = alloc_reg(ee, instr.get_source(), REG_TYPE::GPR, REG_STATE::READ);
    REG_64 dest = alloc_reg(ee, instr.get_dest(), REG_TYPE::GPR, REG_STATE::READ);
    int64_t offset = instr.get_source2();

    if (offset)
        emitter.LEA32_M(dest, addr, offset);
    else
        emitter.MOV32_REG(dest, addr);
    emitter.MOV32_REG(addr, addr_backup);
    emitter.AND32_REG_IMM(~0x3, addr);
    prepare_abi((uint64_t)&ee);
    prepare_abi_reg(addr);
    call_abi_func((uint64_t)ee_read32);
    emitter.MOV32_REG(addr_backup, RCX);
    emitter.AND32_REG_IMM(0x3, RCX);
    emitter.SHL32_REG_IMM(0x3, RCX);
    emitter.MOV32_REG(source, addr);
    emitter.SHL32_CL(addr);
    emitter.SUB16_REG_IMM(32, RCX);
    emitter.NEG16(RCX);
    emitter.MOV32_REG_IMM(0x1F, addr_backup);
    emitter.CMP16_IMM(0x20, RCX);
    emitter.CMOVCC16_REG(ConditionCode::E, addr_backup, RCX);
    emitter.SHL32_CL(REG_64::RAX);
    emitter.SHR32_CL(REG_64::RAX);
    emitter.OR32_REG(REG_64::RAX, addr);
    if (offset)
        emitter.LEA32_M(dest, addr_backup, offset);
    else
        emitter.MOV32_REG(dest, addr_backup);
    emitter.AND32_REG_IMM(~0x3, addr_backup);
    prepare_abi((uint64_t)&ee);
    prepare_abi_reg(addr_backup);
    prepare_abi_reg(addr);
    free_int_reg(ee, RCX);
    free_int_reg(ee, addr);
    free_int_reg(ee, addr_backup);
    call_abi_func((uint64_t)ee_write32);
}

void EE_JIT64::store_quadword(EmotionEngine& ee, IR::Instruction &instr)
{
    REG_64 source = alloc_reg(ee, instr.get_source(), REG_TYPE::GPREXTENDED, REG_STATE::READ);
    REG_64 dest = alloc_reg(ee, instr.get_dest(), REG_TYPE::GPR, REG_STATE::READ);
    REG_64 addr = lalloc_int_reg(ee, 0, REG_TYPE::INTSCRATCHPAD, REG_STATE::SCRATCHPAD);
    int64_t offset = instr.get_source2();

    if (offset)
        emitter.LEA32_M(dest, addr, offset);
    else
        emitter.MOV32_REG(dest, addr);
    emitter.AND32_REG_IMM(0xFFFFFFF0, addr);

    // Due to differences in how the uint128_t struct is passed as an argument on different platforms,
    // we simply allocate space for it on the stack and pass a pointer to our wrapper function.
    // Note: The 0x1A0 here is the SQ/LQ uint128_t offset noted in recompile_block
    // TODO: Store 0x1A0 in some sort of constant
    emitter.MOVAPS_TO_MEM(source, REG_64::RSP, 0x1A0);
    prepare_abi((uint64_t)&ee);
    prepare_abi_reg(addr);
    prepare_abi_reg(REG_64::RSP, 0x1A0);
    free_int_reg(ee, addr);
    call_abi_func((uint64_t)ee_write128);
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

    REG_64 temp = lalloc_int_reg(ee, 0, REG_TYPE::INTSCRATCHPAD, REG_STATE::SCRATCHPAD);

    // Update PC before calling exception handler
    emitter.load_addr((uint64_t)&ee.PC, REG_64::RAX);
    emitter.MOV32_REG_IMM(instr.get_return_addr(), temp);
    emitter.MOV32_TO_MEM(temp, REG_64::RAX);

    // Cleanup branch_on, exception handler expects this to be false
    emitter.load_addr((uint64_t)&ee.branch_on, REG_64::RAX);
    emitter.MOV8_REG_IMM(false, temp);
    emitter.MOV8_TO_MEM(temp, REG_64::RAX);

    prepare_abi((uint64_t)&ee);
    call_abi_func((uint64_t)ee_syscall_exception);

    // Since the interpreter decrements PC by 4, we reset it here to account for that.
    emitter.load_addr((uint64_t)&ee.PC, REG_64::RAX);
    emitter.MOV32_FROM_MEM(REG_64::RAX, temp);
    emitter.ADD32_REG_IMM(4, temp);
    emitter.MOV32_TO_MEM(temp, REG_64::RAX);

    free_int_reg(ee, temp);
}

void EE_JIT64::vcall_ms(EmotionEngine& ee, IR::Instruction& instr)
{
    prepare_abi((uint64_t)ee.vu0);
    prepare_abi(instr.get_source());
    call_abi_func((uint64_t)vu0_start_program);
}

void EE_JIT64::vcall_msr(EmotionEngine& ee, IR::Instruction& instr)
{
    prepare_abi((uint64_t)ee.vu0);
    call_abi_func((uint64_t)vu0_read_CMSAR0_shl3);

    prepare_abi((uint64_t)ee.vu0);
    prepare_abi_reg(REG_64::RAX);
    call_abi_func((uint64_t)vu0_start_program);
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
