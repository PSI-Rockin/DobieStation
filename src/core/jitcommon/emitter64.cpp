#include "emitter64.hpp"

#define DISP32 0b101

Emitter64::Emitter64(JitCache* cache) : cache(cache)
{

}

void Emitter64::rex_r(REG_64 reg)
{
    if (reg & 0x8)
        cache->write<uint8_t>(0x44);
}

void Emitter64::rex_rm(REG_64 rm)
{
    if (rm & 0x8)
        cache->write<uint8_t>(0x41);
}

void Emitter64::rex_r_rm(REG_64 reg, REG_64 rm)
{
    uint8_t rex = 0x40;
    if (reg & 0x8)
        rex |= 0x4;
    if (rm & 0x8)
        rex |= 0x1;

    if (rex & 0xF)
        cache->write<uint8_t>(rex);
}

void Emitter64::rexw_r(REG_64 reg)
{
    uint8_t rex = 0x48;
    if (reg & 0x8)
        rex |= 0x4;
    cache->write<uint8_t>(rex);
}

void Emitter64::rexw_rm(REG_64 rm)
{
    uint8_t rex = 0x48;
    if (rm & 0x8)
        rex |= 0x1;
    cache->write<uint8_t>(rex);
}

void Emitter64::rexw_r_rm(REG_64 reg, REG_64 rm)
{
    uint8_t rex = 0x48;
    if (reg & 0x8)
        rex |= 0x4;
    if (rm & 0x8)
        rex |= 0x1;
    cache->write<uint8_t>(rex);
}

void Emitter64::modrm(uint8_t mode, uint8_t reg, uint8_t rm)
{
    cache->write<uint8_t>(((mode & 0x3) << 6) | ((reg & 0x7) << 3) | (rm & 0x7));
}

int Emitter64::get_rip_offset(uint64_t addr)
{
    int64_t offset = (uint64_t)cache->get_literal_offset<uint64_t>(addr);
    offset -= (uint64_t)cache->get_current_block_pos();
    return offset;
}

void Emitter64::load_addr(uint64_t addr, REG_64 dest)
{
    uint8_t* start = cache->get_current_block_start();
    uint8_t* pos = cache->get_current_block_pos();
    int offset = get_rip_offset(addr);
    rexw_r(dest);
    cache->write<uint8_t>(0x8B);
    modrm(0, dest, DISP32);
    cache->write<uint32_t>(offset - 7);
}

void Emitter64::ADD16_REG(REG_64 source, REG_64 dest)
{
    cache->write<uint8_t>(0x66);
    rex_r_rm(source, dest);
    cache->write<uint8_t>(0x01);
    modrm(0b11, source, dest);
}

void Emitter64::ADD16_REG_IMM(uint16_t imm, REG_64 dest)
{
    cache->write<uint8_t>(0x66);
    rex_rm(dest);
    cache->write<uint8_t>(0x81);
    modrm(0b11, 0, dest);
    cache->write<uint16_t>(imm);
}

void Emitter64::ADD32_REG(REG_64 source, REG_64 dest)
{
    rex_r_rm(source, dest);
    cache->write<uint8_t>(0x01);
    modrm(0b11, source, dest);
}

void Emitter64::ADD32_REG_IMM(uint32_t imm, REG_64 dest)
{
    rex_rm(dest);

    if (imm >> 8)
    {
        cache->write<uint8_t>(0x81);
        modrm(0b11, 0, dest);
        cache->write<uint32_t>(imm);
    }
    else
    {
        cache->write<uint8_t>(0x83);
        modrm(0b11, 0, dest);
        cache->write<uint8_t>(imm);
    }
}

void Emitter64::ADD64_REG(REG_64 source, REG_64 dest)
{
    rexw_r_rm(source, dest);
    cache->write<uint8_t>(0x01);
    modrm(0b11, source, dest);
}

void Emitter64::ADD64_REG_IMM(uint32_t imm, REG_64 dest)
{
    rexw_rm(dest);
    cache->write<uint8_t>(0x81);
    modrm(0b11, 0, dest);
    cache->write<uint32_t>(imm);
}

void Emitter64::INC16(REG_64 dest)
{
    cache->write<uint8_t>(0x66);
    rex_rm(dest);
    cache->write<uint8_t>(0xFF);
    modrm(0b11, 0, dest);
}

void Emitter64::AND8_REG_IMM(uint8_t imm, REG_64 dest)
{
    rex_rm(dest);
    cache->write<uint8_t>(0x80);
    modrm(0b11, 4, dest);
    cache->write<uint8_t>(imm);
}

void Emitter64::AND16_AX(uint16_t imm)
{
    cache->write<uint8_t>(0x66);
    cache->write<uint8_t>(0x25);
    cache->write<uint16_t>(imm);
}

void Emitter64::AND16_REG(REG_64 source, REG_64 dest)
{
    cache->write<uint8_t>(0x66);
    rex_r_rm(source, dest);
    cache->write<uint8_t>(0x21);
    modrm(0b11, source, dest);
}

void Emitter64::AND16_REG_IMM(uint16_t imm, REG_64 dest)
{
    cache->write<uint8_t>(0x66);
    rex_rm(dest);
    cache->write<uint8_t>(0x81);
    modrm(0b11, 4, dest);
    cache->write<uint16_t>(imm);
}

void Emitter64::AND32_EAX(uint32_t imm)
{
    cache->write<uint8_t>(0x25);
    cache->write<uint32_t>(imm);
}

void Emitter64::AND32_REG(REG_64 source, REG_64 dest)
{
    rex_r_rm(source, dest);
    cache->write<uint8_t>(0x21);
    modrm(0b11, source, dest);
}

void Emitter64::AND32_REG_IMM(uint32_t imm, REG_64 dest)
{
    rex_rm(dest);
    cache->write<uint8_t>(0x81);
    modrm(0b11, 4, dest);
    cache->write<uint32_t>(imm);
}

void Emitter64::AND64_REG(REG_64 source, REG_64 dest)
{
    rexw_r_rm(source, dest);
    cache->write<uint8_t>(0x21);
    modrm(0b11, source, dest);
}

void Emitter64::CMOVCC16_REG(ConditionCode cc, REG_64 source, REG_64 dest)
{
    cache->write<uint8_t>(0x66);
    rex_r_rm(dest, source);
    cache->write<uint8_t>(0x0F);
    cache->write<uint8_t>((int)cc | 0x40);
    modrm(0b11, dest, source);
}

void Emitter64::CMOVCC32_REG(ConditionCode cc, REG_64 source, REG_64 dest)
{
    rex_r_rm(dest, source);
    cache->write<uint8_t>(0x0F);
    cache->write<uint8_t>((int)cc | 0x40);
    modrm(0b11, dest, source);
}

void Emitter64::CMOVCC64_REG(ConditionCode cc, REG_64 source, REG_64 dest)
{
    rexw_r_rm(dest, source);
    cache->write<uint8_t>(0x0F);
    cache->write<uint8_t>((int)cc | 0x40);
    modrm(0b11, dest, source);
}

void Emitter64::CMOVCC16_MEM(ConditionCode cc, REG_64 source, REG_64 indir_dest, uint32_t offset)
{
    cache->write<uint8_t>(0x66);
    cache->write<uint8_t>(0x0F);
    cache->write<uint8_t>((int)cc | 0x40);

    // 32-bit offsets only for now
    if (offset)
    {
        modrm(0b10, indir_dest, source);
        cache->write<uint32_t>(offset);
    }
    else
        modrm(0b00, indir_dest, source);
}

void Emitter64::CMOVCC32_MEM(ConditionCode cc, REG_64 source, REG_64 indir_dest, uint32_t offset)
{
    cache->write<uint8_t>(0x0F);
    cache->write<uint8_t>((int)cc | 0x40);

    // 32-bit offsets only for now
    if (offset)
    {
        modrm(0b10, indir_dest, source);
        cache->write<uint32_t>(offset);
    }
    else
        modrm(0b00, indir_dest, source);
}

void Emitter64::CMOVCC64_MEM(ConditionCode cc, REG_64 source, REG_64 indir_dest, uint32_t offset)
{
    rexw_r_rm(indir_dest, source);
    cache->write<uint8_t>(0x0F);
    cache->write<uint8_t>((int)cc | 0x40);

    // 32-bit offsets only for now
    if (offset)
    {
        modrm(0b10, indir_dest, source);
        cache->write<uint32_t>(offset);
    }
    else
        modrm(0b00, indir_dest, source);
}

void Emitter64::CMP8_REG(REG_64 op2, REG_64 op1)
{
    rex_r_rm(op2, op1);
    cache->write<uint8_t>(0x38);
    modrm(0b11, op2, op1);
}

void Emitter64::CMP16_IMM(uint16_t imm, REG_64 op)
{
    cache->write<uint8_t>(0x66);
    rex_rm(op);
    cache->write<uint8_t>(0x81);
    modrm(0b11, 7, op);
    cache->write<uint16_t>(imm);
}

void Emitter64::CMP16_REG(REG_64 op2, REG_64 op1)
{
    cache->write<uint8_t>(0x66);
    rex_r_rm(op2, op1);
    cache->write<uint8_t>(0x39);
    modrm(0b11, op2, op1);
}

void Emitter64::CMP32_IMM(uint32_t imm, REG_64 op)
{
    rex_rm(op);
    cache->write<uint8_t>(0x81);
    modrm(0b11, 7, op);
    cache->write<uint32_t>(imm);
}

void Emitter64::CMP32_REG(REG_64 op2, REG_64 op1)
{
    cache->write<uint8_t>(0x39);
    modrm(0b11, op2, op1);
}

void Emitter64::CMP32_EAX(uint32_t imm)
{
    cache->write<uint8_t>(0x3D);
    cache->write<uint32_t>(imm);
}

void Emitter64::CMP64_IMM(uint32_t imm, REG_64 op)
{
    rexw_rm(op);
    cache->write<uint8_t>(0x81);
    modrm(0b11, 7, op);
    cache->write<uint32_t>(imm);
}

void Emitter64::CMP64_REG(REG_64 op2, REG_64 op1)
{
    rexw_r_rm(op2, op1);
    cache->write<uint8_t>(0x39);
    modrm(0b11, op2, op1);
}

void Emitter64::DEC16(REG_64 dest)
{
    cache->write<uint8_t>(0x66);
    rex_rm(dest);
    cache->write<uint8_t>(0xFF);
    modrm(0b11, 1, dest);
}

void Emitter64::NOT16(REG_64 dest)
{
    cache->write<uint8_t>(0x66);
    rex_rm(dest);
    cache->write<uint8_t>(0xF7);
    modrm(0b11, 2, dest);
}

void Emitter64::OR16_REG(REG_64 source, REG_64 dest)
{
    cache->write<uint8_t>(0x66);
    rex_r_rm(source, dest);
    cache->write<uint8_t>(0x09);
    modrm(0b11, source, dest);
}

void Emitter64::OR32_REG(REG_64 source, REG_64 dest)
{
    rex_r_rm(source, dest);
    cache->write<uint8_t>(0x09);
    modrm(0b11, source, dest);
}

void Emitter64::OR32_EAX(uint32_t imm)
{
    cache->write<uint8_t>(0x0D);
    cache->write<uint32_t>(imm);
}

void Emitter64::OR64_REG(REG_64 source, REG_64 dest)
{
    rexw_r_rm(source, dest);
    cache->write<uint8_t>(0x09);
    modrm(0b11, source, dest);
}

void Emitter64::SETCC_REG(ConditionCode cc, REG_64 dest)
{
    rex_rm(dest);

    // FIXME: fix encoding so we don't explicitly write 0x40 here :D
    if (!(dest & 0x8))
        cache->write<uint8_t>(0x40);

    cache->write<uint8_t>(0x0F);
    cache->write<uint8_t>((int)cc | 0x90);
    modrm(0b11, 0, dest);
}

void Emitter64::SETCC_MEM(ConditionCode cc, REG_64 indir_dest, uint32_t offset)
{
    rex_rm(indir_dest);
    cache->write<uint8_t>(0x0F);
    cache->write<uint8_t>((int)cc | 0x90);

    // 32-bit offsets only for now
    if (offset)
    {
        modrm(0b10, 0, indir_dest);
        cache->write<uint32_t>(offset);
    }
    else
        modrm(0b00, 0, indir_dest);
}

void Emitter64::SHL16_REG_1(REG_64 dest)
{
    cache->write<uint8_t>(0x66);
    rex_rm(dest);
    cache->write<uint8_t>(0xD1);
    modrm(0b11, 4, dest);
}

void Emitter64::SHL16_CL(REG_64 dest)
{
    cache->write<uint8_t>(0x66);
    rex_rm(dest);
    cache->write<uint8_t>(0xD3);
    modrm(0b11, 4, dest);
}

void Emitter64::SHL16_REG_IMM(uint8_t shift, REG_64 dest)
{
    cache->write<uint8_t>(0x66);
    rex_rm(dest);
    cache->write<uint8_t>(0xC1);
    modrm(0b11, 4, dest);
    cache->write<uint8_t>(shift);
}

void Emitter64::SHL32_CL(REG_64 dest)
{
    rex_rm(dest);
    cache->write<uint8_t>(0xD3);
    modrm(0b11, 4, dest);
}

void Emitter64::SHL32_REG_IMM(uint8_t shift, REG_64 dest)
{
    rex_rm(dest);
    cache->write<uint8_t>(0xC1);
    modrm(0b11, 4, dest);
    cache->write<uint8_t>(shift);
}

void Emitter64::SHL64_CL(REG_64 dest)
{
    rexw_rm(dest);
    cache->write<uint8_t>(0xD3);
    modrm(0b11, 4, dest);
}

void Emitter64::SHL64_REG_IMM(uint8_t shift, REG_64 dest)
{
    rexw_rm(dest);
    cache->write<uint8_t>(0xC1);
    modrm(0b11, 4, dest);
    cache->write<uint8_t>(shift);
}

void Emitter64::SHR16_CL(REG_64 dest)
{
    cache->write<uint8_t>(0x66);
    rex_rm(dest);
    cache->write<uint8_t>(0xD3);
    modrm(0b11, 5, dest);
}

void Emitter64::SHR16_REG_IMM(uint8_t shift, REG_64 dest)
{
    cache->write<uint8_t>(0x66);
    rex_rm(dest);
    cache->write<uint8_t>(0xC1);
    modrm(0b11, 5, dest);
    cache->write<uint8_t>(shift);
}

void Emitter64::SHR32_CL(REG_64 dest)
{
    rex_rm(dest);
    cache->write<uint8_t>(0xD3);
    modrm(0b11, 5, dest);
}

void Emitter64::SHR32_REG_IMM(uint8_t shift, REG_64 dest)
{
    rex_rm(dest);
    cache->write<uint8_t>(0xC1);
    modrm(0b11, 5, dest);
    cache->write<uint8_t>(shift);
}

void Emitter64::SHR64_CL(REG_64 dest)
{
    rexw_rm(dest);
    cache->write<uint8_t>(0xD3);
    modrm(0b11, 5, dest);
}

void Emitter64::SHR64_REG_IMM(uint8_t shift, REG_64 dest)
{
    rexw_rm(dest);
    cache->write<uint8_t>(0xC1);
    modrm(0b11, 5, dest);
    cache->write<uint8_t>(shift);
}

void Emitter64::SAR16_CL(REG_64 dest)
{
    cache->write<uint8_t>(0x66);
    rex_rm(dest);
    cache->write<uint8_t>(0xD3);
    modrm(0b11, 7, dest);
}

void Emitter64::SAR16_REG_IMM(uint8_t shift, REG_64 dest)
{
    cache->write<uint8_t>(0x66);
    rex_rm(dest);
    cache->write<uint8_t>(0xC1);
    modrm(0b11, 7, dest);
    cache->write<uint8_t>(shift);
}

void Emitter64::SAR32_CL(REG_64 dest)
{
    rex_rm(dest);
    cache->write<uint8_t>(0xD3);
    modrm(0b11, 7, dest);
}

void Emitter64::SAR32_REG_IMM(uint8_t shift, REG_64 dest)
{
    rex_rm(dest);
    cache->write<uint8_t>(0xC1);
    modrm(0b11, 7, dest);
    cache->write<uint8_t>(shift);
}

void Emitter64::SAR64_CL(REG_64 dest)
{
    rexw_rm(dest);
    cache->write<uint8_t>(0xD3);
    modrm(0b11, 7, dest);
}

void Emitter64::SAR64_REG_IMM(uint8_t shift, REG_64 dest)
{
    rexw_rm(dest);
    cache->write<uint8_t>(0xC1);
    modrm(0b11, 7, dest);
    cache->write<uint8_t>(shift);
}

void Emitter64::SUB16_REG_IMM(uint16_t imm, REG_64 dest)
{
    cache->write<uint8_t>(0x66);
    rex_rm(dest);
    cache->write<uint8_t>(0x81);
    modrm(0b11, 5, dest);
    cache->write<uint16_t>(imm);
}

void Emitter64::SUB32_REG(REG_64 source, REG_64 dest)
{
    rex_r_rm(source, dest);
    cache->write<uint8_t>(0x29);
    modrm(0b11, source, dest);
}

void Emitter64::SUB64_REG_IMM(uint32_t imm, REG_64 dest)
{
    rexw_rm(dest);
    cache->write<uint8_t>(0x81);
    modrm(0b11, 5, dest);
    cache->write<uint32_t>(imm);
}

void Emitter64::TEST8_REG(REG_64 op2, REG_64 op1)
{
    rex_r_rm(op2, op1);
    cache->write<uint8_t>(0x84);
    modrm(0b11, op2, op1);
}

void Emitter64::TEST16_REG(REG_64 op2, REG_64 op1)
{
    cache->write<uint8_t>(0x66);
    rex_r_rm(op2, op1);
    cache->write<uint8_t>(0x85);
    modrm(0b11, op2, op1);
}

void Emitter64::TEST32_EAX(uint32_t imm)
{
    cache->write<uint8_t>(0xA9);
    cache->write<uint32_t>(imm);
}

void Emitter64::TEST32_REG(REG_64 op2, REG_64 op1)
{
    rex_r_rm(op2, op1);
    cache->write<uint8_t>(0x85);
    modrm(0b11, op2, op1);
}

void Emitter64::TEST64_REG(REG_64 op2, REG_64 op1)
{
    rexw_r_rm(op2, op1);
    cache->write<uint8_t>(0x85);
    modrm(0b11, op2, op1);
}

void Emitter64::XOR16_REG(REG_64 source, REG_64 dest)
{
    cache->write<uint8_t>(0x66);
    rex_r_rm(source, dest);
    cache->write<uint8_t>(0x31);
    modrm(0b11, source, dest);
}

void Emitter64::XOR32_REG(REG_64 source, REG_64 dest)
{
    rex_r_rm(source, dest);
    cache->write<uint8_t>(0x31);
    modrm(0b11, source, dest);
}

void Emitter64::XOR64_REG(REG_64 source, REG_64 dest)
{
    rexw_r_rm(source, dest);
    cache->write<uint8_t>(0x31);
    modrm(0b11, source, dest);
}

void Emitter64::MOV8_REG(REG_64 source, REG_64 dest)
{
    rex_r_rm(source, dest);

    // FIXME: I still don't know anything about encoding these instructions, woooo
    if (!(dest & 0x8) && !(source & 0x8))
        cache->write<uint8_t>(0x40);

    cache->write<uint8_t>(0x88);
    modrm(0b11, source, dest);
}

void Emitter64::MOV8_REG_IMM(uint8_t imm, REG_64 dest)
{
    rex_rm(dest);
    cache->write<uint8_t>(0xB0 + (dest & 0x7));
    cache->write<uint8_t>(imm);
}

void Emitter64::MOV8_TO_MEM(REG_64 source, REG_64 indir_dest, uint32_t offset)
{
    rex_r_rm(source, indir_dest);
    cache->write<uint8_t>(0x88);

    if (indir_dest == REG_64::RBP || offset != 0)
    {
        modrm(0b10, source, indir_dest);
        cache->write<uint32_t>(offset);
    }
    else
        modrm(0, source, indir_dest);
}

void Emitter64::MOV8_FROM_MEM(REG_64 indir_source, REG_64 dest, uint32_t offset)
{
    rex_r_rm(dest, indir_source);
    cache->write<uint8_t>(0x8A);

    if (indir_source == REG_64::RBP || offset != 0)
    {
        modrm(0b10, dest, indir_source);
        cache->write<uint32_t>(offset);
    }
    else
        modrm(0, dest, indir_source);
}

void Emitter64::MOV8_IMM_MEM(uint8_t imm, REG_64 indir_dest)
{
    rex_rm(indir_dest);
    cache->write<uint8_t>(0xC6);
    modrm(0, 0, indir_dest);
    cache->write<uint8_t>(imm);
}

void Emitter64::MOV16_REG(REG_64 source, REG_64 dest)
{
    cache->write<uint8_t>(0x66);
    rex_r_rm(source, dest);
    cache->write<uint8_t>(0x89);
    modrm(0b11, source, dest);
}

void Emitter64::MOV16_REG_IMM(uint16_t imm, REG_64 dest)
{
    cache->write<uint8_t>(0x66);
    rex_rm(dest);
    cache->write<uint8_t>(0xB8 + (dest & 0x7));
    cache->write<uint16_t>(imm);
}

void Emitter64::MOV16_TO_MEM(REG_64 source, REG_64 indir_dest, uint32_t offset)
{
    cache->write<uint8_t>(0x66);
    rex_r_rm(source, indir_dest);
    cache->write<uint8_t>(0x89);

    if (indir_dest == REG_64::RBP || offset != 0)
    {
        modrm(0b10, source, indir_dest);
        cache->write<uint32_t>(offset);
    }
    else
        modrm(0, source, indir_dest);
} 

void Emitter64::MOV16_FROM_MEM(REG_64 indir_source, REG_64 dest, uint32_t offset)
{
    cache->write<uint8_t>(0x66);
    rex_r_rm(dest, indir_source);
    cache->write<uint8_t>(0x8B);

    if (indir_source == REG_64::RBP || offset != 0)
    {
        modrm(0b10, dest, indir_source);
        cache->write<uint32_t>(offset);
    }
    else
        modrm(0, dest, indir_source);
}

void Emitter64::MOV16_IMM_MEM(uint16_t imm, REG_64 indir_dest)
{
    cache->write<uint8_t>(0x66);
    rex_rm(indir_dest);
    cache->write<uint8_t>(0xC7);
    modrm(0, 0, indir_dest);
    cache->write<uint16_t>(imm);
}

void Emitter64::MOV32_REG(REG_64 source, REG_64 dest)
{
    rex_r_rm(source, dest);
    cache->write<uint8_t>(0x89);
    modrm(0b11, source, dest);
}

void Emitter64::MOV32_REG_IMM(uint32_t imm, REG_64 dest)
{
    rex_rm(dest);
    cache->write<uint8_t>(0xB8 + (dest & 0x7));
    cache->write<uint32_t>(imm);
}

void Emitter64::MOV32_IMM_MEM(uint32_t imm, REG_64 indir_dest)
{
    rex_rm(indir_dest);
    cache->write<uint8_t>(0xC7);
    modrm(0, 0, indir_dest);
    cache->write<uint32_t>(imm);
}

void Emitter64::MOV32_FROM_MEM(REG_64 indir_source, REG_64 dest, uint32_t offset)
{
    rex_r_rm(dest, indir_source);
    cache->write<uint8_t>(0x8B);

    if (indir_source == REG_64::RBP || offset != 0)
    {
        modrm(0b10, dest, indir_source);
        cache->write<uint32_t>(offset);
    }
    else
        modrm(0, dest, indir_source);
}

void Emitter64::MOV32_TO_MEM(REG_64 source, REG_64 indir_dest, uint32_t offset)
{
    rex_r_rm(source, indir_dest);
    cache->write<uint8_t>(0x89);

    if (indir_dest == REG_64::RBP || offset != 0)
    {
        modrm(0b10, source, indir_dest);
        cache->write<uint32_t>(offset);
    }
    else
        modrm(0, source, indir_dest);
}

void Emitter64::MOV64_MR(REG_64 source, REG_64 dest)
{
    rexw_r_rm(source, dest);
    cache->write<uint8_t>(0x89);
    modrm(0b11, source, dest);
}

void Emitter64::MOV64_OI(uint64_t imm, REG_64 dest)
{
    rexw_rm(dest);
    cache->write<uint8_t>(0xB8 + (dest & 0x7));
    cache->write<uint64_t>(imm);
}

void Emitter64::MOV64_FROM_MEM(REG_64 indir_source, REG_64 dest, uint32_t offset)
{
    rexw_r_rm(dest, indir_source);
    cache->write<uint8_t>(0x8B);

    if (indir_source == REG_64::RBP || offset != 0)
    {
        modrm(0b10, dest, indir_source);
        cache->write<uint32_t>(offset);
    }
    else
        modrm(0, dest, indir_source);
}

void Emitter64::MOV64_TO_MEM(REG_64 source, REG_64 indir_dest, uint32_t offset)
{
    rexw_r_rm(source, indir_dest);
    cache->write<uint8_t>(0x89);

    if (indir_dest == REG_64::RBP || offset != 0)
    {
        modrm(0b10, source, indir_dest);
        cache->write<uint32_t>(offset);
    }
    else
        modrm(0, source, indir_dest);
}

void Emitter64::MOVSX16_TO_32(REG_64 source, REG_64 dest)
{
    rex_r_rm(dest, source);
    cache->write<uint8_t>(0x0F);
    cache->write<uint8_t>(0xBF);
    modrm(0b11, dest, source);
}

void Emitter64::MOVSX16_TO_64(REG_64 source, REG_64 dest)
{
    rexw_r_rm(dest, source);
    cache->write<uint8_t>(0x0F);
    cache->write<uint8_t>(0xBF);
    modrm(0b11, dest, source);
}

void Emitter64::MOVSX32_TO_64(REG_64 source, REG_64 dest)
{
    rexw_r_rm(dest, source);
    cache->write<uint8_t>(0x63);
    modrm(0b11, dest, source);
}

void Emitter64::MOVZX8_TO_64(REG_64 source, REG_64 dest)
{
    rexw_r_rm(dest, source);
    cache->write<uint8_t>(0x0F);
    cache->write<uint8_t>(0xB6);
    modrm(0b11, dest, source);
}

void Emitter64::MOVZX16_TO_64(REG_64 source, REG_64 dest)
{
    rexw_r_rm(dest, source);
    cache->write<uint8_t>(0x0F);
    cache->write<uint8_t>(0xB7);
    modrm(0b11, dest, source);
}

void Emitter64::MOVD_FROM_XMM(REG_64 xmm_source, REG_64 dest)
{
    cache->write<uint8_t>(0x66);
    rex_r_rm(xmm_source, dest);
    cache->write<uint8_t>(0x0F);
    cache->write<uint8_t>(0x7E);
    modrm(0b11, xmm_source, dest);
}

void Emitter64::MOVD_TO_XMM(REG_64 source, REG_64 xmm_dest)
{
    cache->write<uint8_t>(0x66);
    rex_r_rm(xmm_dest, source);
    cache->write<uint8_t>(0x0F);
    cache->write<uint8_t>(0x6E);
    modrm(0b11, xmm_dest, source);
}

void Emitter64::MOVAPS_REG(REG_64 xmm_source, REG_64 xmm_dest)
{
    rex_r_rm(xmm_source, xmm_dest);
    cache->write<uint8_t>(0x0F);
    cache->write<uint8_t>(0x29);
    modrm(0b11, xmm_source, xmm_dest);
}

void Emitter64::MOVAPS_FROM_MEM(REG_64 indir_source, REG_64 xmm_dest)
{
    rex_r_rm(xmm_dest, indir_source);
    cache->write<uint8_t>(0x0F);
    cache->write<uint8_t>(0x28);
    modrm(0, xmm_dest, indir_source);
}

void Emitter64::MOVAPS_TO_MEM(REG_64 xmm_source, REG_64 indir_dest)
{
    rex_r_rm(xmm_source, indir_dest);
    cache->write<uint8_t>(0x0F);
    cache->write<uint8_t>(0x29);
    modrm(0, xmm_source, indir_dest);
}

void Emitter64::MOVMSKPS(REG_64 xmm_source, REG_64 dest)
{
    rex_r_rm(dest, xmm_source);
    cache->write<uint8_t>(0x0F);
    cache->write<uint8_t>(0x50);
    modrm(0b11, dest, xmm_source);
}

uint8_t* Emitter64::JMP_NEAR_DEFERRED()
{
    cache->write<uint8_t>(0xE9);
    uint8_t* addr = cache->get_current_block_pos();

    cache->write<uint32_t>(0);
    return addr;
}

uint8_t* Emitter64::JCC_NEAR_DEFERRED(ConditionCode cc)
{
    cache->write<uint8_t>(0x0F);
    cache->write<uint8_t>((int)cc | 0x80);
    uint8_t* addr = cache->get_current_block_pos();

    cache->write<uint32_t>(0);
    return addr;
}

void Emitter64::set_jump_dest(uint8_t *jump)
{
    uint8_t* jump_dest_addr = cache->get_current_block_pos();

    cache->set_current_block_pos(jump);
    int jump_offset = jump_dest_addr - jump - 4;
    cache->write<uint32_t>(jump_offset);

    cache->set_current_block_pos(jump_dest_addr);
}

void Emitter64::PUSH(REG_64 reg)
{
    rex_rm(reg);
    cache->write<uint8_t>(0x50 + (reg & 0x7));
}

void Emitter64::POP(REG_64 reg)
{
    rex_rm(reg);
    cache->write<uint8_t>(0x58 + (reg & 0x7));
}

void Emitter64::CALL(uint64_t func)
{
    int offset = func;
    offset -= (uint64_t)cache->get_current_block_pos();
    cache->write<uint8_t>(0xE8);
    cache->write<uint32_t>(offset - 5);
}

void Emitter64::CALL_INDIR(REG_64 source)
{
    cache->write<uint8_t>(0xFF);
    modrm(0b11, 2, source);
}

void Emitter64::RET()
{
    cache->write<uint8_t>(0xC3);
}

void Emitter64::PAND_XMM(REG_64 xmm_source, REG_64 xmm_dest)
{
    cache->write<uint8_t>(0x66);
    rex_r_rm(xmm_dest, xmm_source);
    cache->write<uint8_t>(0x0F);
    cache->write<uint8_t>(0xDB);
    modrm(0b11, xmm_dest, xmm_source);
}

void Emitter64::PAND_XMM_MEM(REG_64 indir_source, REG_64 xmm_dest)
{
    cache->write<uint8_t>(0x66);
    rex_r_rm(xmm_dest, indir_source);
    cache->write<uint8_t>(0x0F);
    cache->write<uint8_t>(0xDB);
    modrm(0, xmm_dest, indir_source);
}

void Emitter64::PMAXSD_XMM(REG_64 xmm_source, REG_64 xmm_dest)
{
    cache->write<uint8_t>(0x66);
    rex_r_rm(xmm_dest, xmm_source);
    cache->write<uint8_t>(0x0F);
    cache->write<uint8_t>(0x38);
    cache->write<uint8_t>(0x3D);
    modrm(0b11, xmm_dest, xmm_source);
}

void Emitter64::PMINSD_XMM(REG_64 xmm_source, REG_64 xmm_dest)
{
    cache->write<uint8_t>(0x66);
    rex_r_rm(xmm_dest, xmm_source);
    cache->write<uint8_t>(0x0F);
    cache->write<uint8_t>(0x38);
    cache->write<uint8_t>(0x39);
    modrm(0b11, xmm_dest, xmm_source);
}

void Emitter64::PMINSD_XMM_FROM_MEM(REG_64 indir_source, REG_64 xmm_dest)
{
    cache->write<uint8_t>(0x66);
    rex_r_rm(xmm_dest, indir_source);
    cache->write<uint8_t>(0x0F);
    cache->write<uint8_t>(0x38);
    cache->write<uint8_t>(0x39);
    modrm(0, xmm_dest, indir_source);
}

void Emitter64::PMINUD_XMM_FROM_MEM(REG_64 indir_source, REG_64 xmm_dest)
{
    cache->write<uint8_t>(0x66);
    rex_r_rm(xmm_dest, indir_source);
    cache->write<uint8_t>(0x0F);
    cache->write<uint8_t>(0x38);
    cache->write<uint8_t>(0x3B);
    modrm(0, xmm_dest, indir_source);
}

void Emitter64::PSHUFD(uint8_t imm, REG_64 xmm_source, REG_64 xmm_dest)
{
    cache->write<uint8_t>(0x66);
    rex_r_rm(xmm_dest, xmm_source);
    cache->write<uint8_t>(0x0F);
    cache->write<uint8_t>(0x70);
    modrm(0b11, xmm_dest, xmm_source);
    cache->write<uint8_t>(imm);
}

void Emitter64::DIVPS(REG_64 xmm_source, REG_64 xmm_dest)
{
    rex_r_rm(xmm_dest, xmm_source);
    cache->write<uint8_t>(0x0F);
    cache->write<uint8_t>(0x5E);
    modrm(0b11, xmm_dest, xmm_source);
}

void Emitter64::ADDPS(REG_64 xmm_source, REG_64 xmm_dest)
{
    rex_r_rm(xmm_dest, xmm_source);
    cache->write<uint8_t>(0x0F);
    cache->write<uint8_t>(0x58);
    modrm(0b11, xmm_dest, xmm_source);
}

void Emitter64::BLENDPS(uint8_t imm, REG_64 xmm_source, REG_64 xmm_dest)
{
    cache->write<uint8_t>(0x66);
    rex_r_rm(xmm_dest, xmm_source);
    cache->write<uint8_t>(0x0F);
    cache->write<uint8_t>(0x3A);
    cache->write<uint8_t>(0x0C);
    modrm(0b11, xmm_dest, xmm_source);
    cache->write<uint8_t>(imm);
}

void Emitter64::CMPEQPS(REG_64 xmm_source, REG_64 xmm_dest)
{
    rex_r_rm(xmm_dest, xmm_source);
    cache->write<uint8_t>(0x0F);
    cache->write<uint8_t>(0xC2);
    modrm(0b11, xmm_dest, xmm_source);
    cache->write<uint8_t>(0x00);
}

void Emitter64::CMPNLEPS(REG_64 xmm_source, REG_64 xmm_dest)
{
    rex_r_rm(xmm_dest, xmm_source);
    cache->write<uint8_t>(0x0F);
    cache->write<uint8_t>(0xC2);
    modrm(0b11, xmm_dest, xmm_source);
    cache->write<uint8_t>(0x06);
}

void Emitter64::DPPS(uint8_t imm, REG_64 xmm_source, REG_64 xmm_dest)
{
    cache->write<uint8_t>(0x66);
    rex_r_rm(xmm_dest, xmm_source);
    cache->write<uint8_t>(0x0F);
    cache->write<uint8_t>(0x3A);
    cache->write<uint8_t>(0x40);
    modrm(0b11, xmm_dest, xmm_source);
    cache->write<uint8_t>(imm);
}

void Emitter64::INSERTPS(uint8_t count_s, uint8_t count_d, uint8_t zmask, REG_64 xmm_source, REG_64 xmm_dest)
{
    cache->write<uint8_t>(0x66);
    rex_r_rm(xmm_dest, xmm_source);
    cache->write<uint8_t>(0x0F);
    cache->write<uint8_t>(0x3A);
    cache->write<uint8_t>(0x21);
    modrm(0b11, xmm_dest, xmm_source);

    uint8_t imm = (count_s & 0x3) << 6;
    imm |= (count_d & 0x3) << 4;
    imm |= zmask & 0xF;
    cache->write<uint8_t>(imm);
}

void Emitter64::MAXPS(REG_64 xmm_source, REG_64 xmm_dest)
{
    rex_r_rm(xmm_dest, xmm_source);
    cache->write<uint8_t>(0x0F);
    cache->write<uint8_t>(0x5F);
    modrm(0b11, xmm_dest, xmm_source);
}

void Emitter64::MINPS(REG_64 xmm_source, REG_64 xmm_dest)
{
    rex_r_rm(xmm_dest, xmm_source);
    cache->write<uint8_t>(0x0F);
    cache->write<uint8_t>(0x5D);
    modrm(0b11, xmm_dest, xmm_source);
}

void Emitter64::MULPS(REG_64 xmm_source, REG_64 xmm_dest)
{
    rex_r_rm(xmm_dest, xmm_source);
    cache->write<uint8_t>(0x0F);
    cache->write<uint8_t>(0x59);
    modrm(0b11, xmm_dest, xmm_source);
}

void Emitter64::SHUFPS(uint8_t imm, REG_64 xmm_source, REG_64 xmm_dest)
{
    rex_r_rm(xmm_dest, xmm_source);
    cache->write<uint8_t>(0x0F);
    cache->write<uint8_t>(0xC6);
    modrm(0b11, xmm_dest, xmm_source);
    cache->write<uint8_t>(imm);
}

void Emitter64::SQRTPS(REG_64 xmm_source, REG_64 xmm_dest)
{
    rex_r_rm(xmm_dest, xmm_source);
    cache->write<uint8_t>(0x0F);
    cache->write<uint8_t>(0x51);
    modrm(0b11, xmm_dest, xmm_source);
}

void Emitter64::SUBPS(REG_64 xmm_source, REG_64 xmm_dest)
{
    rex_r_rm(xmm_dest, xmm_source);
    cache->write<uint8_t>(0x0F);
    cache->write<uint8_t>(0x5C);
    modrm(0b11, xmm_dest, xmm_source);
}

void Emitter64::XORPS(REG_64 xmm_source, REG_64 xmm_dest)
{
    rex_r_rm(xmm_dest, xmm_source);
    cache->write<uint8_t>(0x0F);
    cache->write<uint8_t>(0x57);
    modrm(0b11, xmm_dest, xmm_source);
}

void Emitter64::CVTDQ2PS(REG_64 xmm_source, REG_64 xmm_dest)
{
    rex_r_rm(xmm_dest, xmm_source);
    cache->write<uint8_t>(0x0F);
    cache->write<uint8_t>(0x5B);
    modrm(0b11, xmm_dest, xmm_source);
}

void Emitter64::CVTTPS2DQ(REG_64 xmm_source, REG_64 xmm_dest)
{
    cache->write<uint8_t>(0xF3);
    rex_r_rm(xmm_dest, xmm_source);
    cache->write<uint8_t>(0x0F);
    cache->write<uint8_t>(0x5B);
    modrm(0b11, xmm_dest, xmm_source);
}
