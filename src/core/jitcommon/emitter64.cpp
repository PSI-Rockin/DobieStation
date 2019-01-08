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
    int offset = get_rip_offset(addr);
    rexw_r(dest);
    cache->write<uint8_t>(0x8B);
    modrm(0, dest, DISP32);
    cache->write<uint32_t>(offset - 7);
}

void Emitter64::ADD16_REG_IMM(uint16_t imm, REG_64 dest)
{
    cache->write<uint8_t>(0x66);
    rex_rm(dest);
    cache->write<uint8_t>(0x81);
    modrm(0b11, 0, dest);
    cache->write<uint16_t>(imm);
}

void Emitter64::ADD64_REG(REG_64 source, REG_64 dest)
{
    rexw_r_rm(source, dest);
    cache->write<uint8_t>(0x01);
    modrm(0b11, source, dest);
}

void Emitter64::INC16(REG_64 dest)
{
    cache->write<uint8_t>(0x66);
    rex_rm(dest);
    cache->write<uint8_t>(0xFF);
    modrm(0b11, 0, dest);
}

void Emitter64::AND16_AX(uint16_t imm)
{
    cache->write<uint8_t>(0x66);
    cache->write<uint8_t>(0x25);
    cache->write<uint16_t>(imm);
}

void Emitter64::SHL32_REG_IMM(uint8_t shift, REG_64 dest)
{
    rex_rm(dest);
    cache->write<uint8_t>(0xC1);
    modrm(0b11, 4, dest);
    cache->write<uint8_t>(shift);
}

void Emitter64::MOV16_REG(REG_64 source, REG_64 dest)
{
    cache->write<uint8_t>(0x66);
    rex_r_rm(source, dest);
    cache->write<uint8_t>(0x89);
    modrm(0b11, source, dest);
}

void Emitter64::MOVZX32_REG(REG_64 source, REG_64 dest)
{
    rex_r_rm(dest, source);
    cache->write<uint8_t>(0x0F);
    cache->write<uint8_t>(0xB7);
    modrm(0b11, dest, source);
}

void Emitter64::MOV16_REG_IMM(uint16_t imm, REG_64 dest)
{
    cache->write<uint8_t>(0x66);
    rex_rm(dest);
    cache->write<uint8_t>(0xB8 + (dest & 0x7));
    cache->write<uint16_t>(imm);
}

void Emitter64::MOV16_TO_MEM(REG_64 source, REG_64 indir_dest)
{
    cache->write<uint8_t>(0x66);
    rex_r(source);
    cache->write<uint8_t>(0x89);
    modrm(0, source, indir_dest);
}

void Emitter64::MOV16_IMM_MEM(uint16_t imm, REG_64 indir_dest)
{
    cache->write<uint8_t>(0x66);
    rex_rm(indir_dest);
    cache->write<uint8_t>(0xC7);
    modrm(0, 0, indir_dest);
    cache->write<uint16_t>(imm);
}

void Emitter64::MOV32_TO_MEM(REG_64 source, REG_64 indir_dest)
{
    rex_r(source);
    cache->write<uint8_t>(0x89);
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

void Emitter64::MOV64_FROM_MEM(REG_64 indir_source, REG_64 dest)
{
    rexw_r(dest);
    cache->write<uint8_t>(0x8B);
    modrm(0, dest, indir_source);
}

void Emitter64::MOV64_TO_MEM(REG_64 source, REG_64 indir_dest)
{
    rexw_r(source);
    cache->write<uint8_t>(0x89);
    modrm(0, source, indir_dest);
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
    offset -= (uint64_t)cache->get_current_addr();
    cache->write<uint8_t>(0xE8);
    cache->write<uint32_t>(offset - 5);
}

void Emitter64::RET()
{
    cache->write<uint8_t>(0xC3);
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
