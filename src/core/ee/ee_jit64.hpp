#ifndef EE_JIT64_HPP
#define EE_JIT64_HPP
#include "../jitcommon/emitter64.hpp"
#include "../jitcommon/ir_block.hpp"
#include "ee_jittrans.hpp"
#include "emotion.hpp"
#include "vu.hpp"
#include <stack>
#include <cstddef>

enum class REG_TYPE;

struct AllocReg
{
    bool used = false;
    bool locked = false; //Prevent the register from being allocated
    bool modified = false;
    bool stored = false; // Register is stored on the stack
    int age = 0;
    int reg;
    REG_TYPE type;
    uint8_t needs_clamping;
};

enum class REG_TYPE_X86
{
    INT,
    XMM
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
alignas(16) static uint32_t FPU_MASK_ABS[4] = { 0x7FFFFFFF, 0x7FFFFFFF, 0x7FFFFFFF, 0x7FFFFFFF };
alignas(16) static uint32_t FPU_MASK_NEG[4] = { 0x80000000, 0x80000000, 0x80000000, 0x80000000 };
alignas(16) static uint32_t max_flt_constant[4] = { 0x7F7FFFFF, 0x7F7FFFFF, 0x7F7FFFFF, 0x7F7FFFFF };
alignas(16) static uint32_t min_flt_constant[4] = { 0xFF7FFFFF, 0xFF7FFFFF, 0xFF7FFFFF, 0xFF7FFFFF };
alignas(16) static uint32_t PPACB_MASK[4] = { 0x00FF00FF, 0x00FF00FF, 0x00FF00FF, 0x00FF00FF };
alignas(16) static uint32_t PPACH_MASK[4] = { 0x0000FFFF, 0x0000FFFF, 0x0000FFFF, 0x0000FFFF };

extern "C" uint8_t* exec_block_ee(EE_JIT64& jit, EmotionEngine& ee);

typedef void (*EEJitPrologue)(EE_JIT64& jit, EmotionEngine& ee, EEJitBlockRecord** cache);

class EE_JIT64
{
private:
    AllocReg xmm_regs[16];
    AllocReg int_regs[16];
    JitBlock jit_block;
    EEJitHeap jit_heap;
    Emitter64 emitter;
    EE_JitTranslator ir;

    int sp_offset;
    std::vector<REG_64> saved_int_regs;
    std::vector<REG_64> saved_xmm_regs;
    int abi_int_count;
    int abi_xmm_count;

    bool ee_branch, likely_branch;
    uint32_t ee_branch_dest, ee_branch_fail_dest;
    uint32_t ee_branch_delay_dest, ee_branch_delay_fail_dest;
    uint16_t cycle_count;
    uint32_t saved_mxcsr;
    uint32_t ee_mxcsr;
    // Cycles added to the cycle count in the middle of the block, e.g. UpdateVU0
    uint64_t cycles_added;

    bool should_update_mac;

    //Pointer to the dispatcher prologue that begins execution of recompiled code
    EEJitPrologue prologue_block;

    void handle_branch_likely(EmotionEngine& ee, IR::Block& block);

    // Instructions
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
    void divide_unsigned_word(EmotionEngine& ee, IR::Instruction &instr, bool hi);
    void divide_word(EmotionEngine& ee, IR::Instruction &instr, bool hi);
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
    void floating_point_reciprocal_square_root(EmotionEngine& ee, IR::Instruction& instr);
    void jump(EmotionEngine& ee, IR::Instruction& instr);
    void jump_indirect(EmotionEngine& ee, IR::Instruction& instr);
    void load_byte(EmotionEngine& ee, IR::Instruction& instr);
    void load_byte_unsigned(EmotionEngine& ee, IR::Instruction& instr);
    void load_const(EmotionEngine& ee, IR::Instruction &instr);
    void load_doubleword(EmotionEngine& ee, IR::Instruction& instr);
    void load_doubleword_left(EmotionEngine& ee, IR::Instruction& instr);
    void load_doubleword_right(EmotionEngine& ee, IR::Instruction& instr);
    void load_halfword(EmotionEngine& ee, IR::Instruction& instr);
    void load_halfword_unsigned(EmotionEngine& ee, IR::Instruction& instr);
    void load_word(EmotionEngine& ee, IR::Instruction& instr);
    void load_word_coprocessor1(EmotionEngine& ee, IR::Instruction& instr);
    void load_word_left(EmotionEngine& ee, IR::Instruction& instr);
    void load_word_right(EmotionEngine& ee, IR::Instruction& instr);
    void load_word_unsigned(EmotionEngine& ee, IR::Instruction& instr);
    void load_quadword(EmotionEngine& ee, IR::Instruction& instr);
    void load_quadword_coprocessor2(EmotionEngine& ee, IR::Instruction& instr);
    void move_conditional_on_not_zero(EmotionEngine& ee, IR::Instruction& instr);
    void move_conditional_on_zero(EmotionEngine& ee, IR::Instruction& instr);
    void move_control_word_from_floating_point(EmotionEngine& ee, IR::Instruction& instr);
    void move_control_word_to_floating_point(EmotionEngine& ee, IR::Instruction& instr);
    void move_doubleword_reg(EmotionEngine& ee, IR::Instruction& instr);
    void move_from_coprocessor1(EmotionEngine& ee, IR::Instruction &instr);
    void move_to_coprocessor1(EmotionEngine& ee, IR::Instruction &instr);
    void move_word_reg(EmotionEngine& ee, IR::Instruction& instr);
    void move_quadword_reg(EmotionEngine& ee, IR::Instruction& instr);
    void move_xmm_reg(EmotionEngine& ee, IR::Instruction& instr);
    void multiply_add_unsigned_word(EmotionEngine& ee, IR::Instruction& instr, bool hi);
    void multiply_add_word(EmotionEngine& ee, IR::Instruction& instr, bool hi);
    void multiply_unsigned_word(EmotionEngine& ee, IR::Instruction& instr, bool hi);
    void multiply_word(EmotionEngine& ee, IR::Instruction& instr, bool hi);
    void negate_doubleword_reg(EmotionEngine& ee, IR::Instruction &instr);
    void negate_word_reg(EmotionEngine& ee, IR::Instruction &instr);
    void nor_reg(EmotionEngine& ee, IR::Instruction& instr);
    void or_imm(EmotionEngine& ee, IR::Instruction& instr);
    void or_reg(EmotionEngine& ee, IR::Instruction& instr);
    void parallel_absolute_halfword(EmotionEngine& ee, IR::Instruction& instr);
    void parallel_absolute_word(EmotionEngine& ee, IR::Instruction& instr);
    void parallel_and(EmotionEngine& ee, IR::Instruction& instr);
    void parallel_nor(EmotionEngine& ee, IR::Instruction& instr);
    void parallel_or(EmotionEngine& ee, IR::Instruction& instr);
    void parallel_xor(EmotionEngine& ee, IR::Instruction& instr);
    void parallel_add_byte(EmotionEngine& ee, IR::Instruction& instr);
    void parallel_add_halfword(EmotionEngine& ee, IR::Instruction& instr);
    void parallel_add_word(EmotionEngine& ee, IR::Instruction& instr);
    void parallel_add_with_signed_saturation_byte(EmotionEngine& ee, IR::Instruction& instr);
    void parallel_add_with_signed_saturation_halfword(EmotionEngine& ee, IR::Instruction& instr);
    void parallel_add_with_signed_saturation_word(EmotionEngine& ee, IR::Instruction& instr);
    void parallel_add_with_unsigned_saturation_byte(EmotionEngine& ee, IR::Instruction& instr);
    void parallel_add_with_unsigned_saturation_halfword(EmotionEngine& ee, IR::Instruction& instr);
    void parallel_add_with_unsigned_saturation_word(EmotionEngine& ee, IR::Instruction& instr);
    void parallel_compare_equal_byte(EmotionEngine& ee, IR::Instruction& instr);
    void parallel_compare_equal_halfword(EmotionEngine& ee, IR::Instruction& instr);
    void parallel_compare_equal_word(EmotionEngine& ee, IR::Instruction& instr);
    void parallel_compare_greater_than_byte(EmotionEngine& ee, IR::Instruction& instr);
    void parallel_compare_greater_than_halfword(EmotionEngine& ee, IR::Instruction& instr);
    void parallel_compare_greater_than_word(EmotionEngine& ee, IR::Instruction& instr);
    void parallel_divide_word(EmotionEngine& ee, IR::Instruction& instr);
    void parallel_exchange_halfword(EmotionEngine& ee, IR::Instruction& instr, bool even);
    void parallel_exchange_word(EmotionEngine& ee, IR::Instruction& instr, bool even);
    void parallel_pack_to_byte(EmotionEngine& ee, IR::Instruction& instr);
    void parallel_maximize_halfword(EmotionEngine& ee, IR::Instruction& instr);
    void parallel_maximize_word(EmotionEngine& ee, IR::Instruction& instr);
    void parallel_minimize_halfword(EmotionEngine& ee, IR::Instruction& instr);
    void parallel_minimize_word(EmotionEngine& ee, IR::Instruction& instr);
    void parallel_pack_to_halfword(EmotionEngine& ee, IR::Instruction& instr);
    void parallel_pack_to_word(EmotionEngine& ee, IR::Instruction& instr);
    void parallel_shift_left_logical_halfword(EmotionEngine& ee, IR::Instruction& instr);
    void parallel_shift_left_logical_word(EmotionEngine& ee, IR::Instruction& instr);
    void parallel_shift_right_arithmetic_halfword(EmotionEngine& ee, IR::Instruction& instr);
    void parallel_shift_right_arithmetic_word(EmotionEngine& ee, IR::Instruction& instr);
    void parallel_shift_right_logical_halfword(EmotionEngine& ee, IR::Instruction& instr);
    void parallel_shift_right_logical_word(EmotionEngine& ee, IR::Instruction& instr);
    void parallel_subtract_byte(EmotionEngine& ee, IR::Instruction& instr);
    void parallel_subtract_halfword(EmotionEngine& ee, IR::Instruction& instr);
    void parallel_subtract_word(EmotionEngine& ee, IR::Instruction& instr);
    void parallel_subtract_with_signed_saturation_byte(EmotionEngine& ee, IR::Instruction& instr);
    void parallel_subtract_with_signed_saturation_halfword(EmotionEngine& ee, IR::Instruction& instr);
    void parallel_subtract_with_signed_saturation_word(EmotionEngine& ee, IR::Instruction& instr);
    void parallel_subtract_with_unsigned_saturation_byte(EmotionEngine& ee, IR::Instruction& instr);
    void parallel_subtract_with_unsigned_saturation_halfword(EmotionEngine& ee, IR::Instruction& instr);
    void parallel_subtract_with_unsigned_saturation_word(EmotionEngine& ee, IR::Instruction& instr);
    void parallel_reverse_halfword(EmotionEngine& ee, IR::Instruction& instr);
    void parallel_rotate_3_words_left(EmotionEngine& ee, IR::Instruction& instr);
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
    void store_doubleword_left(EmotionEngine& ee, IR::Instruction& instr);
    void store_doubleword_right(EmotionEngine& ee, IR::Instruction& instr);
    void store_halfword(EmotionEngine& ee, IR::Instruction& instr);
    void store_word(EmotionEngine& ee, IR::Instruction& instr);
    void store_word_coprocessor1(EmotionEngine& ee, IR::Instruction& instr);
    void store_word_left(EmotionEngine& ee, IR::Instruction& instr);
    void store_word_right(EmotionEngine& ee, IR::Instruction& instr);
    void store_quadword(EmotionEngine& ee, IR::Instruction& instr);
    void store_quadword_coprocessor2(EmotionEngine& ee, IR::Instruction& instr);
    void sub_doubleword_reg(EmotionEngine& ee, IR::Instruction& instr);
    void sub_word_reg(EmotionEngine& ee, IR::Instruction& instr);
    void system_call(EmotionEngine& ee, IR::Instruction& instr);
    void vabs(EmotionEngine& ee, IR::Instruction& instr);
    void vadd_vectors(EmotionEngine& ee, IR::Instruction& instr);
    void vcall_ms(EmotionEngine& ee, IR::Instruction& instr);
    void vcall_msr(EmotionEngine& ee, IR::Instruction& instr);
    void vmul_vectors(EmotionEngine& ee, IR::Instruction& instr);
    void vsub_vectors(EmotionEngine& ee, IR::Instruction& instr);
    void xor_imm(EmotionEngine& ee, IR::Instruction& instr);
    void xor_reg(EmotionEngine& ee, IR::Instruction& instr);
    void fallback_interpreter(EmotionEngine& ee, const IR::Instruction &instr);

    // COP1 helper functions
    void clamp_freg(REG_64 freg);

    // COP2 helper functions
    uint8_t convert_field(uint8_t value);
    void clamp_vfreg(EmotionEngine& ee, uint8_t field, REG_64 vfreg);
    bool needs_clamping(int reg, uint8_t field);
    void set_clamping(int reg, bool value, uint8_t field);
    void update_mac_flags(EmotionEngine& ee, REG_64 reg, uint8_t field);
    void check_interlock_vu0(EmotionEngine& ee, IR::Instruction& instr);
    void update_vu0(EmotionEngine& ee, IR::Instruction& instr);
    void wait_for_vu0(EmotionEngine& ee, IR::Instruction& instr);

    // ABI prep/function call
    void prepare_abi(uint64_t value);
    void prepare_abi_xmm(float value);
    void prepare_abi_reg(REG_64 reg, uint32_t offset = 0);
    void prepare_abi_reg_from_xmm(REG_64 reg);
    void prepare_abi_xmm_reg(REG_64 reg);
    void call_abi_func(uint64_t addr);
    void restore_int_regs(const std::vector<REG_64>& regs, bool restore_values = true);
    void restore_xmm_regs(const std::vector<REG_64>& regs, bool restore_values = true);

    // Register alloc
    int search_for_register_priority(AllocReg *regs);
    int search_for_register_scratchpad(AllocReg *regs);
    int search_for_register_xmm(AllocReg *regs);
    REG_64 alloc_reg(EmotionEngine& ee, int reg, REG_TYPE type, REG_STATE state, REG_64 destination = (REG_64)-1);
    REG_64 lalloc_int_reg(EmotionEngine& ee, int reg, REG_TYPE type, REG_STATE state, REG_64 destination = (REG_64)-1);
    REG_64 lalloc_xmm_reg(EmotionEngine& ee, int reg, REG_TYPE type, REG_STATE state, REG_64 destination = (REG_64)-1);

    // Address lookup
    uint64_t get_gpr_addr(const EmotionEngine &ee, int index) const;
    uint64_t get_gpr_offset(int index) const;
    uint64_t get_vi_addr(const EmotionEngine &ee, int index) const;
    uint64_t get_fpu_addr(const EmotionEngine &ee, int index) const;
    uint64_t get_vf_addr(const EmotionEngine &ee, int index) const;

    // Flush/free registers
    void flush_int_reg(EmotionEngine& ee, int reg);
    void flush_xmm_reg(EmotionEngine& ee, int reg);
    void flush_regs(EmotionEngine& ee);
    void free_int_reg(EmotionEngine& ee, REG_64 reg);
    void free_xmm_reg(EmotionEngine& ee, REG_64 reg);

    // Recompile + Cleanup
    EEJitPrologue create_prologue_block();
    void emit_prologue();
    void emit_dispatcher();
    void emit_instruction(EmotionEngine &ee, IR::Instruction &instr);
    EEJitBlockRecord* recompile_block(EmotionEngine& ee, IR::Block& block);
    void cleanup_recompiler(EmotionEngine& ee, bool clear_regs, bool dispatcher, uint64_t cycles);
    void emit_epilogue();
public:
    EE_JIT64();

    void reset(bool clear_cache = true);
    uint16_t run(EmotionEngine& ee);

    friend uint8_t* exec_block_ee(EE_JIT64& jit, EmotionEngine& ee);
};

// Various wrapper functions
uint8_t ee_read8(EmotionEngine& ee, uint32_t addr);
uint16_t ee_read16(EmotionEngine& ee, uint32_t addr);
uint32_t ee_read32(EmotionEngine& ee, uint32_t addr);
uint64_t ee_read64(EmotionEngine& ee, uint32_t addr);
void ee_read128(EmotionEngine& ee, uint32_t addr, uint128_t& dest);
void ee_write8(EmotionEngine& ee, uint32_t addr, uint8_t value);
void ee_write16(EmotionEngine& ee, uint32_t addr, uint16_t value);
void ee_write32(EmotionEngine& ee, uint32_t addr, uint32_t value);
void ee_write64(EmotionEngine& ee, uint32_t addr, uint64_t value);
void ee_write128(EmotionEngine& ee, uint32_t addr, uint128_t& value);
void ee_syscall_exception(EmotionEngine& ee);
void vu0_start_program(VectorUnit& vu0, uint32_t addr);
uint32_t vu0_read_CMSAR0_shl3(VectorUnit& vu0);
bool ee_vu0_wait(EmotionEngine& ee);
bool ee_check_interlock(EmotionEngine& ee);
void ee_clear_interlock(EmotionEngine& ee);

#endif // EE_JIT64_HPP
