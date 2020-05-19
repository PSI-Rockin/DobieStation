#ifndef MACINPUT_H
#define MACINPUT_H

#include <iostream>
#include "input_common/common_input.hpp"
#include <GCController>


class MacInput : CommonInput
{
    private:
    string device_name;

    public:
    int getEvent(int i);

    bool reset();
    PAD_DATA poll();


}

#endif