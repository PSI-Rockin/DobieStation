#include "linux_input.hpp"

bool LinuxInput::reset()
{

    device_api = DEVICE_API::EVDEV;

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

                temp_dev = libevdev_new();
                rc = libevdev_set_fd(temp_dev, fd);

                if (rc < 0)
                {
                    // If it's just a bad file descriptor, don't bother logging, but otherwise, log it.
                    if (rc != -9)
                        printf("Failed to connect to device at %s, the error was: %s", current_path.c_str(), strerror(-rc));
                    libevdev_free(temp_dev);
                    continue;
                }

                rc = libevdev_new_from_fd(fd, &temp_dev);

                std::string device_name = libevdev_get_name(temp_dev);

                //std::cout << "Device: " << device_name << std::endl;
                if ( temp_dev!= nullptr)
                {
                    if (libevdev_has_event_type(temp_dev, EV_KEY) &&
                        libevdev_has_event_code(temp_dev, EV_ABS, ABS_X) &&
                        libevdev_has_event_code(temp_dev, EV_ABS, ABS_Y))
                    {

                        dev = new evdev_controller();
                        dev->dev = libevdev_new();


                        dev->controller_name = device_name;

                        dev->dev = temp_dev;

                        std::cout << "Controller Detected" << std::endl;
                        interesting_devices.push_back(dev);
                    }

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

std::vector<evdev_controller *> LinuxInput::get_interesting_devices()
{
    return interesting_devices;
}

int LinuxInput::getEvent(int i)
{
    int key;
    int rc = LIBEVDEV_READ_STATUS_SUCCESS;

    if (i < interesting_devices.size())
    {
        if (rc == LIBEVDEV_READ_STATUS_SYNC)
        {
            // Controller not synced or connected
            rc = libevdev_next_event(interesting_devices[i]->dev, LIBEVDEV_READ_FLAG_SYNC, &ev);
        }
   
        rc = libevdev_next_event(interesting_devices[i]->dev, LIBEVDEV_READ_FLAG_NORMAL, &ev);

        if (rc == LIBEVDEV_READ_STATUS_SUCCESS)
        {
            for (const auto& entry : button_list)
	        {
		        const auto code = entry.first;
 

                libevdev_fetch_event_value(interesting_devices[i]->dev, EV_KEY, code, &key);
                std::cout << "Key Code: " << key << std::endl;
                //interesting_devices[i]->code_name = libevdev_event_code_get_name(EV_KEY, key);
                //std::cout << "Code: " << interesting_devices[i]->code_name << std::endl;
            }
        }
    }   
        return key;
}

PAD_DATA LinuxInput::poll()
{
    int rc = LIBEVDEV_READ_STATUS_SUCCESS;

    if (rc == LIBEVDEV_READ_STATUS_SYNC)
    {
        rc = libevdev_next_event(interesting_devices[0]->dev, LIBEVDEV_READ_FLAG_SYNC, &ev);
    }
   
    rc = libevdev_next_event(interesting_devices[0]->dev, LIBEVDEV_READ_FLAG_NORMAL, &ev);
    
    //std::cout << "event: " << libevdev_event_type_get_name(ev.type) << " code: " << libevdev_event_code_get_name(ev.type, ev.code) << " value: " << ev.value << std::endl;

    PAD_DATA event = {};

    //uint16_t buttons = 0;

    int x1, y1, x2, y2;
    int min_x1, max_x1, min_y1, max_y1, min_x2, max_x2, min_y2, max_y2;

    //std::cout << "Device: " <<  libevdev_get_name(interesting_devices[0]->dev) << std::endl;

    min_x1 = libevdev_get_abs_minimum(interesting_devices[0]->dev, ABS_X);
    max_x1 = libevdev_get_abs_maximum(interesting_devices[0]->dev, ABS_X);

    min_y1 = libevdev_get_abs_minimum(interesting_devices[0]->dev, ABS_Y);
    max_y1 = libevdev_get_abs_maximum(interesting_devices[0]->dev, ABS_Y);

    min_x2 = libevdev_get_abs_minimum(interesting_devices[0]->dev, ABS_RX);
    max_x2 = libevdev_get_abs_maximum(interesting_devices[0]->dev, ABS_RX);

    min_y2 = libevdev_get_abs_minimum(interesting_devices[0]->dev, ABS_RY);
    max_y2 = libevdev_get_abs_maximum(interesting_devices[0]->dev, ABS_RY);

    libevdev_fetch_event_value(interesting_devices[0]->dev, EV_ABS, ABS_X, &x1);
    libevdev_fetch_event_value(interesting_devices[0]->dev, EV_ABS, ABS_Y, &y1);
    libevdev_fetch_event_value(interesting_devices[0]->dev, EV_ABS, ABS_RX, &x2);
    libevdev_fetch_event_value(interesting_devices[0]->dev, EV_ABS, ABS_RY, &y2);

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
        libevdev_fetch_event_value(interesting_devices[0]->dev, EV_KEY, button.first, &key);
        event.button[button.second].input = key;
        
        if (event.button[button.second].input)
        {
            event.button[button.second].pressure = 255;
        }

        else
        {
            event.button[button.second].pressure = 0;
        }
    }
    return event;
}
