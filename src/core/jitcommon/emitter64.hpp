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

        void ADD16_REG(REG_64 source, REG_64 dest);
        void ADD16_REG_IMM(uint16_t imm, REG_64 dest);
        void ADD64_REG(REG_64 source, REG_64 dest);

        void INC16(REG_64 dest);

        void AND16_AX(uint16_t imm);
        void AND16_REG(REG_64 source, REG_64 dest);
        void AND32_EAX(uint32_t imm);
        void AND32_REG_IMM(uint32_t imm, REG_64 dest);

        void CMP16_IMM(uint16_t imm, REG_64 op);
        void CMP16_REG(REG_64 op2, REG_64 op1);
        void CMP32_EAX(uint32_t imm);

        void DEC16(REG_64 dest);

        void NOT16(REG_64 dest);

        void OR16_REG(REG_64 source, REG_64 dest);
        void OR32_REG(REG_64 source, REG_64 dest);
        void OR32_EAX(uint32_t imm);

        void SETE_REG(REG_64 dest);
        void SETE_MEM(REG_64 indir_dest);
        void SETG_MEM(REG_64 indir_dest);
        void SETGE_MEM(REG_64 indir_dest);
        void SETL_MEM(REG_64 indir_dest);
        void SETLE_MEM(REG_64 indir_dest);
        void SETNE_REG(REG_64 dest);
        void SETNE_MEM(REG_64 indir_dest);

        void SHL16_REG_1(REG_64 dest);
        void SHL16_REG_IMM(uint8_t shift, REG_64 dest);
        void SHL32_REG_IMM(uint8_t shift, REG_64 dest);

        void SUB16_REG_IMM(uint16_t imm, REG_64 dest);
        void SUB32_REG(REG_64 source, REG_64 dest);

        void TEST16_REG(REG_64 op2, REG_64 op1);
        void TEST32_EAX(uint32_t imm);

        void XOR16_REG(REG_64 source, REG_64 dest);
        void XOR32_REG(REG_64 source, REG_64 dest);

        void MOV8_TO_MEM(REG_64 source, REG_64 indir_dest);
        void MOV8_IMM_MEM(uint8_t imm, REG_64 indir_dest);
        void MOV16_REG(REG_64 source, REG_64 dest);
        void MOV16_REG_IMM(uint16_t imm, REG_64 dest);
        void MOV16_TO_MEM(REG_64 source, REG_64 indir_dest);
        void MOV16_FROM_MEM(REG_64 indir_source, REG_64 dest);
        void MOV16_IMM_MEM(uint16_t imm, REG_64 indir_dest);
        void MOV32_REG(REG_64 source, REG_64 dest);
        void MOV32_REG_IMM(uint32_t imm, REG_64 dest);
        void MOV32_IMM_MEM(uint32_t imm, REG_64 indir_dest);
        void MOV32_FROM_MEM(REG_64 indir_source, REG_64 dest);
        void MOV32_TO_MEM(REG_64 source, REG_64 indir_dest);
        void MOV64_MR(REG_64 source, REG_64 dest);
        void MOV64_OI(uint64_t imm, REG_64 dest);
        void MOV64_FROM_MEM(REG_64 indir_source, REG_64 dest);
        void MOV64_TO_MEM(REG_64 source, REG_64 indir_dest);

        void MOVSX64_REG(REG_64 source, REG_64 dest);
        void MOVZX64_REG(REG_64 source, REG_64 dest);

        void MOVD_FROM_XMM(REG_64 xmm_source, REG_64 dest);
        void MOVD_TO_XMM(REG_64 source, REG_64 xmm_dest);
        void MOVAPS_REG(REG_64 xmm_source, REG_64 xmm_dest);
        void MOVAPS_FROM_MEM(REG_64 indir_source, REG_64 xmm_dest);
        void MOVAPS_TO_MEM(REG_64 xmm_source, REG_64 indir_dest);
        void MOVMSKPS(REG_64 xmm_source, REG_64 dest);

        uint8_t* JMP_NEAR_DEFERRED();
        uint8_t* JE_NEAR_DEFERRED();
        uint8_t* JNE_NEAR_DEFERRED();

        void set_jump_dest(uint8_t* jump);

        void PUSH(REG_64 reg);
        void POP(REG_64 reg);
        void CALL(uint64_t addr);
        void RET();

        void PAND_XMM(REG_64 xmm_source, REG_64 xmm_dest);
        void PAND_XMM_MEM(REG_64 indir_source, REG_64 xmm_dest);
        void PMAXSD_XMM(REG_64 xmm_source, REG_64 xmm_dest);
        void PMINSD_XMM(REG_64 xmm_source, REG_64 xmm_dest);
        void PMINSD_XMM_FROM_MEM(REG_64 indir_source, REG_64 xmm_dest);
        void PMINUD_XMM_FROM_MEM(REG_64 indir_source, REG_64 xmm_dest);
        void PSHUFD(uint8_t imm, REG_64 xmm_source, REG_64 xmm_dest);

        void DIVPS(REG_64 xmm_source, REG_64 xmm_dest);

        void ADDPS(REG_64 xmm_source, REG_64 xmm_dest);
        void BLENDPS(uint8_t imm, REG_64 xmm_source, REG_64 xmm_dest);
        void CMPEQPS(REG_64 xmm_source, REG_64 xmm_dest);
        void CMPNLEPS(REG_64 xmm_source, REG_64 xmm_dest);
        void DPPS(uint8_t imm, REG_64 xmm_source, REG_64 xmm_dest);
        void INSERTPS(uint8_t count_s, uint8_t count_d, uint8_t zmask, REG_64 xmm_source, REG_64 xmm_dest);
        void MAXPS(REG_64 xmm_source, REG_64 xmm_dest);
        void MINPS(REG_64 xmm_source, REG_64 xmm_dest);
        void MULPS(REG_64 xmm_source, REG_64 xmm_dest);
        void SHUFPS(uint8_t imm, REG_64 xmm_source, REG_64 xmm_dest);
        void SQRTPS(REG_64 xmm_source, REG_64 xmm_dest);
        void SUBPS(REG_64 xmm_source, REG_64 xmm_dest);
        void XORPS(REG_64 xmm_source, REG_64 xmm_dest);

        //Convert 32-bit signed integers into floats
        void CVTDQ2PS(REG_64 xmm_source, REG_64 xmm_dest);

        //Convert truncated floats into 32-bit signed integers
        void CVTTPS2DQ(REG_64 xmm_source, REG_64 xmm_dest);
};

#endif // EMITTER64_HPP
