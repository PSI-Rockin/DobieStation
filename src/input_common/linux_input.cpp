#include "linux_input.hpp"

bool LinuxInput::reset()
{
    //currentPath = filePath + events[i];
    if ((dir = opendir(filePath.c_str())) != nullptr)
    {

        while ((files = readdir(dir)))
        {
            std::string temp = files->d_name;

            if (temp.find("event") != std::string::npos)
            {
                currentPath = filePath + files->d_name;
                fd = open(currentPath.c_str(), O_RDONLY | O_NONBLOCK);

                std::cout << "event: " << temp << std::endl;

                if (fd < 0)
                {
                    fprintf(stderr, "error: %d %s\n", errno, strerror(errno));
                    continue;
                }

                rc = libevdev_new_from_fd(fd, &tempDev);

                if (rc < 0)
                {
                    // If it's just a bad file descriptor, don't bother logging, but otherwise, log it.
                    if (rc != -9)
                        printf("Failed to connect to device at %s, the error was: %s", currentPath.c_str(), strerror(-rc));
                    libevdev_free(tempDev);                    
                    continue;
                }

                std::string deviceName = libevdev_get_name(tempDev);

                std::cout << "Device: " << deviceName << std::endl;

                if (libevdev_has_event_type(tempDev, EV_KEY) &&
                    libevdev_has_event_code(tempDev, EV_ABS, ABS_X) &&
                    libevdev_has_event_code(tempDev, EV_ABS, ABS_Y))
                {

                    dev = new evdevController();
                    dev->controllerName = deviceName;
                    dev->dev = tempDev;

                    std::cout << "Controller Detected" << std::endl;
                    interestingDevices.push_back(dev);
                    continue;
                }

                else
                {
                    std::cout << "No Controller!" << std::endl;
                    libevdev_free(tempDev);
                    continue;
                }
            }
        }
    }
    currentPath = " ";
    closedir(dir);
return true;
}

inputEvent LinuxInput::poll()
{
    int rc = LIBEVDEV_READ_STATUS_SUCCESS;

    while (rc >= 0)
    {
        if (rc == LIBEVDEV_READ_STATUS_SYNC)
        {
            rc = libevdev_next_event(interestingDevices[0]->dev, LIBEVDEV_READ_FLAG_SYNC, &ev);
        }
        else
        {
            rc = libevdev_next_event(interestingDevices[0]->dev, LIBEVDEV_READ_FLAG_NORMAL, &ev);
        }
        //std::cout << "event: " << libevdev_event_type_get_name(ev.type) << " code: " << libevdev_event_code_get_name(ev.type, ev.code) << " value: " << ev.value << std::endl;
    }

    inputEvent event = {};

    //uint16_t buttons = 0;

    int x1, y1, x2, y2;
    int minX1, maxX1, minY1, maxY1, minX2, maxX2, minY2, maxY2;

    std::cout << "Device: " << interestingDevices[0]->controllerName << std::endl;

    minX1 = libevdev_get_abs_minimum(interestingDevices[0]->dev, ABS_X);
    maxX1 = libevdev_get_abs_maximum(interestingDevices[0]->dev, ABS_X);

    minY1 = libevdev_get_abs_minimum(interestingDevices[0]->dev, ABS_Y);
    maxY1 = libevdev_get_abs_maximum(interestingDevices[0]->dev, ABS_Y);

    minX2 = libevdev_get_abs_minimum(interestingDevices[0]->dev, ABS_THROTTLE);
    maxX2 = libevdev_get_abs_maximum(interestingDevices[0]->dev, ABS_THROTTLE);

    minY2 = libevdev_get_abs_minimum(interestingDevices[0]->dev, ABS_RZ);
    maxY2 = libevdev_get_abs_maximum(interestingDevices[0]->dev, ABS_RZ);

    libevdev_fetch_event_value(interestingDevices[0]->dev, EV_ABS, ABS_X, &x1);
    libevdev_fetch_event_value(interestingDevices[0]->dev, EV_ABS, ABS_Y, &y1);
    libevdev_fetch_event_value(interestingDevices[0]->dev, EV_ABS, ABS_THROTTLE, &x2);
    libevdev_fetch_event_value(interestingDevices[0]->dev, EV_ABS, ABS_RZ, &y2);

    float LX = ((float)(x1 - minX1) / (maxX1 - minX1)) * 2;
    float LY = ((float)(y1 - minY1) / (maxY1 - minY1)) * 2;
    float RX = ((float)(x2 - minX2) / (maxX2 - minX2)) * 2;
    float RY = ((float)(y2 - minY2) / (maxY2 - minY2)) * 2;

    float normX1 = (LX - 1.0);
    float normY1 = (LY - 1.0);
    float normX2 = (RX - 1.0);
    float normY2 = (RY - 1.0);

    std::cout << "normLX: " << normX1 << std::endl;
    //std::cout << "normLY: " << normY1 << std::endl;

    event.lStickXAxis = normX1;
    event.lStickYAxis = normY1 * -1;
    event.rStickXAxis = normX2;
    event.rStickYAxis = normY2 * -1;

    for (auto button : buttonMap)
    {
        int key;
        libevdev_fetch_event_value(interestingDevices[0]->dev, EV_KEY, button.first, &key);
        event.input[button.second] = key;
    }
    return event;
}
