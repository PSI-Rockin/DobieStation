#ifndef EE_JIT64_HPP
#define EE_JIT64_HPP
#include "../jitcommon/emitter64.hpp"
#include "../jitcommon/ir_block.hpp"
#include "ee_jittrans.hpp"
#include "emotion.hpp"
#include "vu.hpp"

enum class REG_TYPE;

struct AllocReg
{
    bool used = false;
    bool locked = false; //Prevent the register from being allocated
    bool modified = false;
    int age = 0;
    int reg;
    REG_TYPE type;
    uint8_t needs_clamping;
};

enum class REG_TYPE
{
    GPR,
    VI,
    INTSCRATCHPAD,
    GPREXTENDED,
    FPU,
    VF,
    XMMSCRATCHPAD
};

enum class REG_STATE
{
    SCRATCHPAD,
    READ,
    WRITE,
    READ_WRITE
};

class EE_JIT64;

// FPU bitmasks
// I've specifically made these *not* const because accessing them in the debug
// build after marking them const throws an access violation.
// You'll have to ask the guys who made the C++ standard why that happens,
// but please don't touch! - Souzooka
static uint32_t FPU_MASK_ABS[4] = { 0x7FFFFFFF, 0x7FFFFFFF, 0x7FFFFFFF, 0x7FFFFFFF };
static uint32_t FPU_MASK_NEG[4] = { 0x80000000, 0x80000000, 0x80000000, 0x80000000 };

extern "C" uint8_t* exec_block_ee(EE_JIT64& jit, EmotionEngine& ee);

class EE_JIT64
{
private:
    AllocReg xmm_regs[16];
    AllocReg int_regs[16];
    JitCache cache;
    Emitter64 emitter;
    EE_JitTranslator ir;

    //Set to 0x7FFFFFFF, repeated four times
    VU_GPR abs_constant;
    VU_GPR max_flt_constant, min_flt_constant;
    VU_GPR ftoi_table[4], itof_table[4];
    bool should_update_mac;

    int abi_int_count;
    int abi_xmm_count;

    bool ee_branch, likely_branch;
    uint32_t ee_branch_dest, ee_branch_fail_dest;
    uint32_t ee_branch_delay_dest, ee_branch_delay_fail_dest;
    uint16_t cycle_count;

    void handle_branch_likely(EmotionEngine& ee, IR::Block& block);
    void clamp_freg(EmotionEngine& ee, REG_64 freg);

    bool needs_clamping(int xmmreg, uint8_t field);
    void set_clamping(int xmmreg, bool value, uint8_t field);
    void clamp_vfreg(EmotionEngine& ee, uint8_t field, REG_64 xmm_reg);
    void update_mac_flags(EmotionEngine &ee, REG_64 xmm_reg, uint8_t field);
    void add_vector_by_scalar(EmotionEngine& ee, IR::Instruction& instr);

    void add_doubleword_imm(EmotionEngine& ee, IR::Instruction& instr);
    void add_doubleword_reg(EmotionEngine& ee, IR::Instruction& instr);
    void add_word_imm(EmotionEngine& ee, IR::Instruction& instr);
    void add_word_reg(EmotionEngine& ee, IR::Instruction& instr);
    void and_imm(EmotionEngine& ee, IR::Instruction& instr);
    void and_reg(EmotionEngine& ee, IR::Instruction& instr);
    void branch_cop0(EmotionEngine& ee, IR::Instruction& instr);
    void branch_cop1(EmotionEngine& ee, IR::Instruction& instr);
    void branch_cop2(EmotionEngine& ee, IR::Instruction& instr);
    void branch_equal(EmotionEngine& ee, IR::Instruction &instr);
    void branch_equal_zero(EmotionEngine& ee, IR::Instruction &instr);
    void branch_greater_than_or_equal_zero(EmotionEngine& ee, IR::Instruction &instr);
    void branch_greater_than_zero(EmotionEngine& ee, IR::Instruction &instr);
    void branch_less_than_or_equal_zero(EmotionEngine& ee, IR::Instruction &instr);
    void branch_less_than_zero(EmotionEngine& ee, IR::Instruction &instr);
    void branch_not_equal(EmotionEngine& ee, IR::Instruction &instr);
    void branch_not_equal_zero(EmotionEngine& ee, IR::Instruction &instr);
    void clear_doubleword_reg(EmotionEngine& ee, IR::Instruction& instr);
    void clear_word_reg(EmotionEngine& ee, IR::Instruction& instr);
    void divide_unsigned_word(EmotionEngine& ee, IR::Instruction &instr);
    void divide_word(EmotionEngine& ee, IR::Instruction &instr);
    void doubleword_shift_left_logical(EmotionEngine& ee, IR::Instruction& instr);
    void doubleword_shift_left_logical_variable(EmotionEngine& ee, IR::Instruction& instr);
    void doubleword_shift_right_arithmetic(EmotionEngine& ee, IR::Instruction& instr);
    void doubleword_shift_right_arithmetic_variable(EmotionEngine& ee, IR::Instruction& instr);
    void doubleword_shift_right_logical(EmotionEngine& ee, IR::Instruction& instr);
    void doubleword_shift_right_logical_variable(EmotionEngine& ee, IR::Instruction& instr);
    void exception_return(EmotionEngine& ee, IR::Instruction& instr);
    void fixed_point_convert_to_floating_point(EmotionEngine& ee, IR::Instruction& instr);
    void floating_point_absolute_value(EmotionEngine& ee, IR::Instruction& instr);
    void floating_point_add(EmotionEngine& ee, IR::Instruction& instr);
    void floating_point_clear_control(EmotionEngine& ee, IR::Instruction& instr);
    void floating_point_compare_equal(EmotionEngine& ee, IR::Instruction& instr);
    void floating_point_compare_less_than(EmotionEngine& ee, IR::Instruction& instr);
    void floating_point_compare_less_than_or_equal(EmotionEngine& ee, IR::Instruction& instr);
    void floating_point_convert_to_fixed_point(EmotionEngine& ee, IR::Instruction& instr);
    void floating_point_divide(EmotionEngine& ee, IR::Instruction& instr);
    void floating_point_negate(EmotionEngine& ee, IR::Instruction& instr);
    void floating_point_maximum(EmotionEngine& ee, IR::Instruction& instr);
    void floating_point_maximum_AVX(EmotionEngine& ee, IR::Instruction& instr);
    void floating_point_minimum(EmotionEngine& ee, IR::Instruction& instr);
    void floating_point_minimum_AVX(EmotionEngine& ee, IR::Instruction& instr);
    void floating_point_multiply(EmotionEngine& ee, IR::Instruction& instr);
    void floating_point_multiply_add(EmotionEngine& ee, IR::Instruction& instr);
    void floating_point_multiply_subtract(EmotionEngine& ee, IR::Instruction& instr);
    void floating_point_square_root(EmotionEngine& ee, IR::Instruction& instr);
    void floating_point_subtract(EmotionEngine& ee, IR::Instruction& instr);
    void jump(EmotionEngine& ee, IR::Instruction& instr);
    void jump_indirect(EmotionEngine& ee, IR::Instruction& instr);
    void load_byte(EmotionEngine& ee, IR::Instruction& instr);
    void load_byte_unsigned(EmotionEngine& ee, IR::Instruction& instr);
    void load_const(EmotionEngine& ee, IR::Instruction &instr);
    void load_doubleword(EmotionEngine& ee, IR::Instruction& instr);
    void load_halfword(EmotionEngine& ee, IR::Instruction& instr);
    void load_halfword_unsigned(EmotionEngine& ee, IR::Instruction& instr);
    void load_word(EmotionEngine& ee, IR::Instruction& instr);
    void load_word_coprocessor1(EmotionEngine& ee, IR::Instruction& instr);
    void load_word_unsigned(EmotionEngine& ee, IR::Instruction& instr);
    void move_conditional_on_not_zero(EmotionEngine& ee, IR::Instruction& instr);
    void move_conditional_on_zero(EmotionEngine& ee, IR::Instruction& instr);
    void move_doubleword_reg(EmotionEngine& ee, IR::Instruction& instr);
    void move_from_coprocessor1(EmotionEngine& ee, IR::Instruction &instr);
    void move_to_coprocessor1(EmotionEngine& ee, IR::Instruction &instr);
    void move_to_lo_hi(EmotionEngine& ee, IR::Instruction &instr);
    void move_to_lo_hi(EmotionEngine& ee, REG_64 loSource, REG_64 hiSource, REG_64 loDest, REG_64 hiDest);
    void move_to_lo_hi_imm(EmotionEngine& ee, int64_t loValue, int64_t hiValue);
    void move_word_reg(EmotionEngine& ee, IR::Instruction& instr);
    void move_xmm_reg(EmotionEngine& ee, IR::Instruction& instr);
    void multiply_unsigned_word(EmotionEngine& ee, IR::Instruction& instr);
    void multiply_word(EmotionEngine& ee, IR::Instruction& instr);
    void negate_doubleword_reg(EmotionEngine& ee, IR::Instruction &instr);
    void negate_word_reg(EmotionEngine& ee, IR::Instruction &instr);
    void nor_reg(EmotionEngine& ee, IR::Instruction& instr);
    void or_imm(EmotionEngine& ee, IR::Instruction& instr);
    void or_reg(EmotionEngine& ee, IR::Instruction& instr);
    void set_on_less_than(EmotionEngine& ee, IR::Instruction& instr);
    void set_on_less_than_unsigned(EmotionEngine& ee, IR::Instruction& instr);
    void set_on_less_than_immediate(EmotionEngine& ee, IR::Instruction& instr);
    void set_on_less_than_immediate_unsigned(EmotionEngine& ee, IR::Instruction& instr);
    void shift_left_logical(EmotionEngine& ee, IR::Instruction& instr);
    void shift_left_logical_variable(EmotionEngine& ee, IR::Instruction& instr);
    void shift_right_arithmetic(EmotionEngine& ee, IR::Instruction& instr);
    void shift_right_arithmetic_variable(EmotionEngine& ee, IR::Instruction& instr);
    void shift_right_logical(EmotionEngine& ee, IR::Instruction& instr);
    void shift_right_logical_variable(EmotionEngine& ee, IR::Instruction& instr);
    void store_byte(EmotionEngine& ee, IR::Instruction& instr);
    void store_doubleword(EmotionEngine& ee, IR::Instruction& instr);
    void store_halfword(EmotionEngine& ee, IR::Instruction& instr);
    void store_word(EmotionEngine& ee, IR::Instruction& instr);
    void store_word_coprocessor1(EmotionEngine& ee, IR::Instruction& instr);
    void sub_doubleword_reg(EmotionEngine& ee, IR::Instruction& instr);
    void sub_word_reg(EmotionEngine& ee, IR::Instruction& instr);
    void system_call(EmotionEngine& ee, IR::Instruction& instr);
    void vcall_ms(EmotionEngine& ee, IR::Instruction& instr);
    void vcall_msr(EmotionEngine& ee, IR::Instruction& instr);
    void xor_imm(EmotionEngine& ee, IR::Instruction& instr);
    void xor_reg(EmotionEngine& ee, IR::Instruction& instr);
    void fallback_interpreter(EmotionEngine& ee, const IR::Instruction &instr);

    static void ee_syscall_exception(EmotionEngine& ee);

    void alloc_abi_regs(int count);
    void prepare_abi(EmotionEngine& ee, uint64_t value);
    void prepare_abi_xmm(EmotionEngine& ee, float value);
    void prepare_abi_reg(EmotionEngine& ee, REG_64 reg);
    void prepare_abi_reg_from_xmm(EmotionEngine& ee, REG_64 reg);
    void prepare_abi_xmm_reg(EmotionEngine& ee, REG_64 reg);
    void call_abi_func(EmotionEngine& ee, uint64_t addr);
    void recompile_block(EmotionEngine& ee, IR::Block& block);

    int search_for_register_priority(AllocReg *regs);
    int search_for_register_scratchpad(AllocReg *regs);
    int search_for_register_xmm(AllocReg *regs);
    REG_64 alloc_reg(EmotionEngine& ee, int reg, REG_TYPE type, REG_STATE state, REG_64 destination = (REG_64)-1);
    REG_64 lalloc_int_reg(EmotionEngine& ee, int reg, REG_TYPE type, REG_STATE state, REG_64 destination = (REG_64)-1);
    REG_64 lalloc_xmm_reg(EmotionEngine& ee, int reg, REG_TYPE type, REG_STATE state, REG_64 destination = (REG_64)-1);
    void free_int_reg(EmotionEngine& ee, REG_64 reg);
    void free_xmm_reg(EmotionEngine& ee, REG_64 reg);

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
