#include "linux_input.hpp"

bool LinuxInput::reset()
{
    //currentPath = filePath + events[i];
    if ((dir = opendir(file_path.c_str())) != nullptr)
    {

        while ((files = readdir(dir)))
        {
            std::string temp = files->d_name;

            if (temp.find("event") != std::string::npos)
            {
                current_path = file_path + files->d_name;
                fd = open(current_path.c_str(), O_RDONLY | O_NONBLOCK);

                std::cout << "event: " << temp << std::endl;

                if (fd < 0)
                {
                    fprintf(stderr, "error: %d %s\n", errno, strerror(errno));
                    continue;
                }

                rc = libevdev_new_from_fd(fd, &temp_dev);

                if (rc < 0)
                {
                    // If it's just a bad file descriptor, don't bother logging, but otherwise, log it.
                    if (rc != -9)
                        printf("Failed to connect to device at %s, the error was: %s", current_path.c_str(), strerror(-rc));
                    libevdev_free(temp_dev);
                    continue;
                }

                std::string device_name = libevdev_get_name(temp_dev);

                std::cout << "Device: " << device_name << std::endl;

                if (libevdev_has_event_type(temp_dev, EV_KEY) &&
                    libevdev_has_event_code(temp_dev, EV_ABS, ABS_X) &&
                    libevdev_has_event_code(temp_dev, EV_ABS, ABS_Y))
                {

                    dev = new evdevController();
                    dev->controllerName = device_name;
                    dev->dev = temp_dev;

                    std::cout << "Controller Detected" << std::endl;
                    interestingDevices.push_back(dev);
                    continue;
                }

                else
                {
                    std::cout << "No Controller!" << std::endl;
                    libevdev_free(temp_dev);
                    continue;
                }
            }
        }
    }
    current_path = " ";
    closedir(dir);
    return true;
}

PAD_DATA LinuxInput::poll()
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

    PAD_DATA event = {};

    //uint16_t buttons = 0;

    int x1, y1, x2, y2;
    int min_x1, max_x1, min_y1, max_y1, min_x2, max_x2, min_y2, max_y2;

    //std::cout << "Device: " << interestingDevices[0]->controllerName << std::endl;

    min_x1 = libevdev_get_abs_minimum(interestingDevices[0]->dev, ABS_X);
    max_x1 = libevdev_get_abs_maximum(interestingDevices[0]->dev, ABS_X);

    min_y1 = libevdev_get_abs_minimum(interestingDevices[0]->dev, ABS_Y);
    max_y1 = libevdev_get_abs_maximum(interestingDevices[0]->dev, ABS_Y);

    min_x2 = libevdev_get_abs_minimum(interestingDevices[0]->dev, ABS_THROTTLE);
    max_x2 = libevdev_get_abs_maximum(interestingDevices[0]->dev, ABS_THROTTLE);

    min_y2 = libevdev_get_abs_minimum(interestingDevices[0]->dev, ABS_RZ);
    max_y2 = libevdev_get_abs_maximum(interestingDevices[0]->dev, ABS_RZ);

    libevdev_fetch_event_value(interestingDevices[0]->dev, EV_ABS, ABS_X, &x1);
    libevdev_fetch_event_value(interestingDevices[0]->dev, EV_ABS, ABS_Y, &y1);
    libevdev_fetch_event_value(interestingDevices[0]->dev, EV_ABS, ABS_THROTTLE, &x2);
    libevdev_fetch_event_value(interestingDevices[0]->dev, EV_ABS, ABS_RZ, &y2);

    float LX = ((float)(x1 - min_x1) / (max_x1 - min_x1)) * 2;
    float LY = ((float)(y1 - min_y1) / (max_y1 - min_y1)) * 2;
    float RX = ((float)(x2 - min_x2) / (max_x2 - min_x2)) * 2;
    float RY = ((float)(y2 - min_y2) / (max_y2 - min_y2)) * 2;

    float norm_x1 = (LX - 1.0);
    float norm_y1 = (LY - 1.0);
    float norm_x2 = (RX - 1.0);
    float norm_y2 = (RY - 1.0);

    //std::cout << "normLX: " << norm_x1 << std::endl;
    //std::cout << "normLY: " << norm_y1 << std::endl;

    event.lStickXAxis = norm_x1;
    event.lStickYAxis = norm_y1 * -1;
    event.rStickXAxis = norm_x2;
    event.rStickYAxis = norm_y2 * -1;

    for (auto button : button_map)
    {
        int key;
        libevdev_fetch_event_value(interestingDevices[0]->dev, EV_KEY, button.first, &key);
        event.input[button.second] = key;
    }
    return event;
}
