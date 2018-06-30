#ifndef LUMTABLE_HPP
#define LUMTABLE_HPP
#include "vlc_table.hpp"

class LumTable : public VLC_Table
{
    private:
        static VLC_Entry table[];

        constexpr static int SIZE = 12;
    public:
        LumTable();
};

#endif // LUMTABLE_HPP
