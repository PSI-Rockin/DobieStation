#ifndef LINUXINPUT_H
#define LINUXINPUT_H

#include <iostream>
#include <libevdev.h>
#include <libudev.h>
#include <vector>
#include "common_input.hpp"

struct controller // testing the structure type of controller which Xinput can do as well in this each controller gets an interface a player number and an event.s
{
    int playerNumber;
    inputEvent pressed;
    udev *controller; // the interface itself
};


class LinuxInput : CommonInput
{
 
std::vector<controller*> controllers; // Oh sweet pointers how good it is to have you all back! :*)

void initalize();
void sendInput (u_int16_t input);
bool press (u_int16_t input);

};



#endif