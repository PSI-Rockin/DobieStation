#ifndef CODEDBLOCKPATTERN_HPP
#define CODEDBLOCKPATTERN_HPP
#include "vlc_table.hpp"

class CodedBlockPattern : public VLC_Table
{
    private:
        static VLC_Entry table[];

        constexpr static int SIZE = 64;
    public:
        CodedBlockPattern();
};

#endif // CODEDBLOCKPATTERN_HPP
