#ifndef __SPU_UTILS_H_
#define __SPU_UTILS_H_
#include <cstdint>
#include <algorithm>

inline int16_t clamp16(int input)
{
    return static_cast<int16_t>(std::max(std::min(input, 32767), -32768));
}

inline int16_t mulvol(int one, int two)
{
    return static_cast<int16_t>((one * two) >> 15);
}



#endif // __SPU_UTILS_H_
