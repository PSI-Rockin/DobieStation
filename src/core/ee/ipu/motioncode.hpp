#ifndef MOTIONCODE_HPP
#define MOTIONCODE_HPP
#include "vlc_table.hpp"

class MotionCode : public VLC_Table
{
    private:
        static VLC_Entry table[];

        constexpr static int SIZE = 8;
    public:
        MotionCode();
};

#endif // MOTIONCODE_HPP
