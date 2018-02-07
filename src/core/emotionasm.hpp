#ifndef EMOTIONASM_HPP
#define EMOTIONASM_HPP
#include <cstdint>
#include <string>

enum MIPS_REG
{
    zero, //hardwired to zero
    at, //used only for pseudo-instructions such as li
    v0, v1, //return values
    a0, a1, a2, a3, //function arguments
    t0, t1, t2, t3, t4, t5, t6, t7, //temporary data
    s0, s1, s2, s3, s4, s5, s6, s7, //preserved data
    t8, t9, //more temporary data
    k0, k1, //reserved for kernal
    gp, //global pointer
    sp, //stack pointer
    fp, //frame pointer
    ra  //return address
};

namespace EmotionAssembler
{
    uint32_t jr(uint8_t addr);
    uint32_t jalr(uint8_t return_addr, uint8_t addr);
    uint32_t add(uint8_t dest, uint8_t reg1, uint8_t reg2);
    uint32_t and_ee(uint8_t dest, uint8_t reg1, uint8_t reg2);
    uint32_t addiu(uint8_t dest, uint8_t source, int16_t offset);
    uint32_t ori(uint8_t dest, uint8_t source, uint16_t offset);
    uint32_t lui(uint8_t dest, int32_t offset);
    uint32_t mfc0(uint8_t dest, uint8_t source);
    uint32_t eret();
    uint32_t lq(uint8_t dest, uint8_t base, int16_t offset);
    uint32_t sq(uint8_t source, uint8_t base, int16_t offset);
    uint32_t lw(uint8_t dest, uint8_t base, int16_t offset);
    uint32_t sw(uint8_t source, uint8_t base, int16_t offset);
};

#endif // EMOTIONASM_HPP
