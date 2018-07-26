#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "gamepad.hpp"
#include "../errors.hpp"

//Reply buffers taken from PCSX2's LilyPad

const uint8_t Gamepad::config_exit[7] = {0x5A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
const uint8_t Gamepad::set_mode[7] = {0x5A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
const uint8_t Gamepad::query_model[7] = {0x5A, 0x01, 0x02, 0x00, 0x02, 0x01, 0x00};
const uint8_t Gamepad::query_act[2][7] =
{
    {0x5A, 0x00, 0x00, 0x01, 0x02, 0x00, 0x0A},
    {0x5A, 0x00, 0x00, 0x01, 0x01, 0x01, 0x14}
};
const uint8_t Gamepad::query_comb[7] = {0x5A, 0x00, 0x00, 0x02, 0x00, 0x01, 0x00};
const uint8_t Gamepad::query_mode[7] = {0x5A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

Gamepad::Gamepad()
{

}

void Gamepad::reset()
{
    //The first reply is always high-z
    command_buffer[0] = 0xFF;
    config_mode = false;
    buttons = 0xFFFF;
    command_length = 0;
    analog_mode = false;
    halfwords_transfer = 1;
    command = 0;
    data_count = 0;
    for (int i = 0; i < 7; i++)
        rumble_values[i] = 0xFF;
}

void Gamepad::press_button(PAD_BUTTON button)
{
    buttons &= ~(1 << (int)button);
}

void Gamepad::release_button(PAD_BUTTON button)
{
    buttons |= 1 << (int)button;
}

void Gamepad::set_result(const uint8_t *result)
{
    int len = sizeof(result);
    memcpy(command_buffer + 2, result, len);
    command_length = len + 2;
}

uint8_t Gamepad::start_transfer(uint8_t value)
{
    data_count = 0;
    command_length = 5;
    return 0xFF;
}

uint8_t Gamepad::write_SIO(uint8_t value)
{
    if (data_count > command_length)
    {
        printf("[PAD] Transfer (%d) exceeds command length (%d)!\n", data_count + 1, command_length);
        return 0;
    }
    if (data_count == 0)
    {
        if (!config_mode && (value != 'B' && value != 'C'))
        {
            printf("[PAD] Command %c ($%02X) called while not in config mode!\n", value, value);
            command_length = 0;
            data_count = 1;
            return 0xF3;
        }

        printf("[PAD] New command!\n");

        command = value;
        data_count++;

        switch (value)
        {
            case 'C': //0x43 - enter/exit config
                if (config_mode)
                {
                    set_result(config_exit);
                    return 0xF3;
                }
                //Config has the same response as 0x42 when not in config mode, so fallthrough is intentional here
            case 'B': //0x42 - read data
                command_buffer[2] = 0x5A;
                command_buffer[3] = buttons & 0xFF;
                command_buffer[4] = buttons >> 8;
                if (analog_mode)
                {
                    command_length = 9;
                    command_buffer[5] = 0x80;
                    command_buffer[6] = 0x80;
                    command_buffer[7] = 0x80;
                    command_buffer[8] = 0x80;
                    return 0x73;
                }
                else
                {
                    command_length = 5;
                    return 0x41;
                }

                break;
            case 'D': //0x44 - set mode and lock
                set_result(set_mode);
                return 0xF3;
            case 'E': //0x45 - query model and mode
                set_result(query_model);
                command_buffer[5] = analog_mode;
                return 0xF3;
            case 'F': //0x46 - query act
                set_result(query_act[0]);
                return 0xF3;
            case 'G': //0x47 - query comb
                set_result(query_comb);
                return 0xF3;
            case 'L': //0x4C - query mode
                set_result(query_mode);
                return 0xF3;
            case 'M': //0x4D - vibration toggle
                command_length = 9;
                memcpy(command_buffer + 2, rumble_values, 7);
                return 0xF3;
            default:
                Errors::die("[PAD] Unrecognized command %c ($%02X)\n", value, value);
        }
    }

    data_count++;

    switch (command)
    {
        case 'C':
            if (data_count == 3)
            {
                config_mode = value;
                printf("[PAD] Config mode: %d\n", config_mode);
            }
            break;
        case 'D':
            if (data_count == 3)
            {
                if (value < 2)
                {
                    analog_mode = value;
                }
            }
            break;
        case 'F':
            if (data_count == 3)
            {
                if (value < 2)
                    set_result(query_act[1]);
            }
            break;
        case 'L':
            if (data_count == 3)
            {
                if (value < 2)
                    command_buffer[6] = 4 + (value * 3);
            }
            break;
        case 'M':
            if (data_count >= 3)
                rumble_values[data_count - 2] = value;
            break;
    }

    return command_buffer[data_count];
}
