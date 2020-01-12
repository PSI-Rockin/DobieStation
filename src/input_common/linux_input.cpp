#include "linux_input.hpp"

bool LinuxInput::reset() 
{

    //std::string deviceName = libevdev_get_name(dev);

    fd = open(filePath.c_str(), O_RDONLY|O_NONBLOCK);

    if (fd < 0)
    {
        fprintf(stderr, "error: %d %s\n", errno, strerror(errno));
    }

	rc = libevdev_new_from_fd(fd, &dev);

    if (rc < 0)    
    {
        fprintf(stderr, "error: %d %s\n", -rc, strerror(-rc));    
    }

    if (libevdev_has_event_type(dev, EV_KEY) && libevdev_has_event_code(dev, EV_ABS, ABS_X) && libevdev_has_event_code(dev, EV_ABS, ABS_Y))
    {
        std::cout << "Controller Detected" << std::endl;
    }

    else 
    {
        std::cout << "No Controller!" << std::endl;
    }

}

void LinuxInput::poll()
{

    rc = libevdev_next_event(dev, LIBEVDEV_READ_FLAG_NORMAL, &ev);


}
