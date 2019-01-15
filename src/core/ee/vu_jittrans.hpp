#ifndef VU_JITTRANS_HPP
#define VU_JITTRANS_HPP
#include <cstdint>
#include <vector>
#include "../jitcommon/ir_block.hpp"

enum VU_SpecialReg
{
    VU_Regular = 0,
    ACC = 32,
    I,
    Q,
    P,
    R
};

class VU_JitTranslator
{
    private:
        int q_pipeline_delay;

        void manage_pipelining();

        void op_vectors(IR::Instruction& instr, uint32_t upper);
        void op_acc_and_vectors(IR::Instruction& instr, uint32_t upper);
        void op_vector_by_scalar(IR::Instruction& instr, uint32_t upper, VU_SpecialReg scalar = VU_Regular);
        void op_acc_by_scalar(IR::Instruction& instr, uint32_t upper, VU_SpecialReg scalar = VU_Regular);

        void translate_upper(std::vector<IR::Instruction>& instrs, uint32_t upper);
        void upper_special(std::vector<IR::Instruction>& instrs, uint32_t upper);

        void translate_lower(std::vector<IR::Instruction>& instrs, uint32_t lower, uint32_t PC);
        void lower1(std::vector<IR::Instruction>& instrs, uint32_t lower);
        void lower1_special(std::vector<IR::Instruction>& instrs, uint32_t lower);
        void lower2(std::vector<IR::Instruction>& instrs, uint32_t lower, uint32_t PC);
    public:
        IR::Block translate(uint32_t PC, uint8_t *instr_mem);
};

#endif // VU_JITTRANS_HPP
