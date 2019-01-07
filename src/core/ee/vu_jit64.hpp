#ifndef VU_JIT64_HPP
#define VU_JIT64_HPP
#include "../jitcommon/emitter64.hpp"
#include "../jitcommon/ir_block.hpp"

class VectorUnit;

struct AllocReg
{
    bool used;
    bool locked; //Prevent the register from being allocated
    int age;
    int vu_reg;
};

class VU_JIT64
{
    private:
        AllocReg xmm_regs[16];
        AllocReg int_regs[16];
        JitCache cache;
        Emitter64 emitter;

        int abi_int_count;
        int abi_xmm_count;

        void load_const(VectorUnit& vu, IR::Instruction& instr);
        void move_int_reg(VectorUnit& vu, IR::Instruction& instr);
        void jump(VectorUnit& vu, IR::Instruction& instr);
        void jump_and_link(VectorUnit& vu, IR::Instruction& instr);
        void jump_indirect(VectorUnit& vu, IR::Instruction& instr);

        void add_unsigned_imm(VectorUnit& vu, IR::Instruction& instr);

        void mul_vector_by_scalar(VectorUnit& vu, IR::Instruction& instr);

        REG_64 alloc_int_reg(VectorUnit& vu, int vi_reg, bool load_state = true);
        REG_64 alloc_sse_reg(VectorUnit& vu, int vf_reg, bool load_state = true);
        void flush_regs(VectorUnit& vu);

        void recompile_block(VectorUnit& vu, IR::Block& block);
        uint8_t* exec_block(VectorUnit& vu);

        void prepare_abi(VectorUnit& vu, uint64_t value);
        void call_abi_func(uint64_t addr);
    public:
        VU_JIT64();

        void reset();
        int run(VectorUnit& vu);

        friend void flush_regs(VU_JIT64& jit, VectorUnit& vu);
};

#endif // VU_JIT64_HPP
