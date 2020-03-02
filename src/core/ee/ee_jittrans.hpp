#ifndef EE_JITTRANS_HPP
#define EE_JITTRANS_HPP
#include <cstdint>
#include <vector>
#include "../jitcommon/ir_block.hpp"
#include "emotioninterpreter.hpp"

class EmotionEngine;
class VectorUnit;

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

class EE_JitTranslator
{
private:
    //TODO
    int cycles_this_block;

    uint16_t end_PC;
    uint16_t cur_PC;
    bool cop2_encountered;
    bool eret_op;
    bool branch_op;
    bool branch_delayslot;
    int cycle_count;
    int di_delay;

    void interpreter_pass(EmotionEngine &ee, uint32_t pc);
    void get_block_operations(std::vector<EE_InstrInfo>& dest, EmotionEngine& cpu, uint32_t pc);

    void issue_cycle_analysis(std::vector<EE_InstrInfo>& instr_info);
    bool dual_issue_analysis(const EE_InstrInfo& instr1, const EE_InstrInfo& instr2);
    bool check_pipeline_combination(EE_InstrInfo::Pipeline pipeline1, EE_InstrInfo::Pipeline pipeline2);
    bool check_eret_combination(EE_InstrInfo::Pipeline pipeline1, EE_InstrInfo::Pipeline pipeline2);
    bool check_mmi_combination(EE_InstrInfo::Pipeline pipeline1, EE_InstrInfo::Pipeline pipeline2);
    void load_store_analysis(std::vector<EE_InstrInfo>& instr_info);
    void data_dependency_analysis(std::vector<EE_InstrInfo>& instr_info);

    void translate_op(uint32_t opcode, uint32_t pc, EE_InstrInfo& info, std::vector<IR::Instruction>& instrs);
    void translate_op_special(uint32_t opcode, uint32_t PC, EE_InstrInfo& info, std::vector<IR::Instruction>& instrs);
    void translate_op_regimm(uint32_t opcode, uint32_t PC, EE_InstrInfo& info, std::vector<IR::Instruction>& instrs) const;
    void translate_op_mmi(uint32_t opcode, uint32_t PC, EE_InstrInfo& info, std::vector<IR::Instruction>& instrs) const;
    void translate_op_mmi0(uint32_t opcode, uint32_t PC, EE_InstrInfo& info, std::vector<IR::Instruction>& instrs) const;
    void translate_op_mmi1(uint32_t opcode, uint32_t PC, EE_InstrInfo& info, std::vector<IR::Instruction>& instrs) const;
    void translate_op_mmi2(uint32_t opcode, uint32_t PC, EE_InstrInfo& info, std::vector<IR::Instruction>& instrs) const;
    void translate_op_mmi3(uint32_t opcode, uint32_t PC, EE_InstrInfo& info, std::vector<IR::Instruction>& instrs) const;
    void translate_op_cop0(uint32_t opcode, uint32_t PC, EE_InstrInfo& info, std::vector<IR::Instruction>& instrs);
    void translate_op_cop0_type2(uint32_t opcode, uint32_t PC, EE_InstrInfo& info, std::vector<IR::Instruction>& instrs);
    void translate_op_cop1(uint32_t opcode, uint32_t PC, EE_InstrInfo& info, std::vector<IR::Instruction>& instrs) const;
    void translate_op_cop1_fpu(uint32_t opcode, uint32_t PC, EE_InstrInfo& info, std::vector<IR::Instruction>& instrs) const;
    void translate_op_cop2(uint32_t opcode, uint32_t PC, EE_InstrInfo& info, std::vector<IR::Instruction>& instrs);
    void translate_op_cop2_special(uint32_t opcode, uint32_t PC, EE_InstrInfo& info, std::vector<IR::Instruction>& instrs);
    void translate_op_cop2_special2(uint32_t opcode, uint32_t PC, EE_InstrInfo& info, std::vector<IR::Instruction>& instrs) const;
    void check_interlock(uint32_t opcode, uint32_t PC, EE_InstrInfo& info, std::vector<IR::Instruction>& instrs, IR::Opcode op) const;
    void wait_vu0(uint32_t opcode, uint32_t PC, EE_InstrInfo& info, std::vector<IR::Instruction>& instrs) const;
    void fallback_interpreter(IR::Instruction& instr, uint32_t opcode, void(*interpreter_fn)(EmotionEngine&, uint32_t)) const;

    void op_vector_by_scalar(IR::Instruction &instr, uint32_t upper, VU_SpecialReg scalar = VU_Regular) const;
public:
    IR::Block translate(EmotionEngine& ee);
};

#endif // EE_JITTRANS_HPP
