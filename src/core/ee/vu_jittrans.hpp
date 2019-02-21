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
    bool swap_ops;
    bool update_q_pipeline;
    bool update_mac_pipeline;
    bool has_mac_result;
    bool has_clip_result;
    int flag_instruction;
    bool advance_mac_pipeline;
    int stall_amount;
};

class VU_JitTranslator
{
    private:
        int q_pipeline_delay;

        int cycles_this_block;
        int cycles_since_xgkick_update;

        uint64_t stall_pipe[4];

        VU_InstrInfo instr_info[1024 * 16];
        uint16_t end_PC;
        bool has_q_stalled;

        int fdiv_pipe_cycles(uint32_t lower_instr);
        int is_flag_instruction(uint32_t lower_instr);
        bool updates_mac_flags(uint32_t upper_instr);
        bool updates_mac_flags_special(uint32_t upper_instr);

        void update_pipeline(VectorUnit &vu, int cycles);
        void analyze_FMAC_stalls(VectorUnit &vu, uint16_t PC);
        void interpreter_pass(VectorUnit& vu, uint8_t *instr_mem);
        void flag_pass(VectorUnit& vu);

        void fallback_interpreter(IR::Instruction& instr, uint32_t instr_word, bool is_upper);
        void update_xgkick(std::vector<IR::Instruction>& instrs);
        void check_q_stall(std::vector<IR::Instruction>& instrs);
        void start_q_event(std::vector<IR::Instruction>& instrs, int latency);

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
        IR::Block translate(VectorUnit& vu, uint8_t *instr_mem);
};

#endif // VU_JITTRANS_HPP
