#include <cstdlib>
#include <cstdio>
#include <cstring>
#include "gamepad.hpp"
#include "../errors.hpp"

//Reply buffers taken from PCSX2's LilyPad

uint8_t Gamepad::mask_mode[7] = {0x5A, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x5A};
const uint8_t Gamepad::vref_param[7] = {0x5A, 0x00, 0x00, 0x02, 0x00, 0x00, 0x5A};
const uint8_t Gamepad::config_exit[7] = {0x5A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
const uint8_t Gamepad::set_mode[7] = {0x5A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
const uint8_t Gamepad::query_model_DS2[7] = {0x5A, 0x03, 0x02, 0x00, 0x02, 0x01, 0x00};
const uint8_t Gamepad::query_model_DS1[7] = {0x5A, 0x01, 0x02, 0x00, 0x02, 0x01, 0x00};
const uint8_t Gamepad::query_act[2][7] =
{
    {0x5A, 0x00, 0x00, 0x01, 0x02, 0x00, 0x0A},
    {0x5A, 0x00, 0x00, 0x01, 0x01, 0x01, 0x14}
};
const uint8_t Gamepad::query_comb[7] = {0x5A, 0x00, 0x00, 0x02, 0x00, 0x01, 0x00};
const uint8_t Gamepad::query_mode[7] = {0x5A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
const uint8_t Gamepad::native_mode[7] = {0x5A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x5A};

Gamepad::Gamepad()
{

}

void Gamepad::reset()
{
    //The first reply is always high-z
    command_buffer[0] = 0xFF;
    config_mode = false;
    buttons = 0xFFFF;
    joysticks[0][0] = 0x80;
    joysticks[0][1] = 0x80;
    joysticks[1][0] = 0x80;
    joysticks[1][1] = 0x80;
    command_length = 0;
    pad_mode = DIGITAL;
    command = 0;
    data_count = 0;
    mask[0] = 0xFF;
    mask[1] = 0xFF;
    mode_lock = 0;
    reset_vibrate();
}

void Gamepad::press_button(PAD_BUTTON button)
{
    buttons &= ~(1 << (int)button);
    button_pressure[(int)button] = 0xFF;
}

void Gamepad::release_button(PAD_BUTTON button)
{
    buttons |= 1 << (int)button;
    button_pressure[(int)button] = 0;
}

void Gamepad::update_joystick(JOYSTICK joystick, JOYSTICK_AXIS axis, uint8_t val)
{
    std::cout << "Value: " << unsigned(val) << std::endl;
    joysticks[(int)joystick][(int)axis] = val;
}

void Gamepad::set_result(const uint8_t *result)
{
    int len = sizeof(result);
    memcpy(command_buffer + 2, result, len);
    command_length = len + 2;
}

void Gamepad::reset_vibrate()
{
    rumble_values[0] = 0x5A;
    for (int i = 1; i < 8; ++i)
        rumble_values[i] = 0xFF;
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
                if (pad_mode != DIGITAL)
                {
                    command_length = 9;
                    command_buffer[5] = joysticks[0][0]; // right, y axis
                    command_buffer[6] = joysticks[0][1]; // right, x axis
                    command_buffer[7] = joysticks[1][0]; // left, y axis
                    command_buffer[8] = joysticks[1][1]; // left, x axis
                    if (pad_mode != ANALOG && !config_mode)
                    {
                        command_buffer[9] = button_pressure[(int)PAD_BUTTON::RIGHT];
                        command_buffer[10] = button_pressure[(int)PAD_BUTTON::LEFT];
                        command_buffer[11] = button_pressure[(int)PAD_BUTTON::UP];
                        command_buffer[12] = button_pressure[(int)PAD_BUTTON::DOWN];

                        command_buffer[13] = button_pressure[(int)PAD_BUTTON::TRIANGLE];
                        command_buffer[14] = button_pressure[(int)PAD_BUTTON::CIRCLE];
                        command_buffer[15] = button_pressure[(int)PAD_BUTTON::CROSS];
                        command_buffer[16] = button_pressure[(int)PAD_BUTTON::SQUARE];

                        command_buffer[17] = button_pressure[(int)PAD_BUTTON::L1];
                        command_buffer[18] = button_pressure[(int)PAD_BUTTON::R1];
                        command_buffer[19] = button_pressure[(int)PAD_BUTTON::L2];
                        command_buffer[20] = button_pressure[(int)PAD_BUTTON::R2];
                        command_length = 21;
                    }
                }
                else
                {
                    command_length = 5;
                }
                return pad_mode;
            case '@': //0x40 - set VREF param
                set_result(vref_param);
                return 0xF3;
            case 'A': //0x41 - query masked mode
                if (pad_mode == DIGITAL)
                {
                    mask_mode[3] = 0;
                    mask_mode[2] = 0;
                    mask_mode[1] = 0;
                    mask_mode[6] = 0;
                }
                else
                {
                    mask_mode[1] = mask[0];
                    mask_mode[2] = mask[1];
                    mask_mode[3] = 0x03;
                    mask_mode[6] = 0x5A;
                }
                set_result(mask_mode);
                return 0xF3;
            case 'D': //0x44 - set mode and lock
                set_result(set_mode);
                reset_vibrate();
                return 0xF3;
            case 'E': //0x45 - query model and mode
                set_result(query_model_DS2);
                command_buffer[5] = (pad_mode & 0xF) != 0x1;
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
                reset_vibrate();
                return 0xF3;
            case 'O': //0x4F - set DS2 native mode
                set_result(native_mode);
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
                if (value < 2 && !mode_lock)
                {
                    if (value)
                        pad_mode = ANALOG;
                    else
                        pad_mode = DIGITAL;
                }
            }
            else if (data_count == 4)
            {
                if (value == 3)
                    mode_lock = 3;
                else
                    mode_lock = 0;
            }
            break;
        case 'F':
            if (data_count == 3)
            {
                if (value < 2)
                    set_result(query_act[value]);
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
        case 'O':
            if (data_count == 3 || data_count == 4)
                mask[data_count - 3] = value;
            else if (data_count == 5)
            {
                if (!(value & 0x1))
                    pad_mode = DIGITAL;
                else if (!(value & 0x2))
                    pad_mode = ANALOG;
                else
                    pad_mode = DS2_NATIVE;
            }
            break;
    }

    return command_buffer[data_count];
}
