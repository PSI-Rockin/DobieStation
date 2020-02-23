#ifndef EMOTIONINTERPRETER_HPP
#define EMOTIONINTERPRETER_HPP
#include "emotion.hpp"
#include <vector>

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
    SA,
    COP1_ACC,

    MAX_VALUE
};

enum class COP1_Control_SpecialReg
{
    CONDITION
};

enum class COP2_Control_SpecialReg
{
    CONDITION
};

enum class RegType
{
    UNK,
    GPR,
    COP0,
    COP1,
    COP1_CONTROL,
    COP2,
    COP2_CONTROL,
    MAX_VALUE
};

struct EE_DependencyInfo
{
    RegType type = RegType::UNK;
    uint8_t reg = 0xCD;
};

enum class DependencyType
{
    Read,
    Write
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
    enum class Pipeline: uint16_t
    {
        //Uninitialized. Error out if an instruction's pipeline is set to this.
        Unk = 0,

        //ALU instruction that can only be used in integer pipeline 0.
        Int0 = 1,

        //ALU instruction that can only be used in integer pipeline 1.
        Int1 = 2,

        //MMI instruction. Takes up both integer pipelines.
        IntWide = Int0 | Int1,

        //Generic ALU instruction. Can be placed in either integer pipeline.
        IntGeneric = 4,

        //Load/store instructions.
        LoadStore = 8,

        //Branches, jumps, and calls. This includes COP1 and COP2 branches.
        Branch = 0x10,

        //COP0
        COP0 = 0x20,

        //COP1
        COP1 = 0x40,

        //COP2
        COP2 = 0x80,

        // SA operations
        SA = 0x100,

        // ERET
        ERET = 0x200,

        // SYNC
        SYNC = 0x400,

        // LZC
        LZC = 0x800,

        // MAC0, e.g. mult, div, madd
        MAC0 = 0x1000,

        // MAC1, e.g. mult1, div1, madd1
        MAC1 = 0x2000,
    };

    friend Pipeline operator& (Pipeline x, Pipeline y)
    {
        return (Pipeline)((uint8_t)x & (uint8_t)y);
    }

    friend Pipeline operator| (Pipeline x, Pipeline y)
    {
        return (Pipeline)((uint8_t)x | (uint8_t)y);
    }

    friend Pipeline operator^ (Pipeline x, Pipeline y)
    {
        return (Pipeline)((uint8_t)x ^ (uint8_t)y);
    }

    // Used for throughput calculations
    enum class InstructionType : uint8_t
    {
        OTHER = 0,
        MULT,
        MULT1,
        DIV,
        DIV1,
        MADD,
        MADD1,
        FPU_DIV,
        FPU_SQRT,
        FPU_RSQRT,
        MAX_VALUE
    };

    // By using basic_string instead of vector, constructing this structure
    // requires no dynamic memory allocation
    std::basic_string<uint16_t> write_dependencies = std::basic_string<uint16_t>();
    std::basic_string<uint16_t> read_dependencies = std::basic_string<uint16_t>();
    void(*interpreter_fn)(EmotionEngine&, uint32_t) = nullptr;
    Pipeline pipeline = Pipeline::Unk;
    InstructionType instruction_type = InstructionType::OTHER;

    uint8_t latency = 1;
    uint8_t throughput = 1;
    uint32_t cycles_before = 0;
    uint32_t cycles_after = 0;

    inline void add_dependency(DependencyType dtype, RegType rtype, uint8_t reg)
    {
        switch (dtype)
        {
            case DependencyType::Read:
                read_dependencies.push_back((int)rtype << 8 | reg);
                break;
            case DependencyType::Write:
                write_dependencies.push_back((int)rtype << 8 | reg);
                break;
            default:
                break;
        }
    }

    inline void get_dependency(EE_DependencyInfo &dest, size_t idx, DependencyType dtype) const
    {
        switch (dtype)
        {
            case DependencyType::Read:
                dest.type = (RegType)(read_dependencies[idx] >> 8);
                dest.reg = read_dependencies[idx] & 0xFF;
                break;
            case DependencyType::Write:
                dest.type = (RegType)(write_dependencies[idx] >> 8);
                dest.reg = write_dependencies[idx] & 0xFF;
                break;
            default:
                break;
        }
    }
};

namespace EmotionInterpreter
{
    void interpret(EmotionEngine& cpu, uint32_t instruction);
    void lookup(EE_InstrInfo&, uint32_t instruction);

    void special(EE_InstrInfo &instr_info, uint32_t instruction);
    void sll(EmotionEngine& cpu, uint32_t instruction);
    void srl(EmotionEngine& cpu, uint32_t instruction);
    void sra(EmotionEngine& cpu, uint32_t instruction);
    void sllv(EmotionEngine& cpu, uint32_t instruction);
    void srlv(EmotionEngine& cpu, uint32_t instruction);
    void srav(EmotionEngine& cpu, uint32_t instruction);
    void jr(EmotionEngine& cpu, uint32_t instruction);
    void jalr(EmotionEngine& cpu, uint32_t instruction);
    void movz(EmotionEngine& cpu, uint32_t instruction);
    void movn(EmotionEngine& cpu, uint32_t instruction);
    void syscall_ee(EmotionEngine& cpu, uint32_t instruction);
    void break_ee(EmotionEngine& cpu, uint32_t instruction);
    void mfhi(EmotionEngine& cpu, uint32_t instruction);
    void mthi(EmotionEngine& cpu, uint32_t instruction);
    void mflo(EmotionEngine& cpu, uint32_t instruction);
    void mtlo(EmotionEngine& cpu, uint32_t instruction);
    void dsllv(EmotionEngine& cpu, uint32_t instruction);
    void dsrlv(EmotionEngine& cpu, uint32_t instruction);
    void dsrav(EmotionEngine& cpu, uint32_t instruction);
    void mult(EmotionEngine& cpu, uint32_t instruction);
    void multu(EmotionEngine& cpu, uint32_t instruction);
    void div(EmotionEngine& cpu, uint32_t instruction);
    void divu(EmotionEngine& cpu, uint32_t instruction);
    void add(EmotionEngine& cpu, uint32_t instruction);
    void addu(EmotionEngine& cpu, uint32_t instruction);
    void sub(EmotionEngine& cpu, uint32_t instruction);
    void subu(EmotionEngine& cpu, uint32_t instruction);
    void and_ee(EmotionEngine& cpu, uint32_t instruction);
    void or_ee(EmotionEngine& cpu, uint32_t instruction);
    void xor_ee(EmotionEngine& cpu, uint32_t instruction);
    void nor(EmotionEngine& cpu, uint32_t instruction);
    void mfsa(EmotionEngine& cpu, uint32_t instruction);
    void mtsa(EmotionEngine& cpu, uint32_t instruction);
    void slt(EmotionEngine& cpu, uint32_t instruction);
    void sltu(EmotionEngine& cpu, uint32_t instruction);
    void dadd(EmotionEngine& cpu, uint32_t instruction);
    void daddu(EmotionEngine& cpu, uint32_t instruction);
    void dsub(EmotionEngine& cpu, uint32_t instruction);
    void dsubu(EmotionEngine& cpu, uint32_t instruction);
    void teq(EmotionEngine& cpu, uint32_t instruction);
    void dsll(EmotionEngine& cpu, uint32_t instruction);
    void dsrl(EmotionEngine& cpu, uint32_t instruction);
    void dsra(EmotionEngine& cpu, uint32_t instruction);
    void dsll32(EmotionEngine& cpu, uint32_t instruction);
    void dsrl32(EmotionEngine& cpu, uint32_t instruction);
    void dsra32(EmotionEngine& cpu, uint32_t instruction);

    void regimm(EE_InstrInfo &instr_info, uint32_t instruction);
    void bltz(EmotionEngine& cpu, uint32_t instruction);
    void bltzl(EmotionEngine& cpu, uint32_t instruction);
    void bltzal(EmotionEngine &cpu, uint32_t instruction);
    void bltzall(EmotionEngine &cpu, uint32_t instruction);
    void bgez(EmotionEngine& cpu, uint32_t instruction);
    void bgezl(EmotionEngine& cpu, uint32_t instruction);
    void bgezal(EmotionEngine &cpu, uint32_t instruction);
    void bgezall(EmotionEngine &cpu, uint32_t instruction);
    void mtsab(EmotionEngine &cpu, uint32_t instruction);
    void mtsah(EmotionEngine& cpu, uint32_t instruction);

    void j(EmotionEngine& cpu, uint32_t instruction);
    void jal(EmotionEngine& cpu, uint32_t instruction);
    void beq(EmotionEngine& cpu, uint32_t instruction);
    void bne(EmotionEngine& cpu, uint32_t instruction);
    void blez(EmotionEngine& cpu, uint32_t instruction);
    void bgtz(EmotionEngine& cpu, uint32_t instruction);
    void addi(EmotionEngine& cpu, uint32_t instruction);
    void addiu(EmotionEngine& cpu, uint32_t instruction);
    void slti(EmotionEngine& cpu, uint32_t instruction);
    void sltiu(EmotionEngine& cpu, uint32_t instruction);
    void andi(EmotionEngine& cpu, uint32_t instruction);
    void ori(EmotionEngine& cpu, uint32_t instruction);
    void xori(EmotionEngine& cpu, uint32_t instruction);
    void lui(EmotionEngine& cpu, uint32_t instruction);
    void beql(EmotionEngine& cpu, uint32_t instruction);
    void bnel(EmotionEngine& cpu, uint32_t instruction);
    void blezl(EmotionEngine& cpu, uint32_t instruction);
    void bgtzl(EmotionEngine& cpu, uint32_t instruction);
    void daddi(EmotionEngine& cpu, uint32_t instruction);
    void daddiu(EmotionEngine& cpu, uint32_t instruction);
    void ldl(EmotionEngine& cpu, uint32_t instruction);
    void ldr(EmotionEngine& cpu, uint32_t instruction);
    void lq(EmotionEngine& cpu, uint32_t instruction);
    void sq(EmotionEngine& cpu, uint32_t instruction);
    void lb(EmotionEngine& cpu, uint32_t instruction);
    void lh(EmotionEngine& cpu, uint32_t instruction);
    void lwl(EmotionEngine& cpu, uint32_t instruction);
    void lw(EmotionEngine& cpu, uint32_t instruction);
    void lbu(EmotionEngine& cpu, uint32_t instruction);
    void lhu(EmotionEngine& cpu, uint32_t instruction);
    void lwr(EmotionEngine& cpu, uint32_t instruction);
    void lwu(EmotionEngine& cpu, uint32_t instruction);
    void sb(EmotionEngine& cpu, uint32_t instruction);
    void sh(EmotionEngine& cpu, uint32_t instruction);
    void swl(EmotionEngine& cpu, uint32_t instruction);
    void sw(EmotionEngine& cpu, uint32_t instruction);
    void sdl(EmotionEngine& cpu, uint32_t instruction);
    void sdr(EmotionEngine& cpu, uint32_t instruction);
    void swr(EmotionEngine& cpu, uint32_t instruction);
    void lwc1(EmotionEngine& cpu, uint32_t instruction);
    void lqc2(EmotionEngine& cpu, uint32_t instruction);
    void ld(EmotionEngine& cpu, uint32_t instruction);
    void swc1(EmotionEngine& cpu, uint32_t instruction);
    void sqc2(EmotionEngine& cpu, uint32_t instruction);
    void sd(EmotionEngine& cpu, uint32_t instruction);

    void cache(EmotionEngine& cpu, uint32_t instruction);

    void tlbwi(EmotionEngine& cpu, uint32_t instruction);
    void eret(EmotionEngine& cpu, uint32_t instruction);
    void ei(EmotionEngine& cpu, uint32_t instruction);
    void di(EmotionEngine& cpu, uint32_t instruction);

    void cop(EE_InstrInfo& info, uint32_t instruction);
    void cop0_mfc(EmotionEngine& cpu, uint32_t instruction);
    void cop1_mfc(EmotionEngine& cpu, uint32_t instruction);
    void cop0_mtc(EmotionEngine& cpu, uint32_t instruction);
    void cop1_mtc(EmotionEngine& cpu, uint32_t instruction);
    void cop1_cfc(EmotionEngine& cpu, uint32_t instruction);
    void cop2_cfc(EmotionEngine& cpu, uint32_t instruction);
    void cop1_ctc(EmotionEngine& cpu, uint32_t instruction);
    void cop2_ctc(EmotionEngine& cpu, uint32_t instruction);
    void cop_bc0(EmotionEngine& cpu, uint32_t instruction);

    void cop_s(EE_InstrInfo& info, uint32_t instruction);
    void fpu_add(EmotionEngine& cpu, uint32_t instruction);
    void fpu_sub(EmotionEngine& cpu, uint32_t instruction);
    void fpu_mul(EmotionEngine& cpu, uint32_t instruction);
    void fpu_div(EmotionEngine& cpu, uint32_t instruction);
    void fpu_sqrt(EmotionEngine& cpu, uint32_t instruction);
    void fpu_abs(EmotionEngine& cpu, uint32_t instruction);
    void fpu_mov(EmotionEngine& cpu, uint32_t instruction);
    void fpu_neg(EmotionEngine& cpu, uint32_t instruction);
    void fpu_rsqrt(EmotionEngine& cpu, uint32_t instruction);
    void fpu_adda(EmotionEngine& cpu, uint32_t instruction);
    void fpu_suba(EmotionEngine& cpu, uint32_t instruction);
    void fpu_mula(EmotionEngine& cpu, uint32_t instruction);
    void fpu_madd(EmotionEngine& cpu, uint32_t instruction);
    void fpu_msub(EmotionEngine& cpu, uint32_t instruction);
    void fpu_madda(EmotionEngine& cpu, uint32_t instruction);
    void fpu_msuba(EmotionEngine& cpu, uint32_t instruction);
    void fpu_cvt_w_s(EmotionEngine& cpu, uint32_t instruction);
    void fpu_max_s(EmotionEngine& cpu, uint32_t instruction);
    void fpu_min_s(EmotionEngine& cpu, uint32_t instruction);
    void fpu_c_f_s(EmotionEngine& cpu, uint32_t instruction);
    void fpu_c_eq_s(EmotionEngine& cpu, uint32_t instruction);
    void fpu_c_lt_s(EmotionEngine& cpu, uint32_t instruction);
    void fpu_c_le_s(EmotionEngine& cpu, uint32_t instruction);
    void cop_bc1(EmotionEngine& cpu, uint32_t instruction);
    void cop2_bc2(EmotionEngine& cpu, uint32_t instruction);
    void cop_cvt_s_w(EmotionEngine& cpu, uint32_t instruction);

    void cop2_qmfc2(EmotionEngine& cpu, uint32_t instruction);
    void cop2_qmtc2(EmotionEngine& cpu, uint32_t instruction);

    void cop2_special(EE_InstrInfo& info, uint32_t instruction);
    void cop2_vaddbc(EmotionEngine& cpu, uint32_t instruction);
    void cop2_vsubbc(EmotionEngine& cpu, uint32_t instruction);
    void cop2_vmaddbc(EmotionEngine& cpu, uint32_t instruction);
    void cop2_vmsubbc(EmotionEngine& cpu, uint32_t instruction);
    void cop2_vmaxbc(EmotionEngine& cpu, uint32_t instruction);
    void cop2_vminibc(EmotionEngine& cpu, uint32_t instruction);
    void cop2_vmulbc(EmotionEngine& cpu, uint32_t instruction);
    void cop2_vmulq(EmotionEngine& cpu, uint32_t instruction);
    void cop2_vmaxi(EmotionEngine& cpu, uint32_t instruction);
    void cop2_vmuli(EmotionEngine& cpu, uint32_t instruction);
    void cop2_vminii(EmotionEngine& cpu, uint32_t instruction);
    void cop2_vaddq(EmotionEngine& cpu, uint32_t instruction);
    void cop2_vmaddq(EmotionEngine& cpu, uint32_t instruction);
    void cop2_vaddi(EmotionEngine& cpu, uint32_t instruction);
    void cop2_vmaddi(EmotionEngine& cpu, uint32_t instruction);
    void cop2_vsubq(EmotionEngine& cpu, uint32_t instruction);
    void cop2_vmsubq(EmotionEngine& cpu, uint32_t instruction);
    void cop2_vsubi(EmotionEngine& cpu, uint32_t instruction);
    void cop2_vmsubi(EmotionEngine& cpu, uint32_t instruction);
    void cop2_vadd(EmotionEngine& cpu, uint32_t instruction);
    void cop2_vmadd(EmotionEngine& cpu, uint32_t instruction);
    void cop2_vmul(EmotionEngine& cpu, uint32_t instruction);
    void cop2_vmax(EmotionEngine& cpu, uint32_t instruction);
    void cop2_vsub(EmotionEngine& cpu, uint32_t instruction);
    void cop2_vmsub(EmotionEngine& cpu, uint32_t instruction);
    void cop2_vopmsub(EmotionEngine& cpu, uint32_t instruction);
    void cop2_vmini(EmotionEngine& cpu, uint32_t instruction);
    void cop2_viadd(EmotionEngine& cpu, uint32_t instruction);
    void cop2_visub(EmotionEngine& cpu, uint32_t instruction);
    void cop2_viaddi(EmotionEngine& cpu, uint32_t instruction);
    void cop2_viand(EmotionEngine& cpu, uint32_t instruction);
    void cop2_vior(EmotionEngine& cpu, uint32_t instruction);
    void cop2_vcallms(EmotionEngine& cpu, uint32_t instruction);
    void cop2_vcallmsr(EmotionEngine& cpu, uint32_t instruction);

    void cop2_special2(EE_InstrInfo& info, uint32_t instruction);
    void cop2_vaddabc(EmotionEngine& cpu, uint32_t instruction);
    void cop2_vsubabc(EmotionEngine& cpu, uint32_t instruction);
    void cop2_vmaddabc(EmotionEngine& cpu, uint32_t instruction);
    void cop2_vmsubabc(EmotionEngine& cpu, uint32_t instruction);
    void cop2_vitof0(EmotionEngine& cpu, uint32_t instruction);
    void cop2_vitof4(EmotionEngine& cpu, uint32_t instruction);
    void cop2_vitof12(EmotionEngine& cpu, uint32_t instruction);
    void cop2_vitof15(EmotionEngine& cpu, uint32_t instruction);
    void cop2_vftoi0(EmotionEngine& cpu, uint32_t instruction);
    void cop2_vftoi4(EmotionEngine& cpu, uint32_t instruction);
    void cop2_vftoi12(EmotionEngine& cpu, uint32_t instruction);
    void cop2_vftoi15(EmotionEngine& cpu, uint32_t instruction);
    void cop2_vmulabc(EmotionEngine& cpu, uint32_t instruction);
    void cop2_vmulaq(EmotionEngine& cpu, uint32_t instruction);
    void cop2_vabs(EmotionEngine& cpu, uint32_t instruction);
    void cop2_vmulai(EmotionEngine& cpu, uint32_t instruction);
    void cop2_vclip(EmotionEngine& cpu, uint32_t instruction);
    void cop2_vaddaq(EmotionEngine& cpu, uint32_t instruction);
    void cop2_vmaddaq(EmotionEngine& cpu, uint32_t instruction);
    void cop2_vmaddai(EmotionEngine& cpu, uint32_t instruction);
    void cop2_vmsubaq(EmotionEngine& cpu, uint32_t instruction);
    void cop2_vaddai(EmotionEngine& cpu, uint32_t instruction);
    void cop2_vsubai(EmotionEngine& cpu, uint32_t instruction);
    void cop2_vmsubai(EmotionEngine& cpu, uint32_t instruction);
    void cop2_vadda(EmotionEngine& cpu, uint32_t instruction);
    void cop2_vmadda(EmotionEngine& cpu, uint32_t instruction);
    void cop2_vmula(EmotionEngine& cpu, uint32_t instruction);
    void cop2_vsuba(EmotionEngine& cpu, uint32_t instruction);
    void cop2_vmsuba(EmotionEngine& cpu, uint32_t instruction);
    void cop2_vopmula(EmotionEngine& cpu, uint32_t instruction);
    void cop2_vnop(EmotionEngine& cpu, uint32_t instruction);
    void cop2_vmove(EmotionEngine& cpu, uint32_t instruction);
    void cop2_vmr32(EmotionEngine& cpu, uint32_t instruction);
    void cop2_vlqi(EmotionEngine& cpu, uint32_t instruction);
    void cop2_vsqi(EmotionEngine& cpu, uint32_t instruction);
    void cop2_vlqd(EmotionEngine& cpu, uint32_t instruction);
    void cop2_vsqd(EmotionEngine& cpu, uint32_t instruction);
    void cop2_vdiv(EmotionEngine& cpu, uint32_t instruction);
    void cop2_vsqrt(EmotionEngine& cpu, uint32_t instruction);
    void cop2_vrsqrt(EmotionEngine& cpu, uint32_t instruction);
    void cop2_vwaitq(EmotionEngine& cpu, uint32_t instruction);
    void cop2_vmtir(EmotionEngine& cpu, uint32_t instruction);
    void cop2_vmfir(EmotionEngine& cpu, uint32_t instruction);
    void cop2_vilwr(EmotionEngine& cpu, uint32_t instruction);
    void cop2_viswr(EmotionEngine& cpu, uint32_t instruction);
    void cop2_vrnext(EmotionEngine& cpu, uint32_t instruction);
    void cop2_vrget(EmotionEngine& cpu, uint32_t instruction);
    void cop2_vrinit(EmotionEngine& cpu, uint32_t instruction);
    void cop2_vrxor(EmotionEngine& cpu, uint32_t instruction);

    bool cop2_sync(EmotionEngine& cpu, uint32_t instruction);

    void mmi(EE_InstrInfo& info, uint32_t instruction);
    void madd(EmotionEngine& cpu, uint32_t instruction);
    void maddu(EmotionEngine& cpu, uint32_t instruction);
    void plzcw(EmotionEngine& cpu, uint32_t instruction);
    void psllh(EmotionEngine& cpu, uint32_t instruction);
    void psrlh(EmotionEngine& cpu, uint32_t instruction);
    void psrah(EmotionEngine& cpu, uint32_t instruction);
    void psllw(EmotionEngine& cpu, uint32_t instruction);
    void psrlw(EmotionEngine& cpu, uint32_t instruction);
    void psraw(EmotionEngine& cpu, uint32_t instruction);

    void pmfhlfmt(EE_InstrInfo& info, uint32_t instruction);
    void mmi0(EE_InstrInfo& info, uint32_t instruction);
    int16_t clamp_halfword(int32_t word);
    int64_t clamp_doubleword(int64_t word);
    void pmthllw(EmotionEngine& cpu, uint32_t instruction);
    void pmfhllw(EmotionEngine& cpu, uint32_t instruction);
    void pmfhluw(EmotionEngine& cpu, uint32_t instruction);
    void pmfhlslw(EmotionEngine& cpu, uint32_t instruction);
    void pmfhllh(EmotionEngine& cpu, uint32_t instruction);    
    void pmfhlsh(EmotionEngine& cpu, uint32_t instruction);
    void paddw(EmotionEngine& cpu, uint32_t instruction);
    void psubw(EmotionEngine& cpu, uint32_t instruction);
    void pcgtw(EmotionEngine& cpu, uint32_t instruction);
    void pmaxw(EmotionEngine& cpu, uint32_t instruction);
    void paddb(EmotionEngine& cpu, uint32_t instruction);
    void paddh(EmotionEngine& cpu, uint32_t instruction);
    void psubh(EmotionEngine& cpu, uint32_t instruction);
    void pcgth(EmotionEngine& cpu, uint32_t instruction);
    void pmaxh(EmotionEngine& cpu, uint32_t instruction);
    void psubb(EmotionEngine& cpu, uint32_t instruction);
    void pcgtb(EmotionEngine& cpu, uint32_t instruction);
    void paddsw(EmotionEngine& cpu, uint32_t instruction);
    void psubsw(EmotionEngine& cpu, uint32_t instruction);
    void pextlw(EmotionEngine& cpu, uint32_t instruction);
    void ppacw(EmotionEngine& cpu, uint32_t instruction);
    void paddsh(EmotionEngine& cpu, uint32_t instruction);
    void psubsh(EmotionEngine& cpu, uint32_t instruction);
    void pextlh(EmotionEngine& cpu, uint32_t instruction);
    void ppach(EmotionEngine& cpu, uint32_t instruction);
    void paddsb(EmotionEngine& cpu, uint32_t instruction);
    void psubsb(EmotionEngine& cpu, uint32_t instruction);
    void pextlb(EmotionEngine& cpu, uint32_t instruction);
    void ppacb(EmotionEngine& cpu, uint32_t instruction);
    void pext5(EmotionEngine& cpu, uint32_t instruction);
    void ppac5(EmotionEngine& cpu, uint32_t instruction);

    void mmi1(EE_InstrInfo& info, uint32_t instruction);
    void pabsw(EmotionEngine& cpu, uint32_t instruction);
    void pceqw(EmotionEngine& cpu, uint32_t instruction);
    void pminw(EmotionEngine& cpu, uint32_t instruction);
    void padsbh(EmotionEngine& cpu, uint32_t instruction);
    void pabsh(EmotionEngine& cpu, uint32_t instruction);
    void pceqh(EmotionEngine& cpu, uint32_t instruction);
    void pminh(EmotionEngine& cpu, uint32_t instruction);
    void pceqb(EmotionEngine& cpu, uint32_t instruction);
    void padduw(EmotionEngine& cpu, uint32_t instruction);
    void psubuw(EmotionEngine& cpu, uint32_t instruction);
    void pextuw(EmotionEngine& cpu, uint32_t instruction);
    void padduh(EmotionEngine& cpu, uint32_t instruction);
    void psubuh(EmotionEngine& cpu, uint32_t instruction);
    void pextuh(EmotionEngine& cpu, uint32_t instruction);
    void paddub(EmotionEngine& cpu, uint32_t instruction);
    void psubub(EmotionEngine& cpu, uint32_t instruction);
    void pextub(EmotionEngine& cpu, uint32_t instruction);
    void qfsrv(EmotionEngine& cpu, uint32_t instruction);

    void mmi2(EE_InstrInfo& info, uint32_t instruction);
    void pmaddw(EmotionEngine &cpu, uint32_t instruction);
    void psllvw(EmotionEngine& cpu, uint32_t instruction);
    void psrlvw(EmotionEngine& cpu, uint32_t instruction);
    void pmsubw(EmotionEngine &cpu, uint32_t instruction);
    void pmfhi(EmotionEngine& cpu, uint32_t instruction);
    void pmflo(EmotionEngine& cpu, uint32_t instruction);
    void pinth(EmotionEngine& cpu, uint32_t instruction);
    void pcpyld(EmotionEngine& cpu, uint32_t instruction);
    void pmaddh(EmotionEngine &cpu, uint32_t instruction);
    void phmadh(EmotionEngine& cpu, uint32_t instruction);
    void pand(EmotionEngine& cpu, uint32_t instruction);
    void pxor(EmotionEngine& cpu, uint32_t instruction);
    void pmsubh(EmotionEngine &cpu, uint32_t instruction);
    void phmsbh(EmotionEngine &cpu, uint32_t instruction);
    void pexeh(EmotionEngine& cpu, uint32_t instruction);
    void prevh(EmotionEngine& cpu, uint32_t instruction);
    void pmulth(EmotionEngine& cpu, uint32_t instruction);
    void pdivbw(EmotionEngine& cpu, uint32_t instruction);
    void pmultw(EmotionEngine &cpu, uint32_t instruction);
    void pdivw(EmotionEngine& cpu, uint32_t instruction);    
    void pexew(EmotionEngine& cpu, uint32_t instruction);
    void prot3w(EmotionEngine& cpu, uint32_t instruction);

    void mfhi1(EmotionEngine& cpu, uint32_t instruction);
    void mthi1(EmotionEngine& cpu, uint32_t instruction);
    void mflo1(EmotionEngine& cpu, uint32_t instruction);
    void mtlo1(EmotionEngine& cpu, uint32_t instruction);
    void mult1(EmotionEngine& cpu, uint32_t instruction);
    void multu1(EmotionEngine& cpu, uint32_t instruction);
    void div1(EmotionEngine& cpu, uint32_t instruction);
    void divu1(EmotionEngine& cpu, uint32_t instruction);
    void madd1(EmotionEngine& cpu, uint32_t instruction);
    void maddu1(EmotionEngine& cpu, uint32_t instruction);

    void mmi3(EE_InstrInfo& info, uint32_t instruction);
    void pmadduw(EmotionEngine &cpu, uint32_t instruction);
    void psravw(EmotionEngine& cpu, uint32_t instruction);
    void pmthi(EmotionEngine& cpu, uint32_t instruction);
    void pmtlo(EmotionEngine& cpu, uint32_t instruction);
    void pinteh(EmotionEngine& cpu, uint32_t instruction);
    void pmultuw(EmotionEngine &cpu, uint32_t instruction);
    void pdivuw(EmotionEngine& cpu, uint32_t instruction);
    void pcpyud(EmotionEngine& cpu, uint32_t instruction);
    void por(EmotionEngine& cpu, uint32_t instruction);
    void pnor(EmotionEngine& cpu, uint32_t instruction);
    void pexch(EmotionEngine& cpu, uint32_t instruction);
    void pcpyh(EmotionEngine& cpu, uint32_t instruction);
    void pexcw(EmotionEngine& cpu, uint32_t instruction);

    void nop(EmotionEngine &cpu, uint32_t instruction);

    [[ noreturn ]] void unknown_op(const char* type, uint32_t instruction, uint16_t op);
};

#endif // EMOTIONINTERPRETER_HPP
