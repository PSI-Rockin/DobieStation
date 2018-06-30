#include "chromtable.hpp"

VLC_Entry ChromTable::table[] =
{
    {0x0, 0, 2},
    {0x1, 1, 2},
    {0x2, 2, 2},
    {0x6, 3, 3},
    {0xE, 4, 4},
    {0x1E, 5, 5},
    {0x3E, 6, 6},
    {0x7E, 7, 7},
    {0xFE, 8, 8},
    {0x1FE, 9, 9},
    {0x3FE, 10, 10},
    {0x3FF, 11, 10}
};

ChromTable::ChromTable() : VLC_Table(table, SIZE, 10)
{

}
