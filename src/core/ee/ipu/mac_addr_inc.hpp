#ifndef MAC_ADDR_INC_H
#define MAC_ADDR_INC_H
#include "vlc_table.hpp"

class MacroblockAddrInc : public VLC_Table
{
    private:
        static VLC_Entry table[];

        constexpr static int SIZE = 3;
    public:
        MacroblockAddrInc();
};

#endif // MAC_ADDR_INC_H
