#ifndef EE_JIT64_HPP
#define EE_JIT64_HPP
#include "../jitcommon/emitter64.hpp"
#include "../jitcommon/ir_block.hpp"
#include "ee_jittrans.hpp"
#include "emotion.hpp"

struct AllocReg
{
    bool used = false;
    bool locked = false; //Prevent the register from being allocated
    bool modified = false;
    int age = 0;
    int reg;
    bool is_gpr_reg;
    bool is_fpu_reg;
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

    bool ee_branch, likely_branch;
    uint32_t ee_branch_dest, ee_branch_fail_dest;
    uint32_t ee_branch_delay_dest, ee_branch_delay_fail_dest;
    uint16_t cycle_count;

    void handle_branch_likely(EmotionEngine& ee, IR::Block& block);

    void and_int(EmotionEngine& ee, IR::Instruction& instr);
    void add_unsigned_imm(EmotionEngine& ee, IR::Instruction& instr);
    void branch_cop0(EmotionEngine& ee, IR::Instruction& instr);
    void branch_cop1(EmotionEngine& ee, IR::Instruction& instr);
    void branch_cop2(EmotionEngine& ee, IR::Instruction& instr);
    void branch_equal(EmotionEngine& ee, IR::Instruction &instr);
    void branch_greater_than_or_equal_zero(EmotionEngine& ee, IR::Instruction &instr);
    void branch_greater_than_zero(EmotionEngine& ee, IR::Instruction &instr);
    void branch_less_than_or_equal_zero(EmotionEngine& ee, IR::Instruction &instr);
    void branch_less_than_zero(EmotionEngine& ee, IR::Instruction &instr);
    void branch_not_equal(EmotionEngine& ee, IR::Instruction &instr);
    void doubleword_shift_left_logical(EmotionEngine& ee, IR::Instruction& instr);
    void doubleword_shift_left_logical_variable(EmotionEngine& ee, IR::Instruction& instr);
    void doubleword_shift_right_arithmetic(EmotionEngine& ee, IR::Instruction& instr);
    void doubleword_shift_right_arithmetic_variable(EmotionEngine& ee, IR::Instruction& instr);
    void doubleword_shift_right_logical(EmotionEngine& ee, IR::Instruction& instr);
    void doubleword_shift_right_logical_variable(EmotionEngine& ee, IR::Instruction& instr);
    void exception_return(EmotionEngine& ee, IR::Instruction& instr);
    void jump(EmotionEngine& ee, IR::Instruction& instr);
    void jump_indirect(EmotionEngine& ee, IR::Instruction& instr);
    void load_upper_immediate(EmotionEngine& ee, IR::Instruction &instr);
    void or_int(EmotionEngine& ee, IR::Instruction& instr);
    void set_on_less_than_immediate(EmotionEngine& ee, IR::Instruction& instr);
    void set_on_less_than_immediate_unsigned(EmotionEngine& ee, IR::Instruction& instr);
    void shift_left_logical(EmotionEngine& ee, IR::Instruction& instr);
    void shift_left_logical_variable(EmotionEngine& ee, IR::Instruction& instr);
    void shift_right_arithmetic(EmotionEngine& ee, IR::Instruction& instr);
    void shift_right_arithmetic_variable(EmotionEngine& ee, IR::Instruction& instr);
    void shift_right_logical(EmotionEngine& ee, IR::Instruction& instr);
    void shift_right_logical_variable(EmotionEngine& ee, IR::Instruction& instr);
    void system_call(EmotionEngine& ee, IR::Instruction& instr);
    void vcall_ms(EmotionEngine& ee, IR::Instruction& instr);
    void vcall_msr(EmotionEngine& ee, IR::Instruction& instr);
    void xor_int(EmotionEngine& ee, IR::Instruction& instr);
    void fallback_interpreter(EmotionEngine& ee, const IR::Instruction &instr);

    static void ee_syscall_exception(EmotionEngine& ee);

    void prepare_abi(EmotionEngine& ee, uint64_t value);
    void prepare_abi_reg(EmotionEngine& ee, REG_64 value);
    void call_abi_func(EmotionEngine& ee, uint64_t addr);
    void recompile_block(EmotionEngine& ee, IR::Block& block);

    int search_for_register(AllocReg *regs);
    REG_64 alloc_gpr_reg(EmotionEngine& ee, int gpr_reg, REG_STATE state);
    REG_64 alloc_gpr_reg(EmotionEngine& ee, int gpr_reg, REG_STATE state, REG_64 destination);
    REG_64 lalloc_gpr_reg(EmotionEngine& ee, int gpr_reg, REG_STATE state);
    REG_64 lalloc_gpr_reg(EmotionEngine& ee, int gpr_reg, REG_STATE state, REG_64 destination);
    void free_gpr_reg(EmotionEngine& ee, REG_64 reg);
    REG_64 alloc_vi_reg(EmotionEngine& ee, int vi_reg, REG_STATE state);
    REG_64 alloc_fpu_reg(EmotionEngine& ee, int fpu_reg, REG_STATE state);
    REG_64 alloc_vf_reg(EmotionEngine& ee, int vf_reg, REG_STATE state);

    void emit_prologue();
    void emit_instruction(EmotionEngine &ee, IR::Instruction &instr);

    uint64_t get_gpr_addr(const EmotionEngine &ee, int index) const;
    uint64_t get_vi_addr(const EmotionEngine &ee, int index) const;
    uint64_t get_fpu_addr(const EmotionEngine &ee, int index) const;
    uint64_t get_vf_addr(const EmotionEngine &ee, int index) const;
    
    void flush_int_reg(EmotionEngine& ee, int reg);
    void flush_xmm_reg(EmotionEngine& ee, int reg);
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