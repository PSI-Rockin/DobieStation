#ifndef EE_JIT64_HPP
#define EE_JIT64_HPP
#include "../jitcommon/emitter64.hpp"
//#include "../jitcommon/ir_block.hpp"
//#include "vu_jittrans.hpp"
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

extern "C" uint8_t* exec_block(EE_JIT64& jit, EmotionEngine& ee);

class EE_JIT64
{
private:
    AllocReg xmm_regs[16];
    AllocReg int_regs[16];
    JitCache cache;
    Emitter64 emitter;
    //VU_JitTranslator ir;

    //TODO
public:
    EE_JIT64();

    void reset(bool clear_cache = true);
    uint16_t run(EmotionEngine& ee);

    friend uint8_t* exec_block(EE_JIT64& jit, EmotionEngine& ee);
};

#endif // EE_JIT64_HPP
