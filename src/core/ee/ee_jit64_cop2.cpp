#include <cmath>
#include <algorithm>
#include "ee_jit64.hpp"

void EE_JIT64::store_quadword_coprocessor2(EmotionEngine& ee, IR::Instruction &instr)
{
    alloc_abi_regs(3);

    REG_64 source = alloc_reg(ee, instr.get_source(), REG_TYPE::VF, REG_STATE::READ);
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
    emitter.PEXTRQ_XMM(1, source, REG_64::RAX);
    emitter.PUSH(REG_64::RAX);
    emitter.MOVQ_FROM_XMM(source, REG_64::RAX);
    emitter.PUSH(REG_64::RAX);
    prepare_abi(ee, (uint64_t)&ee);
    prepare_abi_reg(ee, addr);
    prepare_abi_reg(ee, REG_64::RSP);
    call_abi_func(ee, (uint64_t)ee_write128);
    free_int_reg(ee, addr);
    emitter.ADD64_REG_IMM(0x10, REG_64::RSP);
}

void EE_JIT64::load_quadword_coprocessor2(EmotionEngine& ee, IR::Instruction &instr)
{
    alloc_abi_regs(3);
    REG_64 source = alloc_reg(ee, instr.get_source(), REG_TYPE::GPR, REG_STATE::READ);
    REG_64 addr = lalloc_int_reg(ee, 0, REG_TYPE::INTSCRATCHPAD, REG_STATE::SCRATCHPAD);

    int64_t offset = instr.get_source2();

    if (offset)
        emitter.LEA32_M(source, addr, offset);
    else
        emitter.MOV32_REG(source, addr);

    // Due to differences in how the uint128_t struct is returned on different platforms,
    // we simply allocate space for it on the stack, which the wrapper function will store the
    // result into.
    emitter.SUB64_REG_IMM(0x10, REG_64::RSP);
    prepare_abi(ee, (uint64_t)&ee);
    prepare_abi_reg(ee, addr);
    prepare_abi_reg(ee, REG_64::RSP);
    free_int_reg(ee, addr);
    call_abi_func(ee, (uint64_t)ee_read128);

    REG_64 dest = alloc_reg(ee, instr.get_dest(), REG_TYPE::VF, REG_STATE::WRITE);
    emitter.POP(REG_64::RAX);
    emitter.MOVQ_TO_XMM(REG_64::RAX, dest);
    emitter.POP(REG_64::RAX);
    emitter.PINSRQ_XMM(1, REG_64::RAX, dest);
}