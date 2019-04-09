#ifndef VU_JITTRANS_HPP
#define VU_JITTRANS_HPP
#include <cstdint>
//#include <vector>
#include "../jitcommon/ir_block.hpp"

class EmotionEngine;
class VectorUnit;

enum class VU_SpecialReg
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
    //TODO
};

class EE_JitTranslator
{
private:
    //TODO
    int cycles_this_block;

    EE_InstrInfo instr_info[1024 * 16];
    uint16_t end_PC;
    uint16_t cur_PC;

    bool is_branch(uint32_t instr_word) const noexcept;
    void interpreter_pass(EmotionEngine &ee, uint32_t pc);
    void fallback_interpreter(IR::Instruction& instr, uint32_t instr_word) const noexcept;

    void translate_op(std::vector<IR::Instruction>& instrs, uint32_t opcode, uint32_t pc) const;
    void translate_op_special(std::vector<IR::Instruction>& instrs, uint32_t opcode) const;
    void translate_op_regimm(std::vector<IR::Instruction>& instrs, uint32_t opcode) const;
    void translate_op_mmi(std::vector<IR::Instruction>& instrs, uint32_t opcode) const;
    void translate_op_mmi0(std::vector<IR::Instruction>& instrs, uint32_t opcode) const;
    void translate_op_mmi1(std::vector<IR::Instruction>& instrs, uint32_t opcode) const;
    void translate_op_mmi2(std::vector<IR::Instruction>& instrs, uint32_t opcode) const;
    void translate_op_mmi3(std::vector<IR::Instruction>& instrs, uint32_t opcode) const;
    void translate_op_cop0(std::vector<IR::Instruction>& instrs, uint32_t opcode) const;
    void translate_op_cop0_type2(std::vector<IR::Instruction>& instrs, uint32_t opcode) const;
    void translate_op_cop1(std::vector<IR::Instruction>& instrs, uint32_t opcode) const;
    void translate_op_cop1_fpu(std::vector<IR::Instruction>& instrs, uint32_t opcode) const;
    void translate_op_cop2(std::vector<IR::Instruction>& instrs, uint32_t opcode) const;
    void translate_op_cop2_special(std::vector<IR::Instruction>& instrs, uint32_t opcode) const;
    void translate_op_cop2_special2(std::vector<IR::Instruction>& instrs, uint32_t opcode) const;
public:
    IR::Block translate(EmotionEngine& ee);
};

#endif // EE_JITTRANS_HPP
