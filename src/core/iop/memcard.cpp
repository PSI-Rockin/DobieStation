#include <fstream>
#include "../errors.hpp"
#include "memcard.hpp"

Memcard::Memcard()
{
    mem = nullptr;
    file_opened = false;
}

Memcard::~Memcard()
{
    delete[] mem;
}

void Memcard::reset()
{
    cmd_length = 0;
    response_read_pos = 0;
    response_write_pos = 0;
    response_size = 0;

    //Initial value is needed for newer MCMANs to work
    terminator = 0x55;
}

bool Memcard::open(std::string file_name)
{
    std::ifstream file(file_name);

    if (mem)
    {
        delete[] mem;
        mem = nullptr;
    }

    if (file.is_open())
    {
        file_opened = true;

        mem = new uint8_t[0x840000];
        file.read((char*)mem, 0x840000);
        file.close();
    }
    else
        file_opened = false;

    return file_opened;
}

bool Memcard::is_connected()
{
    return file_opened;
}

void Memcard::start_transfer()
{
    reset();

    memset(response_buffer, 0, sizeof(response_buffer));
}

uint8_t Memcard::write_serial(uint8_t data)
{
    if (cmd_length == 0)
    {
        cmd = data;
        switch (data)
        {
            case 0x11:
                //First command sent when trying to detect the card. Probe command?
                //*RECV3 = 0x8C;
                cmd_length = 2;
                response_end();
                break;
            case 0xF3:
                //Used for authentication
                cmd_length = 3;
                response_end();
                break;
            default:
                Errors::die("[Memcard] Unrecognized command $%02X", data);
        }
        return 0xFF;
    }
    else
    {
        switch (cmd)
        {
            default:
                break;
        }
        return read_response();
    }
}

uint8_t Memcard::read_response()
{
    if (response_read_pos >= sizeof(response_buffer))
        Errors::die("[Memcard] Reading more response data than is available!");
    uint8_t value = response_buffer[response_read_pos];
    printf("[Memcard] Response: $%02X\n", value);
    response_read_pos++;
    return value;
}

void Memcard::write_response(uint8_t value)
{
    response_buffer[response_write_pos] = value;
    response_write_pos++;
    response_size++;
    if (response_write_pos >= sizeof(response_buffer))
        Errors::die("[Memcard] Response size exceeds buffer length!");
}

void Memcard::response_end()
{
    response_buffer[cmd_length - 2] = 0x2B;
    response_buffer[cmd_length - 1] = terminator;
}
