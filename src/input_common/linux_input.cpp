#include "linux_input.hpp"

bool LinuxInput::reset()
{

    //currentPath = filePath + events[i];
    fd = open(filePath.c_str(), O_RDONLY | O_NONBLOCK);

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

inputEvent LinuxInput::poll()
{
    int rc = LIBEVDEV_READ_STATUS_SUCCESS;

    while (rc >= 0)
    {
        if (rc == LIBEVDEV_READ_STATUS_SYNC)
        {
            rc = libevdev_next_event(dev, LIBEVDEV_READ_FLAG_SYNC, &ev);
        }
        else
        {
            rc = libevdev_next_event(dev, LIBEVDEV_READ_FLAG_NORMAL, &ev);
        }
        //std::cout << "event: " << libevdev_event_type_get_name(ev.type) << " code: " << libevdev_event_code_get_name(ev.type, ev.code) << " value: " << ev.value << std::endl;
    }

    inputEvent event = {};

    //uint16_t buttons = 0;

    int x1, y1, x2, y2;

    libevdev_fetch_event_value(dev, EV_ABS, ABS_X, &x1);
    libevdev_fetch_event_value(dev, EV_ABS, ABS_Y, &y1);
    libevdev_fetch_event_value(dev, EV_ABS, ABS_THROTTLE, &x2);
    libevdev_fetch_event_value(dev, EV_ABS, ABS_RZ, &y2);

    float normLX = fmaxf(-1, x1 / 32767);
    float normLY = fmaxf(-1, y1 / 32767);

    std::cout << "normLX: " << normLX << std::endl;
    std::cout << "normLY: " << normLY << std::endl;

    event.lStickXAxis = normLX;
    event.lStickYAxis = normLY;
    event.rStickXAxis = x2;
    event.rStickYAxis = y2;

    for (auto button : buttonMap)
    {
        int key;
        libevdev_fetch_event_value(dev, EV_KEY, button.first, &key);

        event.input[button.second] = key;
    }

    return event;
}
