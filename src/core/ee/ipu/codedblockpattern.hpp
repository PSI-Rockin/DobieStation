#ifndef CODEDBLOCKPATTERN_HPP
#define CODEDBLOCKPATTERN_HPP
#include "vlc_table.hpp"

class CodedBlockPattern : public VLC_Table
{
    private:
        static VLC_Entry table[];
        static unsigned int index_table[];

        constexpr static int SIZE = 64;
    public:
        CodedBlockPattern();
};

#endif // CODEDBLOCKPATTERN_HPP
