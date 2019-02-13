#ifndef VU_JIT64_HPP
#define VU_JIT64_HPP
#include "../jitcommon/emitter64.hpp"
#include "../jitcommon/ir_block.hpp"
#include "vu_jittrans.hpp"
#include "vu.hpp"

struct AllocReg
{
    bool used;
    bool locked; //Prevent the register from being allocated
    bool modified;
    int age;
    int vu_reg;
};

struct alignas(16) FtoiTable
{
    float t[4][4];
};

enum class REG_STATE
{
    SCRATCHPAD,
    READ,
    WRITE,
    READ_WRITE
};

class VU_JIT64
{
    private:
        AllocReg xmm_regs[16];
        AllocReg int_regs[16];
        JitCache cache;
        Emitter64 emitter;
        VU_JitTranslator ir;

        //Set to 0x7FFFFFFF, repeated four times
        VU_GPR abs_constant;

        VU_GPR max_flt_constant, min_flt_constant;

        VU_GPR ftoi_table[4], itof_table[4];

        int abi_int_count;
        int abi_xmm_count;

        bool should_update_mac;

        bool cond_branch;
        uint16_t cond_branch_dest, cond_branch_fail_dest;
        uint16_t cycle_count;

        void clamp_result(REG_64 xmm_reg);
        void sse_abs(REG_64 source, REG_64 dest);
        void sse_div_check(REG_64 num, REG_64 denom, VU_R& dest);

        void handle_cond_branch(VectorUnit& vu);
        void update_mac_flags(VectorUnit& vu, REG_64 xmm_reg, uint8_t field);

        uint64_t get_vf_addr(VectorUnit& vu, int index);

        void load_const(VectorUnit& vu, IR::Instruction& instr);
        void load_float_const(VectorUnit& vu, IR::Instruction& instr);

        void load_int(VectorUnit& vu, IR::Instruction& instr);
        void store_int(VectorUnit& vu, IR::Instruction& instr);
        void load_quad(VectorUnit& vu, IR::Instruction& instr);
        void store_quad(VectorUnit& vu, IR::Instruction& instr);
        void load_quad_inc(VectorUnit& vu, IR::Instruction& instr);
        void store_quad_inc(VectorUnit& vu, IR::Instruction& instr);
        void load_quad_dec(VectorUnit& vu, IR::Instruction& instr);
        void store_quad_dec(VectorUnit& vu, IR::Instruction& instr);

        void move_int_reg(VectorUnit& vu, IR::Instruction& instr);

        void jump(VectorUnit& vu, IR::Instruction& instr);
        void jump_and_link(VectorUnit& vu, IR::Instruction& instr);
        void jump_indirect(VectorUnit& vu, IR::Instruction& instr);
        void jump_and_link_indirect(VectorUnit& vu, IR::Instruction& instr);

        void branch_equal(VectorUnit& vu, IR::Instruction& instr);
        void branch_not_equal(VectorUnit& vu, IR::Instruction& instr);
        void branch_less_than_zero(VectorUnit& vu, IR::Instruction& instr);
        void branch_greater_than_zero(VectorUnit& vu, IR::Instruction& instr);
        void branch_less_or_equal_than_zero(VectorUnit& vu, IR::Instruction& instr);
        void branch_greater_or_equal_than_zero(VectorUnit& vu, IR::Instruction& instr);

        void and_int(VectorUnit& vu, IR::Instruction& instr);
        void or_int(VectorUnit& vu, IR::Instruction& instr);
        void add_int_reg(VectorUnit& vu, IR::Instruction& instr);
        void sub_int_reg(VectorUnit& vu, IR::Instruction& instr);
        void add_unsigned_imm(VectorUnit& vu, IR::Instruction& instr);
        void sub_unsigned_imm(VectorUnit& vu, IR::Instruction& instr);

        void abs(VectorUnit& vu, IR::Instruction& instr);
        void max_vector_by_scalar(VectorUnit& vu, IR::Instruction& instr);
        void max_vectors(VectorUnit& vu, IR::Instruction& instr);
        void min_vector_by_scalar(VectorUnit& vu, IR::Instruction& instr);
        void min_vectors(VectorUnit& vu, IR::Instruction& instr);

        void add_vectors(VectorUnit& vu, IR::Instruction& instr);
        void add_vector_by_scalar(VectorUnit& vu, IR::Instruction& instr);
        void sub_vectors(VectorUnit& vu, IR::Instruction& instr);
        void sub_vector_by_scalar(VectorUnit& vu, IR::Instruction& instr);
        void mul_vectors(VectorUnit& vu, IR::Instruction& instr);
        void mul_vector_by_scalar(VectorUnit& vu, IR::Instruction& instr);
        void madd_vectors(VectorUnit& vu, IR::Instruction& instr);
        void madd_acc_and_vectors(VectorUnit& vu, IR::Instruction& instr);
        void madd_vector_by_scalar(VectorUnit& vu, IR::Instruction& instr);
        void madd_acc_by_scalar(VectorUnit& vu, IR::Instruction& instr);
        void msub_vector_by_scalar(VectorUnit& vu, IR::Instruction& instr);
        void msub_acc_by_scalar(VectorUnit& vu, IR::Instruction& instr);
        void opmula(VectorUnit& vu, IR::Instruction& instr);
        void opmsub(VectorUnit& vu, IR::Instruction& instr);

        void clip(VectorUnit& vu, IR::Instruction& instr);
        void div(VectorUnit& vu, IR::Instruction& instr);
        void rsqrt(VectorUnit& vu, IR::Instruction& instr);

        void fixed_to_float(VectorUnit& vu, IR::Instruction& instr, int table_entry);
        void float_to_fixed(VectorUnit& vu, IR::Instruction& instr, int table_entry);

        void move_to_int(VectorUnit& vu, IR::Instruction& instr);
        void move_from_int(VectorUnit& vu, IR::Instruction& instr);
        void move_float(VectorUnit& vu, IR::Instruction& instr);
        void move_rotated_float(VectorUnit& vu, IR::Instruction& instr);

        void mac_eq(VectorUnit& vu, IR::Instruction& instr);
        void mac_and(VectorUnit& vu, IR::Instruction& instr);
        void set_clip_flags(VectorUnit& vu, IR::Instruction& instr);
        void get_clip_flags(VectorUnit& vu, IR::Instruction& instr);
        void and_clip_flags(VectorUnit& vu, IR::Instruction& instr);
        void or_clip_flags(VectorUnit& vu, IR::Instruction& instr);
        void and_stat_flags(VectorUnit& vu, IR::Instruction& instr);

        void move_from_p(VectorUnit& vu, IR::Instruction& instr);

        void eleng(VectorUnit& vu, IR::Instruction& instr);
        void erleng(VectorUnit& vu, IR::Instruction& instr);
        void esqrt(VectorUnit& vu, IR::Instruction& instr);
        void ersqrt(VectorUnit& vu, IR::Instruction& instr);

        void start_q_event(VectorUnit& vu, IR::Instruction& instr);
        void check_q_pipeline(VectorUnit& vu, IR::Instruction& instr);
        void update_q(VectorUnit& vu, IR::Instruction& instr);
        void update_mac_pipeline(VectorUnit& vu);
        void move_xtop(VectorUnit& vu, IR::Instruction& instr);
        void move_xitop(VectorUnit& vu, IR::Instruction& instr);
        void xgkick(VectorUnit& vu, IR::Instruction& instr);
        void update_xgkick(VectorUnit& vu, IR::Instruction& instr);
        void stop(VectorUnit& vu, IR::Instruction& instr);

        int search_for_register(AllocReg* regs, int vu_reg);
        REG_64 alloc_int_reg(VectorUnit& vu, int vi_reg, REG_STATE state = REG_STATE::READ_WRITE);
        REG_64 alloc_sse_reg(VectorUnit& vu, int vf_reg, REG_STATE state = REG_STATE::READ_WRITE);
        REG_64 alloc_sse_scratchpad(VectorUnit& vu, int vf_reg);
        void flush_regs(VectorUnit& vu);
        void flush_sse_reg(VectorUnit& vu, int vf_reg);

        void emit_instruction(VectorUnit& vu, IR::Instruction& instr);
        void recompile_block(VectorUnit& vu, IR::Block& block);
        uint8_t* exec_block(VectorUnit& vu);
        void cleanup_recompiler(VectorUnit& vu, bool clear_regs);

        void prepare_abi(VectorUnit& vu, uint64_t value);
        void call_abi_func(uint64_t addr);
    public:
        VU_JIT64();

        void reset();
        uint16_t run(VectorUnit& vu);
};

#endif // VU_JIT64_HPP
