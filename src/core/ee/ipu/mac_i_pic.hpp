#ifndef MAC_I_PIC_HPP
#define MAC_I_PIC_HPP
#include "vlc_table.hpp"

class Macroblock_IPic : public VLC_Table
{
    private:
        static VLC_Entry table[];

        constexpr static int SIZE = 2;
    public:
        Macroblock_IPic();
};

#endif // MAC_I_PIC_HPP
