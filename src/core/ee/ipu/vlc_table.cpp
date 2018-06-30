#include <cstdlib>
#include <cstdio>
#include "vlc_table.hpp"

VLC_Table::VLC_Table(VLC_Entry* table, int table_size, int max_bits) :
    table(table), table_size(table_size), max_bits(max_bits)
{

}

bool VLC_Table::peek_symbol(IPU_FIFO &FIFO, VLC_Entry &entry)
{
    for (int i = 0; i < max_bits; i++)
    {
        uint32_t key;
        int bits = i + 1;
        if (!FIFO.get_bits(key, bits))
            return false;
        for (int j = 0; j < table_size; j++)
        {
            if (bits != table[j].bits)
                continue;

            if (key == table[j].key)
            {
                entry = table[j];
                return true;
            }
        }
    }
    printf("[VLC Table] Symbol not found\n");
    exit(1);
    return false;
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
