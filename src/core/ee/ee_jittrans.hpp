#ifndef EE_JITTRANS_HPP
#define EE_JITTRANS_HPP
#include <cstdint>
//#include <vector>
#include "../jitcommon/ir_block.hpp"

class EmotionEngine;
class VectorUnit;

enum EE_NormalReg
{
    zero = 0,
    at = 1,
    v0 = 2, v1 = 3,
    a0 = 4, a1 = 5, a2 = 6, a3 = 7,
    t0 = 8, t1 = 9, t2 = 10, t3 = 11, t4 = 12, t5 = 13, t6 = 14, t7 = 15, t8 = 24, t9 = 25,
    s0 = 16, s1 = 17, s2 = 18, s3 = 19, s4 = 20, s5 = 21, s6 = 22, s7 = 23,
    k0 = 26, k1 = 27,
    gp = 28,
    sp = 29,
    fp = 30,
    ra = 31
};

enum class EE_SpecialReg
{
    EE_Regular = 0,
    LO = 32,
    LO1,
    HI,
    HI1,
    SA
};

enum VU_SpecialReg
{
    VU_Regular = 0,
    ACC = 32,
    I,
    Q,
    P,
    R
};

enum VU_FlagInstrType
{
    FlagInstr_None,
    FlagInstr_Mac,
    FlagInstr_Clip
};

struct VU_InstrInfo
{
    bool branch_delay_slot;
    bool ebit_delay_slot;
    bool is_ebit;
    bool is_branch;
    bool tbit_end;
    bool swap_ops;
    bool update_q_pipeline;
    bool update_p_pipeline;
    bool q_pipeline_instr;
    bool p_pipeline_instr;
    bool update_mac_pipeline;
    bool has_mac_result;
    bool has_clip_result;
    int flag_instruction;
    bool advance_mac_pipeline;
    int stall_amount;
    int backup_vi;
    int q_pipe_delay_int_pass;
    int q_pipe_delay_trans_pass;
    int p_pipe_delay_int_pass;
    int p_pipe_delay_trans_pass;
    uint64_t pipeline_state[2];
    uint64_t stall_state[4];
    uint8_t decoder_vf_write[2];
    uint8_t decoder_vf_write_field[2];
    uint8_t decoder_vi_read0;
    uint8_t decoder_vi_read1;
    uint8_t decoder_vi_write;
    bool use_backup_vi;
};

enum class FPU_SpecialReg
{
    FPU_Regular = 0,
    ACC = 32
};

struct EE_InstrInfo
{
    /**
     * The EE has a dual-issue pipeline, meaning that under ideal circumstances, it can execute two instructions
     * per cycle. The two instructions must have no dependencies with each other, exist in separate physical
     * pipelines, and cause no stalls for the two to be executed in a single cycle.
     *
     * There are six physical pipelines: two integer pipelines, load/store, branch, COP1, and COP2.
     * MMI instructions use both integer pipelines, so an MMI instruction and any other ALU instruction
     * can never both be issued in the same cycle.
     *
     * Because the EE is in-order, assuming no stalls, one can achieve optimal performance by pairing together
     * instructions using different physical pipelines.
     */
    enum class Pipeline
    {
        //Uninitialized. Error out if an instruction's pipeline is set to this.
        Unk,

        //Generic ALU instruction. Can be placed in either integer pipeline.
        IntGeneric,

        //ALU instruction that can only be used in integer pipeline 0.
        Int0,

        //... integer pipeline 1.
        Int1,

        //MMI instruction. Takes up both integer pipelines.
        IntWide,

        //Load/store instructions. This includes COP1 and COP2 load/store instructions.
        LoadStore,

        //Branches, jumps, and calls. This includes COP1 and COP2 branches.
        Branch,

        //COP1 arithmetic.
        COP1,

        //COP2 arithmetic.
        COP2
    };

    Pipeline pipeline;
};

class EE_JitTranslator
{
private:
    //TODO
    int cycles_this_block;

    std::vector<EE_InstrInfo> instr_info;
    uint16_t end_PC;
    uint16_t cur_PC;
    bool cop2_encountered;
    bool eret_op;
    bool branch_op;
    bool branch_delayslot;
    int cycle_count;
    int di_delay;

    void interpreter_pass(EmotionEngine &ee, uint32_t pc);
    void fallback_interpreter(IR::Instruction& instr, uint32_t instr_word) const noexcept;

    void translate_op(uint32_t opcode, uint32_t pc, std::vector<IR::Instruction>& instrs);
    void translate_op_special(uint32_t opcode, uint32_t PC, std::vector<IR::Instruction>& instrs);
    void translate_op_regimm(uint32_t opcode, uint32_t PC, std::vector<IR::Instruction>& instrs) const;
    void translate_op_mmi(uint32_t opcode, uint32_t PC, std::vector<IR::Instruction>& instrs) const;
    void translate_op_mmi0(uint32_t opcode, uint32_t PC, std::vector<IR::Instruction>& instrs) const;
    void translate_op_mmi1(uint32_t opcode, uint32_t PC, std::vector<IR::Instruction>& instrs) const;
    void translate_op_mmi2(uint32_t opcode, uint32_t PC, std::vector<IR::Instruction>& instrs) const;
    void translate_op_mmi3(uint32_t opcode, uint32_t PC, std::vector<IR::Instruction>& instrs) const;
    void translate_op_cop0(uint32_t opcode, uint32_t PC, std::vector<IR::Instruction>& instrs);
    void translate_op_cop0_type2(uint32_t opcode, uint32_t PC, std::vector<IR::Instruction>& instrs);
    void translate_op_cop1(uint32_t opcode, uint32_t PC, std::vector<IR::Instruction>& instrs) const;
    void translate_op_cop1_fpu(uint32_t opcode, uint32_t PC, std::vector<IR::Instruction>& instrs) const;
    void translate_op_cop2(uint32_t opcode, uint32_t PC, std::vector<IR::Instruction>& instrs);
    void translate_op_cop2_special(uint32_t opcode, uint32_t PC, std::vector<IR::Instruction>& instrs);
    void translate_op_cop2_special2(uint32_t opcode, uint32_t PC, std::vector<IR::Instruction>& instrs) const;
    void op_vector_by_scalar(IR::Instruction &instr, uint32_t upper, VU_SpecialReg scalar = VU_Regular) const;
public:
    IR::Block translate(EmotionEngine& ee);
};

#endif // EE_JITTRANS_HPP
