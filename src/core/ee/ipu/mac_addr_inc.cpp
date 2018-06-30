#include "mac_addr_inc.hpp"

VLC_Entry MacroblockAddrInc::table[] =
{
    {0x1, 0x1, 1},
    {0x3, 0x2, 3},
    {0x2, 0x3, 3}
};

MacroblockAddrInc::MacroblockAddrInc() : VLC_Table(table, SIZE, 11)
{

}
