#ifndef EMITTER64_HPP
#define EMITTER64_HPP
#include "jitcache.hpp"

enum REG_64
{
    EAX = 0, RAX = 0, XMM0 = 0,
    ECX = 1, RCX = 1, XMM1 = 1,
    EDX = 2, RDX = 2, XMM2 = 2,
    EBX = 3, RBX = 3, XMM3 = 3,
    ESP = 4, RSP = 4, XMM4 = 4,
    EBP = 5, RBP = 5, XMM5 = 5,
    ESI = 6, RSI = 6, XMM6 = 6,
    EDI = 7, RDI = 7, XMM7 = 7,
    R8 = 8, XMM8 = 8,
    R9 = 9, XMM9 = 9,
    R10 = 10, XMM10 = 10,
    R11 = 11, XMM11 = 11,
    R12 = 12, XMM12 = 12,
    R13 = 13, XMM13 = 13,
    R14 = 14, XMM14 = 14,
    R15 = 15, XMM15 = 15
};

class Emitter64
{
    private:
        JitCache* cache;

        void rex_r(REG_64 reg);
        void rex_rm(REG_64 rm);
        void rex_r_rm(REG_64 reg, REG_64 rm);
        void rexw_r(REG_64 reg);
        void rexw_rm(REG_64 rm);
        void rexw_r_rm(REG_64 reg, REG_64 rm);
        void modrm(uint8_t mode, uint8_t reg, uint8_t rm);

        int get_rip_offset(uint64_t addr);
    public:
        Emitter64(JitCache* cache);

        void load_addr(uint64_t addr, REG_64 dest);

        void ADD16_REG_IMM(uint16_t imm, REG_64 dest);

        void SHL32_REG_IMM(uint8_t shift, REG_64 dest);

        void MOV16_REG(REG_64 source, REG_64 dest);
        void MOV16_REG_IMM(uint16_t imm, REG_64 dest);
        void MOV32_MI_MEM(uint32_t imm, REG_64 indir_dest);
        void MOV32_TO_MEM(REG_64 source, REG_64 indir_dest);
        void MOV64_MR(REG_64 source, REG_64 dest);
        void MOV64_OI(uint64_t imm, REG_64 dest);
        void MOV64_FROM_MEM(REG_64 indir_source, REG_64 dest);
        void MOV64_TO_MEM(REG_64 source, REG_64 indir_dest);

        void MOVAPS_FROM_MEM(REG_64 indir_source, REG_64 xmm_dest);
        void MOVAPS_TO_MEM(REG_64 xmm_source, REG_64 indir_dest);

        void PUSH(REG_64 reg);
        void POP(REG_64 reg);
        void CALL(uint64_t addr);
        void RET();
};

#endif // EMITTER64_HPP
