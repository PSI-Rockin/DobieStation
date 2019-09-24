#include <cmath>
#include <algorithm>
#include "ee_jit64.hpp"

void EE_JIT64::floating_point_minimum_AVX(EmotionEngine& ee, IR::Instruction& instr)
{
    REG_64 source = alloc_reg(ee, instr.get_source(), REG_TYPE::FPU, REG_STATE::READ);
    REG_64 source2 = alloc_reg(ee, instr.get_source2(), REG_TYPE::FPU, REG_STATE::READ);
    REG_64 dest = alloc_reg(ee, instr.get_dest(), REG_TYPE::FPU, REG_STATE::WRITE);

    emitter.load_addr((uint64_t)&ee.fpu->control.u, REG_64::RAX);
    emitter.MOV8_IMM_MEM(false, REG_64::RAX);
    emitter.load_addr((uint64_t)&ee.fpu->control.o, REG_64::RAX);
    emitter.MOV8_IMM_MEM(false, REG_64::RAX);

    emitter.VMINPS(source, source2, dest);
}

void EE_JIT64::floating_point_maximum_AVX(EmotionEngine& ee, IR::Instruction& instr)
{
    REG_64 source = alloc_reg(ee, instr.get_source(), REG_TYPE::FPU, REG_STATE::READ);
    REG_64 source2 = alloc_reg(ee, instr.get_source2(), REG_TYPE::FPU, REG_STATE::READ);
    REG_64 dest = alloc_reg(ee, instr.get_dest(), REG_TYPE::FPU, REG_STATE::WRITE);

    emitter.load_addr((uint64_t)&ee.fpu->control.u, REG_64::RAX);
    emitter.MOV8_IMM_MEM(false, REG_64::RAX);
    emitter.load_addr((uint64_t)&ee.fpu->control.o, REG_64::RAX);
    emitter.MOV8_IMM_MEM(false, REG_64::RAX);

    emitter.VMAXPS(source, source2, dest);
}