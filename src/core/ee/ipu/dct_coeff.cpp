#include <cstdio>
#include <cstdlib>
#include "dct_coeff.hpp"

DCT_Coeff::DCT_Coeff(VLC_Entry* table, int table_size, int max_bits) :
    VLC_Table(table, table_size, max_bits)
{

}

bool DCT_Coeff::peek_value(IPU_FIFO &FIFO, int bits, int &bit_count, uint32_t &result)
{
    if (bits + bit_count > 32)
    {
        printf("[DCT Coeff] Bit count > 32!\n");
        exit(1);
    }
    if (!FIFO.get_bits(result, bits + bit_count))
        return false;

    result &= ~(0xFFFFFFFF << bits);
    bit_count += bits;
    return true;
}
