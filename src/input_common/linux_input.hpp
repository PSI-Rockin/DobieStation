#ifndef LINUXINPUT_H
#define LINUXINPUT_H
#include <fcntl.h>
#include <vector>
#include <iostream>
#include "common_input.hpp"
#include <libevdev/libevdev.h>

struct controller // testing the structure type of controller which Xinput can do as well in this each controller gets an interface a player number and an event
{
    inputEvent pressed;
    libevdev *controller; // the interface itself
};

enum evdev_controls{AXIS_X1 = 0, AXIS_X2 = 2, AXIS_Y1 = 1, AXIS_Y2 = 3, A = 4, B = 5, C = 6, D = 7}; // A = X, B = Circle, C = Square, D = Triangle

class LinuxInput : CommonInput
{
 
private:

controller *controllers; // Oh sweet pointers how good it is to have you all back! :*)

struct input_event ev;
struct libevdev *dev = NULL;

std::string filePath = "/dev/input";


int fd;
int rc;



public:

void initalize();
void poll(int playerNumber);
void sendInput ();
bool press (u_int16_t input);

};


#endif