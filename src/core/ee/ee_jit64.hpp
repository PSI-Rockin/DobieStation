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

enum class REG_STATE
{
    SCRATCHPAD,
    READ,
    WRITE,
    READ_WRITE
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
    void handle_branch_destinations(EmotionEngine& ee, IR::Instruction& instr);

    void branch_equal(EmotionEngine& ee, IR::Instruction &instr);
    void branch_not_equal(EmotionEngine& ee, IR::Instruction &instr);
    void jump_and_link(EmotionEngine& ee, IR::Instruction& instr);
    void jump_and_link_indirect(EmotionEngine &ee, IR::Instruction &instr);
    void jump_indirect(EmotionEngine& ee, IR::Instruction& instr);
    void fallback_interpreter(EmotionEngine& ee, const IR::Instruction &instr);

    void prepare_abi(const EmotionEngine& ee, uint64_t value);
    void call_abi_func(uint64_t addr);
    void recompile_block(EmotionEngine& ee, IR::Block& block);

    int search_for_register(AllocReg *regs, int vu_reg);
    REG_64 alloc_int_reg(EmotionEngine& ee, int vi_reg, REG_STATE state);

    void emit_prologue();
    void emit_instruction(EmotionEngine &ee, IR::Instruction &instr);
    uint64_t get_fpu_addr(EmotionEngine &ee, FPU_SpecialReg index);

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
