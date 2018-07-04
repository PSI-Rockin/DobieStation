#include <iomanip>
#include <sstream>
#include "emotion.hpp"
#include "emotiondisasm.hpp"

using namespace std;

#define RS ((instruction >> 21) & 0x1F)
#define RT ((instruction >> 16) & 0x1F)
#define RD ((instruction >> 11) & 0x1F)
#define SA ((instruction >> 6 ) & 0x1F)
#define IMM ((int16_t)(instruction & 0xFFFF))

string EmotionDisasm::disasm_instr(uint32_t instruction, uint32_t instr_addr)
{
    if (!instruction)
        return "nop";
    switch (instruction >> 26)
    {
        case 0x00:
            return disasm_special(instruction);
        case 0x01:
            return disasm_regimm(instruction, instr_addr);
        case 0x02:
            return disasm_j(instruction, instr_addr);
        case 0x03:
            return disasm_jal(instruction, instr_addr);
        case 0x04:
            return disasm_beq(instruction, instr_addr);
        case 0x05:
            return disasm_bne(instruction, instr_addr);
        case 0x06:
            return disasm_blez(instruction, instr_addr);
        case 0x07:
            return disasm_bgtz(instruction, instr_addr);
        case 0x08:
            return disasm_addi(instruction);
        case 0x09:
            return disasm_addiu(instruction);
        case 0x0A:
            return disasm_slti(instruction);
        case 0x0B:
            return disasm_sltiu(instruction);
        case 0x0C:
            return disasm_andi(instruction);
        case 0x0D:
            return disasm_ori(instruction);
        case 0x0E:
            return disasm_xori(instruction);
        case 0x0F:
            return disasm_lui(instruction);
        case 0x10:
        case 0x11:
        case 0x12:
        case 0x13:
            return disasm_cop(instruction, instr_addr);
        case 0x14:
            return disasm_beql(instruction, instr_addr);
        case 0x15:
            return disasm_bnel(instruction, instr_addr);
        case 0x16:
            return disasm_branch_inequality("blezl", instruction, instr_addr);
        case 0x17:
            return disasm_branch_inequality("bgtzl", instruction, instr_addr);
        case 0x19:
            return disasm_daddiu(instruction);
        case 0x1A:
            return disasm_ldl(instruction);
        case 0x1B:
            return disasm_ldr(instruction);
        case 0x1C:
            return disasm_mmi(instruction, instr_addr);
        case 0x1E:
            return disasm_lq(instruction);
        case 0x1F:
            return disasm_sq(instruction);
        case 0x20:
            return disasm_lb(instruction);
        case 0x21:
            return disasm_lh(instruction);
        case 0x22:
            return disasm_lwl(instruction);
        case 0x23:
            return disasm_lw(instruction);
        case 0x24:
            return disasm_lbu(instruction);
        case 0x25:
            return disasm_lhu(instruction);
        case 0x26:
            return disasm_lwr(instruction);
        case 0x27:
            return disasm_lwu(instruction);
        case 0x28:
            return disasm_sb(instruction);
        case 0x29:
            return disasm_sh(instruction);
        case 0x2A:
            return disasm_swl(instruction);
        case 0x2B:
            return disasm_sw(instruction);
        case 0x2C:
            return disasm_sdl(instruction);
        case 0x2D:
            return disasm_sdr(instruction);
        case 0x2E:
            return disasm_swr(instruction);
        case 0x2F:
            return "cache";
        case 0x30:
            return disasm_loadstore("lwc0", instruction);
        case 0x31:
            return disasm_lwc1(instruction);
        case 0x33:
            return "pref";
        case 0x36:
            return disasm_lqc2(instruction);
        case 0x37:
            return disasm_ld(instruction);
        case 0x39:
            return disasm_swc1(instruction);
        case 0x3E:
            return disasm_sqc2(instruction);
        case 0x3F:
            return disasm_sd(instruction);
        default:
            return unknown_op("normal", instruction >> 26, 2);
    }
}

string EmotionDisasm::disasm_j(uint32_t instruction, uint32_t instr_addr)
{
    return disasm_jump("j", instruction, instr_addr);
}

string EmotionDisasm::disasm_jal(uint32_t instruction, uint32_t instr_addr)
{
    return disasm_jump("jal", instruction, instr_addr);
}

string EmotionDisasm::disasm_jump(const string opcode, uint32_t instruction, uint32_t instr_addr)
{
    stringstream output;
    uint32_t addr = (instruction & 0x3FFFFFF) << 2;
    addr += (instr_addr + 4) & 0xF0000000;
    output << "$" << setfill('0') << setw(8) << hex << addr;

    return opcode + " " + output.str();
}

string EmotionDisasm::disasm_bne(uint32_t instruction, uint32_t instr_addr)
{
    return disasm_branch_equality("bne", instruction, instr_addr);
}

string EmotionDisasm::disasm_branch_equality(string opcode, uint32_t instruction, uint32_t instr_addr)
{
    stringstream output;
    int offset = IMM;
    offset <<=2;
    uint64_t rs = RS;
    uint64_t rt = RT;

    output << EmotionEngine::REG(rs) << ", ";
    if (!rt)
        opcode += "z";
    else
        output << EmotionEngine::REG(rt) << ", ";
    output << "$" << setfill('0') << setw(8) << hex << (instr_addr + offset + 4);

    return opcode + " " + output.str();
}

string EmotionDisasm::disasm_addiu(uint32_t instruction)
{
    return disasm_math("addiu", instruction);
}

string EmotionDisasm::disasm_regimm(uint32_t instruction, uint32_t instr_addr)
{
    stringstream output;
    string opcode = "";
    switch (RT)
    {
        case 0x00:
            opcode += "bltz";
            break;
        case 0x01:
            opcode += "bgez";
            break;
        case 0x02:
            opcode += "bltzl";
            break;
        case 0x03:
            opcode += "bgezl";
            break;
        case 0x10:
            opcode += "bltzal";
            break;
        case 0x11:
            opcode += "bgezal";
            break;
        case 0x12:
            opcode += "bltzall";
            break;
        case 0x13:
            opcode += "bgezall";
            break;
        case 0x19:
            return disasm_mtsah(instruction);
        default:
            return unknown_op("regimm", RT, 2);
    }
    int32_t offset = IMM;
    offset <<= 2;
    output << EmotionEngine::REG(RS) << ", "
           << "$" << setfill('0') << setw(8) << hex << (instr_addr + offset + 4);

    return opcode + " " + output.str();

}

string EmotionDisasm::disasm_mtsah(uint32_t instruction)
{
    stringstream output;
    output << "mtsah " << EmotionEngine::REG(RS) << ", " << (uint16_t)IMM;
    return output.str();
}

string EmotionDisasm::disasm_special(uint32_t instruction)
{
    switch (instruction & 0x3F)
    {
        case 0x00:
            return disasm_sll(instruction);
        case 0x02:
            return disasm_srl(instruction);
        case 0x03:
            return disasm_sra(instruction);
        case 0x04:
            return disasm_sllv(instruction);
        case 0x06:
            return disasm_srlv(instruction);
        case 0x07:
            return disasm_srav(instruction);
        case 0x08:
            return disasm_jr(instruction);
        case 0x09:
            return disasm_jalr(instruction);
        case 0x0A:
            return disasm_movz(instruction);
        case 0x0B:
            return disasm_movn(instruction);
        case 0x0C:
            return disasm_syscall_ee(instruction);
        case 0x0F:
            return "sync";
        case 0x10:
            return disasm_mfhi(instruction);
        case 0x11:
            return disasm_mthi(instruction);
        case 0x12:
            return disasm_mflo(instruction);
        case 0x13:
            return disasm_mtlo(instruction);
        case 0x14:
            return disasm_dsllv(instruction);
        case 0x16:
            return disasm_dsrlv(instruction);
        case 0x17:
            return disasm_dsrav(instruction);
        case 0x18:
            return disasm_mult(instruction);
        case 0x19:
            return disasm_multu(instruction);
        case 0x1A:
            return disasm_div(instruction);
        case 0x1B:
            return disasm_divu(instruction);
        case 0x20:
            return disasm_add(instruction);
        case 0x21:
            return disasm_addu(instruction);
        case 0x22:
            return disasm_sub(instruction);
        case 0x23:
            return disasm_subu(instruction);
        case 0x24:
            return disasm_and_ee(instruction);
        case 0x25:
            return disasm_or_ee(instruction);
        case 0x26:
            return disasm_xor_ee(instruction);
        case 0x27:
            return disasm_nor(instruction);
        case 0x28:
            return disasm_mfsa(instruction);
        case 0x29:
            return disasm_mtsa(instruction);
        case 0x2A:
            return disasm_slt(instruction);
        case 0x2B:
            return disasm_sltu(instruction);
        case 0x2C:
            return disasm_dadd(instruction);
        case 0x2D:
            return disasm_daddu(instruction);
        case 0x2E:
            return disasm_dsub(instruction);
        case 0x2F:
            return disasm_dsubu(instruction);
        case 0x38:
            return disasm_dsll(instruction);
        case 0x3A:
            return disasm_dsrl(instruction);
        case 0x3B:
            return disasm_dsra(instruction);
        case 0x3C:
            return disasm_dsll32(instruction);
        case 0x3E:
            return disasm_dsrl32(instruction);
        case 0x3F:
            return disasm_dsra32(instruction);
        default:
            return unknown_op("special", instruction & 0x3F, 2);
    }
}

string EmotionDisasm::disasm_sll(uint32_t instruction)
{
    return disasm_special_shift("sll", instruction);
}

string EmotionDisasm::disasm_srl(uint32_t instruction)
{
    return disasm_special_shift("srl", instruction);
}

string EmotionDisasm::disasm_sra(uint32_t instruction)
{
    return disasm_special_shift("sra", instruction);
}

string EmotionDisasm::disasm_variableshift(const string opcode, uint32_t instruction)
{
    stringstream output;
    output << EmotionEngine::REG(RD) << ", "
           << EmotionEngine::REG(RT) << ", "
           << EmotionEngine::REG(RS);

    return opcode + " " + output.str();
}

string EmotionDisasm::disasm_sllv(uint32_t instruction)
{
    return disasm_variableshift("sllv", instruction);
}

string EmotionDisasm::disasm_srlv(uint32_t instruction)
{
    return disasm_variableshift("srlv", instruction);
}

string EmotionDisasm::disasm_srav(uint32_t instruction)
{
    return disasm_variableshift("srav", instruction);
}

string EmotionDisasm::disasm_jr(uint32_t instruction)
{
    stringstream output;
    string opcode = "jr";
    output << EmotionEngine::REG(RS);

    return opcode + " " + output.str();
}

string EmotionDisasm::disasm_jalr(uint32_t instruction)
{
    stringstream output;
    string opcode = "jalr";
    if (RD != 31)
        output << EmotionEngine::REG(RD) << ", ";
    output << EmotionEngine::REG(RS);

    return opcode + " " + output.str();
}

string EmotionDisasm::disasm_conditional_move(const string opcode, uint32_t instruction)
{
    stringstream output;
    output << EmotionEngine::REG(RD) << ", "
           << EmotionEngine::REG(RS) << ", "
           << EmotionEngine::REG(RT);

    return opcode + " " + output.str();
}

string EmotionDisasm::disasm_movz(uint32_t instruction)
{
    return disasm_conditional_move("movz", instruction);
}

string EmotionDisasm::disasm_movn(uint32_t instruction)
{
    return disasm_conditional_move("movn", instruction);
}

string EmotionDisasm::disasm_syscall_ee(uint32_t instruction)
{
    stringstream output;
    string opcode = "syscall";
    uint32_t code = (instruction >> 6) & 0xFFFFF;
    output << "$" << setfill('0') << setw(8) << hex << code;

    return opcode + " " + output.str();
}

string EmotionDisasm::disasm_movereg(const string opcode, uint32_t instruction)
{
    stringstream output;
    output << EmotionEngine::REG(RD);
    return opcode + " " + output.str();
}

string EmotionDisasm::disasm_mfhi(uint32_t instruction)
{
    return disasm_movereg("mfhi", instruction);
}

string EmotionDisasm::disasm_moveto(const string opcode, uint32_t instruction)
{
    stringstream output;
    output << EmotionEngine::REG(RS);
    return opcode + " " + output.str();
}

string EmotionDisasm::disasm_mthi(uint32_t instruction)
{
    return disasm_moveto("mthi", instruction);
}

string EmotionDisasm::disasm_mflo(uint32_t instruction)
{
    return disasm_movereg("mflo", instruction);
}

string EmotionDisasm::disasm_mtlo(uint32_t instruction)
{
    return disasm_moveto("mtlo", instruction);
}

string EmotionDisasm::disasm_dsllv(uint32_t instruction)
{
    return disasm_variableshift("dsllv", instruction);
}

string EmotionDisasm::disasm_dsrlv(uint32_t instruction)
{
    return disasm_variableshift("dsrlv", instruction);
}

string EmotionDisasm::disasm_dsrav(uint32_t instruction)
{
    return disasm_variableshift("dsrav", instruction);
}

string EmotionDisasm::disasm_mult(uint32_t instruction)
{
    return disasm_special_simplemath("mult", instruction);
}

string EmotionDisasm::disasm_multu(uint32_t instruction)
{
    return disasm_special_simplemath("multu", instruction);
}

string EmotionDisasm::disasm_division(const string opcode, uint32_t instruction)
{
    stringstream output;
    output << EmotionEngine::REG(RS) << ", "
           << EmotionEngine::REG(RT);

    return opcode + " " + output.str();
}

string EmotionDisasm::disasm_div(uint32_t instruction)
{
    return disasm_division("div", instruction);
}

string EmotionDisasm::disasm_divu(uint32_t instruction)
{
    return disasm_division("divu", instruction);
}

string EmotionDisasm::disasm_special_simplemath(const string opcode, uint32_t instruction)
{
    stringstream output;
    output << EmotionEngine::REG(RD) << ", "
           << EmotionEngine::REG(RS) << ", "
           << EmotionEngine::REG(RT);

    return opcode + " " + output.str();
}

string EmotionDisasm::disasm_add(uint32_t instruction)
{
    return disasm_special_simplemath("add", instruction);
}

string EmotionDisasm::disasm_addu(uint32_t instruction)
{
    return disasm_special_simplemath("addu", instruction);
}

string EmotionDisasm::disasm_sub(uint32_t instruction)
{
    return disasm_special_simplemath("add", instruction);
}

string EmotionDisasm::disasm_subu(uint32_t instruction)
{
    return disasm_special_simplemath("subu", instruction);
}

string EmotionDisasm::disasm_and_ee(uint32_t instruction)
{
    return disasm_special_simplemath("and", instruction);
}

string EmotionDisasm::disasm_or_ee(uint32_t instruction)
{
    return disasm_special_simplemath("or", instruction);
}

string EmotionDisasm::disasm_xor_ee(uint32_t instruction)
{
    return disasm_special_simplemath("xor", instruction);
}

string EmotionDisasm::disasm_nor(uint32_t instruction)
{
    return disasm_special_simplemath("nor", instruction);
}

string EmotionDisasm::disasm_mfsa(uint32_t instruction)
{
    return disasm_movereg("mfsa", instruction);
}

string EmotionDisasm::disasm_mtsa(uint32_t instruction)
{
    return disasm_moveto("mtsa", instruction);
}

string EmotionDisasm::disasm_slt(uint32_t instruction)
{
    return disasm_special_simplemath("slt", instruction);
}

string EmotionDisasm::disasm_sltu(uint32_t instruction)
{
    return disasm_special_simplemath("sltu", instruction);
}

string EmotionDisasm::disasm_dadd(uint32_t instruction)
{
    return disasm_special_simplemath("dadd", instruction);
}

string EmotionDisasm::disasm_daddu(uint32_t instruction)
{
    if (RT == 0)
        return disasm_move(instruction);
    return disasm_special_simplemath("daddu", instruction);
}

string EmotionDisasm::disasm_dsub(uint32_t instruction)
{
    return disasm_special_simplemath("dsub", instruction);
}

string EmotionDisasm::disasm_dsubu(uint32_t instruction)
{
    return disasm_special_simplemath("dsubu", instruction);
}

string EmotionDisasm::disasm_special_shift(const string opcode, uint32_t instruction)
{
    stringstream output;
    output << EmotionEngine::REG(RD) << ", "
           << EmotionEngine::REG(RT) << ", "
           << SA;
    return opcode + " " + output.str();
}

string EmotionDisasm::disasm_dsll(uint32_t instruction)
{
    return disasm_special_shift("dsll", instruction);
}

string EmotionDisasm::disasm_dsrl(uint32_t instruction)
{
    return disasm_special_shift("dsrl", instruction);
}

string EmotionDisasm::disasm_dsra(uint32_t instruction)
{
    return disasm_special_shift("dsra", instruction);
}

string EmotionDisasm::disasm_dsll32(uint32_t instruction)
{
    return disasm_special_shift("dsll32", instruction);
}

string EmotionDisasm::disasm_dsrl32(uint32_t instruction)
{
    return disasm_special_shift("dsrl32", instruction);
}

string EmotionDisasm::disasm_dsra32(uint32_t instruction)
{
    return disasm_special_shift("dsra32", instruction);
}


string EmotionDisasm::disasm_beq(uint32_t instruction, uint32_t instr_addr)
{
    return disasm_branch_equality("beq", instruction, instr_addr);
}


string EmotionDisasm::disasm_branch_inequality(const std::string opcode, uint32_t instruction, uint32_t instr_addr)
{
    stringstream output;
    int32_t offset = IMM;
    offset <<= 2;

    output << EmotionEngine::REG(RS) << ", "
           << "$" << setfill('0') << setw(8) << hex << (instr_addr + offset + 4);

    return opcode + " " + output.str();

}

string EmotionDisasm::disasm_blez(uint32_t instruction, uint32_t instr_addr)
{
    return disasm_branch_inequality("blez", instruction, instr_addr);
}

string EmotionDisasm::disasm_bgtz(uint32_t instruction, uint32_t instr_addr)
{
    return disasm_branch_inequality("bgtz", instruction, instr_addr);
}

string EmotionDisasm::disasm_math(const string opcode, uint32_t instruction)
{
    stringstream output;
    output << EmotionEngine::REG(RT) << ", " << EmotionEngine::REG(RS) << ", "
           << "$" << setfill('0') << setw(4) << hex << IMM;
    return opcode + " " + output.str();
}

string EmotionDisasm::disasm_addi(uint32_t instruction)
{
    return disasm_math("addi", instruction);
}

string EmotionDisasm::disasm_slti(uint32_t instruction)
{
    return disasm_math("slti", instruction);
}

string EmotionDisasm::disasm_sltiu(uint32_t instruction)
{
    return disasm_math("sltiu", instruction);
}

string EmotionDisasm::disasm_andi(uint32_t instruction)
{
    return disasm_math("andi", instruction);
}

string EmotionDisasm::disasm_ori(uint32_t instruction)
{
    if (IMM == 0)
        return disasm_move(instruction);
    return disasm_math("ori", instruction);
}

string EmotionDisasm::disasm_move(uint32_t instruction)
{
    stringstream output;
    string opcode = "move";
    output << EmotionEngine::REG(RD) << ", "
           << EmotionEngine::REG(RS);

    return opcode + " " + output.str();
}

string EmotionDisasm::disasm_xori(uint32_t instruction)
{
    return disasm_math("xori", instruction);
}

string EmotionDisasm::disasm_lui(uint32_t instruction)
{
    stringstream output;
    string opcode = "lui";
    output << EmotionEngine::REG(RT) << ", "
           << "$" << setfill('0') << setw(4) << hex << IMM;

    return opcode + " " + output.str();
}

string EmotionDisasm::disasm_beql(uint32_t instruction, uint32_t instr_addr)
{
    return disasm_branch_equality("beql", instruction, instr_addr);
}

string EmotionDisasm::disasm_bnel(uint32_t instruction, uint32_t instr_addr)
{
    return disasm_branch_equality("bnel", instruction, instr_addr);
}

string EmotionDisasm::disasm_daddiu(uint32_t instruction)
{
    return disasm_math("daddiu", instruction);
}

string EmotionDisasm::disasm_loadstore(const std::string opcode, uint32_t instruction)
{
    stringstream output;
    output << EmotionEngine::REG(RT) << ", "
           << IMM
           << "{" << EmotionEngine::REG(RS) << "}";
    return opcode + " " + output.str();
}

string EmotionDisasm::disasm_cop2_loadstore(const string opcode, uint32_t instruction)
{
    stringstream output;
    output << "vf" << RT << ", "
           << IMM
           << "{" << EmotionEngine::REG(RS) << "}";
    return opcode + " " + output.str();
}

string EmotionDisasm::disasm_ldl(uint32_t instruction)
{
    return disasm_loadstore("ldl", instruction);
}

string EmotionDisasm::disasm_ldr(uint32_t instruction)
{
    return disasm_loadstore("ldr", instruction);
}

string EmotionDisasm::disasm_lq(uint32_t instruction)
{
    return disasm_loadstore("lq", instruction);
}

string EmotionDisasm::disasm_sq(uint32_t instruction)
{
    return disasm_loadstore("sq", instruction);
}

string EmotionDisasm::disasm_lb(uint32_t instruction)
{
    return disasm_loadstore("lb", instruction);
}

string EmotionDisasm::disasm_lh(uint32_t instruction)
{
    return disasm_loadstore("lh", instruction);
}

string EmotionDisasm::disasm_lwl(uint32_t instruction)
{
    return disasm_loadstore("lwl", instruction);
}

string EmotionDisasm::disasm_lw(uint32_t instruction)
{
    return disasm_loadstore("lw", instruction);
}

string EmotionDisasm::disasm_lbu(uint32_t instruction)
{
    return disasm_loadstore("lbu", instruction);
}

string EmotionDisasm::disasm_lhu(uint32_t instruction)
{
    return disasm_loadstore("lhu", instruction);
}

string EmotionDisasm::disasm_lwr(uint32_t instruction)
{
    return disasm_loadstore("lwr", instruction);
}

string EmotionDisasm::disasm_lwu(uint32_t instruction)
{
    return disasm_loadstore("lwu", instruction);
}

string EmotionDisasm::disasm_sb(uint32_t instruction)
{
    return disasm_loadstore("sb", instruction);
}

string EmotionDisasm::disasm_sh(uint32_t instruction)
{
    return disasm_loadstore("sh", instruction);
}

string EmotionDisasm::disasm_swl(uint32_t instruction)
{
    return disasm_loadstore("swl", instruction);
}

string EmotionDisasm::disasm_sw(uint32_t instruction)
{
    return disasm_loadstore("sw", instruction);
}

string EmotionDisasm::disasm_sdl(uint32_t instruction)
{
    return disasm_loadstore("sdl", instruction);
}

string EmotionDisasm::disasm_sdr(uint32_t instruction)
{
    return disasm_loadstore("sdr", instruction);
}

string EmotionDisasm::disasm_swr(uint32_t instruction)
{
    return disasm_loadstore("swr", instruction);
}

string EmotionDisasm::disasm_lwc1(uint32_t instruction)
{
    return disasm_loadstore("lwc1", instruction);
}

string EmotionDisasm::disasm_lqc2(uint32_t instruction)
{
    return disasm_cop2_loadstore("lqc2", instruction);
}

string EmotionDisasm::disasm_ld(uint32_t instruction)
{
    return disasm_loadstore("ld", instruction);
}

string EmotionDisasm::disasm_swc1(uint32_t instruction)
{
    return disasm_loadstore("swc1", instruction);
}

string EmotionDisasm::disasm_sqc2(uint32_t instruction)
{
    return disasm_cop2_loadstore("sqc2", instruction);
}

string EmotionDisasm::disasm_sd(uint32_t instruction)
{
    return disasm_loadstore("sd", instruction);
}

string EmotionDisasm::disasm_cop(uint32_t instruction, uint32_t instr_addr)
{
    uint16_t op = RS;
    uint8_t cop_id = ((instruction >> 26) & 0x3);
    switch (op | (cop_id * 0x100))
    {
        case 0x000:
        case 0x100:
            return disasm_cop_mfc(instruction);
        case 0x004:
        case 0x104:
            return disasm_cop_mtc(instruction);
        case 0x010:
        {
            uint8_t op2 = instruction & 0x3F;
            switch (op2)
            {
                case 0x2:
                    return "tlbwi";
                case 0x18:
                    return "eret";
                case 0x38:
                    return "ei";
                case 0x39:
                    return "di";
                default:
                    return unknown_op("cop0x010", op2, 2);

            }
        }
        case 0x106:
        case 0x206:
            return disasm_cop_ctc(instruction);
        case 0x108:
            return disasm_cop_bc1(instruction, instr_addr);
        case 0x110:
            return disasm_cop_s(instruction);
        case 0x114:
            return disasm_cop_cvt_s_w(instruction);
        case 0x102:
        case 0x202:
            return disasm_cop_cfc(instruction);
        default:
            if (cop_id == 2)
                return disasm_cop2(instruction);
            return unknown_op("cop", op, 2);
    }
}

string EmotionDisasm::disasm_cop_move(string opcode, uint32_t instruction)
{
    stringstream output;

    int cop_id = (instruction >> 26) & 0x3;

    output << opcode << cop_id << " " << EmotionEngine::REG(RT) << ", "
           << RD;

    return output.str();
}

string EmotionDisasm::disasm_cop_mfc(uint32_t instruction)
{
    return disasm_cop_move("mfc", instruction);
}

string EmotionDisasm::disasm_cop_mtc(uint32_t instruction)
{
    return disasm_cop_move("mtc", instruction);
}

string EmotionDisasm::disasm_cop_cfc(uint32_t instruction)
{
    return disasm_cop_move("cfc", instruction);
}

string EmotionDisasm::disasm_cop_ctc(uint32_t instruction)
{
    return disasm_cop_move("ctc", instruction);
}

string EmotionDisasm::disasm_cop_s(uint32_t instruction)
{
    uint8_t op = instruction & 0x3F;
    switch (op)
    {
        case 0x0:
            return disasm_fpu_add(instruction);
        case 0x1:
            return disasm_fpu_sub(instruction);
        case 0x2:
            return disasm_fpu_mul(instruction);
        case 0x3:
            return disasm_fpu_div(instruction);
        case 0x4:
            return disasm_fpu_sqrt(instruction);
        case 0x5:
            return disasm_fpu_abs(instruction);
        case 0x6:
            return disasm_fpu_mov(instruction);
        case 0x7:
            return disasm_fpu_neg(instruction);
        case 0x18:
            return disasm_fpu_adda(instruction);
        case 0x19:
            return disasm_fpu_suba(instruction);
        case 0x1A:
            return disasm_fpu_mula(instruction);
        case 0x1C:
            return disasm_fpu_madd(instruction);
        case 0x1D:
            return disasm_fpu_msub(instruction);
        case 0x1E:
            return disasm_fpu_madda(instruction);
        case 0x24:
            return disasm_fpu_cvt_w_s(instruction);
        case 0x28:
            return disasm_fpu_math("max.s", instruction);
        case 0x29:
            return disasm_fpu_math("min.s", instruction);
        case 0x30:
            return disasm_fpu_c_f_s(instruction);
        case 0x32:
            return disasm_fpu_c_eq_s(instruction);
        case 0x34:
            return disasm_fpu_c_lt_s(instruction);
        case 0x36:
            return disasm_fpu_c_le_s(instruction);
        default:
            return unknown_op("FPU-S", op, 2);
    }
}

string EmotionDisasm::disasm_fpu_math(const string opcode, uint32_t instruction)
{
    stringstream output;
    output << "f" << SA << ", "
           << "f" << RD << ", "
           << "f" << RT;

    return opcode + " " + output.str();
}

string EmotionDisasm::disasm_fpu_add(uint32_t instruction)
{
    return disasm_fpu_math("add.s", instruction);
}

string EmotionDisasm::disasm_fpu_sub(uint32_t instruction)
{
    return disasm_fpu_math("sub.s", instruction);
}

string EmotionDisasm::disasm_fpu_mul(uint32_t instruction)
{
    return disasm_fpu_math("mul.s", instruction);
}

string EmotionDisasm::disasm_fpu_div(uint32_t instruction)
{
    return disasm_fpu_math("div.s", instruction);
}

string EmotionDisasm::disasm_fpu_sqrt(uint32_t instruction)
{
    return disasm_fpu_singleop_math("sqrt.s", instruction);
}

string EmotionDisasm::disasm_fpu_abs(uint32_t instruction)
{
    return disasm_fpu_singleop_math("abs.s", instruction);
}

string EmotionDisasm::disasm_fpu_singleop_math(const string opcode, uint32_t instruction)
{
    stringstream output;
    output << "f" << SA << ", "
           << "f" << RD;

    return opcode + " " + output.str();
}

string EmotionDisasm::disasm_fpu_mov(uint32_t instruction)
{
    return disasm_fpu_singleop_math("mov.s", instruction);
}

string EmotionDisasm::disasm_fpu_neg(uint32_t instruction)
{
    return disasm_fpu_singleop_math("neg.s", instruction);
}

string EmotionDisasm::disasm_fpu_acc(const string opcode, uint32_t instruction)
{
    stringstream output;
    output << "f" << RD << ", "
           << "f" << RT;

    return opcode + " " + output.str();
}

string EmotionDisasm::disasm_fpu_adda(uint32_t instruction)
{
    return disasm_fpu_acc("adda.s", instruction);
}

string EmotionDisasm::disasm_fpu_suba(uint32_t instruction)
{
    return disasm_fpu_acc("suba.s", instruction);
}

string EmotionDisasm::disasm_fpu_mula(uint32_t instruction)
{
    return disasm_fpu_acc("mula.s", instruction);
}

string EmotionDisasm::disasm_fpu_madd(uint32_t instruction)
{
    return disasm_fpu_math("madd.s", instruction);
}

string EmotionDisasm::disasm_fpu_msub(uint32_t instruction)
{
    return disasm_fpu_math("msub.s", instruction);
}

string EmotionDisasm::disasm_fpu_madda(uint32_t instruction)
{
    return disasm_fpu_acc("madda.s", instruction);
}

string EmotionDisasm::disasm_fpu_convert(const string opcode, uint32_t instruction)
{
    stringstream output;
    output << "f" << SA << ", "
           << "f" << RD;

    return opcode + " " + output.str();
}

string EmotionDisasm::disasm_fpu_cvt_w_s(uint32_t instruction)
{
    return disasm_fpu_convert("cvt.w.s", instruction);
}

string EmotionDisasm::disasm_fpu_compare(const string opcode, uint32_t instruction)
{
    stringstream output;
    output << "f" << RD << ", "
           << "f" << RT;

    return opcode + " " + output.str();
}

string EmotionDisasm::disasm_fpu_c_f_s(uint32_t instruction)
{
    return disasm_fpu_compare("c.f.s", instruction);
}

string EmotionDisasm::disasm_fpu_c_lt_s(uint32_t instruction)
{
    return disasm_fpu_compare("c.lt.s", instruction);
}

string EmotionDisasm::disasm_fpu_c_eq_s(uint32_t instruction)
{
    return disasm_fpu_compare("c.eq.s", instruction);
}

string EmotionDisasm::disasm_fpu_c_le_s(uint32_t instruction)
{
    return disasm_fpu_compare("c.le.s", instruction);
}

string EmotionDisasm::disasm_cop_bc1(uint32_t instruction, uint32_t instr_addr)
{

    stringstream output;
    string opcode = "";
    const static char* ops[] = {"bc1f", "bc1fl", "bc1t", "bc1tl"};
    int32_t offset = IMM << 2;
    uint8_t op = RT;
    if (op > 3)
        return unknown_op("BC1", op, 2);
    opcode = ops[op];
    output << "$" << setfill('0') << setw(8) << hex << (instr_addr + 4 + offset);

    return opcode + " " + output.str();
}

string EmotionDisasm::disasm_cop_cvt_s_w(uint32_t instruction)
{
    return disasm_fpu_convert("cvt.s.w", instruction);
}

string EmotionDisasm::get_dest_field(uint8_t field)
{
    const static char vectors[] = {'w', 'z', 'y', 'x'};
    string out;
    for (int i = 3; i >= 0; i--)
    {
        if (field & (1 << i))
            out += vectors[i];
    }
    return out;
}

string EmotionDisasm::get_fsf(uint8_t fsf)
{
    const static string vectors[] = {"x", "y", "z", "w"};
    return vectors[fsf];
}

string EmotionDisasm::disasm_cop2_qmove(const string opcode, uint32_t instruction)
{
    stringstream output;
    output << opcode;
    if (instruction & 1)
        output << ".i";
    output << " " << EmotionEngine::REG(RT) << ", vf" << ((instruction >> 11) & 0x1F);
    return output.str();
}

string EmotionDisasm::disasm_cop2(uint32_t instruction)
{
    uint8_t op = RS;
    if (op >= 0x10)
        return disasm_cop2_special(instruction);
    switch (op)
    {
        case 0x1:
            return disasm_cop2_qmove("qmfc2", instruction);
        case 0x5:
            return disasm_cop2_qmove("qmtc2", instruction);
        default:
            return unknown_op("cop2", op, 2);
    }
}

string EmotionDisasm::disasm_cop2_special_simplemath(const string opcode, uint32_t instruction)
{
    stringstream output;
    uint32_t fd = (instruction >> 6) & 0x1F;
    uint32_t fs = (instruction >> 11) & 0x1F;
    uint32_t ft = (instruction >> 16) & 0x1F;
    uint8_t dest_field = (instruction >> 21) & 0xF;
    string field = "." + get_dest_field(dest_field);
    output << opcode << field;
    output << " vf" << fd;
    output << ", vf" << fs;
    output << ", vf" << ft;
    return output.str();
}

string EmotionDisasm::disasm_cop2_special_bc(const string opcode, uint32_t instruction)
{
    stringstream output;
    string bc_field = get_fsf(instruction & 0x3);
    uint32_t fd = (instruction >> 6) & 0x1F;
    uint32_t fs = (instruction >> 11) & 0x1F;
    uint32_t ft = (instruction >> 16) & 0x1F;
    uint32_t dest_field = (instruction >> 21) & 0xF;

    output << opcode << bc_field << "." << get_dest_field(dest_field);
    output << " vf" << fd << ", vf" << fs << ", vf" << ft << bc_field;
    return output.str();
}

string EmotionDisasm::disasm_cop2_special_q(const string opcode, uint32_t instruction)
{
    stringstream output;
    uint32_t fd = (instruction >> 6) & 0x1F;
    uint32_t fs = (instruction >> 11) & 0x1F;
    uint32_t dest_field = (instruction >> 21) & 0xF;
    output << opcode << "q" << "." << get_dest_field(dest_field);
    output << " vf" << fd << ", vf" << fs << ", Q";
    return output.str();
}

string EmotionDisasm::disasm_cop2_special(uint32_t instruction)
{
    uint8_t op = instruction & 0x3F;
    if (op >= 0x3C)
        return disasm_cop2_special2(instruction);
    switch (op)
    {
        case 0x00:
        case 0x01:
        case 0x02:
        case 0x03:
            return disasm_vaddbc(instruction);
        case 0x04:
        case 0x05:
        case 0x06:
        case 0x07:
            return disasm_vsubbc(instruction);
        case 0x08:
        case 0x09:
        case 0x0A:
        case 0x0B:
            return disasm_vmaddbc(instruction);
        case 0x10:
        case 0x11:
        case 0x12:
        case 0x13:
            return disasm_vmaxbc(instruction);
        case 0x14:
        case 0x15:
        case 0x16:
        case 0x17:
            return disasm_vminibc(instruction);
        case 0x18:
        case 0x19:
        case 0x1A:
        case 0x1B:
            return disasm_vmulbc(instruction);
        case 0x1C:
            return disasm_vmulq(instruction);
        case 0x20:
            return disasm_vaddq(instruction);
        case 0x28:
            return disasm_vadd(instruction);
        case 0x2A:
            return disasm_vmul(instruction);
        case 0x2C:
            return disasm_vsub(instruction);
        case 0x2E:
            return disasm_vopmsub(instruction);
        case 0x30:
            return disasm_viadd(instruction);
        case 0x32:
            return disasm_viaddi(instruction);
        default:
            return unknown_op("cop2 special", op, 2);
    }
}

string EmotionDisasm::disasm_vaddbc(uint32_t instruction)
{
    return disasm_cop2_special_bc("vadd", instruction);
}

string EmotionDisasm::disasm_vsubbc(uint32_t instruction)
{
    return disasm_cop2_special_bc("vsub", instruction);
}

string EmotionDisasm::disasm_vmaddbc(uint32_t instruction)
{
    return disasm_cop2_special_bc("vmadd", instruction);
}

string EmotionDisasm::disasm_vmaxbc(uint32_t instruction)
{
    return disasm_cop2_special_bc("vmax", instruction);
}

string EmotionDisasm::disasm_vminibc(uint32_t instruction)
{
    return disasm_cop2_special_bc("vmini", instruction);
}

string EmotionDisasm::disasm_vmulbc(uint32_t instruction)
{
    return disasm_cop2_special_bc("vmul", instruction);
}

string EmotionDisasm::disasm_vmulq(uint32_t instruction)
{
    return disasm_cop2_special_q("vmul", instruction);
}

string EmotionDisasm::disasm_vaddq(uint32_t instruction)
{
    return disasm_cop2_special_q("vadd", instruction);
}

string EmotionDisasm::disasm_vadd(uint32_t instruction)
{
    return disasm_cop2_special_simplemath("vadd", instruction);
}

string EmotionDisasm::disasm_vmul(uint32_t instruction)
{
    return disasm_cop2_special_simplemath("vmul", instruction);
}

string EmotionDisasm::disasm_vsub(uint32_t instruction)
{
    return disasm_cop2_special_simplemath("vsub", instruction);
}

string EmotionDisasm::disasm_vopmsub(uint32_t instruction)
{
    return disasm_cop2_special_simplemath("vopmsub", instruction);
}

string EmotionDisasm::disasm_viadd(uint32_t instruction)
{
    stringstream output;
    uint32_t id = (instruction >> 6) & 0x1F;
    uint32_t is = (instruction >> 11) & 0x1F;
    uint32_t it = (instruction >> 16) & 0x1F;
    output << "viadd vi" << id << ", vi" << is << ", vi" << it;
    return output.str();
}

string EmotionDisasm::disasm_viaddi(uint32_t instruction)
{
    stringstream output;
    uint32_t imm = (instruction >> 6) & 0x3F;
    uint32_t is = (instruction >> 11) & 0x1F;
    uint32_t id = (instruction >> 16) & 0x1F;
    output << "viaddi vi" << id << ", vi" << is << ", 0x" << imm;
    return output.str();
}

string EmotionDisasm::disasm_cop2_special2(uint32_t instruction)
{
    uint16_t op = (instruction & 0x3) | ((instruction >> 4) & 0x7C);
    switch (op)
    {
        case 0x00:
        case 0x01:
        case 0x02:
        case 0x03:
            return disasm_vaddabc(instruction);
        case 0x08:
        case 0x09:
        case 0x0A:
        case 0x0B:
            return disasm_vmaddabc(instruction);
        case 0x0C:
        case 0x0D:
        case 0x0E:
        case 0x0F:
            return disasm_vmsubabc(instruction);
        case 0x10:
            return disasm_vitof0(instruction);
        case 0x12:
            return disasm_vitof12(instruction);
        case 0x14:
            return disasm_vftoi0(instruction);
        case 0x15:
            return disasm_vftoi4(instruction);
        case 0x16:
            return disasm_vftoi12(instruction);
        case 0x17:
            return disasm_vftoi15(instruction);
        case 0x18:
        case 0x19:
        case 0x1A:
        case 0x1B:
            return disasm_vmulabc(instruction);
        case 0x1D:
            return disasm_vabs(instruction);
        case 0x1F:
            return disasm_vclip(instruction);
        case 0x28:
            return disasm_vadda(instruction);
        case 0x29:
            return disasm_vmadda(instruction);
        case 0x2E:
            return disasm_vopmula(instruction);
        case 0x2F:
            return "vnop";
        case 0x30:
            return disasm_vmove(instruction);
        case 0x31:
            return disasm_vmr32(instruction);
        case 0x35:
            return disasm_vsqi(instruction);
        case 0x38:
            return disasm_vdiv(instruction);
        case 0x39:
            return disasm_vsqrt(instruction);
        case 0x3A:
            return disasm_vrsqrt(instruction);
        case 0x3B:
            return "vwaitq";
        case 0x3F:
            return disasm_viswr(instruction);
        case 0x40:
            return disasm_vrnext(instruction);
        case 0x41:
            return disasm_vrget(instruction);
        case 0x42:
            return disasm_vrinit(instruction);
        case 0x43:
            return disasm_vrxor(instruction);
        default:
            return unknown_op("cop2 special2", op, 2);
    }
}

string EmotionDisasm::disasm_cop2_special2_move(const string opcode, uint32_t instruction)
{
    stringstream output;
    uint32_t fs = (instruction >> 11) & 0x1F;
    uint32_t ft = (instruction >> 16) & 0x1F;
    string field = get_dest_field((instruction >> 21) & 0xF);
    output << opcode << "." << field;
    output << " vf" << fs << ", vf" << ft;
    return output.str();
}

string EmotionDisasm::disasm_cop2_acc(const string opcode, uint32_t instruction)
{
    stringstream output;
    uint32_t fs = (instruction >> 11) & 0x1F;
    uint32_t ft = (instruction >> 16) & 0x1F;
    uint8_t dest_field = (instruction >> 21) & 0xF;
    string field = "." + get_dest_field(dest_field);
    output << opcode << field;
    output << " ACC, vf" << fs;
    output << ", vf" << ft;
    return output.str();
}

string EmotionDisasm::disasm_cop2_acc_bc(const string opcode, uint32_t instruction)
{
    stringstream output;
    string bc_field = get_fsf(instruction & 0x3);
    uint32_t fs = (instruction >> 11) & 0x1F;
    uint32_t ft = (instruction >> 16) & 0x1F;
    uint32_t dest_field = (instruction >> 21) & 0xF;

    output << opcode << bc_field << "." << get_dest_field(dest_field);
    output << " ACC" << ", vf" << fs << ", vf" << ft << bc_field;
    return output.str();
}

string EmotionDisasm::disasm_vaddabc(uint32_t instruction)
{
    return disasm_cop2_acc_bc("vadda", instruction);
}

string EmotionDisasm::disasm_vmaddabc(uint32_t instruction)
{
    return disasm_cop2_acc_bc("vmadda", instruction);
}

string EmotionDisasm::disasm_vmsubabc(uint32_t instruction)
{
    return disasm_cop2_acc_bc("vmsuba", instruction);
}

string EmotionDisasm::disasm_vitof0(uint32_t instruction)
{
    return disasm_cop2_special2_move("vitof0", instruction);
}

string EmotionDisasm::disasm_vitof12(uint32_t instruction)
{
    return disasm_cop2_special2_move("vitof12", instruction);
}

string EmotionDisasm::disasm_vftoi0(uint32_t instruction)
{
    return disasm_cop2_special2_move("vftoi0", instruction);
}

string EmotionDisasm::disasm_vftoi4(uint32_t instruction)
{
    return disasm_cop2_special2_move("vftoi4", instruction);
}

string EmotionDisasm::disasm_vftoi12(uint32_t instruction)
{
    return disasm_cop2_special2_move("vftoi12", instruction);
}

string EmotionDisasm::disasm_vftoi15(uint32_t instruction)
{
    return disasm_cop2_special2_move("vftoi15", instruction);
}

string EmotionDisasm::disasm_vmulabc(uint32_t instruction)
{
    return disasm_cop2_acc_bc("vmula", instruction);
}

string EmotionDisasm::disasm_vabs(uint32_t instruction)
{
    return disasm_cop2_special2_move("vabs", instruction);
}

string EmotionDisasm::disasm_vclip(uint32_t instruction)
{
    stringstream output;
    uint32_t fs = (instruction >> 11) & 0x1F;
    uint32_t ft = (instruction >> 16) & 0x1F;
    output << "vclipw.xyz vf" << fs << ", vf" << ft;
    return output.str();
}

string EmotionDisasm::disasm_vadda(uint32_t instruction)
{
    return disasm_cop2_acc("vadda", instruction);
}

string EmotionDisasm::disasm_vmadda(uint32_t instruction)
{
    return disasm_cop2_acc("vmadda", instruction);
}

string EmotionDisasm::disasm_vopmula(uint32_t instruction)
{
    stringstream output;
    uint32_t fs = (instruction >> 11) & 0x1F;
    uint32_t ft = (instruction >> 16) & 0x1F;
    output << "vopmula.xyz ACC, vf" << fs << ", vf" << ft;
    return output.str();
}

string EmotionDisasm::disasm_vmove(uint32_t instruction)
{
    return disasm_cop2_special2_move("vmove", instruction);
}

string EmotionDisasm::disasm_vmr32(uint32_t instruction)
{
    return disasm_cop2_special2_move("vmr32", instruction);
}

string EmotionDisasm::disasm_vsqi(uint32_t instruction)
{
    stringstream output;
    uint32_t fs = (instruction >> 11) & 0x1F;
    uint32_t it = (instruction >> 16) & 0x1F;
    string field = get_dest_field((instruction >> 21) & 0xF);
    output << "vsqi." << field;
    output << " vf" << fs << ", (vi" << it << "++)";
    return output.str();
}

string EmotionDisasm::disasm_vdiv(uint32_t instruction)
{
    stringstream output;
    uint32_t fs = (instruction >> 11) & 0x1F;
    uint32_t ft = (instruction >> 16) & 0x1F;
    string fsf = get_fsf((instruction >> 21) & 0x3);
    string ftf = get_fsf((instruction >> 23) & 0x3);
    output << "vdiv Q, vf" << fs << fsf << ", vf" << ft << ftf;
    return output.str();
}

string EmotionDisasm::disasm_vsqrt(uint32_t instruction)
{
    stringstream output;
    uint32_t ft = (instruction >> 16) & 0x1F;
    string ftf = get_fsf((instruction >> 23) & 0x3);
    output << "vsqrt Q, vf" << ft << ftf;
    return output.str();
}

string EmotionDisasm::disasm_vrsqrt(uint32_t instruction)
{
    stringstream output;
    uint32_t fs = (instruction >> 11) & 0x1F;
    uint32_t ft = (instruction >> 16) & 0x1F;
    string fsf = get_fsf((instruction >> 21) & 0x3);
    string ftf = get_fsf((instruction >> 23) & 0x3);
    output << "vrsqrt Q, vf" << fs << fsf << ", vf" << ft << ftf;
    return output.str();
}

string EmotionDisasm::disasm_viswr(uint32_t instruction)
{
    stringstream output;
    uint32_t is = (instruction >> 11) & 0x1F;
    uint32_t it = (instruction >> 16) & 0x1F;
    uint8_t dest_field = (instruction >> 21) & 0xF;
    output << "viswr." << get_dest_field(dest_field);
    output << " vi" << it << ", (vi" << is << ")";
    return output.str();
}

string EmotionDisasm::disasm_vrnext(uint32_t instruction)
{
    stringstream output;
    uint32_t ft = (instruction >> 16) & 0x1F;
    uint8_t dest_field = (instruction >> 21) & 0xF;
    output << "vrnext." << get_dest_field(dest_field);
    output << " vf" << ft << ", R";
    return output.str();
}

string EmotionDisasm::disasm_vrget(uint32_t instruction)
{
    stringstream output;
    uint32_t ft = (instruction >> 16) & 0x1F;
    uint8_t dest_field = (instruction >> 21) & 0xF;
    output << "vrget." << get_dest_field(dest_field);
    output << " vf" << ft << ", R";
    return output.str();
}

string EmotionDisasm::disasm_vrinit(uint32_t instruction)
{
    stringstream output;
    uint32_t fs = (instruction >> 11) & 0x1F;
    string fsf = "." + get_fsf((instruction >> 21) & 0x3);
    output << "vrinit R, vf" << fs << fsf;
    return output.str();
}

string EmotionDisasm::disasm_vrxor(uint32_t instruction)
{
    stringstream output;
    uint32_t fs = (instruction >> 11) & 0x1F;
    string fsf = "." + get_fsf((instruction >> 21) & 0x3);
    output << "vrxor R, vf" << fs << fsf;
    return output.str();
}

string EmotionDisasm::disasm_mmi_copy(const string opcode, uint32_t instruction)
{
    stringstream output;
    output << EmotionEngine::REG(RD) << ", "
           << EmotionEngine::REG(RT);

    return opcode + " " + output.str();
}

string EmotionDisasm::disasm_mmi(uint32_t instruction, uint32_t instr_addr)
{
    int op = instruction & 0x3F;
    switch (op)
    {
        case 0x04:
            return disasm_plzcw(instruction);
        case 0x08:
            return disasm_mmi0(instruction);
        case 0x09:
            return disasm_mmi2(instruction);
        case 0x10:
            return disasm_mfhi1(instruction);
        case 0x11:
            return disasm_mthi1(instruction);
        case 0x12:
            return disasm_mflo1(instruction);
        case 0x13:
            return disasm_mtlo1(instruction);
        case 0x18:
            return disasm_mult1(instruction);
        case 0x1A:
            return disasm_div1(instruction);
        case 0x1B:
            return disasm_divu1(instruction);
        case 0x28:
            return disasm_mmi1(instruction);
        case 0x29:
            return disasm_mmi3(instruction);
        case 0x34:
            return disasm_special_shift("psllh", instruction);
        case 0x36:
            return disasm_special_shift("psrlh", instruction);
        case 0x37:
            return disasm_special_shift("psrah", instruction);
        case 0x3C:
            return disasm_special_shift("psllw", instruction);
        case 0x3E:
            return disasm_special_shift("psrlw", instruction);
        case 0x3F:
            return disasm_special_shift("psraw", instruction);
        default:
            return unknown_op("mmi", op, 2);
    }
}

string EmotionDisasm::disasm_plzcw(uint32_t instruction)
{
    stringstream output;
    string opcode = "plzcw";
    output << EmotionEngine::REG(RD) << ", "
           << EmotionEngine::REG(RS);
    return opcode + " " + output.str();
}

string EmotionDisasm::disasm_mmi0(uint32_t instruction)
{
    int op = (instruction >> 6) & 0x1F;
    switch (op)
    {
        case 0x00:
            return disasm_special_simplemath("paddw", instruction);
        case 0x01:
            return disasm_special_simplemath("psubw", instruction);
        case 0x02:
            return disasm_special_simplemath("pcgtw", instruction);
        case 0x03:
            return disasm_special_simplemath("pmaxw", instruction);
        case 0x04:
            return disasm_special_simplemath("paddh", instruction);
        case 0x05:
            return disasm_special_simplemath("psubh", instruction);
        case 0x06:
            return disasm_special_simplemath("pcgth", instruction);
        case 0x07:
            return disasm_special_simplemath("pmaxh", instruction);
        case 0x08:
            return disasm_special_simplemath("paddb", instruction);
        case 0x09:
            return disasm_psubb(instruction);
        case 0x0A:
            return disasm_special_simplemath("pcgtb", instruction);
        case 0x10:
            return disasm_special_simplemath("paddsw", instruction);
        case 0x11:
            return disasm_special_simplemath("psubsw", instruction);
        case 0x12:
            return disasm_special_simplemath("pextlw", instruction);
        case 0x13:
            return disasm_special_simplemath("ppacw", instruction);
        case 0x14:
            return disasm_special_simplemath("paddsh", instruction);
        case 0x15:
            return disasm_special_simplemath("psubsh", instruction);
        case 0x16:
            return disasm_special_simplemath("pextlh", instruction);
        case 0x17:
            return disasm_special_simplemath("ppach", instruction);
        case 0x18:
            return disasm_special_simplemath("paddsb", instruction);
        case 0x19:
            return disasm_special_simplemath("psubsb", instruction);
        case 0x1A:
            return disasm_special_simplemath("pextlb", instruction);
        case 0x1B:
            return disasm_special_simplemath("ppacb", instruction);
        case 0x1E:
            return disasm_pext5(instruction);
        case 0x1F:
            return disasm_mmi_copy("ppac5", instruction);
        default:
            return unknown_op("mmi0", op, 2);
    }
}

string EmotionDisasm::disasm_psubb(uint32_t instruction)
{
    return disasm_special_simplemath("psubb", instruction);
}

string EmotionDisasm::disasm_pext5(uint32_t instruction)
{
    stringstream output;
    output << "pext5 " << EmotionEngine::REG(RD) << ", " << EmotionEngine::REG(RT);
    return output.str();
}

string EmotionDisasm::disasm_mmi1(uint32_t instruction)
{
    uint8_t op = (instruction >> 6) & 0x1F;
    switch (op)
    {
        case 0x01:
            return disasm_pabsw(instruction);
        case 0x02:
            return disasm_special_simplemath("pceqw", instruction);
        case 0x03:
            return disasm_special_simplemath("pminw", instruction);
        case 0x04:
            return disasm_special_simplemath("padsbh", instruction);
        case 0x05:
            return disasm_pabsh(instruction);
        case 0x06:
            return disasm_special_simplemath("pceqh", instruction);
        case 0x07:
            return disasm_special_simplemath("pminh", instruction);
        case 0x0A:
            return disasm_special_simplemath("pceqb", instruction);
        case 0x10:
            return disasm_special_simplemath("padduw", instruction);
        case 0x11:
            return disasm_special_simplemath("psubuw", instruction);
        case 0x12:
            return disasm_special_simplemath("pextuw", instruction);
        case 0x14:
            return disasm_special_simplemath("padduh", instruction);
        case 0x15:
            return disasm_special_simplemath("psubuh", instruction);
        case 0x16:
            return disasm_mmi_copy("pextuh", instruction);
        case 0x18:
            return disasm_special_simplemath("paddub", instruction);
        case 0x19:
            return disasm_special_simplemath("psubub", instruction);
        case 0x1A:
            return disasm_mmi_copy("pextub", instruction);
        default:
            return unknown_op("mmi1", op, 2);
    }
}

string EmotionDisasm::disasm_pabsw(uint32_t instruction)
{
    stringstream output;
    output << "pabsw " << EmotionEngine::REG(RD) << ", " << EmotionEngine::REG(RT);
    return output.str();
}

string EmotionDisasm::disasm_pabsh(uint32_t instruction)
{
    stringstream output;
    output << "pabsh " << EmotionEngine::REG(RD) << ", " << EmotionEngine::REG(RT);
    return output.str();
}

string EmotionDisasm::disasm_mmi2(uint32_t instruction)
{
    int op = (instruction >> 6) & 0x1F;
    switch (op)
    {
        case 0x02:
            return disasm_variableshift("psllvw", instruction);
        case 0x03:
            return disasm_variableshift("psrlvw", instruction);
        case 0x08:
            return disasm_pmfhi(instruction);
        case 0x09:
            return disasm_pmflo(instruction);
        case 0x0A:
            return disasm_special_simplemath("pinth", instruction);
        case 0x0E:
            return disasm_pcpyld(instruction);
        case 0x12:
            return disasm_pand(instruction);
        case 0x13:
            return disasm_pxor(instruction);
        case 0x1A:
            return disasm_mmi_copy("pexeh", instruction);
        case 0x1B:
            return disasm_mmi_copy("prevh", instruction);
        case 0x1C:
            return disasm_special_simplemath("pmulth", instruction);
        case 0x1E:
            return disasm_mmi_copy("pexew", instruction);
        case 0x1F:
            return disasm_mmi_copy("prot3w", instruction);
        default:
            return unknown_op("mmi2", op, 2);
    }
}

string EmotionDisasm::disasm_pmfhi(uint32_t instruction)
{
    return disasm_movereg("pmfhi", instruction);
}

string EmotionDisasm::disasm_pmflo(uint32_t instruction)
{
    return disasm_movereg("pmflo", instruction);
}

string EmotionDisasm::disasm_pcpyld(uint32_t instruction)
{
    return disasm_special_simplemath("pcpyld", instruction);
}

string EmotionDisasm::disasm_pand(uint32_t instruction)
{
    return disasm_special_simplemath("pand", instruction);
}

string EmotionDisasm::disasm_pxor(uint32_t instruction)
{
    return disasm_special_simplemath("pxor", instruction);
}

string EmotionDisasm::disasm_mfhi1(uint32_t instruction)
{
    return disasm_movereg("mfhi1", instruction);
}

string EmotionDisasm::disasm_mthi1(uint32_t instruction)
{
    return disasm_moveto("mthi1", instruction);
}

string EmotionDisasm::disasm_mflo1(uint32_t instruction)
{
    return disasm_movereg("mflo1", instruction);
}

string EmotionDisasm::disasm_mtlo1(uint32_t instruction)
{
    return disasm_moveto("mtlo1", instruction);
}

string EmotionDisasm::disasm_mult1(uint32_t instruction)
{
    return disasm_special_simplemath("mult1", instruction);
}

string EmotionDisasm::disasm_div1(uint32_t instruction)
{
    return disasm_division("div1", instruction);
}

string EmotionDisasm::disasm_divu1(uint32_t instruction)
{
    return disasm_division("divu1", instruction);
}

string EmotionDisasm::disasm_mmi3(uint32_t instruction)
{
    switch (SA)
    {
        case 0x03:
            return disasm_variableshift("psravw", instruction);
        case 0x08:
            return disasm_pmthi(instruction);
        case 0x09:
            return disasm_pmtlo(instruction);
        case 0x0A:
            return disasm_special_simplemath("pinteh", instruction);
        case 0x0E:
            return disasm_pcpyud(instruction);
        case 0x12:
            return disasm_por(instruction);
        case 0x13:
            return disasm_pnor(instruction);
        case 0x1A:
            return disasm_mmi_copy("pexch", instruction);
        case 0x1B:
            return disasm_mmi_copy("pcpyh", instruction);
        case 0x1E:
            return disasm_mmi_copy("pexcw", instruction);
        default:
            return unknown_op("mmi3", SA, 2);
    }
}

string EmotionDisasm::disasm_pmthi(uint32_t instruction)
{
    return disasm_moveto("pmthi", instruction);
}

string EmotionDisasm::disasm_pmtlo(uint32_t instruction)
{
    return disasm_moveto("pmtlo", instruction);
}

string EmotionDisasm::disasm_pcpyud(uint32_t instruction)
{
    return disasm_special_simplemath("pcpyud", instruction);
}

string EmotionDisasm::disasm_por(uint32_t instruction)
{
    return disasm_special_simplemath("por", instruction);
}

string EmotionDisasm::disasm_pnor(uint32_t instruction)
{
    return disasm_special_simplemath("pnor", instruction);
}

string EmotionDisasm::unknown_op(const string optype, uint32_t op, int width)
{
    stringstream output;
    output << "Unrecognized " << optype << " op "
           << "$" << setfill('0') << setw(width) << hex << op;
    return output.str();
}

