#ifndef VU_JIT64_HPP
#define VU_JIT64_HPP
#include "../jitcommon/emitter64.hpp"
#include "../jitcommon/ir_block.hpp"

class VectorUnit;

struct AllocReg
{
    bool used;
    int reg;
};

class VU_JIT64
{
    private:
        JitCache cache;
        Emitter64 emitter;

        void jump(VectorUnit& vu, IR::Instruction& instr);
        void jump_and_link(VectorUnit& vu, IR::Instruction& instr);
        void mul_vector_by_scalar(VectorUnit& vu, IR::Instruction& instr);

        REG_64 alloc_int_reg();
        REG_64_SSE alloc_sse_reg();
        void flush_regs(VectorUnit& vu);

        void recompile_block(VectorUnit& vu, IR::Block& block);
        uint8_t* exec_block(VectorUnit& vu);
    public:
        VU_JIT64();

        void reset();
        int run(VectorUnit& vu);
};

#endif // VU_JIT64_HPP
