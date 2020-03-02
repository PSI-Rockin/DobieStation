#ifndef EMITTER64_HPP
#define EMITTER64_HPP
#include <cstdint>
#include "jitcache.hpp"

enum REG_64
{
    AL = 0, AX = 0, EAX = 0, RAX = 0, XMM0 = 0,
    CL = 1, CX = 1, ECX = 1, RCX = 1, XMM1 = 1,
    DL = 2, DX = 2, EDX = 2, RDX = 2, XMM2 = 2,
    BL = 3, BX = 3, EBX = 3, RBX = 3, XMM3 = 3,
    SPL = 4, SP = 4, ESP = 4, RSP = 4, XMM4 = 4,
    BPL = 5, BP = 5, EBP = 5, RBP = 5, XMM5 = 5,
    SIL = 6, SI = 6, ESI = 6, RSI = 6, XMM6 = 6,
    DIL = 7, DI = 7, EDI = 7, RDI = 7, XMM7 = 7,
    R8B = 8, R8W = 8, R8D = 8, R8 = 8, XMM8 = 8,
    R9B = 9, R9W = 9, R9D = 9, R9 = 9, XMM9 = 9,
    R10B = 10, R10W = 10, R10D = 10, R10 = 10, XMM10 = 10,
    R11B = 11, R11W = 11, R11D = 11, R11 = 11, XMM11 = 11,
    R12B = 12, R12W = 12, R12D = 12, R12 = 12, XMM12 = 12,
    R13B = 13, R13W = 13, R13D = 13, R13 = 13, XMM13 = 13,
    R14B = 14, R14W = 14, R14D = 14, R14 = 14, XMM14 = 14,
    R15B = 15, R15W = 15, R15D = 15, R15 = 15, XMM15 = 15,
};

/*
    Alternate condition codes are defined for some values
    because there are different mnemonics for the same operation
    in some circumstances. For example, cmovb and cmovc are the same
    operation while having different names.
*/
enum class ConditionCode
{
    O = 0,
    NO = 1,
    B = 2, C = 2, NAE = 2,
    AE = 3, NB = 3, NC = 3,
    E = 4, Z = 4,
    NE = 5, NZ = 5,
    BE = 6, NA = 6,
    A = 7, NBE = 7,
    S = 8,
    NS = 9,
    P = 10, PE = 10,
    NP = 11, PO = 11,
    L = 12, NGE = 12,
    GE = 13, NL = 13,
    LE = 14, NG = 14,
    G = 15, NLE = 15
};

class Emitter64
{
    private:
        JitBlock* block;

        void rex_r(REG_64 reg);
        void rex_rm(REG_64 rm);
        void rex_r_rm(REG_64 reg, REG_64 rm);
        void rexw_r(REG_64 reg);
        void rexw_rm(REG_64 rm);
        void rexw_r_rm(REG_64 reg, REG_64 rm);
        void modrm(uint8_t mode, uint8_t reg, uint8_t rm);
        void vex(REG_64 source, REG_64 source2, REG_64 dest, bool packed);
        void vex2(REG_64 source, REG_64 dest, bool packed);
        void vex3(REG_64 source, REG_64 source2, REG_64 dest, bool packed);

        int get_rip_offset(uint64_t addr);
    public:
        Emitter64(JitBlock* cache);

        void load_addr(uint64_t addr, REG_64 dest);

        void ADD16_REG(REG_64 source, REG_64 dest);
        void ADD16_REG_IMM(uint16_t imm, REG_64 dest);
        void ADD32_REG(REG_64 source, REG_64 dest);
        void ADD32_REG_IMM(uint32_t imm, REG_64 dest);
        void ADD64_REG(REG_64 source, REG_64 dest);
        void ADD64_REG_IMM(uint32_t imm, REG_64 dest);

        void INC16(REG_64 dest);

        void AND8_REG_IMM(uint8_t imm, REG_64 dest);
        void AND16_AX(uint16_t imm);
        void AND16_REG(REG_64 source, REG_64 dest);
        void AND16_REG_IMM(uint16_t imm, REG_64 dest);
        void AND32_EAX(uint32_t imm);
        void AND32_REG(REG_64 source, REG_64 dest);
        void AND32_REG_IMM(uint32_t imm, REG_64 dest);
        void AND64_REG(REG_64 source, REG_64 dest);

        void CMP8_REG(REG_64 op2, REG_64 op1);
        void CMP8_IMM_MEM(uint32_t imm, REG_64 mem, uint32_t offset = 0);
        void CMP16_IMM(uint16_t imm, REG_64 op);
        void CMP16_REG(REG_64 op2, REG_64 op1);
        void CMP32_IMM(uint32_t imm, REG_64 op);
        void CMP32_IMM_MEM(uint32_t imm, REG_64 mem, uint32_t offset = 0);
        void CMP32_EAX(uint32_t imm);
        void CMP32_REG(REG_64 op2, REG_64 op1);
        void CMP64_IMM(uint32_t imm, REG_64 op);
        void CMP64_REG(REG_64 op2, REG_64 op1);

        void CWD();
        void CDQ();
        void CQO();

        void DEC8(REG_64 dest);
        void DEC16(REG_64 dest);
        void DEC32(REG_64 dest);
        void DEC64(REG_64 dest);

        void DIV32(REG_64 dest);
        void IDIV32(REG_64 dest);
        void MUL32(REG_64 dest);
        void IMUL32(REG_64 dest);

        void NOT16(REG_64 dest);
        void NOT32(REG_64 dest);
        void NOT64(REG_64 dest);
        void NEG16(REG_64 dest);
        void NEG32(REG_64 dest);
        void NEG64(REG_64 dest);

        void OR16_REG(REG_64 source, REG_64 dest);
        void OR16_REG_IMM(uint16_t imm, REG_64 source);
        void OR32_REG(REG_64 source, REG_64 dest);
        void OR32_REG_IMM(uint32_t imm, REG_64 source);
        void OR32_EAX(uint32_t imm);
        void OR64_REG(REG_64 source, REG_64 dest);

        void SHL8_REG_1(REG_64 dest);
        void SHL16_REG_1(REG_64 dest);
        void SHL16_CL(REG_64 dest);
        void SHL16_REG_IMM(uint8_t shift, REG_64 dest);
        void SHL32_CL(REG_64 dest);
        void SHL32_REG_IMM(uint8_t shift, REG_64 dest);
        void SHL64_CL(REG_64 dest);
        void SHL64_REG_IMM(uint8_t shift, REG_64 dest);
        void SHR16_CL(REG_64 dest);
        void SHR16_REG_IMM(uint8_t shift, REG_64 dest);
        void SHR32_CL(REG_64 dest);
        void SHR32_REG_IMM(uint8_t shift, REG_64 dest);
        void SHR64_CL(REG_64 dest);
        void SHR64_REG_IMM(uint8_t shift, REG_64 dest);
        void SAR16_CL(REG_64 dest);
        void SAR16_REG_IMM(uint8_t shift, REG_64 dest);
        void SAR32_CL(REG_64 dest);
        void SAR32_REG_IMM(uint8_t shift, REG_64 dest);
        void SAR64_CL(REG_64 dest);
        void SAR64_REG_IMM(uint8_t shift, REG_64 dest);

        void SUB16_REG_IMM(uint16_t imm, REG_64 dest);
        void SUB32_REG(REG_64 source, REG_64 dest);
        void SUB32_MEM_IMM(uint32_t imm, REG_64 mem, uint32_t offset = 0);
        void SUB64_REG(REG_64 source, REG_64 dest);
        void SUB64_REG_IMM(uint32_t imm, REG_64 dest);

        void TEST8_REG(REG_64 op2, REG_64 op1);
        void TEST8_REG_IMM(uint8_t imm, REG_64 op1);
        void TEST8_IMM_MEM(uint8_t imm, REG_64 mem, uint32_t offset = 0);
        void TEST16_REG(REG_64 op2, REG_64 op1);
        void TEST16_REG_IMM(uint16_t imm, REG_64 op1);
        void TEST32_EAX(uint32_t imm);
        void TEST32_REG(REG_64 op2, REG_64 op1);
        void TEST32_REG_IMM(uint32_t imm, REG_64 op1);
        void TEST64_REG(REG_64 op2, REG_64 op1);
        void TEST64_REG_IMM(uint32_t imm, REG_64 op1);

        void XOR16_REG(REG_64 source, REG_64 dest);
        void XOR16_REG_IMM(uint16_t imm, REG_64 source);
        void XOR32_REG(REG_64 source, REG_64 dest);
        void XOR32_EAX(uint32_t imm);
        void XOR64_REG(REG_64 source, REG_64 dest);

        void LEA32_M(REG_64 source, REG_64 dest, uint32_t offset = 0, uint32_t shift = 0);
        void LEA32_REG(REG_64 source, REG_64 source2, REG_64 dest, uint32_t offset = 0, uint32_t shift = 0);
        void LEA64_M(REG_64 source, REG_64 dest, uint32_t offset = 0, uint32_t shift = 0);
        void LEA64_REG(REG_64 source, REG_64 source2, REG_64 dest, uint32_t offset = 0, uint32_t shift = 0);
        void MOV8_REG(REG_64 source, REG_64 dest);
        void MOV8_REG_IMM(uint8_t imm, REG_64 dest);
        void MOV8_TO_MEM(REG_64 source, REG_64 indir_dest, uint32_t offset = 0);
        void MOV8_FROM_MEM(REG_64 indir_source, REG_64 dest, uint32_t offset = 0);
        void MOV8_IMM_MEM(uint8_t imm, REG_64 indir_dest, uint32_t offset = 0);
        void MOV16_REG(REG_64 source, REG_64 dest);
        void MOV16_REG_IMM(uint16_t imm, REG_64 dest);
        void MOV16_TO_MEM(REG_64 source, REG_64 indir_dest, uint32_t offset = 0);
        void MOV16_FROM_MEM(REG_64 indir_source, REG_64 dest, uint32_t offset = 0);
        void MOV16_IMM_MEM(uint16_t imm, REG_64 indir_dest, uint32_t offset = 0);
        void MOV32_REG(REG_64 source, REG_64 dest);
        void MOV32_REG_IMM(uint32_t imm, REG_64 dest);
        void MOV32_IMM_MEM(uint32_t imm, REG_64 indir_dest, uint32_t offset = 0);
        void MOV32_FROM_MEM(REG_64 indir_source, REG_64 dest, uint32_t offset = 0);
        void MOV32_TO_MEM(REG_64 source, REG_64 indir_dest, uint32_t offset = 0);
        void MOV64_MR(REG_64 source, REG_64 dest);
        void MOV64_OI(uint64_t imm, REG_64 dest);
        void MOV64_FROM_MEM(REG_64 indir_source, REG_64 dest, uint32_t offset = 0);
        void MOV64_TO_MEM(REG_64 source, REG_64 indir_dest, uint32_t offset = 0);
        void MOV32SX_IMM_MEM(uint32_t imm, REG_64 indir_dest, uint32_t offset = 0);

        void MOVSX8_TO_64(REG_64 source, REG_64 dest);
        void MOVSX16_TO_32(REG_64 source, REG_64 dest);
        void MOVSX16_TO_64(REG_64 source, REG_64 dest);
        void MOVSX32_TO_64(REG_64 source, REG_64 dest);
        void MOVZX8_TO_32(REG_64 source, REG_64 dest);
        void MOVZX8_TO_64(REG_64 source, REG_64 dest);
        void MOVZX16_TO_64(REG_64 source, REG_64 dest);

        void MOVD_FROM_MEM(REG_64 indir_source, REG_64 xmm_dest);
        void MOVD_FROM_XMM(REG_64 xmm_source, REG_64 dest);
        void MOVD_TO_MEM(REG_64 xmm_source, REG_64 indir_dest);
        void MOVD_TO_XMM(REG_64 source, REG_64 xmm_dest);
        void MOVQ_FROM_XMM(REG_64 xmm_source, REG_64 dest);
        void MOVQ_TO_XMM(REG_64 source, REG_64 xmm_dest);
        void MOVSS_REG(REG_64 xmm_source, REG_64 xmm_dest);
        void MOVAPS_REG(REG_64 xmm_source, REG_64 xmm_dest);
        void MOVAPS_FROM_MEM(REG_64 indir_source, REG_64 xmm_dest, uint32_t offset = 0);
        void MOVAPS_TO_MEM(REG_64 xmm_source, REG_64 indir_dest, uint32_t offset = 0);
        void MOVUPS_FROM_MEM(REG_64 indir_source, REG_64 xmm_dest, uint32_t offset = 0);
        void MOVUPS_TO_MEM(REG_64 xmm_source, REG_64 indir_dest, uint32_t offset = 0);
        void MOVMSKPS(REG_64 xmm_source, REG_64 dest);

        void SETCC_REG(ConditionCode cc, REG_64 dest);
        void SETCC_MEM(ConditionCode cc, REG_64 indir_dest, uint32_t offset = 0);
        void CMOVCC16_REG(ConditionCode cc, REG_64 source, REG_64 dest);
        void CMOVCC32_REG(ConditionCode cc, REG_64 source, REG_64 dest);
        void CMOVCC64_REG(ConditionCode cc, REG_64 source, REG_64 dest);
        void CMOVCC16_MEM(ConditionCode cc, REG_64 source, REG_64 dest, uint32_t offset = 0);
        void CMOVCC32_MEM(ConditionCode cc, REG_64 source, REG_64 dest, uint32_t offset = 0);
        void CMOVCC64_MEM(ConditionCode cc, REG_64 source, REG_64 dest, uint32_t offset = 0);

        uint8_t* JMP_NEAR_DEFERRED();
        uint8_t* JCC_NEAR_DEFERRED(ConditionCode cc);

        void set_jump_dest(uint8_t* jump);

        void PUSH(REG_64 reg);
        void POP(REG_64 reg);
        void LDMXCSR(REG_64 mem, uint32_t offset = 0);
        void STMXCSR(REG_64 mem, uint32_t offset = 0);
        void CALL(uint64_t addr);
        void CALL_INDIR(REG_64 source);
        void JMP_INDIR(REG_64 source);
        void RET();

        void PACKUSDW(REG_64 xmm_source, REG_64 xmm_dest);
        void PACKUSWB(REG_64 xmm_source, REG_64 xmm_dest);
        void PACKSSDW(REG_64 xmm_source, REG_64 xmm_dest);
        void PABSB(REG_64 xmm_source, REG_64 xmm_dest);
        void PABSD(REG_64 xmm_source, REG_64 xmm_dest);
        void PABSW(REG_64 xmm_source, REG_64 xmm_dest);
        void PADDB(REG_64 xmm_source, REG_64 xmm_dest);
        void PADDD(REG_64 xmm_source, REG_64 xmm_dest);
        void PADDW(REG_64 xmm_source, REG_64 xmm_dest);
        void PADDSB(REG_64 xmm_source, REG_64 xmm_dest);
        void PADDSW(REG_64 xmm_source, REG_64 xmm_dest);
        void PADDUSB(REG_64 xmm_source, REG_64 xmm_dest);
        void PADDUSW(REG_64 xmm_source, REG_64 xmm_dest);
        void PAND_XMM(REG_64 xmm_source, REG_64 xmm_dest);
        void PAND_XMM_FROM_MEM(REG_64 indir_source, REG_64 xmm_dest, uint32_t offset = 0);
        void PANDN_XMM(REG_64 xmm_source, REG_64 xmm_dest);
        void PANDN_XMM_FROM_MEM(REG_64 indir_source, REG_64 xmm_dest, uint32_t offset = 0);
        void PCMPEQB_XMM(REG_64 xmm_source, REG_64 xmm_dest);
        void PCMPEQW_XMM(REG_64 xmm_source, REG_64 xmm_dest);
        void PCMPEQD_XMM(REG_64 xmm_source, REG_64 xmm_dest);
        void PCMPGTB_XMM(REG_64 xmm_source, REG_64 xmm_dest);
        void PCMPGTD_XMM(REG_64 xmm_source, REG_64 xmm_dest);
        void PCMPGTW_XMM(REG_64 xmm_source, REG_64 xmm_dest);
        void PEXTRB_XMM(uint8_t imm, REG_64 xmm_source, REG_64 dest);
        void PEXTRW_XMM(uint8_t imm, REG_64 xmm_source, REG_64 dest);
        void PEXTRD_XMM(uint8_t imm, REG_64 xmm_source, REG_64 dest);
        void PEXTRQ_XMM(uint8_t imm, REG_64 xmm_source, REG_64 dest);
        void PINSRB_XMM(uint8_t imm, REG_64 source, REG_64 xmm_dest);
        void PINSRW_XMM(uint8_t imm, REG_64 source, REG_64 xmm_dest);
        void PINSRD_XMM(uint8_t imm, REG_64 source, REG_64 xmm_dest);
        void PINSRQ_XMM(uint8_t imm, REG_64 source, REG_64 xmm_dest);
        void PMAXSB_XMM(REG_64 xmm_source, REG_64 xmm_dest);
        void PMAXSD_XMM(REG_64 xmm_source, REG_64 xmm_dest);
        void PMAXSW_XMM(REG_64 xmm_source, REG_64 xmm_dest);
        void PMAXSW_XMM_FROM_MEM(REG_64 indir_source, REG_64 xmm_dest, uint32_t offset = 0);
        void PMAXUB_XMM(REG_64 xmm_source, REG_64 xmm_dest);
        void PMAXUD_XMM(REG_64 xmm_source, REG_64 xmm_dest);
        void PMAXUW_XMM(REG_64 xmm_source, REG_64 xmm_dest);
        void PMINSB_XMM(REG_64 xmm_source, REG_64 xmm_dest);
        void PMINSD_XMM(REG_64 xmm_source, REG_64 xmm_dest);
        void PMINSW_XMM(REG_64 xmm_source, REG_64 xmm_dest);
        void PMINUB_XMM(REG_64 xmm_source, REG_64 xmm_dest);
        void PMINUD_XMM(REG_64 xmm_source, REG_64 xmm_dest);
        void PMINUW_XMM(REG_64 xmm_source, REG_64 xmm_dest);
        void PMINSW_XMM_FROM_MEM(REG_64 indir_source, REG_64 xmm_dest, uint32_t offset = 0);
        void PMINUW_XMM_FROM_MEM(REG_64 indir_source, REG_64 xmm_dest, uint32_t offset = 0);
        void PMINSD_XMM_FROM_MEM(REG_64 indir_source, REG_64 xmm_dest, uint32_t offset = 0);
        void PMINUD_XMM_FROM_MEM(REG_64 indir_source, REG_64 xmm_dest, uint32_t offset = 0);
        void PMOVZX8_TO_16(REG_64 xmm_source, REG_64 xmm_dest);
        void PMOVZX16_TO_32(REG_64 xmm_source, REG_64 xmm_dest);
        void PMOVSX16_TO_32(REG_64 xmm_source, REG_64 xmm_dest);
        void PMULLD(REG_64 xmm_source, REG_64 xmm_dest);
        void PMULLW(REG_64 xmm_source, REG_64 xmm_dest);
        void POR_XMM(REG_64 xmm_source, REG_64 xmm_dest);
        void POR_XMM_FROM_MEM(REG_64 indir_source, REG_64 xmm_dest, uint32_t offset = 0);
        void PSHUFD(uint8_t imm, REG_64 xmm_source, REG_64 xmm_dest);
        void PSHUFHW(uint8_t imm, REG_64 xmm_source, REG_64 xmm_dest);
        void PSHUFLW(uint8_t imm, REG_64 xmm_source, REG_64 xmm_dest);
        void PSLLW(int shift, REG_64 xmm_dest);
        void PSLLD(int shift, REG_64 xmm_dest);
        void PSLLQ(int shift, REG_64 xmm_dest);
        void PSRAW(int shift, REG_64 xmm_dest);
        void PSRAD(int shift, REG_64 xmm_dest);
        void PSRLW(int shift, REG_64 xmm_dest);
        void PSRLD(int shift, REG_64 xmm_dest);
        void PSRLQ(int shift, REG_64 xmm_dest);
        void PSUBB(REG_64 xmm_source, REG_64 xmm_dest);
        void PSUBD(REG_64 xmm_source, REG_64 xmm_dest);
        void PSUBW(REG_64 xmm_source, REG_64 xmm_dest);
        void PSUBSB(REG_64 xmm_source, REG_64 xmm_dest);
        void PSUBSW(REG_64 xmm_source, REG_64 xmm_dest);
        void PSUBSD(REG_64 xmm_source, REG_64 xmm_dest);
        void PSUBUSB(REG_64 xmm_source, REG_64 xmm_dest);
        void PSUBUSW(REG_64 xmm_source, REG_64 xmm_dest);
        void PXOR_XMM(REG_64 xmm_source, REG_64 xmm_dest);
        void PXOR_XMM_FROM_MEM(REG_64 indir_source, REG_64 xmm_dest, uint32_t offset = 0);

        void ADDPS(REG_64 xmm_source, REG_64 xmm_dest);
        void ADDSS(REG_64 xmm_source, REG_64 xmm_dest);
        void BLENDPS(uint8_t imm, REG_64 xmm_source, REG_64 xmm_dest);
        void BLENDVPS_XMM0(REG_64 xmm_source, REG_64 xmm_dest);
        void CMPEQPS(REG_64 xmm_source, REG_64 xmm_dest);
        void CMPNLEPS(REG_64 xmm_source, REG_64 xmm_dest);
        void DPPS(uint8_t imm, REG_64 xmm_source, REG_64 xmm_dest);
        void DIVPS(REG_64 xmm_source, REG_64 xmm_dest);
        void DIVSS(REG_64 xmm_source, REG_64 xmm_dest);
        void INSERTPS(uint8_t count_s, uint8_t count_d, uint8_t zmask, REG_64 xmm_source, REG_64 xmm_dest);
        void MAXPS(REG_64 xmm_source, REG_64 xmm_dest);
        void MAXSS(REG_64 xmm_source, REG_64 xmm_dest);
        void MINPS(REG_64 xmm_source, REG_64 xmm_dest);
        void MINSS(REG_64 xmm_source, REG_64 xmm_dest);
        void MULPS(REG_64 xmm_source, REG_64 xmm_dest);
        void MULSS(REG_64 xmm_source, REG_64 xmm_dest);
        void SHUFPS(uint8_t imm, REG_64 xmm_source, REG_64 xmm_dest);
        void SQRTSS(REG_64 xmm_source, REG_64 xmm_dest);
        void SQRTPS(REG_64 xmm_source, REG_64 xmm_dest);
        void SUBPS(REG_64 xmm_source, REG_64 xmm_dest);
        void SUBSS(REG_64 xmm_source, REG_64 xmm_dest);
        void RSQRTSS(REG_64 xmm_source, REG_64 xmm_dest);
        void RSQRTPS(REG_64 xmm_source, REG_64 xmm_dest);
        void UCOMISS(REG_64 xmm_source, REG_64 xmm_dest);
        void XORPS(REG_64 xmm_source, REG_64 xmm_dest);

        //Convert 32-bit signed integers into floats
        void CVTDQ2PS(REG_64 xmm_source, REG_64 xmm_dest);

        void CVTSS2SI(REG_64 xmm_source, REG_64 int_dest);
        void CVTSI2SS(REG_64 int_source, REG_64 xmm_dest);

        //Convert truncated floats into 32-bit signed integers
        void CVTTPS2DQ(REG_64 xmm_source, REG_64 xmm_dest);

        void VADDSS(REG_64 xmm_source, REG_64 xmm_source2, REG_64 xmm_dest);
        void VMINPS(REG_64 xmm_source, REG_64 xmm_source2, REG_64 xmm_dest);
        void VMAXPS(REG_64 xmm_source, REG_64 xmm_source2, REG_64 xmm_dest);
};

#endif // EMITTER64_HPP

