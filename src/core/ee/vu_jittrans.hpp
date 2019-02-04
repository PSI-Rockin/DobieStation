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

struct VU_InstrInfo
{
    bool swap_ops;
    bool update_q_pipeline;
    bool reads_mac_flags;
    bool reads_clip_flags;
    bool updates_mac_pipeline;
    bool updates_clip_pipeline;
    int stall_amount;
};

class VU_JitTranslator
{
    private:
        int q_pipeline_delay;

        int cycles_this_block;
        int cycles_since_xgkick_update;

        VU_InstrInfo instr_info[1024 * 16];
        uint16_t end_PC;
        bool has_q_stalled;

        bool updates_mac_flags(uint32_t upper_instr);
        bool updates_mac_flags_special(uint32_t upper_instr);

        void interpreter_pass(VectorUnit& vu, uint8_t *instr_mem);
        void flag_pass(VectorUnit& vu, uint8_t *instr_mem);

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
