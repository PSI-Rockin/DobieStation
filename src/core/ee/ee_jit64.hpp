#ifndef EE_JIT64_HPP
#define EE_JIT64_HPP
#include "../jitcommon/emitter64.hpp"
#include "../jitcommon/ir_block.hpp"
#include "ee_jittrans.hpp"
#include "emotion.hpp"

struct AllocReg
{
    bool used;
    bool locked; //Prevent the register from being allocated
    bool modified;
    int age;
    int vu_reg;
    uint8_t needs_clamping;
};

class EE_JIT64;

extern "C" uint8_t* exec_block_ee(EE_JIT64& jit, EmotionEngine& ee);

class EE_JIT64
{
private:
    AllocReg xmm_regs[16];
    AllocReg int_regs[16];
    JitCache cache;
    Emitter64 emitter;
    EE_JitTranslator ir;

    int abi_int_count;
    int abi_xmm_count;

    bool ee_branch;
    uint32_t ee_branch_dest, ee_branch_fail_dest;
    uint32_t ee_branch_delay_dest, ee_branch_delay_fail_dest;
    uint16_t cycle_count;

    void handle_branch(EmotionEngine& ee);

    void fallback_interpreter(EmotionEngine& ee, const IR::Instruction &instr);

    void prepare_abi(const EmotionEngine& ee, uint64_t value);
    void call_abi_func(uint64_t addr);
    void recompile_block(EmotionEngine& ee, IR::Block& block);

    void emit_prologue();
    void emit_instruction(EmotionEngine &ee, const IR::Instruction &instr);
    uint64_t get_fpu_addr(EmotionEngine &ee, int index);

    void flush_regs(EmotionEngine& ee);
    void cleanup_recompiler(EmotionEngine& ee, bool clear_regs);
    void emit_epilogue();
public:
    EE_JIT64();

    void reset(bool clear_cache = true);
    uint16_t run(EmotionEngine& ee);

    friend uint8_t* exec_block_ee(EE_JIT64& jit, EmotionEngine& ee);
};

#endif // EE_JIT64_HPP
