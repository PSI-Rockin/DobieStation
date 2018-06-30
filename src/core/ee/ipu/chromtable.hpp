#ifndef CHROMTABLE_HPP
#define CHROMTABLE_HPP
#include "vlc_table.hpp"

class ChromTable : public VLC_Table
{
    private:
        static VLC_Entry table[];

        constexpr static int SIZE = 12;
    public:
        ChromTable();
};

#endif // CHROMTABLE_HPP
