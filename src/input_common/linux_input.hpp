#ifndef LINUXINPUT_H
#define LINUXINPUT_H
#include <fcntl.h>
#include <vector>
#include <iostream>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include "common_input.hpp"
#include <libevdev-1.0/libevdev/libevdev.h>

enum evdev_controls
{
    AXIS_X1 = 0,
    AXIS_X2 = 2,
    AXIS_Y1 = 1,
    AXIS_Y2 = 3,
    A = 4,
    B = 5,
    C = 6,
    D = 7
}; // A = X, B = Circle, C = Square, D = Triangle

class LinuxInput : public CommonInput
{

private:
    struct input_event ev;
    struct libevdev *dev;

    std::vector<struct libevdev *> interestingDevices;

    std::string events[27] = {
        "event0", "event1", "event2", "event3", "event4", "event5",
        "event6", "event7", "event8", "event9", "event10", "event11",
        "event12", "event13", "event14", "event15", "event16", "event17", "event18", "event19"
                                                                                     "event20",
        "event21", "event22", "event23", "event24", "event25", "event26", "event27"};

    std::string filePath = "/dev/input/event27";

    std::string currentPath;

    int fd;
    int rc;

public:
    bool reset();
    void poll();
};

#endif