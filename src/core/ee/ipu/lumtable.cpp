#include "lumtable.hpp"

VLC_Entry LumTable::table[] =
{
    {0x0000, 1, 2},
    {0x0001, 2, 2},
    {0x0004, 0, 3},
    {0x0005, 3, 3},
    {0x0006, 4, 3},
    {0x000E, 5, 4},
    {0x001E, 6, 5},
    {0x003E, 7, 6},
    {0x007E, 8, 7},
    {0x00FE, 9, 8},
    {0x01FE, 10, 9},
    {0x01FF, 11, 9}
};

LumTable::LumTable() : VLC_Table(table, SIZE, 9)
{

}
