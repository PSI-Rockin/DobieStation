#ifndef MOTIONCODE_HPP
#define MOTIONCODE_HPP
#include "vlc_table.hpp"

class MotionCode : public VLC_Table
{
    private:
        static VLC_Entry table[];
        static unsigned int index_table[];

        constexpr static int SIZE = 33;
    public:
        MotionCode();
};

#endif // MOTIONCODE_HPP
