#ifndef EMITTER64_HPP
#define EMITTER64_HPP
#include "jitcache.hpp"

enum REG_64
{
    EAX = 0, RAX = 0,
    ECX = 1, RCX = 1,
    EDX = 2, RDX = 2,
    EBX = 3, RBX = 3,
    ESP = 4, RSP = 4,
    EBP = 5, RBP = 5,
    ESI = 6, RSI = 6,
    EDI = 7, RDI = 7,
    R8 = 8,
    R9 = 9,
    R10 = 10,
    R11 = 11,
    R12 = 12,
    R13 = 13,
    R14 = 14,
    R15 = 15
};

class Emitter64
{
    private:
        JitCache* cache;

        void rex_extend_gpr(REG_64 reg);
        void rexw_rm(REG_64 rm);
        void rexw_r_rm(REG_64 reg, REG_64 rm);
        void modrm(uint8_t mode, uint8_t reg, uint8_t rm);
    public:
        Emitter64(JitCache* cache);

        void MOV64_MR(REG_64 source, REG_64 dest);
        void MOV64_OI(uint64_t imm, REG_64 dest);

        void PUSH(REG_64 reg);
        void POP(REG_64 reg);
        void CALL(uint64_t addr);
        void RET();
};

#endif // EMITTER64_HPP
