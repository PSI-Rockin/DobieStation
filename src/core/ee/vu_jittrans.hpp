#ifndef VU_JITTRANS_HPP
#define VU_JITTRANS_HPP
#include <cstdint>
#include <vector>
#include "../jitcommon/ir_block.hpp"

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
    uint8_t decoder_vi_write_from_load;
    bool use_backup_vi;
};

class VU_JitTranslator
{
    private:
        bool trans_branch_delay_slot;
        bool trans_ebit_delay_slot;

        int cycles_this_block;
        int cycles_since_xgkick_update;

        uint64_t stall_pipe[4];

        VU_InstrInfo instr_info[1024 * 16];
        uint16_t end_PC;
        uint16_t cur_PC;

        int fdiv_pipe_cycles(uint32_t lower_instr);
        int efu_pipe_cycles(uint32_t lower_instr);
        int is_flag_instruction(uint32_t lower_instr);
        bool updates_mac_flags(uint32_t upper_instr);
        bool updates_mac_flags_special(uint32_t upper_instr);

        void update_pipeline(VectorUnit &vu, int cycles);
        void handle_vu_stalls(VectorUnit &vu, uint16_t PC, uint32_t lower, int &q_pipe_delay, int &p_pipe_delay);
        void analyze_FMAC_stalls(VectorUnit &vu, uint16_t PC);
        void interpreter_pass(VectorUnit& vu, uint8_t *instr_mem, uint32_t prev_pc);
        void flag_pass(VectorUnit& vu);

        void fallback_interpreter(IR::Instruction& instr, uint32_t instr_word, bool is_upper);
        void update_xgkick(std::vector<IR::Instruction>& instrs);

        void op_vectors(IR::Instruction& instr, uint32_t upper);
        void op_acc_and_vectors(IR::Instruction& instr, uint32_t upper);
        void op_vector_by_scalar(IR::Instruction& instr, uint32_t upper, VU_SpecialReg scalar = VU_Regular);
        void op_acc_by_scalar(IR::Instruction& instr, uint32_t upper, VU_SpecialReg scalar = VU_Regular);

        void op_conversion(IR::Instruction& instr, uint32_t upper);

        void translate_upper(std::vector<IR::Instruction>& instrs, uint32_t upper);
        void upper_special(std::vector<IR::Instruction>& instrs, uint32_t upper);

        void translate_lower(std::vector<IR::Instruction>& instrs, uint32_t lower, uint32_t PC);
        void lower1(std::vector<IR::Instruction>& instrs, uint32_t lower);
        void lower1_special(std::vector<IR::Instruction>& instrs, uint32_t lower);
        void lower2(std::vector<IR::Instruction>& instrs, uint32_t lower, uint32_t PC);
    public:
        IR::Block translate(VectorUnit& vu, uint8_t *instr_mem, uint32_t prev_pc);
        void reset_instr_info();
};

#endif // VU_JITTRANS_HPP
