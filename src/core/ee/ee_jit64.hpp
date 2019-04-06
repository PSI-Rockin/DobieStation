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

    uint16_t cycle_count;

    void fallback_interpreter(EmotionEngine& ee, const IR::Instruction &instr);

    void prepare_abi(EmotionEngine& ee, uint64_t value);
    void call_abi_func(uint64_t addr);

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
