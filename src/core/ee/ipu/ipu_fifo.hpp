#ifndef IPU_FIFO_HPP
#define IPU_FIFO_HPP
#include <cstdint>
#include <queue>

#include "../../int128.hpp"

struct IPU_FIFO
{
    std::deque<uint128_t> f;
    int bit_pointer;
    uint64_t cached_bits;
    bool bit_cache_dirty;
    bool get_bits(uint32_t& data, int bits);
    bool advance_stream(uint8_t amount);

    void reset();
    void byte_align();
};

#endif // IPU_FIFO_HPP
