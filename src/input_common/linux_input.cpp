#include "linux_input.hpp"


void LinuxInput::initalize()
{

controllers = new controller[4];

for (int i = 0; i < 4; i++)
{
    std::string deviceName = libevdev_get_name(controllers[i].controller);

    fd = open(filePath.c_str(), O_RDWR | O_NONBLOCK);

	rc = libevdev_new_from_fd(fd, &dev);

    if (rc != -9)
    {
        std::cerr << "Error bad Descriptor file!" << std::endl;
    }

    if (libevdev_has_event_type(dev, EV_KEY) && libevdev_has_event_code(dev, EV_ABS, ABS_X) && libevdev_has_event_code(dev, EV_ABS, ABS_Y))
    {
        controllers[i].controller = dev; // ladies and gents we have a controller!
    }
}


}

void LinuxInput::poll(int playerNumber)
{
    rc = libevdev_next_event(controllers[playerNumber].controller, LIBEVDEV_READ_FLAG_NORMAL, &ev);
}


void LinuxInput::sendInput(u_int16_t input)
{

}
