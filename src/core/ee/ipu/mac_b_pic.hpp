#ifndef MAC_B_PIC_HPP
#define MAC_B_PIC_HPP
#include "vlc_table.hpp"

class Macroblock_BPic : public VLC_Table
{
    private:
        static VLC_Entry table[];

        constexpr static int SIZE = 11;
    public:
        Macroblock_BPic();
};

#endif // MAC_B_PIC_HPP
