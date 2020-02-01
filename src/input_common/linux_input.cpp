#include "linux_input.hpp"

bool LinuxInput::reset()
{

    for (int i = 0; i < 5; i++)
    {

        currentPath = filePath + events[i];
        fd = open(currentPath.c_str(), O_RDONLY | O_NONBLOCK);

        if (fd < 0)
        {
            fprintf(stderr, "error: %d %s\n", errno, strerror(errno));
        }

        rc = libevdev_new_from_fd(fd, &dev);
        if (rc < 0)
        {
            // If it's just a bad file descriptor, don't bother logging, but otherwise, log it.
            if (rc != -9)
                printf("Failed to connect to device at %s, the error was: %s", currentPath, strerror(-rc));
            libevdev_free(dev);
            continue;
        }

        std::string deviceName = libevdev_get_name(dev);

        if (libevdev_has_event_type(dev, EV_KEY) &&
            libevdev_has_event_code(dev, EV_ABS, ABS_X) &&
            libevdev_has_event_code(dev, EV_ABS, ABS_Y))
        {

            std::cout << "Controller Detected" << std::endl;
            interestingDevices.push_back(dev);
        }

        else
        {
            std::cout << "No Controller!" << std::endl;
        }
    }
}

void LinuxInput::poll()
{
    rc = libevdev_next_event(dev, LIBEVDEV_READ_FLAG_NORMAL, &ev);
    // Grab any pending sync event.
    if (rc == LIBEVDEV_READ_STATUS_SYNC)
    {
        std::cout << "SYNC" << std::endl;
        rc = libevdev_next_event(dev, LIBEVDEV_READ_FLAG_NORMAL | LIBEVDEV_READ_FLAG_SYNC, &ev);
    }

    /*if (libevdev_get_event_value(dev, EV_ABS, BTN_A) == 0)
        printf("Button is up\n");
    else
        printf("Button is down\n");*/
}
