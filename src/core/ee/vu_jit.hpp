#ifndef VU_JIT_HPP
#define VU_JIT_HPP
#include <cstdint>

class VectorUnit;

namespace VU_JIT
{

uint16_t run(VectorUnit* vu);
void reset();

};

#endif // VU_JIT_HPP
