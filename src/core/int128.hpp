#ifndef INT128_HPP
#define INT128_HPP
#include <cstdint>

/**
 * These 128-bit integer definitions were taken from PCSX2 and modified for DobieStation.
 * All credits go to the PCSX2 team.
 * View the original code here: https://github.com/PCSX2/pcsx2/blob/171e7f016dc9e132f9faf40a22f0312d45d356a5/common/include/Pcsx2Types.h#L58
 */
union uint128_t
{
    struct
    {
        uint64_t lo;
        uint64_t hi;
    };

    uint64_t _u64[2];
    uint32_t _u32[4];
    uint16_t _u16[8];
    uint8_t _u8[16];

    // Explicit conversion from u64. Zero-extends the source through 128 bits.
    static uint128_t from_u64(uint64_t src)
    {
        uint128_t retval;
        retval.lo = src;
        retval.hi = 0;
        return retval;
    }

    // Explicit conversion from u32. Zero-extends the source through 128 bits.
    static uint128_t from_u32(uint32_t src)
    {
        uint128_t retval;
        retval._u32[0] = src;
        retval._u32[1] = 0;
        retval.hi = 0;
        return retval;
    }

    operator uint32_t() const { return _u32[0]; }
    operator uint16_t() const { return _u16[0]; }
    operator uint8_t() const { return _u8[0]; }

    bool operator==(const uint128_t &right) const
    {
        return (lo == right.lo) && (hi == right.hi);
    }

    bool operator!=(const uint128_t &right) const
    {
        return (lo != right.lo) || (hi != right.hi);
    }
};

struct int128_t
{
    int64_t lo;
    int64_t hi;

    // explicit conversion from s64, with sign extension.
    static int128_t from_s64(int64_t src)
    {
        int128_t retval = {src, (src < 0) ? -1 : 0};
        return retval;
    }

    // explicit conversion from s32, with sign extension.
    static int128_t from_s32(int32_t src)
    {
        int128_t retval = {src, (src < 0) ? -1 : 0};
        return retval;
    }

    operator uint32_t() const { return (int32_t)lo; }
    operator uint16_t() const { return (int16_t)lo; }
    operator uint8_t() const { return (int8_t)lo; }

    bool operator==(const int128_t &right) const
    {
        return (lo == right.lo) && (hi == right.hi);
    }

    bool operator!=(const int128_t &right) const
    {
        return (lo != right.lo) || (hi != right.hi);
    }
};

#endif // INT128_HPP
