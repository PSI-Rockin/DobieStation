#include <cstdio>
#include <cstdlib>
#include "gamepad.hpp"

Gamepad::Gamepad()
{

}

void Gamepad::reset()
{
    command = GAMEPAD_COMMAND::NONE;
    data_sent = 0;
    command_length = 0;
    config_count = 0;
}

uint8_t Gamepad::write_SIO(uint8_t value)
{
    switch (command)
    {
        case GAMEPAD_COMMAND::NONE:
            if (value == 1)
                command = GAMEPAD_COMMAND::WAITING;
            return 0xFF;
        case GAMEPAD_COMMAND::WAITING:
            get_command(value);
            return 0xF3;
        case GAMEPAD_COMMAND::READ_DATA:
            return read_data(value);
        case GAMEPAD_COMMAND::CONFIG:
            return config(value);
        case GAMEPAD_COMMAND::QUERY_MODEL:
            return query_model(value);
        case GAMEPAD_COMMAND::QUERY_ACT:
            return query_act(value);
        case GAMEPAD_COMMAND::QUERY_COMB:
            return query_comb(value);
        case GAMEPAD_COMMAND::QUERY_MODE:
            return query_mode(value);
        case GAMEPAD_COMMAND::ENABLE_VIBRATION:
            return enable_vibration(value);
    }
    return 0xFF;
}

void Gamepad::get_command(uint8_t value)
{
    switch (value)
    {
        case 0x42:
            printf("[PAD] Read data\n");
            command = GAMEPAD_COMMAND::READ_DATA;
            command_length = 3;
            break;
        case 0x43:
            printf("[PAD] Config\n");
            command = GAMEPAD_COMMAND::CONFIG;
            switch (config_count)
            {
                case 0:
                    command_length = 7;
                    break;
                case 1:
                    command_length = 2;
                    break;
                case 2:
                    command_length = 7;
                    break;
                case 3:
                    command_length = 19;
                    break;
            }
            break;
        case 0x45:
            printf("[PAD] Query model\n");
            command = GAMEPAD_COMMAND::QUERY_MODEL;
            command_length = 7;
            break;
        case 0x46:
            printf("[PAD] Query act\n");
            command = GAMEPAD_COMMAND::QUERY_ACT;
            command_length = 7;
            break;
        case 0x47:
            printf("[PAD] Query comb\n");
            command = GAMEPAD_COMMAND::QUERY_COMB;
            command_length = 7;
            break;
        case 0x4C:
            printf("[PAD] Query mode\n");
            command = GAMEPAD_COMMAND::QUERY_MODE;
            command_length = 7;
            break;
        case 0x4D:
            printf("[PAD] Vibration\n");
            command = GAMEPAD_COMMAND::ENABLE_VIBRATION;
            command_length = 7;
            break;
        default:
            printf("[PAD] Unrecognized command $%02X\n", value);
            exit(1);
    }
}

uint8_t Gamepad::read_data(uint8_t value)
{
    uint8_t ret = 0;
    switch (data_sent)
    {
        case 0:
            ret = 0x5A;
            break;
        case 1:
            ret = 0xFF;
            break;
        case 2:
            ret = 0xFF;
            break;
    }
    data_sent++;
    if (data_sent == 3)
    {
        command = GAMEPAD_COMMAND::NONE;
        data_sent = 0;
    }
    return ret;
}

uint8_t Gamepad::config(uint8_t value)
{
    uint8_t ret = 0;
    if (!data_sent)
        ret = 0x5A;
    else
        ret = 0;

    if (data_sent == 1 && value == 0)
        config_count = 0;
    data_sent++;
    if (data_sent == command_length)
    {
        config_count++;
        command = GAMEPAD_COMMAND::NONE;
        data_sent = 0;
    }
    return ret;
}

uint8_t Gamepad::query_model(uint8_t value)
{
    const static uint8_t data[] = {0x5A, 0x01, 0x02, 0x00, 0x02, 0x01, 0x00};
    uint8_t ret = data[data_sent];
    data_sent++;
    if (data_sent == command_length)
    {
        command = GAMEPAD_COMMAND::NONE;
        data_sent = 0;
    }
    return ret;
}

uint8_t Gamepad::query_act(uint8_t value)
{
    const static uint8_t data[] = {0x5A, 0x00, 0x00, 0x01, 0x02, 0x00, 0x0A};
    uint8_t ret = data[data_sent];
    data_sent++;
    if (data_sent == command_length)
    {
        command = GAMEPAD_COMMAND::NONE;
        data_sent = 0;
    }
    return ret;
}

uint8_t Gamepad::query_comb(uint8_t value)
{
    const static uint8_t data[] = {0x5A, 0x00, 0x00, 0x02, 0x00, 0x01, 0x00};
    uint8_t ret = data[data_sent];
    data_sent++;
    if (data_sent == command_length)
    {
        command = GAMEPAD_COMMAND::NONE;
        data_sent = 0;
    }
    return ret;
}

uint8_t Gamepad::query_mode(uint8_t value)
{
    const static uint8_t data[] = {0x5A, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00};
    uint8_t ret = data[data_sent];
    data_sent++;
    if (data_sent == command_length)
    {
        command = GAMEPAD_COMMAND::NONE;
        data_sent = 0;
    }
    return ret;
}

uint8_t Gamepad::enable_vibration(uint8_t value)
{
    const static uint8_t data[] = {0x5A, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    uint8_t ret = data[data_sent];
    data_sent++;
    if (data_sent == command_length)
    {
        command = GAMEPAD_COMMAND::NONE;
        data_sent = 0;
    }
    return ret;
}
