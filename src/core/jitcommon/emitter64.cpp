#include "emitter64.hpp"

#define DISP32 0b101

Emitter64::Emitter64(JitBlock* cache) : block(cache)
{

}

void Emitter64::rex_r(REG_64 reg)
{
    if (reg & 0x8)
        block->write<uint8_t>(0x44);
}

void Emitter64::rex_rm(REG_64 rm)
{
    if (rm & 0x8)
        block->write<uint8_t>(0x41);
}

void Emitter64::rex_r_rm(REG_64 reg, REG_64 rm)
{
    uint8_t rex = 0x40;
    if (reg & 0x8)
        rex |= 0x4;
    if (rm & 0x8)
        rex |= 0x1;

    if (rex & 0xF)
        block->write<uint8_t>(rex);
}

void Emitter64::rexw_r(REG_64 reg)
{
    uint8_t rex = 0x48;
    if (reg & 0x8)
        rex |= 0x4;
    block->write<uint8_t>(rex);
}

void Emitter64::rexw_rm(REG_64 rm)
{
    uint8_t rex = 0x48;
    if (rm & 0x8)
        rex |= 0x1;
    block->write<uint8_t>(rex);
}

void Emitter64::rexw_r_rm(REG_64 reg, REG_64 rm)
{
    uint8_t rex = 0x48;
    if (reg & 0x8)
        rex |= 0x4;
    if (rm & 0x8)
        rex |= 0x1;
    block->write<uint8_t>(rex);
}

void Emitter64::vex(REG_64 source, REG_64 source2, REG_64 dest, bool packed)
{
    if (source2 & 0x8)
        vex3(source, source2, dest, packed);
    else
        vex2(source, dest, packed);
}

void Emitter64::vex2(REG_64 source, REG_64 dest, bool packed)
{
    block->write<uint8_t>(0xC5);
    uint8_t result = 0xF8;
    if (!packed)
        result |= 0x2;
    if (dest & 0x8)
        result -= 0x80;
    result += -(source << 3);
    block->write<uint8_t>(result);
}

void Emitter64::vex3(REG_64 source, REG_64 source2, REG_64 dest, bool packed)
{
    block->write<uint8_t>(0xC4);
    uint8_t result = 0xC1;
    if (dest & 0x8)
        result -= 0x80;
    block->write<uint8_t>(result);
    result = 0x78;
    if (!packed)
        result |= 0x2;
    result += -(source << 3);
    block->write<uint8_t>(result);
}

void Emitter64::modrm(uint8_t mode, uint8_t reg, uint8_t rm)
{
    block->write<uint8_t>(((mode & 0x3) << 6) | ((reg & 0x7) << 3) | (rm & 0x7));
}

int Emitter64::get_rip_offset(uint64_t addr)
{
    int64_t offset = (uint64_t)block->get_literal_offset<uint64_t>(addr);
    offset -= (uint64_t) block->get_code_pos();
    return offset;
}

void Emitter64::load_addr(uint64_t addr, REG_64 dest)
{
    int offset = get_rip_offset(addr);
    rexw_r(dest);
    block->write<uint8_t>(0x8B);
    modrm(0, dest, DISP32);
    block->write<uint32_t>(offset - 7);
}

void Emitter64::ADD16_REG(REG_64 source, REG_64 dest)
{
    block->write<uint8_t>(0x66);
    rex_r_rm(source, dest);
    block->write<uint8_t>(0x01);
    modrm(0b11, source, dest);
}

void Emitter64::ADD16_REG_IMM(uint16_t imm, REG_64 dest)
{
    block->write<uint8_t>(0x66);
    rex_rm(dest);
    block->write<uint8_t>(0x81);
    modrm(0b11, 0, dest);
    block->write<uint16_t>(imm);
}

void Emitter64::ADD32_REG(REG_64 source, REG_64 dest)
{
    rex_r_rm(source, dest);
    block->write<uint8_t>(0x01);
    modrm(0b11, source, dest);
}

void Emitter64::ADD32_REG_IMM(uint32_t imm, REG_64 dest)
{
    rex_rm(dest);
    block->write<uint8_t>(0x81);
    modrm(0b11, 0, dest);
    block->write<uint32_t>(imm);
}

void Emitter64::ADD64_REG(REG_64 source, REG_64 dest)
{
    rexw_r_rm(source, dest);
    block->write<uint8_t>(0x01);
    modrm(0b11, source, dest);
}

void Emitter64::ADD64_REG_IMM(uint32_t imm, REG_64 dest)
{
    rexw_rm(dest);
    block->write<uint8_t>(0x81);
    modrm(0b11, 0, dest);
    block->write<uint32_t>(imm);
}

void Emitter64::INC16(REG_64 dest)
{
    block->write<uint8_t>(0x66);
    rex_rm(dest);
    block->write<uint8_t>(0xFF);
    modrm(0b11, 0, dest);
}

void Emitter64::AND8_REG_IMM(uint8_t imm, REG_64 dest)
{
    rex_rm(dest);
    block->write<uint8_t>(0x80);
    modrm(0b11, 4, dest);
    block->write<uint8_t>(imm);
}

void Emitter64::AND16_AX(uint16_t imm)
{
    block->write<uint8_t>(0x66);
    block->write<uint8_t>(0x25);
    block->write<uint16_t>(imm);
}

void Emitter64::AND16_REG(REG_64 source, REG_64 dest)
{
    block->write<uint8_t>(0x66);
    rex_r_rm(source, dest);
    block->write<uint8_t>(0x21);
    modrm(0b11, source, dest);
}

void Emitter64::AND16_REG_IMM(uint16_t imm, REG_64 dest)
{
    block->write<uint8_t>(0x66);
    rex_rm(dest);
    block->write<uint8_t>(0x81);
    modrm(0b11, 4, dest);
    block->write<uint16_t>(imm);
}

void Emitter64::AND32_EAX(uint32_t imm)
{
    block->write<uint8_t>(0x25);
    block->write<uint32_t>(imm);
}

void Emitter64::AND32_REG(REG_64 source, REG_64 dest)
{
    rex_r_rm(source, dest);
    block->write<uint8_t>(0x21);
    modrm(0b11, source, dest);
}

void Emitter64::AND32_REG_IMM(uint32_t imm, REG_64 dest)
{
    rex_rm(dest);
    block->write<uint8_t>(0x81);
    modrm(0b11, 4, dest);
    block->write<uint32_t>(imm);
}

void Emitter64::AND64_REG(REG_64 source, REG_64 dest)
{
    rexw_r_rm(source, dest);
    block->write<uint8_t>(0x21);
    modrm(0b11, source, dest);
}

void Emitter64::CMOVCC16_REG(ConditionCode cc, REG_64 source, REG_64 dest)
{
    block->write<uint8_t>(0x66);
    rex_r_rm(dest, source);
    block->write<uint8_t>(0x0F);
    block->write<uint8_t>((int)cc | 0x40);
    modrm(0b11, dest, source);
}

void Emitter64::CMOVCC32_REG(ConditionCode cc, REG_64 source, REG_64 dest)
{
    rex_r_rm(dest, source);
    block->write<uint8_t>(0x0F);
    block->write<uint8_t>((int)cc | 0x40);
    modrm(0b11, dest, source);
}

void Emitter64::CMOVCC64_REG(ConditionCode cc, REG_64 source, REG_64 dest)
{
    rexw_r_rm(dest, source);
    block->write<uint8_t>(0x0F);
    block->write<uint8_t>((int)cc | 0x40);
    modrm(0b11, dest, source);
}

void Emitter64::CMOVCC16_MEM(ConditionCode cc, REG_64 source, REG_64 indir_dest, uint32_t offset)
{
    block->write<uint8_t>(0x66);
    block->write<uint8_t>(0x0F);
    block->write<uint8_t>((int)cc | 0x40);

    if ((indir_dest & 7) == 5 || offset)
    {
        modrm(0b10, indir_dest, source);
    }
    else
        modrm(0, indir_dest, source);
    if ((indir_dest & 7) == 4)
        block->write<uint8_t>(0x24);
    if ((indir_dest & 7) == 5 || offset)
        block->write<uint32_t>(offset);
}

void Emitter64::CMOVCC32_MEM(ConditionCode cc, REG_64 source, REG_64 indir_dest, uint32_t offset)
{
    block->write<uint8_t>(0x0F);
    block->write<uint8_t>((int)cc | 0x40);

    if ((indir_dest & 7) == 5 || offset)
    {
        modrm(0b10, indir_dest, source);
    }
    else
        modrm(0, indir_dest, source);
    if ((indir_dest & 7) == 4)
        block->write<uint8_t>(0x24);
    if ((indir_dest & 7) == 5 || offset)
        block->write<uint32_t>(offset);
}

void Emitter64::CMOVCC64_MEM(ConditionCode cc, REG_64 source, REG_64 indir_dest, uint32_t offset)
{
    rexw_r_rm(indir_dest, source);
    block->write<uint8_t>(0x0F);
    block->write<uint8_t>((int)cc | 0x40);

    if ((indir_dest & 7) == 5 || offset)
    {
        modrm(0b10, indir_dest, source);
    }
    else
        modrm(0, indir_dest, source);
    if ((indir_dest & 7) == 4)
        block->write<uint8_t>(0x24);
    if ((indir_dest & 7) == 5 || offset)
        block->write<uint32_t>(offset);
}

void Emitter64::CMP8_REG(REG_64 op2, REG_64 op1)
{
    rex_r_rm(op2, op1);

    // FIXME: I still don't know anything about encoding these instructions, woooo
    if (!(op2 & 0x8) && !(op1 & 0x8))
        block->write<uint8_t>(0x40);

    block->write<uint8_t>(0x38);
    modrm(0b11, op2, op1);
}

void Emitter64::CMP8_IMM_MEM(uint32_t imm, REG_64 mem, uint32_t offset)
{
    rex_rm(mem);
    block->write<uint8_t>(0x80);
    if ((mem & 7) == 5 || offset != 0)
    {
        modrm(0b10, 7, mem);
        block->write<uint32_t>(offset);
    }
    else
    {
        modrm(0, 7, mem);
    }
    block->write<uint8_t>(imm);
}

void Emitter64::CMP16_IMM(uint16_t imm, REG_64 op)
{
    block->write<uint8_t>(0x66);
    rex_rm(op);
    block->write<uint8_t>(0x81);
    modrm(0b11, 7, op);
    block->write<uint16_t>(imm);
}

void Emitter64::CMP16_REG(REG_64 op2, REG_64 op1)
{
    block->write<uint8_t>(0x66);
    rex_r_rm(op2, op1);
    block->write<uint8_t>(0x39);
    modrm(0b11, op2, op1);
}

void Emitter64::CMP32_IMM(uint32_t imm, REG_64 op)
{
    rex_rm(op);
    block->write<uint8_t>(0x81);
    modrm(0b11, 7, op);
    block->write<uint32_t>(imm);
}

void Emitter64::CMP32_IMM_MEM(uint32_t imm, REG_64 mem, uint32_t offset)
{
    rex_rm(mem);
    block->write<uint8_t>(0x81);
    if ((mem & 7) == 5 || offset != 0)
    {
        modrm(0b10, 7, mem);
        block->write<uint32_t>(offset);
    }
    else
    {
        modrm(0, 7, mem);
    }
    block->write<uint32_t>(imm);
}

void Emitter64::CMP32_REG(REG_64 op2, REG_64 op1)
{
    rex_r_rm(op2, op1);
    block->write<uint8_t>(0x39);
    modrm(0b11, op2, op1);
}

void Emitter64::CMP32_EAX(uint32_t imm)
{
    block->write<uint8_t>(0x3D);
    block->write<uint32_t>(imm);
}

void Emitter64::CMP64_IMM(uint32_t imm, REG_64 op)
{
    rexw_rm(op);
    block->write<uint8_t>(0x81);
    modrm(0b11, 7, op);
    block->write<uint32_t>(imm);
}

void Emitter64::CMP64_REG(REG_64 op2, REG_64 op1)
{
    rexw_r_rm(op2, op1);
    block->write<uint8_t>(0x39);
    modrm(0b11, op2, op1);
}

void Emitter64::CWD()
{
    block->write<uint8_t>(0x66);
    block->write<uint8_t>(0x99);
}

void Emitter64::CDQ()
{
    block->write<uint8_t>(0x99);
}

void Emitter64::CQO()
{
    block->write<uint8_t>(0x48);
    block->write<uint8_t>(0x99);
}

void Emitter64::DEC8(REG_64 dest)
{
    rex_rm(dest);
    block->write<uint8_t>(0xFE);
    modrm(0b11, 1, dest);
}

void Emitter64::DEC16(REG_64 dest)
{
    block->write<uint8_t>(0x66);
    rex_rm(dest);
    block->write<uint8_t>(0xFF);
    modrm(0b11, 1, dest);
}

void Emitter64::DEC32(REG_64 dest)
{
    rex_rm(dest);
    block->write<uint8_t>(0xFF);
    modrm(0b11, 1, dest);
}

void Emitter64::DEC64(REG_64 dest)
{
    rexw_rm(dest);
    block->write<uint8_t>(0xFF);
    modrm(0b11, 1, dest);
}

void Emitter64::DIV32(REG_64 dest)
{
    rex_rm(dest);
    block->write<uint8_t>(0xF7);
    modrm(0b11, 6, dest);
}

void Emitter64::IDIV32(REG_64 dest)
{
    rex_rm(dest);
    block->write<uint8_t>(0xF7);
    modrm(0b11, 7, dest);
}

void Emitter64::MUL32(REG_64 dest)
{
    rex_rm(dest);
    block->write<uint8_t>(0xF7);
    modrm(0b11, 4, dest);
}

void Emitter64::IMUL32(REG_64 dest)
{
    rex_rm(dest);
    block->write<uint8_t>(0xF7);
    modrm(0b11, 5, dest);
}

void Emitter64::NOT16(REG_64 dest)
{
    block->write<uint8_t>(0x66);
    rex_rm(dest);
    block->write<uint8_t>(0xF7);
    modrm(0b11, 2, dest);
}

void Emitter64::NOT32(REG_64 dest)
{
    rex_rm(dest);
    block->write<uint8_t>(0xF7);
    modrm(0b11, 2, dest);
}

void Emitter64::NOT64(REG_64 dest)
{
    rexw_rm(dest);
    block->write<uint8_t>(0xF7);
    modrm(0b11, 2, dest);
}

void Emitter64::NEG16(REG_64 dest)
{
    block->write<uint8_t>(0x66);
    rex_rm(dest);
    block->write<uint8_t>(0xF7);
    modrm(0b11, 3, dest);
}

void Emitter64::NEG32(REG_64 dest)
{
    rex_rm(dest);
    block->write<uint8_t>(0xF7);
    modrm(0b11, 3, dest);
}

void Emitter64::NEG64(REG_64 dest)
{
    rexw_rm(dest);
    block->write<uint8_t>(0xF7);
    modrm(0b11, 3, dest);
}

void Emitter64::OR16_REG(REG_64 source, REG_64 dest)
{
    block->write<uint8_t>(0x66);
    rex_r_rm(source, dest);
    block->write<uint8_t>(0x09);
    modrm(0b11, source, dest);
}

void Emitter64::OR16_REG_IMM(uint16_t imm, REG_64 dest)
{
    block->write<uint8_t>(0x66);
    rex_rm(dest);
    block->write<uint8_t>(0x81);
    modrm(0b11, 1, dest);
    block->write<uint16_t>(imm);
}

void Emitter64::OR32_REG(REG_64 source, REG_64 dest)
{
    rex_r_rm(source, dest);
    block->write<uint8_t>(0x09);
    modrm(0b11, source, dest);
}

void Emitter64::OR32_REG_IMM(uint32_t imm, REG_64 dest)
{
    rex_rm(dest);
    block->write<uint8_t>(0x81);
    modrm(0b11, 1, dest);
    block->write<uint32_t>(imm);
}

void Emitter64::OR32_EAX(uint32_t imm)
{
    block->write<uint8_t>(0x0D);
    block->write<uint32_t>(imm);
}

void Emitter64::OR64_REG(REG_64 source, REG_64 dest)
{
    rexw_r_rm(source, dest);
    block->write<uint8_t>(0x09);
    modrm(0b11, source, dest);
}

void Emitter64::SETCC_REG(ConditionCode cc, REG_64 dest)
{
    rex_rm(dest);

    // FIXME: fix encoding so we don't explicitly write 0x40 here :D
    if (!(dest & 0x8))
        block->write<uint8_t>(0x40);

    block->write<uint8_t>(0x0F);
    block->write<uint8_t>((int)cc | 0x90);
    modrm(0b11, 0, dest);
}

void Emitter64::SETCC_MEM(ConditionCode cc, REG_64 indir_dest, uint32_t offset)
{
    rex_rm(indir_dest);
    block->write<uint8_t>(0x0F);
    block->write<uint8_t>((int)cc | 0x90);

    if ((indir_dest & 7) == 5 || offset)
    {
        modrm(0b10, 0, indir_dest);
    }
    else
        modrm(0, 0, indir_dest);
    if ((indir_dest & 7) == 4)
        block->write<uint8_t>(0x24);
    if ((indir_dest & 7) == 5 || offset)
        block->write<uint32_t>(offset);
}

void Emitter64::SHL8_REG_1(REG_64 dest)
{
    rex_rm(dest);
    block->write<uint8_t>(0xD0);
    modrm(0b11, 4, dest);
}

void Emitter64::SHL16_REG_1(REG_64 dest)
{
    block->write<uint8_t>(0x66);
    rex_rm(dest);
    block->write<uint8_t>(0xD1);
    modrm(0b11, 4, dest);
}

void Emitter64::SHL16_CL(REG_64 dest)
{
    block->write<uint8_t>(0x66);
    rex_rm(dest);
    block->write<uint8_t>(0xD3);
    modrm(0b11, 4, dest);
}

void Emitter64::SHL16_REG_IMM(uint8_t shift, REG_64 dest)
{
    block->write<uint8_t>(0x66);
    rex_rm(dest);
    block->write<uint8_t>(0xC1);
    modrm(0b11, 4, dest);
    block->write<uint8_t>(shift);
}

void Emitter64::SHL32_CL(REG_64 dest)
{
    rex_rm(dest);
    block->write<uint8_t>(0xD3);
    modrm(0b11, 4, dest);
}

void Emitter64::SHL32_REG_IMM(uint8_t shift, REG_64 dest)
{
    rex_rm(dest);
    block->write<uint8_t>(0xC1);
    modrm(0b11, 4, dest);
    block->write<uint8_t>(shift);
}

void Emitter64::SHL64_CL(REG_64 dest)
{
    rexw_rm(dest);
    block->write<uint8_t>(0xD3);
    modrm(0b11, 4, dest);
}

void Emitter64::SHL64_REG_IMM(uint8_t shift, REG_64 dest)
{
    rexw_rm(dest);
    block->write<uint8_t>(0xC1);
    modrm(0b11, 4, dest);
    block->write<uint8_t>(shift);
}

void Emitter64::SHR16_CL(REG_64 dest)
{
    block->write<uint8_t>(0x66);
    rex_rm(dest);
    block->write<uint8_t>(0xD3);
    modrm(0b11, 5, dest);
}

void Emitter64::SHR16_REG_IMM(uint8_t shift, REG_64 dest)
{
    block->write<uint8_t>(0x66);
    rex_rm(dest);
    block->write<uint8_t>(0xC1);
    modrm(0b11, 5, dest);
    block->write<uint8_t>(shift);
}

void Emitter64::SHR32_CL(REG_64 dest)
{
    rex_rm(dest);
    block->write<uint8_t>(0xD3);
    modrm(0b11, 5, dest);
}

void Emitter64::SHR32_REG_IMM(uint8_t shift, REG_64 dest)
{
    rex_rm(dest);
    block->write<uint8_t>(0xC1);
    modrm(0b11, 5, dest);
    block->write<uint8_t>(shift);
}

void Emitter64::SHR64_CL(REG_64 dest)
{
    rexw_rm(dest);
    block->write<uint8_t>(0xD3);
    modrm(0b11, 5, dest);
}

void Emitter64::SHR64_REG_IMM(uint8_t shift, REG_64 dest)
{
    rexw_rm(dest);
    block->write<uint8_t>(0xC1);
    modrm(0b11, 5, dest);
    block->write<uint8_t>(shift);
}

void Emitter64::SAR16_CL(REG_64 dest)
{
    block->write<uint8_t>(0x66);
    rex_rm(dest);
    block->write<uint8_t>(0xD3);
    modrm(0b11, 7, dest);
}

void Emitter64::SAR16_REG_IMM(uint8_t shift, REG_64 dest)
{
    block->write<uint8_t>(0x66);
    rex_rm(dest);
    block->write<uint8_t>(0xC1);
    modrm(0b11, 7, dest);
    block->write<uint8_t>(shift);
}

void Emitter64::SAR32_CL(REG_64 dest)
{
    rex_rm(dest);
    block->write<uint8_t>(0xD3);
    modrm(0b11, 7, dest);
}

void Emitter64::SAR32_REG_IMM(uint8_t shift, REG_64 dest)
{
    rex_rm(dest);
    block->write<uint8_t>(0xC1);
    modrm(0b11, 7, dest);
    block->write<uint8_t>(shift);
}

void Emitter64::SAR64_CL(REG_64 dest)
{
    rexw_rm(dest);
    block->write<uint8_t>(0xD3);
    modrm(0b11, 7, dest);
}

void Emitter64::SAR64_REG_IMM(uint8_t shift, REG_64 dest)
{
    rexw_rm(dest);
    block->write<uint8_t>(0xC1);
    modrm(0b11, 7, dest);
    block->write<uint8_t>(shift);
}

void Emitter64::SUB16_REG_IMM(uint16_t imm, REG_64 dest)
{
    block->write<uint8_t>(0x66);
    rex_rm(dest);
    block->write<uint8_t>(0x81);
    modrm(0b11, 5, dest);
    block->write<uint16_t>(imm);
}

void Emitter64::SUB32_REG(REG_64 source, REG_64 dest)
{
    rex_r_rm(source, dest);
    block->write<uint8_t>(0x29);
    modrm(0b11, source, dest);
}

void Emitter64::SUB32_MEM_IMM(uint32_t imm, REG_64 mem, uint32_t offset)
{
    rex_rm(mem);
    block->write<uint8_t>(0x81);
    if ((mem & 7) == 5 || offset != 0)
    {
        modrm(0b10, 5, mem);
        block->write<uint32_t>(offset);
    }
    else
    {
        modrm(0, 5, mem);
    }
    block->write<uint32_t>(imm);
}

void Emitter64::SUB64_REG(REG_64 source, REG_64 dest)
{
    rexw_r_rm(source, dest);
    block->write<uint8_t>(0x29);
    modrm(0b11, source, dest);
}

void Emitter64::SUB64_REG_IMM(uint32_t imm, REG_64 dest)
{
    rexw_rm(dest);
    block->write<uint8_t>(0x81);
    modrm(0b11, 5, dest);
    block->write<uint32_t>(imm);
}

void Emitter64::TEST8_REG(REG_64 op2, REG_64 op1)
{
    rex_r_rm(op2, op1);
    block->write<uint8_t>(0x84);
    modrm(0b11, op2, op1);
}

void Emitter64::TEST8_REG_IMM(uint8_t imm, REG_64 op1)
{
    rex_r_rm((REG_64)0, op1);
    block->write<uint8_t>(0xF6);
    modrm(0b11, 0, op1);
    block->write<uint8_t>(imm);
}

void Emitter64::TEST8_IMM_MEM(uint8_t imm, REG_64 mem, uint32_t offset)
{
    rex_r_rm((REG_64)0, mem);
    block->write<uint8_t>(0xF6);
    if ((mem & 7) == 5 || offset != 0)
    {
        modrm(0b10, 0, mem);
        block->write<uint32_t>(offset);
    }
    else
    {
        modrm(0, 0, mem);
    }
    block->write<uint8_t>(imm);
}

void Emitter64::TEST16_REG(REG_64 op2, REG_64 op1)
{
    block->write<uint8_t>(0x66);
    rex_r_rm(op2, op1);
    block->write<uint8_t>(0x85);
    modrm(0b11, op2, op1);
}

void Emitter64::TEST16_REG_IMM(uint16_t imm, REG_64 op1)
{
    block->write<uint8_t>(0x66);
    rex_r_rm((REG_64)0, op1);
    block->write<uint8_t>(0xF7);
    modrm(0b11, 0, op1);
    block->write<uint16_t>(imm);
}

void Emitter64::TEST32_EAX(uint32_t imm)
{
    block->write<uint8_t>(0xA9);
    block->write<uint32_t>(imm);
}

void Emitter64::TEST32_REG(REG_64 op2, REG_64 op1)
{
    rex_r_rm(op2, op1);
    block->write<uint8_t>(0x85);
    modrm(0b11, op2, op1);
}

void Emitter64::TEST32_REG_IMM(uint32_t imm, REG_64 op1)
{
    rex_r_rm((REG_64)0, op1);
    block->write<uint8_t>(0xF7);
    modrm(0b11, 0, op1);
    block->write<uint32_t>(imm);
}

void Emitter64::TEST64_REG(REG_64 op2, REG_64 op1)
{
    rexw_r_rm(op2, op1);
    block->write<uint8_t>(0x85);
    modrm(0b11, op2, op1);
}

void Emitter64::TEST64_REG_IMM(uint32_t imm, REG_64 op1)
{
    rexw_r_rm((REG_64)0, op1);
    block->write<uint8_t>(0xF7);
    modrm(0b11, 0, op1);
    block->write<uint32_t>(imm);
}

void Emitter64::XOR16_REG(REG_64 source, REG_64 dest)
{
    block->write<uint8_t>(0x66);
    rex_r_rm(source, dest);
    block->write<uint8_t>(0x31);
    modrm(0b11, source, dest);
}

void Emitter64::XOR16_REG_IMM(uint16_t imm, REG_64 dest)
{
    block->write<uint8_t>(0x66);
    rex_rm(dest);
    block->write<uint8_t>(0x81);
    modrm(0b11, 6, dest);
    block->write<uint16_t>(imm);
}

void Emitter64::XOR32_REG(REG_64 source, REG_64 dest)
{
    rex_r_rm(source, dest);
    block->write<uint8_t>(0x31);
    modrm(0b11, source, dest);
}

void Emitter64::XOR32_EAX(uint32_t imm)
{
    block->write<uint8_t>(0x35);
    block->write<uint32_t>(imm);
}

void Emitter64::XOR64_REG(REG_64 source, REG_64 dest)
{
    rexw_r_rm(source, dest);
    block->write<uint8_t>(0x31);
    modrm(0b11, source, dest);
}

void Emitter64::LEA32_M(REG_64 source, REG_64 dest, uint32_t offset, uint32_t shift)
{
    offset <<= shift;

    rex_r_rm(dest, source);
    block->write<uint8_t>(0x8D);
    modrm(0b10, dest, source);
    if ((source & 7) == 4)
        block->write<uint8_t>(0x24);
    block->write<uint32_t>(offset);
}

void Emitter64::LEA32_REG(REG_64 source, REG_64 source2, REG_64 dest, uint32_t offset, uint32_t shift)
{
    offset <<= shift;

    uint8_t rex = 0x40;
    if (dest & 0x8) rex += 0x4;
    if (source & 0x8) rex += 0x2;
    if (source2 & 0x8) rex += 0x1;
    block->write<uint8_t>(rex);
    block->write<uint8_t>(0x8D);
    modrm(0b10, dest, 4);
    modrm(0b00, source, source2);
    block->write<uint32_t>(offset);
}

void Emitter64::LEA64_M(REG_64 source, REG_64 dest, uint32_t offset, uint32_t shift)
{
    offset <<= shift;

    rexw_r_rm(dest, source);
    block->write<uint8_t>(0x8D);
    modrm(0b10, dest, source);
    if ((source & 7) == 4)
        block->write<uint8_t>(0x24);
    block->write<uint32_t>(offset);
}

void Emitter64::LEA64_REG(REG_64 source, REG_64 source2, REG_64 dest, uint32_t offset, uint32_t shift)
{
    uint8_t rex = 0x48;
    if (dest & 0x8) rex += 0x4;
    if (source & 0x8) rex += 0x2;
    if (source2 & 0x8) rex += 0x1;
    block->write<uint8_t>(rex);
    block->write<uint8_t>(0x8D);
    modrm(0b10, dest, 4);
    modrm(shift, source, source2);
    block->write<uint32_t>(offset);
}

void Emitter64::MOV8_REG(REG_64 source, REG_64 dest)
{
    rex_r_rm(source, dest);

    // FIXME: I still don't know anything about encoding these instructions, woooo
    if (!(dest & 0x8) && !(source & 0x8))
        block->write<uint8_t>(0x40);

    block->write<uint8_t>(0x88);
    modrm(0b11, source, dest);
}

void Emitter64::MOV8_REG_IMM(uint8_t imm, REG_64 dest)
{
    rex_rm(dest);

    // FIXME: I still don't know anything about encoding these instructions, woooo
    if (!(dest & 0x8))
        block->write<uint8_t>(0x40);

    block->write<uint8_t>(0xB0 + (dest & 0x7));
    block->write<uint8_t>(imm);
}

void Emitter64::MOV8_TO_MEM(REG_64 source, REG_64 indir_dest, uint32_t offset)
{
    rex_r_rm(source, indir_dest);
    block->write<uint8_t>(0x88);

    if ((indir_dest & 7) == 5 || offset)
    {
        modrm(0b10, source, indir_dest);
    }
    else
        modrm(0, source, indir_dest);
    if ((indir_dest & 7) == 4)
        block->write<uint8_t>(0x24);
    if ((indir_dest & 7) == 5 || offset)
        block->write<uint32_t>(offset);
}

void Emitter64::MOV8_FROM_MEM(REG_64 indir_source, REG_64 dest, uint32_t offset)
{
    rex_r_rm(dest, indir_source);
    block->write<uint8_t>(0x8A);

    if ((indir_source & 7) == 5 || offset)
    {
        modrm(0b10, dest, indir_source);
    }
    else
        modrm(0, dest, indir_source);
    if ((indir_source & 7) == 4)
        block->write<uint8_t>(0x24);
    if ((indir_source & 7) == 5 || offset)
        block->write<uint32_t>(offset);
}

void Emitter64::MOV8_IMM_MEM(uint8_t imm, REG_64 indir_dest, uint32_t offset)
{
    rex_rm(indir_dest);
    block->write<uint8_t>(0xC6);
    if ((indir_dest & 7) == 5 || offset != 0)
    {
        modrm(0b10, 0, indir_dest);
        block->write<uint32_t>(offset);
    }
    else
    {
        modrm(0, 0, indir_dest);
    }
    if ((indir_dest & 7) == 4)
        block->write<uint8_t>(0x24);
    block->write<uint8_t>(imm);
}

void Emitter64::MOV16_REG(REG_64 source, REG_64 dest)
{
    block->write<uint8_t>(0x66);
    rex_r_rm(source, dest);
    block->write<uint8_t>(0x89);
    modrm(0b11, source, dest);
}

void Emitter64::MOV16_REG_IMM(uint16_t imm, REG_64 dest)
{
    block->write<uint8_t>(0x66);
    rex_rm(dest);
    block->write<uint8_t>(0xB8 + (dest & 0x7));
    block->write<uint16_t>(imm);
}

void Emitter64::MOV16_TO_MEM(REG_64 source, REG_64 indir_dest, uint32_t offset)
{
    block->write<uint8_t>(0x66);
    rex_r_rm(source, indir_dest);
    block->write<uint8_t>(0x89);

    if ((indir_dest & 7) == 5 || offset)
    {
        modrm(0b10, source, indir_dest);
    }
    else
        modrm(0, source, indir_dest);
    if ((indir_dest & 7) == 4)
        block->write<uint8_t>(0x24);
    if ((indir_dest & 7) == 5 || offset)
        block->write<uint32_t>(offset);
}

void Emitter64::MOV16_FROM_MEM(REG_64 indir_source, REG_64 dest, uint32_t offset)
{
    block->write<uint8_t>(0x66);
    rex_r_rm(dest, indir_source);
    block->write<uint8_t>(0x8B);

    if ((indir_source & 7) == 5 || offset)
    {
        modrm(0b10, dest, indir_source);
    }
    else
        modrm(0, dest, indir_source);
    if ((indir_source & 7) == 4)
        block->write<uint8_t>(0x24);
    if ((indir_source & 7) == 5 || offset)
        block->write<uint32_t>(offset);
}

void Emitter64::MOV16_IMM_MEM(uint16_t imm, REG_64 indir_dest, uint32_t offset)
{
    block->write<uint8_t>(0x66);
    rex_rm(indir_dest);
    block->write<uint8_t>(0xC7);
    if ((indir_dest & 7) == 5 || offset != 0)
    {
        modrm(0b10, 0, indir_dest);
        block->write<uint32_t>(offset);
    }
    else
    {
        modrm(0, 0, indir_dest);
    }
    if ((indir_dest & 7) == 4)
        block->write<uint8_t>(0x24);
    block->write<uint16_t>(imm);
}

void Emitter64::MOV32_REG(REG_64 source, REG_64 dest)
{
    rex_r_rm(source, dest);
    block->write<uint8_t>(0x89);
    modrm(0b11, source, dest);
}

void Emitter64::MOV32_REG_IMM(uint32_t imm, REG_64 dest)
{
    rex_rm(dest);
    block->write<uint8_t>(0xB8 + (dest & 0x7));
    block->write<uint32_t>(imm);
}

void Emitter64::MOV32_IMM_MEM(uint32_t imm, REG_64 indir_dest, uint32_t offset)
{
    rex_rm(indir_dest);
    block->write<uint8_t>(0xC7);
    if ((indir_dest & 7) == 5 || offset != 0)
    {
        modrm(0b10, 0, indir_dest);
        block->write<uint32_t>(offset);
    }
    else
    {
        modrm(0, 0, indir_dest);
    }
    if ((indir_dest & 7) == 4)
        block->write<uint8_t>(0x24);
    block->write<uint32_t>(imm);
}

void Emitter64::MOV32_FROM_MEM(REG_64 indir_source, REG_64 dest, uint32_t offset)
{
    rex_r_rm(dest, indir_source);
    block->write<uint8_t>(0x8B);

    if ((indir_source & 7) == 5 || offset)
    {
        modrm(0b10, dest, indir_source);
    }
    else
        modrm(0, dest, indir_source);
    if ((indir_source & 7) == 4)
        block->write<uint8_t>(0x24);
    if ((indir_source & 7) == 5 || offset)
        block->write<uint32_t>(offset);
}

void Emitter64::MOV32_TO_MEM(REG_64 source, REG_64 indir_dest, uint32_t offset)
{
    rex_r_rm(source, indir_dest);
    block->write<uint8_t>(0x89);

    if ((indir_dest & 7) == 5 || offset)
    {
        modrm(0b10, source, indir_dest);
    }
    else
        modrm(0, source, indir_dest);
    if ((indir_dest & 7) == 4)
        block->write<uint8_t>(0x24);
    if ((indir_dest & 7) == 5 || offset)
        block->write<uint32_t>(offset);
}

void Emitter64::MOV32SX_IMM_MEM(uint32_t imm, REG_64 indir_dest, uint32_t offset)
{
    rexw_rm(indir_dest);
    block->write<uint8_t>(0xC7);
    if ((indir_dest & 7) == 5 || offset != 0)
    {
        modrm(0b10, 0, indir_dest);
        block->write<uint32_t>(offset);
    }
    else
    {
        modrm(0, 0, indir_dest);
    }
    if ((indir_dest & 7) == 4)
        block->write<uint8_t>(0x24);
    block->write<uint32_t>(imm);
}

void Emitter64::MOV64_MR(REG_64 source, REG_64 dest)
{
    rexw_r_rm(source, dest);
    block->write<uint8_t>(0x89);
    modrm(0b11, source, dest);
}

void Emitter64::MOV64_OI(uint64_t imm, REG_64 dest)
{
    rexw_rm(dest);
    block->write<uint8_t>(0xB8 + (dest & 0x7));
    block->write<uint64_t>(imm);
}

void Emitter64::MOV64_FROM_MEM(REG_64 indir_source, REG_64 dest, uint32_t offset)
{
    rexw_r_rm(dest, indir_source);
    block->write<uint8_t>(0x8B);

    if ((indir_source & 7) == 5 || offset)
    {
        modrm(0b10, dest, indir_source);
    }
    else
        modrm(0, dest, indir_source);
    if ((indir_source & 7) == 4)
        block->write<uint8_t>(0x24);
    if ((indir_source & 7) == 5 || offset)
        block->write<uint32_t>(offset);
}

void Emitter64::MOV64_TO_MEM(REG_64 source, REG_64 indir_dest, uint32_t offset)
{
    rexw_r_rm(source, indir_dest);
    block->write<uint8_t>(0x89);

    if ((indir_dest & 7) == 5 || offset)
    {
        modrm(0b10, source, indir_dest);
    }
    else
        modrm(0, source, indir_dest);
    if ((indir_dest & 7) == 4)
        block->write<uint8_t>(0x24);
    if ((indir_dest & 7) == 5 || offset)
        block->write<uint32_t>(offset);
}

void Emitter64::MOVSX8_TO_64(REG_64 source, REG_64 dest)
{
    rexw_r_rm(dest, source);
    block->write<uint8_t>(0x0F);
    block->write<uint8_t>(0xBE);
    modrm(0b11, dest, source);
}

void Emitter64::MOVSX16_TO_32(REG_64 source, REG_64 dest)
{
    rex_r_rm(dest, source);
    block->write<uint8_t>(0x0F);
    block->write<uint8_t>(0xBF);
    modrm(0b11, dest, source);
}

void Emitter64::MOVSX16_TO_64(REG_64 source, REG_64 dest)
{
    rexw_r_rm(dest, source);
    block->write<uint8_t>(0x0F);
    block->write<uint8_t>(0xBF);
    modrm(0b11, dest, source);
}

void Emitter64::MOVSX32_TO_64(REG_64 source, REG_64 dest)
{
    rexw_r_rm(dest, source);
    block->write<uint8_t>(0x63);
    modrm(0b11, dest, source);
}

void Emitter64::MOVZX8_TO_32(REG_64 source, REG_64 dest)
{
    rex_r_rm(dest, source);
    block->write<uint8_t>(0x0F);
    block->write<uint8_t>(0xB6);
    modrm(0b11, dest, source);
}

void Emitter64::MOVZX8_TO_64(REG_64 source, REG_64 dest)
{
    rexw_r_rm(dest, source);
    block->write<uint8_t>(0x0F);
    block->write<uint8_t>(0xB6);
    modrm(0b11, dest, source);
}

void Emitter64::MOVZX16_TO_64(REG_64 source, REG_64 dest)
{
    rexw_r_rm(dest, source);
    block->write<uint8_t>(0x0F);
    block->write<uint8_t>(0xB7);
    modrm(0b11, dest, source);
}

void Emitter64::MOVD_FROM_MEM(REG_64 indir_source, REG_64 xmm_dest)
{
    block->write<uint8_t>(0x66);
    rex_r_rm(xmm_dest, indir_source);
    block->write<uint8_t>(0x0F);
    block->write<uint8_t>(0x6E);
    modrm(0, xmm_dest, indir_source);
}

void Emitter64::MOVD_FROM_XMM(REG_64 xmm_source, REG_64 dest)
{
    block->write<uint8_t>(0x66);
    rex_r_rm(xmm_source, dest);
    block->write<uint8_t>(0x0F);
    block->write<uint8_t>(0x7E);
    modrm(0b11, xmm_source, dest);
}

void Emitter64::MOVD_TO_MEM(REG_64 xmm_source, REG_64 indir_dest)
{
    block->write<uint8_t>(0x66);
    rex_r_rm(xmm_source, indir_dest);
    block->write<uint8_t>(0x0F);
    block->write<uint8_t>(0x7E);
    modrm(0, xmm_source, indir_dest);
}

void Emitter64::MOVD_TO_XMM(REG_64 source, REG_64 xmm_dest)
{
    block->write<uint8_t>(0x66);
    rex_r_rm(xmm_dest, source);
    block->write<uint8_t>(0x0F);
    block->write<uint8_t>(0x6E);
    modrm(0b11, xmm_dest, source);
}

void Emitter64::MOVQ_FROM_XMM(REG_64 xmm_source, REG_64 dest)
{
    block->write<uint8_t>(0x66);
    rexw_r_rm(xmm_source, dest);
    block->write<uint8_t>(0x0F);
    block->write<uint8_t>(0x7E);
    modrm(0b11, xmm_source, dest);
}

void Emitter64::MOVQ_TO_XMM(REG_64 source, REG_64 xmm_dest)
{
    block->write<uint8_t>(0x66);
    rexw_r_rm(xmm_dest, source);
    block->write<uint8_t>(0x0F);
    block->write<uint8_t>(0x6E);
    modrm(0b11, xmm_dest, source);
}

void Emitter64::MOVSS_REG(REG_64 xmm_source, REG_64 xmm_dest)
{
    block->write<uint8_t>(0xF3);
    rex_r_rm(xmm_source, xmm_dest);
    block->write<uint8_t>(0x0F);
    block->write<uint8_t>(0x11);
    modrm(0b11, xmm_source, xmm_dest);
}

void Emitter64::MOVAPS_REG(REG_64 xmm_source, REG_64 xmm_dest)
{
    rex_r_rm(xmm_source, xmm_dest);
    block->write<uint8_t>(0x0F);
    block->write<uint8_t>(0x29);
    modrm(0b11, xmm_source, xmm_dest);
}

void Emitter64::MOVAPS_FROM_MEM(REG_64 indir_source, REG_64 xmm_dest, uint32_t offset)
{
    rex_r_rm(xmm_dest, indir_source);
    block->write<uint8_t>(0x0F);
    block->write<uint8_t>(0x28);
    if ((indir_source & 7) == 5 || offset)
        modrm(0b10, xmm_dest, indir_source);
    else
        modrm(0b0, xmm_dest, indir_source);
    if ((indir_source & 7) == 4)
        block->write<uint8_t>(0x24);
    if ((indir_source & 7) == 5 || offset)
        block->write<uint32_t>(offset);
}

void Emitter64::MOVAPS_TO_MEM(REG_64 xmm_source, REG_64 indir_dest, uint32_t offset)
{
    rex_r_rm(xmm_source, indir_dest);
    block->write<uint8_t>(0x0F);
    block->write<uint8_t>(0x29);
    if ((indir_dest & 7) == 5 || offset)
        modrm(0b10, xmm_source, indir_dest);
    else
        modrm(0b0, xmm_source, indir_dest);
    if ((indir_dest & 7) == 4)
        block->write<uint8_t>(0x24);
    if ((indir_dest & 7) == 5 || offset)
        block->write<uint32_t>(offset);
}

void Emitter64::MOVUPS_FROM_MEM(REG_64 indir_source, REG_64 xmm_dest, uint32_t offset)
{
    rex_r_rm(xmm_dest, indir_source);
    block->write<uint8_t>(0x0F);
    block->write<uint8_t>(0x10);
    if ((indir_source & 7) == 5 || offset)
        modrm(0b10, xmm_dest, indir_source);
    else
        modrm(0b0, xmm_dest, indir_source);
    if ((indir_source & 7) == 4)
        block->write<uint8_t>(0x24);
    if ((indir_source & 7) == 5 || offset)
        block->write<uint32_t>(offset);
}

void Emitter64::MOVUPS_TO_MEM(REG_64 xmm_source, REG_64 indir_dest, uint32_t offset)
{
    rex_r_rm(xmm_source, indir_dest);
    block->write<uint8_t>(0x0F);
    block->write<uint8_t>(0x11);
    if ((indir_dest & 7) == 5 || offset)
        modrm(0b10, xmm_source, indir_dest);
    else
        modrm(0b0, xmm_source, indir_dest);
    if ((indir_dest & 7) == 4)
        block->write<uint8_t>(0x24);
    if ((indir_dest & 7) == 5 || offset)
        block->write<uint32_t>(offset);
}

void Emitter64::MOVMSKPS(REG_64 xmm_source, REG_64 dest)
{
    rex_r_rm(dest, xmm_source);
    block->write<uint8_t>(0x0F);
    block->write<uint8_t>(0x50);
    modrm(0b11, dest, xmm_source);
}

uint8_t* Emitter64::JMP_NEAR_DEFERRED()
{
    block->write<uint8_t>(0xE9);
    uint8_t* addr = block->get_code_pos();

    block->write<uint32_t>(0);
    return addr;
}

uint8_t* Emitter64::JCC_NEAR_DEFERRED(ConditionCode cc)
{
    block->write<uint8_t>(0x0F);
    block->write<uint8_t>((int)cc | 0x80);
    uint8_t* addr = block->get_code_pos();

    block->write<uint32_t>(0);
    return addr;
}

void Emitter64::set_jump_dest(uint8_t *jump)
{
    uint8_t* jump_dest_addr = block->get_code_pos();

    block->set_code_pos(jump);
    int jump_offset = jump_dest_addr - jump - 4;
    block->write<uint32_t>(jump_offset);

    block->set_code_pos(jump_dest_addr);
}

void Emitter64::PUSH(REG_64 reg)
{
    rex_rm(reg);
    block->write<uint8_t>(0x50 + (reg & 0x7));
}

void Emitter64::POP(REG_64 reg)
{
    rex_rm(reg);
    block->write<uint8_t>(0x58 + (reg & 0x7));
}

void Emitter64::LDMXCSR(REG_64 mem, uint32_t offset)
{
    rex_rm(mem);
    block->write<uint8_t>(0xF);
    block->write<uint8_t>(0xAE);
    if ((mem & 7) == 5 || offset != 0)
    {
        modrm(0b10, 2, mem);
        block->write<uint32_t>(offset);
    }
    else
    {
        modrm(0, 2, mem);
    }
}

void Emitter64::STMXCSR(REG_64 mem, uint32_t offset)
{
    rex_rm(mem);
    block->write<uint8_t>(0xF);
    block->write<uint8_t>(0xAE);
    if ((mem & 7) == 5 || offset != 0)
    {
        modrm(0b10, 3, mem);
        block->write<uint32_t>(offset);
    }
    else
    {
        modrm(0, 3, mem);
    }
}

void Emitter64::CALL(uint64_t func)
{
    int offset = func;
    offset -= (uint64_t) block->get_code_pos();
    block->write<uint8_t>(0xE8);
    block->write<uint32_t>(offset - 5);
}

void Emitter64::CALL_INDIR(REG_64 source)
{
    rex_rm(source);
    block->write<uint8_t>(0xFF);
    modrm(0b11, 2, source);
}

void Emitter64::JMP_INDIR(REG_64 source)
{
    rex_rm(source);
    block->write<uint8_t>(0xFF);
    modrm(0b11, 4, source);
}

void Emitter64::RET()
{
    block->write<uint8_t>(0xC3);
}

void Emitter64::PACKUSDW(REG_64 xmm_source, REG_64 xmm_dest)
{
    block->write<uint8_t>(0x66);
    rex_r_rm(xmm_dest, xmm_source);
    block->write<uint8_t>(0x0F);
    block->write<uint8_t>(0x38);
    block->write<uint8_t>(0x2B);
    modrm(0b11, xmm_dest, xmm_source);
}

void Emitter64::PACKUSWB(REG_64 xmm_source, REG_64 xmm_dest)
{
    block->write<uint8_t>(0x66);
    rex_r_rm(xmm_dest, xmm_source);
    block->write<uint8_t>(0x0F);
    block->write<uint8_t>(0x67);
    modrm(0b11, xmm_dest, xmm_source);
}

void Emitter64::PACKSSDW(REG_64 xmm_source, REG_64 xmm_dest)
{
    block->write<uint8_t>(0x66);
    rex_r_rm(xmm_dest, xmm_source);
    block->write<uint8_t>(0x0F);
    block->write<uint8_t>(0x6B);
    modrm(0b11, xmm_dest, xmm_source);
}

void Emitter64::PABSB(REG_64 xmm_source, REG_64 xmm_dest)
{
    block->write<uint8_t>(0x66);
    rex_r_rm(xmm_dest, xmm_source);
    block->write<uint8_t>(0x0F);
    block->write<uint8_t>(0x38);
    block->write<uint8_t>(0x1C);
    modrm(0b11, xmm_dest, xmm_source);
}

void Emitter64::PABSD(REG_64 xmm_source, REG_64 xmm_dest)
{
    block->write<uint8_t>(0x66);
    rex_r_rm(xmm_dest, xmm_source);
    block->write<uint8_t>(0x0F);
    block->write<uint8_t>(0x38);
    block->write<uint8_t>(0x1E);
    modrm(0b11, xmm_dest, xmm_source);
}

void Emitter64::PABSW(REG_64 xmm_source, REG_64 xmm_dest)
{
    block->write<uint8_t>(0x66);
    rex_r_rm(xmm_dest, xmm_source);
    block->write<uint8_t>(0x0F);
    block->write<uint8_t>(0x38);
    block->write<uint8_t>(0x1D);
    modrm(0b11, xmm_dest, xmm_source);
}

void Emitter64::PADDB(REG_64 xmm_source, REG_64 xmm_dest)
{
    block->write<uint8_t>(0x66);
    rex_r_rm(xmm_dest, xmm_source);
    block->write<uint8_t>(0x0F);
    block->write<uint8_t>(0xFC);
    modrm(0b11, xmm_dest, xmm_source);
}

void Emitter64::PADDD(REG_64 xmm_source, REG_64 xmm_dest)
{
    block->write<uint8_t>(0x66);
    rex_r_rm(xmm_dest, xmm_source);
    block->write<uint8_t>(0x0F);
    block->write<uint8_t>(0xFE);
    modrm(0b11, xmm_dest, xmm_source);
}

void Emitter64::PADDW(REG_64 xmm_source, REG_64 xmm_dest)
{
    block->write<uint8_t>(0x66);
    rex_r_rm(xmm_dest, xmm_source);
    block->write<uint8_t>(0x0F);
    block->write<uint8_t>(0xFD);
    modrm(0b11, xmm_dest, xmm_source);
}

void Emitter64::PADDSB(REG_64 xmm_source, REG_64 xmm_dest)
{
    block->write<uint8_t>(0x66);
    rex_r_rm(xmm_dest, xmm_source);
    block->write<uint8_t>(0x0F);
    block->write<uint8_t>(0xEC);
    modrm(0b11, xmm_dest, xmm_source);
}

void Emitter64::PADDSW(REG_64 xmm_source, REG_64 xmm_dest)
{
    block->write<uint8_t>(0x66);
    rex_r_rm(xmm_dest, xmm_source);
    block->write<uint8_t>(0x0F);
    block->write<uint8_t>(0xED);
    modrm(0b11, xmm_dest, xmm_source);
}

void Emitter64::PADDUSB(REG_64 xmm_source, REG_64 xmm_dest)
{
    block->write<uint8_t>(0x66);
    rex_r_rm(xmm_dest, xmm_source);
    block->write<uint8_t>(0x0F);
    block->write<uint8_t>(0xDC);
    modrm(0b11, xmm_dest, xmm_source);
}

void Emitter64::PADDUSW(REG_64 xmm_source, REG_64 xmm_dest)
{
    block->write<uint8_t>(0x66);
    rex_r_rm(xmm_dest, xmm_source);
    block->write<uint8_t>(0x0F);
    block->write<uint8_t>(0xDD);
    modrm(0b11, xmm_dest, xmm_source);
}

void Emitter64::PAND_XMM(REG_64 xmm_source, REG_64 xmm_dest)
{
    block->write<uint8_t>(0x66);
    rex_r_rm(xmm_dest, xmm_source);
    block->write<uint8_t>(0x0F);
    block->write<uint8_t>(0xDB);
    modrm(0b11, xmm_dest, xmm_source);
}

void Emitter64::PAND_XMM_FROM_MEM(REG_64 indir_source, REG_64 xmm_dest, uint32_t offset)
{
    block->write<uint8_t>(0x66);
    rex_r_rm(xmm_dest, indir_source);
    block->write<uint8_t>(0x0F);
    block->write<uint8_t>(0xDB);
    modrm(0, xmm_dest, indir_source);
    if ((indir_source & 7) == 4)
        block->write<uint8_t>(0x24);
    if ((indir_source & 7) == 5 || offset)
        block->write<uint32_t>(offset);
}

void Emitter64::PANDN_XMM(REG_64 xmm_source, REG_64 xmm_dest)
{
    block->write<uint8_t>(0x66);
    rex_r_rm(xmm_dest, xmm_source);
    block->write<uint8_t>(0x0F);
    block->write<uint8_t>(0xDF);
    modrm(0b11, xmm_dest, xmm_source);
}

void Emitter64::PANDN_XMM_FROM_MEM(REG_64 indir_source, REG_64 xmm_dest, uint32_t offset)
{
    block->write<uint8_t>(0x66);
    rex_r_rm(xmm_dest, indir_source);
    block->write<uint8_t>(0x0F);
    block->write<uint8_t>(0xDF);
    modrm(0, xmm_dest, indir_source);
    if ((indir_source & 7) == 4)
        block->write<uint8_t>(0x24);
    if ((indir_source & 7) == 5 || offset)
        block->write<uint32_t>(offset);
}

void Emitter64::PCMPEQB_XMM(REG_64 xmm_source, REG_64 xmm_dest)
{
    block->write<uint8_t>(0x66);
    rex_r_rm(xmm_dest, xmm_source);
    block->write<uint8_t>(0x0F);
    block->write<uint8_t>(0x74);
    modrm(0b11, xmm_dest, xmm_source);
}

void Emitter64::PCMPEQD_XMM(REG_64 xmm_source, REG_64 xmm_dest)
{
    block->write<uint8_t>(0x66);
    rex_r_rm(xmm_dest, xmm_source);
    block->write<uint8_t>(0x0F);
    block->write<uint8_t>(0x76);
    modrm(0b11, xmm_dest, xmm_source);
}

void Emitter64::PCMPEQW_XMM(REG_64 xmm_source, REG_64 xmm_dest)
{
    block->write<uint8_t>(0x66);
    rex_r_rm(xmm_dest, xmm_source);
    block->write<uint8_t>(0x0F);
    block->write<uint8_t>(0x75);
    modrm(0b11, xmm_dest, xmm_source);
}

void Emitter64::PCMPGTB_XMM(REG_64 xmm_source, REG_64 xmm_dest)
{
    block->write<uint8_t>(0x66);
    rex_r_rm(xmm_dest, xmm_source);
    block->write<uint8_t>(0x0F);
    block->write<uint8_t>(0x64);
    modrm(0b11, xmm_dest, xmm_source);
}

void Emitter64::PCMPGTW_XMM(REG_64 xmm_source, REG_64 xmm_dest)
{
    block->write<uint8_t>(0x66);
    rex_r_rm(xmm_dest, xmm_source);
    block->write<uint8_t>(0x0F);
    block->write<uint8_t>(0x65);
    modrm(0b11, xmm_dest, xmm_source);
}

void Emitter64::PCMPGTD_XMM(REG_64 xmm_source, REG_64 xmm_dest)
{
    block->write<uint8_t>(0x66);
    rex_r_rm(xmm_dest, xmm_source);
    block->write<uint8_t>(0x0F);
    block->write<uint8_t>(0x66);
    modrm(0b11, xmm_dest, xmm_source);
}


void Emitter64::PEXTRB_XMM(uint8_t imm, REG_64 xmm_source, REG_64 dest)
{
    block->write<uint8_t>(0x66);
    rex_r_rm(xmm_source, dest);
    block->write<uint8_t>(0x0F);
    block->write<uint8_t>(0x3A);
    block->write<uint8_t>(0x14);
    modrm(0b11, xmm_source, dest);
    block->write<uint8_t>(imm);
}

void Emitter64::PEXTRW_XMM(uint8_t imm, REG_64 xmm_source, REG_64 dest)
{
    block->write<uint8_t>(0x66);
    rex_r_rm(dest, xmm_source);
    block->write<uint8_t>(0x0F);
    block->write<uint8_t>(0xC5);
    modrm(0b11, dest, xmm_source);
    block->write<uint8_t>(imm);
}

void Emitter64::PEXTRD_XMM(uint8_t imm, REG_64 xmm_source, REG_64 dest)
{
    block->write<uint8_t>(0x66);
    rex_r_rm(xmm_source, dest);
    block->write<uint8_t>(0x0F);
    block->write<uint8_t>(0x3A);
    block->write<uint8_t>(0x16);
    modrm(0b11, xmm_source, dest);
    block->write<uint8_t>(imm);
}

void Emitter64::PEXTRQ_XMM(uint8_t imm, REG_64 xmm_source, REG_64 dest)
{
    block->write<uint8_t>(0x66);
    rexw_r_rm(xmm_source, dest);
    block->write<uint8_t>(0x0F);
    block->write<uint8_t>(0x3A);
    block->write<uint8_t>(0x16);
    modrm(0b11, xmm_source, dest);
    block->write<uint8_t>(imm);
}

void Emitter64::PINSRB_XMM(uint8_t imm, REG_64 source, REG_64 xmm_dest)
{
    block->write<uint8_t>(0x66);
    rex_r_rm(xmm_dest, source);
    block->write<uint8_t>(0x0F);
    block->write<uint8_t>(0x3A);
    block->write<uint8_t>(0x20);
    modrm(0b11, xmm_dest, source);
    block->write<uint8_t>(imm);
}

void Emitter64::PINSRW_XMM(uint8_t imm, REG_64 source, REG_64 xmm_dest)
{
    block->write<uint8_t>(0x66);
    rex_r_rm(xmm_dest, source);
    block->write<uint8_t>(0x0F);
    block->write<uint8_t>(0xC4);
    modrm(0b11, xmm_dest, source);
    block->write<uint8_t>(imm);
}

void Emitter64::PINSRD_XMM(uint8_t imm, REG_64 source, REG_64 xmm_dest)
{
    block->write<uint8_t>(0x66);
    rex_r_rm(xmm_dest, source);
    block->write<uint8_t>(0x0F);
    block->write<uint8_t>(0x3A);
    block->write<uint8_t>(0x22);
    modrm(0b11, xmm_dest, source);
    block->write<uint8_t>(imm);
}

void Emitter64::PINSRQ_XMM(uint8_t imm, REG_64 source, REG_64 xmm_dest)
{
    block->write<uint8_t>(0x66);
    rexw_r_rm(xmm_dest, source);
    block->write<uint8_t>(0x0F);
    block->write<uint8_t>(0x3A);
    block->write<uint8_t>(0x22);
    modrm(0b11, xmm_dest, source);
    block->write<uint8_t>(imm);
}

void Emitter64::PMAXSB_XMM(REG_64 xmm_source, REG_64 xmm_dest)
{
    block->write<uint8_t>(0x66);
    rex_r_rm(xmm_dest, xmm_source);
    block->write<uint8_t>(0x0F);
    block->write<uint8_t>(0x38);
    block->write<uint8_t>(0x3C);
    modrm(0b11, xmm_dest, xmm_source);
}

void Emitter64::PMAXSD_XMM(REG_64 xmm_source, REG_64 xmm_dest)
{
    block->write<uint8_t>(0x66);
    rex_r_rm(xmm_dest, xmm_source);
    block->write<uint8_t>(0x0F);
    block->write<uint8_t>(0x38);
    block->write<uint8_t>(0x3D);
    modrm(0b11, xmm_dest, xmm_source);
}

void Emitter64::PMAXSW_XMM(REG_64 xmm_source, REG_64 xmm_dest)
{
    block->write<uint8_t>(0x66);
    rex_r_rm(xmm_dest, xmm_source);
    block->write<uint8_t>(0x0F);
    block->write<uint8_t>(0xEE);
    modrm(0b11, xmm_dest, xmm_source);
}

void Emitter64::PMAXSW_XMM_FROM_MEM(REG_64 indir_source, REG_64 xmm_dest, uint32_t offset)
{
    block->write<uint8_t>(0x66);
    rex_r_rm(xmm_dest, indir_source);
    block->write<uint8_t>(0x0F);
    block->write<uint8_t>(0xEE);
    modrm(0, xmm_dest, indir_source);
    if ((indir_source & 7) == 4)
        block->write<uint8_t>(0x24);
    if ((indir_source & 7) == 5 || offset)
        block->write<uint32_t>(offset);
}

void Emitter64::PMAXUB_XMM(REG_64 xmm_source, REG_64 xmm_dest)
{
    block->write<uint8_t>(0x66);
    rex_r_rm(xmm_dest, xmm_source);
    block->write<uint8_t>(0x0F);
    block->write<uint8_t>(0xDE);
    modrm(0b11, xmm_dest, xmm_source);
}

void Emitter64::PMAXUD_XMM(REG_64 xmm_source, REG_64 xmm_dest)
{
    block->write<uint8_t>(0x66);
    rex_r_rm(xmm_dest, xmm_source);
    block->write<uint8_t>(0x0F);
    block->write<uint8_t>(0x38);
    block->write<uint8_t>(0x3F);
    modrm(0b11, xmm_dest, xmm_source);
}

void Emitter64::PMAXUW_XMM(REG_64 xmm_source, REG_64 xmm_dest)
{
    block->write<uint8_t>(0x66);
    rex_r_rm(xmm_dest, xmm_source);
    block->write<uint8_t>(0x0F);
    block->write<uint8_t>(0x38);
    block->write<uint8_t>(0x3E);
    modrm(0b11, xmm_dest, xmm_source);
}

void Emitter64::PMINSB_XMM(REG_64 xmm_source, REG_64 xmm_dest)
{
    block->write<uint8_t>(0x66);
    rex_r_rm(xmm_dest, xmm_source);
    block->write<uint8_t>(0x0F);
    block->write<uint8_t>(0x38);
    block->write<uint8_t>(0x38);
    modrm(0b11, xmm_dest, xmm_source);
}

void Emitter64::PMINSD_XMM(REG_64 xmm_source, REG_64 xmm_dest)
{
    block->write<uint8_t>(0x66);
    rex_r_rm(xmm_dest, xmm_source);
    block->write<uint8_t>(0x0F);
    block->write<uint8_t>(0x38);
    block->write<uint8_t>(0x39);
    modrm(0b11, xmm_dest, xmm_source);
}

void Emitter64::PMINSW_XMM(REG_64 xmm_source, REG_64 xmm_dest)
{
    block->write<uint8_t>(0x66);
    rex_r_rm(xmm_dest, xmm_source);
    block->write<uint8_t>(0x0F);
    block->write<uint8_t>(0xEA);
    modrm(0b11, xmm_dest, xmm_source);
}

void Emitter64::PMINSW_XMM_FROM_MEM(REG_64 indir_source, REG_64 xmm_dest, uint32_t offset)
{
    block->write<uint8_t>(0x66);
    rex_r_rm(xmm_dest, indir_source);
    block->write<uint8_t>(0x0F);
    block->write<uint8_t>(0xEA);
    modrm(0, xmm_dest, indir_source);
    if ((indir_source & 7) == 4)
        block->write<uint8_t>(0x24);
    if ((indir_source & 7) == 5 || offset)
        block->write<uint32_t>(offset);
}

void Emitter64::PMINUW_XMM_FROM_MEM(REG_64 indir_source, REG_64 xmm_dest, uint32_t offset)
{
    block->write<uint8_t>(0x66);
    rex_r_rm(xmm_dest, indir_source);
    block->write<uint8_t>(0x0F);
    block->write<uint8_t>(0x38);
    block->write<uint8_t>(0x3A);
    modrm(0, xmm_dest, indir_source);
    if ((indir_source & 7) == 4)
        block->write<uint8_t>(0x24);
    if ((indir_source & 7) == 5 || offset)
        block->write<uint32_t>(offset);
}

void Emitter64::PMINSD_XMM_FROM_MEM(REG_64 indir_source, REG_64 xmm_dest, uint32_t offset)
{
    block->write<uint8_t>(0x66);
    rex_r_rm(xmm_dest, indir_source);
    block->write<uint8_t>(0x0F);
    block->write<uint8_t>(0x38);
    block->write<uint8_t>(0x39);
    modrm(0, xmm_dest, indir_source);
    if ((indir_source & 7) == 4)
        block->write<uint8_t>(0x24);
    if ((indir_source & 7) == 5 || offset)
        block->write<uint32_t>(offset);
}

void Emitter64::PMINUB_XMM(REG_64 xmm_source, REG_64 xmm_dest)
{
    block->write<uint8_t>(0x66);
    rex_r_rm(xmm_dest, xmm_source);
    block->write<uint8_t>(0x0F);
    block->write<uint8_t>(0xDA);
    modrm(0b11, xmm_dest, xmm_source);
}

void Emitter64::PMINUD_XMM(REG_64 xmm_source, REG_64 xmm_dest)
{
    block->write<uint8_t>(0x66);
    rex_r_rm(xmm_dest, xmm_source);
    block->write<uint8_t>(0x0F);
    block->write<uint8_t>(0x38);
    block->write<uint8_t>(0x3B);
    modrm(0b11, xmm_dest, xmm_source);
}

void Emitter64::PMINUW_XMM(REG_64 xmm_source, REG_64 xmm_dest)
{
    block->write<uint8_t>(0x66);
    rex_r_rm(xmm_dest, xmm_source);
    block->write<uint8_t>(0x0F);
    block->write<uint8_t>(0x38);
    block->write<uint8_t>(0x3A);
    modrm(0b11, xmm_dest, xmm_source);
}

void Emitter64::PMINUD_XMM_FROM_MEM(REG_64 indir_source, REG_64 xmm_dest, uint32_t offset)
{
    block->write<uint8_t>(0x66);
    rex_r_rm(xmm_dest, indir_source);
    block->write<uint8_t>(0x0F);
    block->write<uint8_t>(0x38);
    block->write<uint8_t>(0x3B);
    modrm(0, xmm_dest, indir_source);
    if ((indir_source & 7) == 4)
        block->write<uint8_t>(0x24);
    if ((indir_source & 7) == 5 || offset)
        block->write<uint32_t>(offset);
}

void Emitter64::PMOVZX8_TO_16(REG_64 xmm_source, REG_64 xmm_dest)
{
    block->write<uint8_t>(0x66);
    rex_r_rm(xmm_dest, xmm_source);
    block->write<uint8_t>(0x0F);
    block->write<uint8_t>(0x38);
    block->write<uint8_t>(0x30);
    modrm(0b11, xmm_dest, xmm_source);
}

void Emitter64::PMOVZX16_TO_32(REG_64 xmm_source, REG_64 xmm_dest)
{
    block->write<uint8_t>(0x66);
    rex_r_rm(xmm_dest, xmm_source);
    block->write<uint8_t>(0x0F);
    block->write<uint8_t>(0x38);
    block->write<uint8_t>(0x33);
    modrm(0b11, xmm_dest, xmm_source);
}

void Emitter64::PMOVSX16_TO_32(REG_64 xmm_source, REG_64 xmm_dest)
{
    block->write<uint8_t>(0x66);
    rex_r_rm(xmm_dest, xmm_source);
    block->write<uint8_t>(0x0F);
    block->write<uint8_t>(0x38);
    block->write<uint8_t>(0x23);
    modrm(0b11, xmm_dest, xmm_source);
}

void Emitter64::PMULLD(REG_64 xmm_source, REG_64 xmm_dest)
{
    block->write<uint8_t>(0x66);
    rex_r_rm(xmm_dest, xmm_source);
    block->write<uint8_t>(0x0F);
    block->write<uint8_t>(0x38);
    block->write<uint8_t>(0x40);
    modrm(0b11, xmm_dest, xmm_source);
}

void Emitter64::PMULLW(REG_64 xmm_source, REG_64 xmm_dest)
{
    block->write<uint8_t>(0x66);
    rex_r_rm(xmm_dest, xmm_source);
    block->write<uint8_t>(0x0F);
    block->write<uint8_t>(0xD5);
    modrm(0b11, xmm_dest, xmm_source);
}

void Emitter64::POR_XMM(REG_64 xmm_source, REG_64 xmm_dest)
{
    block->write<uint8_t>(0x66);
    rex_r_rm(xmm_dest, xmm_source);
    block->write<uint8_t>(0x0F);
    block->write<uint8_t>(0xEB);
    modrm(0b11, xmm_dest, xmm_source);
}

void Emitter64::POR_XMM_FROM_MEM(REG_64 indir_source, REG_64 xmm_dest, uint32_t offset)
{
    block->write<uint8_t>(0x66);
    rex_r_rm(xmm_dest, indir_source);
    block->write<uint8_t>(0x0F);
    block->write<uint8_t>(0xEB);
    modrm(0, xmm_dest, indir_source);
    if ((indir_source & 7) == 4)
        block->write<uint8_t>(0x24);
    if ((indir_source & 7) == 5 || offset)
        block->write<uint32_t>(offset);
}

void Emitter64::PSHUFD(uint8_t imm, REG_64 xmm_source, REG_64 xmm_dest)
{
    block->write<uint8_t>(0x66);
    rex_r_rm(xmm_dest, xmm_source);
    block->write<uint8_t>(0x0F);
    block->write<uint8_t>(0x70);
    modrm(0b11, xmm_dest, xmm_source);
    block->write<uint8_t>(imm);
}

void Emitter64::PSHUFHW(uint8_t imm, REG_64 xmm_source, REG_64 xmm_dest)
{
    block->write<uint8_t>(0xF3);
    rex_r_rm(xmm_dest, xmm_source);
    block->write<uint8_t>(0x0F);
    block->write<uint8_t>(0x70);
    modrm(0b11, xmm_dest, xmm_source);
    block->write<uint8_t>(imm);
}

void Emitter64::PSHUFLW(uint8_t imm, REG_64 xmm_source, REG_64 xmm_dest)
{
    block->write<uint8_t>(0xF2);
    rex_r_rm(xmm_dest, xmm_source);
    block->write<uint8_t>(0x0F);
    block->write<uint8_t>(0x70);
    modrm(0b11, xmm_dest, xmm_source);
    block->write<uint8_t>(imm);
}

void Emitter64::PSLLW(int shift, REG_64 xmm_dest)
{
    block->write<uint8_t>(0x66);
    rex_rm(xmm_dest);
    block->write<uint8_t>(0x0F);
    block->write<uint8_t>(0x71);
    modrm(0b11, 6, xmm_dest);
    block->write<uint8_t>(shift);
}

void Emitter64::PSLLD(int shift, REG_64 xmm_dest)
{
    block->write<uint8_t>(0x66);
    rex_rm(xmm_dest);
    block->write<uint8_t>(0x0F);
    block->write<uint8_t>(0x72);
    modrm(0b11, 6, xmm_dest);
    block->write<uint8_t>(shift);
}

void Emitter64::PSLLQ(int shift, REG_64 xmm_dest)
{
    block->write<uint8_t>(0x66);
    rex_rm(xmm_dest);
    block->write<uint8_t>(0x0F);
    block->write<uint8_t>(0x73);
    modrm(0b11, 6, xmm_dest);
    block->write<uint8_t>(shift);
}

void Emitter64::PSRAW(int shift, REG_64 xmm_dest)
{
    block->write<uint8_t>(0x66);
    rex_rm(xmm_dest);
    block->write<uint8_t>(0x0F);
    block->write<uint8_t>(0x71);
    modrm(0b11, 4, xmm_dest);
    block->write<uint8_t>(shift);
}

void Emitter64::PSRAD(int shift, REG_64 xmm_dest)
{
    block->write<uint8_t>(0x66);
    rex_rm(xmm_dest);
    block->write<uint8_t>(0x0F);
    block->write<uint8_t>(0x72);
    modrm(0b11, 4, xmm_dest);
    block->write<uint8_t>(shift);
}

void Emitter64::PSRLW(int shift, REG_64 xmm_dest)
{
    block->write<uint8_t>(0x66);
    rex_rm(xmm_dest);
    block->write<uint8_t>(0x0F);
    block->write<uint8_t>(0x71);
    modrm(0b11, 2, xmm_dest);
    block->write<uint8_t>(shift);
}

void Emitter64::PSRLD(int shift, REG_64 xmm_dest)
{
    block->write<uint8_t>(0x66);
    rex_rm(xmm_dest);
    block->write<uint8_t>(0x0F);
    block->write<uint8_t>(0x72);
    modrm(0b11, 2, xmm_dest);
    block->write<uint8_t>(shift);
}

void Emitter64::PSRLQ(int shift, REG_64 xmm_dest)
{
    block->write<uint8_t>(0x66);
    rex_rm(xmm_dest);
    block->write<uint8_t>(0x0F);
    block->write<uint8_t>(0x73);
    modrm(0b11, 2, xmm_dest);
    block->write<uint8_t>(shift);
}

void Emitter64::PSUBB(REG_64 xmm_source, REG_64 xmm_dest)
{
    block->write<uint8_t>(0x66);
    rex_r_rm(xmm_dest, xmm_source);
    block->write<uint8_t>(0x0F);
    block->write<uint8_t>(0xF8);
    modrm(0b11, xmm_dest, xmm_source);
}

void Emitter64::PSUBD(REG_64 xmm_source, REG_64 xmm_dest)
{
    block->write<uint8_t>(0x66);
    rex_r_rm(xmm_dest, xmm_source);
    block->write<uint8_t>(0x0F);
    block->write<uint8_t>(0xFA);
    modrm(0b11, xmm_dest, xmm_source);
}

void Emitter64::PSUBW(REG_64 xmm_source, REG_64 xmm_dest)
{
    block->write<uint8_t>(0x66);
    rex_r_rm(xmm_dest, xmm_source);
    block->write<uint8_t>(0x0F);
    block->write<uint8_t>(0xF9);
    modrm(0b11, xmm_dest, xmm_source);
}

void Emitter64::PSUBSB(REG_64 xmm_source, REG_64 xmm_dest)
{
    block->write<uint8_t>(0x66);
    rex_r_rm(xmm_dest, xmm_source);
    block->write<uint8_t>(0x0F);
    block->write<uint8_t>(0xE8);
    modrm(0b11, xmm_dest, xmm_source);
}

void Emitter64::PSUBSW(REG_64 xmm_source, REG_64 xmm_dest)
{
    block->write<uint8_t>(0x66);
    rex_r_rm(xmm_dest, xmm_source);
    block->write<uint8_t>(0x0F);
    block->write<uint8_t>(0xE9);
    modrm(0b11, xmm_dest, xmm_source);
}

void Emitter64::PSUBSD(REG_64 xmm_source, REG_64 xmm_dest)
{
    block->write<uint8_t>(0x66);
    rex_r_rm(xmm_dest, xmm_source);
    block->write<uint8_t>(0x0F);
    block->write<uint8_t>(0xE9);
    modrm(0b11, xmm_dest, xmm_source);
}

void Emitter64::PSUBUSB(REG_64 xmm_source, REG_64 xmm_dest)
{
    block->write<uint8_t>(0x66);
    rex_r_rm(xmm_dest, xmm_source);
    block->write<uint8_t>(0x0F);
    block->write<uint8_t>(0xD8);
    modrm(0b11, xmm_dest, xmm_source);
}

void Emitter64::PSUBUSW(REG_64 xmm_source, REG_64 xmm_dest)
{
    block->write<uint8_t>(0x66);
    rex_r_rm(xmm_dest, xmm_source);
    block->write<uint8_t>(0x0F);
    block->write<uint8_t>(0xD9);
    modrm(0b11, xmm_dest, xmm_source);
}

void Emitter64::PXOR_XMM(REG_64 xmm_source, REG_64 xmm_dest)
{
    block->write<uint8_t>(0x66);
    rex_r_rm(xmm_dest, xmm_source);
    block->write<uint8_t>(0x0F);
    block->write<uint8_t>(0xEF);
    modrm(0b11, xmm_dest, xmm_source);
}

void Emitter64::PXOR_XMM_FROM_MEM(REG_64 indir_source, REG_64 xmm_dest, uint32_t offset)
{
    block->write<uint8_t>(0x66);
    rex_r_rm(xmm_dest, indir_source);
    block->write<uint8_t>(0x0F);
    block->write<uint8_t>(0xEF);
    modrm(0, xmm_dest, indir_source);
    if ((indir_source & 7) == 4)
        block->write<uint8_t>(0x24);
    if ((indir_source & 7) == 5 || offset)
        block->write<uint32_t>(offset);
}

void Emitter64::ADDPS(REG_64 xmm_source, REG_64 xmm_dest)
{
    rex_r_rm(xmm_dest, xmm_source);
    block->write<uint8_t>(0x0F);
    block->write<uint8_t>(0x58);
    modrm(0b11, xmm_dest, xmm_source);
}

void Emitter64::ADDSS(REG_64 xmm_source, REG_64 xmm_dest)
{
    block->write<uint8_t>(0xF3);
    rex_r_rm(xmm_dest, xmm_source);
    block->write<uint8_t>(0x0F);
    block->write<uint8_t>(0x58);
    modrm(0b11, xmm_dest, xmm_source);
}

void Emitter64::BLENDPS(uint8_t imm, REG_64 xmm_source, REG_64 xmm_dest)
{
    block->write<uint8_t>(0x66);
    rex_r_rm(xmm_dest, xmm_source);
    block->write<uint8_t>(0x0F);
    block->write<uint8_t>(0x3A);
    block->write<uint8_t>(0x0C);
    modrm(0b11, xmm_dest, xmm_source);
    block->write<uint8_t>(imm);
}

void Emitter64::BLENDVPS_XMM0(REG_64 xmm_source, REG_64 xmm_dest)
{
    block->write<uint8_t>(0x66);
    rex_r_rm(xmm_dest, xmm_source);
    block->write<uint8_t>(0x0F);
    block->write<uint8_t>(0x38);
    block->write<uint8_t>(0x14);
    modrm(0b11, xmm_dest, xmm_source);
}

void Emitter64::CMPEQPS(REG_64 xmm_source, REG_64 xmm_dest)
{
    rex_r_rm(xmm_dest, xmm_source);
    block->write<uint8_t>(0x0F);
    block->write<uint8_t>(0xC2);
    modrm(0b11, xmm_dest, xmm_source);
    block->write<uint8_t>(0x00);
}

void Emitter64::CMPNLEPS(REG_64 xmm_source, REG_64 xmm_dest)
{
    rex_r_rm(xmm_dest, xmm_source);
    block->write<uint8_t>(0x0F);
    block->write<uint8_t>(0xC2);
    modrm(0b11, xmm_dest, xmm_source);
    block->write<uint8_t>(0x06);
}

void Emitter64::DIVPS(REG_64 xmm_source, REG_64 xmm_dest)
{
    rex_r_rm(xmm_dest, xmm_source);
    block->write<uint8_t>(0x0F);
    block->write<uint8_t>(0x5E);
    modrm(0b11, xmm_dest, xmm_source);
}

void Emitter64::DIVSS(REG_64 xmm_source, REG_64 xmm_dest)
{
    block->write<uint8_t>(0xF3);
    rex_r_rm(xmm_dest, xmm_source);
    block->write<uint8_t>(0x0F);
    block->write<uint8_t>(0x5E);
    modrm(0b11, xmm_dest, xmm_source);
}

void Emitter64::DPPS(uint8_t imm, REG_64 xmm_source, REG_64 xmm_dest)
{
    block->write<uint8_t>(0x66);
    rex_r_rm(xmm_dest, xmm_source);
    block->write<uint8_t>(0x0F);
    block->write<uint8_t>(0x3A);
    block->write<uint8_t>(0x40);
    modrm(0b11, xmm_dest, xmm_source);
    block->write<uint8_t>(imm);
}

void Emitter64::INSERTPS(uint8_t count_s, uint8_t count_d, uint8_t zmask, REG_64 xmm_source, REG_64 xmm_dest)
{
    block->write<uint8_t>(0x66);
    rex_r_rm(xmm_dest, xmm_source);
    block->write<uint8_t>(0x0F);
    block->write<uint8_t>(0x3A);
    block->write<uint8_t>(0x21);
    modrm(0b11, xmm_dest, xmm_source);

    uint8_t imm = (count_s & 0x3) << 6;
    imm |= (count_d & 0x3) << 4;
    imm |= zmask & 0xF;
    block->write<uint8_t>(imm);
}

void Emitter64::MAXPS(REG_64 xmm_source, REG_64 xmm_dest)
{
    rex_r_rm(xmm_dest, xmm_source);
    block->write<uint8_t>(0x0F);
    block->write<uint8_t>(0x5F);
    modrm(0b11, xmm_dest, xmm_source);
}

void Emitter64::MAXSS(REG_64 xmm_source, REG_64 xmm_dest)
{
    block->write<uint8_t>(0xF3);
    rex_r_rm(xmm_dest, xmm_source);
    block->write<uint8_t>(0x0F);
    block->write<uint8_t>(0x5F);
    modrm(0b11, xmm_dest, xmm_source);
}

void Emitter64::MINPS(REG_64 xmm_source, REG_64 xmm_dest)
{
    rex_r_rm(xmm_dest, xmm_source);
    block->write<uint8_t>(0x0F);
    block->write<uint8_t>(0x5D);
    modrm(0b11, xmm_dest, xmm_source);
}

void Emitter64::MINSS(REG_64 xmm_source, REG_64 xmm_dest)
{
    block->write<uint8_t>(0xF3);
    rex_r_rm(xmm_dest, xmm_source);
    block->write<uint8_t>(0x0F);
    block->write<uint8_t>(0x5D);
    modrm(0b11, xmm_dest, xmm_source);
}


void Emitter64::MULPS(REG_64 xmm_source, REG_64 xmm_dest)
{
    rex_r_rm(xmm_dest, xmm_source);
    block->write<uint8_t>(0x0F);
    block->write<uint8_t>(0x59);
    modrm(0b11, xmm_dest, xmm_source);
}

void Emitter64::MULSS(REG_64 xmm_source, REG_64 xmm_dest)
{
    block->write<uint8_t>(0xF3);
    rex_r_rm(xmm_dest, xmm_source);
    block->write<uint8_t>(0x0F);
    block->write<uint8_t>(0x59);
    modrm(0b11, xmm_dest, xmm_source);
}

void Emitter64::SHUFPS(uint8_t imm, REG_64 xmm_source, REG_64 xmm_dest)
{
    rex_r_rm(xmm_dest, xmm_source);
    block->write<uint8_t>(0x0F);
    block->write<uint8_t>(0xC6);
    modrm(0b11, xmm_dest, xmm_source);
    block->write<uint8_t>(imm);
}

void Emitter64::SQRTSS(REG_64 xmm_source, REG_64 xmm_dest)
{
    block->write<uint8_t>(0xF3);
    rex_r_rm(xmm_dest, xmm_source);
    block->write<uint8_t>(0x0F);
    block->write<uint8_t>(0x51);
    modrm(0b11, xmm_dest, xmm_source);
}

void Emitter64::SQRTPS(REG_64 xmm_source, REG_64 xmm_dest)
{
    rex_r_rm(xmm_dest, xmm_source);
    block->write<uint8_t>(0x0F);
    block->write<uint8_t>(0x51);
    modrm(0b11, xmm_dest, xmm_source);
}

void Emitter64::SUBPS(REG_64 xmm_source, REG_64 xmm_dest)
{
    rex_r_rm(xmm_dest, xmm_source);
    block->write<uint8_t>(0x0F);
    block->write<uint8_t>(0x5C);
    modrm(0b11, xmm_dest, xmm_source);
}

void Emitter64::SUBSS(REG_64 xmm_source, REG_64 xmm_dest)
{
    block->write<uint8_t>(0xF3);
    rex_r_rm(xmm_dest, xmm_source);
    block->write<uint8_t>(0x0F);
    block->write<uint8_t>(0x5C);
    modrm(0b11, xmm_dest, xmm_source);
}

void Emitter64::RSQRTSS(REG_64 xmm_source, REG_64 xmm_dest)
{
    block->write<uint8_t>(0xF3);
    rex_r_rm(xmm_dest, xmm_source);
    block->write<uint8_t>(0x0F);
    block->write<uint8_t>(0x52);
    modrm(0b11, xmm_dest, xmm_source);
}

void Emitter64::RSQRTPS(REG_64 xmm_source, REG_64 xmm_dest)
{
    rex_r_rm(xmm_dest, xmm_source);
    block->write<uint8_t>(0x0F);
    block->write<uint8_t>(0x52);
    modrm(0b11, xmm_dest, xmm_source);
}

void Emitter64::UCOMISS(REG_64 xmm_source, REG_64 xmm_dest)
{
    rex_r_rm(xmm_dest, xmm_source);
    block->write<uint8_t>(0x0F);
    block->write<uint8_t>(0x2E);
    modrm(0b11, xmm_dest, xmm_source);
}

void Emitter64::XORPS(REG_64 xmm_source, REG_64 xmm_dest)
{
    rex_r_rm(xmm_dest, xmm_source);
    block->write<uint8_t>(0x0F);
    block->write<uint8_t>(0x57);
    modrm(0b11, xmm_dest, xmm_source);
}

void Emitter64::CVTDQ2PS(REG_64 xmm_source, REG_64 xmm_dest)
{
    rex_r_rm(xmm_dest, xmm_source);
    block->write<uint8_t>(0x0F);
    block->write<uint8_t>(0x5B);
    modrm(0b11, xmm_dest, xmm_source);
}

void Emitter64::CVTSS2SI(REG_64 xmm_source, REG_64 int_dest)
{
    block->write<uint8_t>(0xF3);
    rex_r_rm(int_dest, xmm_source);
    block->write<uint8_t>(0x0F);
    block->write<uint8_t>(0x2D);
    modrm(0b11, int_dest, xmm_source);
}

void Emitter64::CVTSI2SS(REG_64 int_source, REG_64 xmm_dest)
{
    block->write<uint8_t>(0xF3);
    rex_r_rm(xmm_dest, int_source);
    block->write<uint8_t>(0x0F);
    block->write<uint8_t>(0x2A);
    modrm(0b11, xmm_dest, int_source);
}

void Emitter64::CVTTPS2DQ(REG_64 xmm_source, REG_64 xmm_dest)
{
    block->write<uint8_t>(0xF3);
    rex_r_rm(xmm_dest, xmm_source);
    block->write<uint8_t>(0x0F);
    block->write<uint8_t>(0x5B);
    modrm(0b11, xmm_dest, xmm_source);
}

void Emitter64::VADDSS(REG_64 xmm_source, REG_64 xmm_source2, REG_64 xmm_dest)
{
    vex(xmm_source, xmm_source2, xmm_dest, false);
    block->write<uint8_t>(0x58);
    modrm(0b11, 0, xmm_source2);
}

void Emitter64::VMINPS(REG_64 xmm_source, REG_64 xmm_source2, REG_64 xmm_dest)
{
    vex(xmm_source, xmm_source2, xmm_dest, true);
    block->write<uint8_t>(0x5D);
    modrm(0b11, 0, xmm_source2);
}

void Emitter64::VMAXPS(REG_64 xmm_source, REG_64 xmm_source2, REG_64 xmm_dest)
{
    vex(xmm_source, xmm_source2, xmm_dest, true);
    block->write<uint8_t>(0x5F);
    modrm(0b11, 0, xmm_source2);
}
