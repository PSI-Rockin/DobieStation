#ifndef LINUXINPUT_H
#define LINUXINPUT_H
#include <fcntl.h>
#include <vector>
#include <iostream>
#include "input_api.hpp"
#include <libevdev-1.0/libevdev/libevdev.h>

struct controller // testing the structure type of controller which Xinput can do as well in this each controller gets an interface a player number and an event
{
    struct input_event pressed;
    struct libevdev *controller; // the interface itself
};

enum evdev_controls{AXIS_X1 = 0, AXIS_X2 = 2, AXIS_Y1 = 1, AXIS_Y2 = 3, A = 4, B = 5, C = 6, D = 7}; // A = X, B = Circle, C = Square, D = Triangle

class LinuxInput : InputApi
{
 
private:

controller *controllers; // Oh sweet pointers how good it is to have you all back! :*)

struct input_event ev;
struct libevdev *dev = NULL;

std::string filePath = "/dev/input";


int fd;
int rc;

public:

LinuxInput();

bool initalizeAPI();
void update();

};


#endif