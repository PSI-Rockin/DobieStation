#ifndef MACINPUT_H
#define MACINPUT_H

#include <iostream>
#include "common_input.hpp"



class MacInput : CommonInput
{
    private:
    std::string device_name;

    public:
    int getEvent(int i);

    bool reset();
    PAD_DATA poll();


};

#endif