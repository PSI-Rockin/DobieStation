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

struct stereo_sample
{
    int16_t left;
    int16_t right;

    void mix(stereo_sample two, bool l, bool r)
    {
        if (l)
            left = clamp16(left + two.left);
        if (r)
            right = clamp16(right + two.right);
    }
};



#endif // __SPU_UTILS_H_
