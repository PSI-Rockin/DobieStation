#include <cstdlib>
#include "../../logger.hpp"
#include "ipu_fifo.hpp"

bool IPU_FIFO::get_bits(uint32_t &data, int bits)
{
    int bits_available = (f.size() * 128) - bit_pointer;

    if (bits_available < bits)
        return false;

    std::queue<uint128_t> temp_fifo = f;
    uint8_t temp[32];
    *(uint128_t*)&temp[0] = temp_fifo.front();
    temp_fifo.pop();
    *(uint128_t*)&temp[16] = temp_fifo.front();

    uint8_t blorp[8];
    int index = (bit_pointer & ~0x1F) / 8;

    //MPEG is big-endian...
    for (int i = 0; i < 8; i++)
    {
        int fifo_index = index + i;
        blorp[7 - i] = temp[fifo_index];
    }
    int shift = 64 - (bit_pointer % 32) - bits;
    uint64_t mask = ~0x0ULL >> (64 - bits);
    data = (*(uint64_t*)&blorp[0] >> shift) & mask;

    return true;
}

void IPU_FIFO::advance_stream(uint8_t amount)
{
    if (amount > 32)
        amount = 32;

    bit_pointer += amount;
    //Logger::log(Logger::IPU, "Advance stream: %d + %d = %d\n", bit_pointer - amount, amount, bit_pointer);

    if (bit_pointer > (f.size() * 128))
    {
        Logger::log(Logger::IPU, "Bit pointer exceeds FIFO size!\n");
        exit(1);
    }
    while (bit_pointer >= 128)
    {
        bit_pointer -= 128;
        f.pop();
    }
}

void IPU_FIFO::reset()
{
    std::queue<uint128_t> empty;
    f.swap(empty);
    bit_pointer = 0;
}
