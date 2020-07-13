#include <cstdlib>
#include <cstdio>
#include "vlc_table.hpp"
#include "../../errors.hpp"

VLC_Table::VLC_Table(VLC_Entry* table, int table_size, int max_bits, unsigned int* index_table) :
    table(table), table_size(table_size), max_bits(max_bits), index_table(index_table)
{

}

bool VLC_Table::peek_symbol(IPU_FIFO &FIFO, VLC_Entry &entry)
{
    uint32_t key;
    for (int i = 0; i < max_bits; i++)
    {
        int bits = i + 1;
        if (!FIFO.get_bits(key, bits))
            return false;
        for (int j = index_table[i]; j < table_size; j++)
        {
            if (bits != table[j].bits)
                break;

            if (key == table[j].key)
            {
                entry = table[j];
                return true;
            }
        }
    }
    throw VLC_Error("VLC symbol not found");
}

bool VLC_Table::get_symbol(IPU_FIFO& FIFO, uint32_t &result)
{
    VLC_Entry entry;
    if (!peek_symbol(FIFO, entry))
        return false;

    FIFO.advance_stream(entry.bits);
    result = entry.value;
    return true;
}
