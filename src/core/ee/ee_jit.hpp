#ifndef EE_JIT_HPP
#define EE_JIT_HPP
#include <cstdint>

class EmotionEngine;

namespace EE_JIT
{
    uint16_t run(EmotionEngine* ee);
    void reset();
};

#endif // EE_JIT_HPP
