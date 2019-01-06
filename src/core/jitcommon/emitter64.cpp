#include "emitter64.hpp"

Emitter64::Emitter64(JitCache* cache) : cache(cache)
{

}

void Emitter64::rex_extend_gpr(REG_64 reg)
{
    //REX - bit 0 set allows access to r8-r15
    if (reg & 0x8)
        cache->write(0x41);
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
    cache->write<uint8_t>((mode << 6) | (reg << 3) | rm);
}

void Emitter64::MOV32_MI_MEM(uint32_t imm, REG_64 indir_dest)
{
    cache->write<uint8_t>(0xC7);
    modrm(0, 0, indir_dest);
    cache->write<uint32_t>(imm);
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

void Emitter64::PUSH(REG_64 reg)
{
    rex_extend_gpr(reg);
    cache->write<uint8_t>(0x50 + (reg & 0x7));
}

void Emitter64::POP(REG_64 reg)
{
    rex_extend_gpr(reg);
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
