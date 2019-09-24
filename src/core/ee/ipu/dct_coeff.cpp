#include <cstdio>
#include <cstdlib>
#include "dct_coeff.hpp"
#include "../../errors.hpp"

DCT_Coeff::DCT_Coeff(VLC_Entry* table, int table_size, int max_bits, unsigned int* index_table) :
    VLC_Table(table, table_size, max_bits, index_table)
{

}

bool DCT_Coeff::peek_value(IPU_FIFO &FIFO, int bits, int &bit_count, uint32_t &result)
{
    if (bits + bit_count > 32)
    {
        Errors::die("[DCT Coeff] Bit count > 32!\n");
    }
    if (!FIFO.get_bits(result, bits + bit_count))
        return false;

    result &= ~(0xFFFFFFFF << bits);
    bit_count += bits;
    return true;
}
