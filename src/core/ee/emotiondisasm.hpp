#ifndef EMOTIONDISASM_HPP
#define EMOTIONDISASM_HPP
#include <cstdint>
#include <string>

namespace EmotionDisasm
{
    std::string disasm_instr(uint32_t instruction, uint32_t instr_addr);

    std::string disasm_special(uint32_t instruction);
    std::string disasm_special_shift(const std::string opcode, uint32_t instruction);
    std::string disasm_sll(uint32_t instruction);
    std::string disasm_srl(uint32_t instruction);
    std::string disasm_sra(uint32_t instruction);
    std::string disasm_variableshift(const std::string opcode, uint32_t instruction);
    std::string disasm_sllv(uint32_t instruction);
    std::string disasm_srlv(uint32_t instruction);
    std::string disasm_srav(uint32_t instruction);
    std::string disasm_jr(uint32_t instruction);
    std::string disasm_jalr(uint32_t instruction);
    std::string disasm_conditional_move(const std::string opcode, uint32_t instruction);
    std::string disasm_movz(uint32_t instruction);
    std::string disasm_movn(uint32_t instruction);
    std::string disasm_syscall_ee(uint32_t instruction);
    std::string disasm_movereg(const std::string opcode, uint32_t instruction);
    std::string disasm_mfhi(uint32_t instruction);
    std::string disasm_mflo(uint32_t instruction);
    std::string disasm_dsllv(uint32_t instruction);
    std::string disasm_dsrav(uint32_t instruction);
    std::string disasm_dsrlv(uint32_t instruction);
    std::string disasm_mult(uint32_t instruction);
    std::string disasm_multu(uint32_t instruction);
    std::string disasm_division(const std::string opcode, uint32_t instruction);
    std::string disasm_div(uint32_t instruction);
    std::string disasm_divu(uint32_t instruction);
    std::string disasm_special_simplemath(const std::string opcode, uint32_t instruction);
    std::string disasm_add(uint32_t instruction);
    std::string disasm_addu(uint32_t instruction);
    std::string disasm_sub(uint32_t instruction);
    std::string disasm_subu(uint32_t instruction);
    std::string disasm_and_ee(uint32_t instruction);
    std::string disasm_or_ee(uint32_t instruction);
    std::string disasm_nor(uint32_t instruction);
    std::string disasm_mfsa(uint32_t instruction);
    std::string disasm_slt(uint32_t instruction);
    std::string disasm_sltu(uint32_t instruction);
    std::string disasm_dadd(uint32_t instruction);
    std::string disasm_daddu(uint32_t instruction);
    std::string disasm_dsubu(uint32_t instruction);
    std::string disasm_dsll(uint32_t instruction);
    std::string disasm_dsrl(uint32_t instruction);
    std::string disasm_dsll32(uint32_t instruction);
    std::string disasm_dsrl32(uint32_t instruction);
    std::string disasm_dsra32(uint32_t instruction);

    std::string disasm_regimm(uint32_t instruction, uint32_t instr_addr);

    std::string disasm_jump(const std::string opcode, uint32_t instruction, uint32_t instr_addr);
    std::string disasm_j(uint32_t instruction, uint32_t instr_addr);
    std::string disasm_jal(uint32_t instruction, uint32_t instr_addr);
    std::string disasm_branch_equality(std::string opcode, uint32_t instruction, uint32_t instr_addr);
    std::string disasm_beq(uint32_t instruction, uint32_t instr_addr);
    std::string disasm_bne(uint32_t instruction, uint32_t instr_addr);
    std::string disasm_branch_inequality(const std::string opcode, uint32_t instruction, uint32_t instr_addr);
    std::string disasm_blez(uint32_t instruction, uint32_t instr_addr);
    std::string disasm_bgtz(uint32_t instruction, uint32_t instr_addr);
    std::string disasm_math(const std::string opcode, uint32_t instruction);
    std::string disasm_addi(uint32_t instruction);
    std::string disasm_addiu(uint32_t instruction);
    std::string disasm_slti(uint32_t instruction);
    std::string disasm_sltiu(uint32_t instruction);
    std::string disasm_andi(uint32_t instruction);
    std::string disasm_ori(uint32_t instruction);
    std::string disasm_move(uint32_t instruction);
    std::string disasm_xori(uint32_t instruction);
    std::string disasm_lui(uint32_t instruction);
    std::string disasm_beql(uint32_t instruction, uint32_t instr_addr);
    std::string disasm_bnel(uint32_t instruction, uint32_t instr_addr);
    std::string disasm_daddiu(uint32_t instruction);
    std::string disasm_loadstore(const std::string opcode, uint32_t instruction);
    std::string disasm_ldl(uint32_t instruction);
    std::string disasm_ldr(uint32_t instruction);
    std::string disasm_lq(uint32_t instruction);
    std::string disasm_sq(uint32_t instruction);
    std::string disasm_lb(uint32_t instruction);
    std::string disasm_lh(uint32_t instruction);
    std::string disasm_lw(uint32_t instruction);
    std::string disasm_lwl(uint32_t instruction);
    std::string disasm_lbu(uint32_t instruction);
    std::string disasm_lhu(uint32_t instruction);
    std::string disasm_lwr(uint32_t instruction);
    std::string disasm_lwu(uint32_t instruction);
    std::string disasm_sb(uint32_t instruction);
    std::string disasm_sh(uint32_t instruction);
    std::string disasm_swl(uint32_t instruction);
    std::string disasm_sw(uint32_t instruction);
    std::string disasm_sdl(uint32_t instruction);
    std::string disasm_sdr(uint32_t instruction);
    std::string disasm_swr(uint32_t instruction);
    std::string disasm_lwc1(uint32_t instruction);
    std::string disasm_ld(uint32_t instruction);
    std::string disasm_swc1(uint32_t instruction);
    std::string disasm_sd(uint32_t instruction);

    std::string disasm_cop(uint32_t instruction, uint32_t instr_addr);
    std::string disasm_cop_move(std::string opcode, uint32_t instruction);
    std::string disasm_cop_mfc(uint32_t instruction);
    std::string disasm_cop_mtc(uint32_t instruction);
    std::string disasm_cop_ctc(uint32_t instruction);

    std::string disasm_cop_s(uint32_t instruction);
    std::string disasm_fpu_math(const std::string opcode, uint32_t instruction);
    std::string disasm_fpu_add(uint32_t instruction);
    std::string disasm_fpu_sub(uint32_t instruction);
    std::string disasm_fpu_mul(uint32_t instruction);
    std::string disasm_fpu_div(uint32_t instruction);
    std::string disasm_fpu_singleop_math(const std::string opcode, uint32_t instruction);
    std::string disasm_fpu_mov(uint32_t instruction);
    std::string disasm_fpu_neg(uint32_t instruction);
    std::string disasm_fpu_acc(const std::string opcode, uint32_t instruction);
    std::string disasm_fpu_adda(uint32_t instruction);
    std::string disasm_fpu_convert(const std::string opcode, uint32_t instruction);
    std::string disasm_fpu_cvt_w_s(uint32_t instruction);
    std::string disasm_fpu_compare(const std::string opcode, uint32_t instruction);
    std::string disasm_fpu_c_lt_s(uint32_t instruction);
    std::string disasm_fpu_c_eq_s(uint32_t instruction);
    std::string disasm_cop_bc1(uint32_t instruction, uint32_t instr_addr);
    std::string disasm_cop_cvt_s_w(uint32_t instruction);

    std::string disasm_mmi(uint32_t instruction, uint32_t instr_addr);
    std::string disasm_plzcw(uint32_t instruction);
    std::string disasm_mmi0(uint32_t instruction);
    std::string disasm_psubb(uint32_t instruction);
    std::string disasm_mmi1(uint32_t instruction);
    std::string disasm_mmi2(uint32_t instruction);
    std::string disasm_pcpyld(uint32_t instruction);
    std::string disasm_pand(uint32_t instruction);
    std::string disasm_mfhi1(uint32_t instruction);
    std::string disasm_mflo1(uint32_t instruction);
    std::string disasm_mult1(uint32_t instruction);
    std::string disasm_div1(uint32_t instruction);
    std::string disasm_divu1(uint32_t instruction);
    std::string disasm_mmi3(uint32_t instruction);
    std::string disasm_pcpyud(uint32_t instruction);
    std::string disasm_por(uint32_t instruction);
    std::string disasm_pnor(uint32_t instruction);

    std::string unknown_op(const std::string optype, uint32_t op, int width);

};

#endif // EMOTIONDISASM_HPP
