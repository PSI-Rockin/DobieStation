#ifndef VU_JIT_HPP
#define VU_JIT_HPP
#include <cstdint>

class VectorUnit;

namespace VU_JIT
{

uint16_t run(VectorUnit* vu);
void reset(VectorUnit *vu);
void set_current_program(uint32_t crc, VectorUnit *vu);

};

#endif // VU_JIT_HPP
