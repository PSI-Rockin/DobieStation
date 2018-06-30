#ifndef MAC_P_PIC_HPP
#define MAC_P_PIC_HPP
#include "vlc_table.hpp"

class Macroblock_PPic : public VLC_Table
{
    private:
        static VLC_Entry table[];

        constexpr static int SIZE = 7;
    public:
        Macroblock_PPic();
};

#endif // MAC_P_PIC_HPP
