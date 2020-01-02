#include <cstring>
#include <fstream>
#include "../errors.hpp"
#include "memcard.hpp"

/**
 * On a high level, detecting a PS2 memory card follows this procedure:
 * - First, the card is probed with the 0x11 command.
 * - Next, the card is authenticated with the help of SECRMAN (security manager).
 *   This entails using MagicGate commands of some sort using the MECHACON, the DVD drive controller.
 *   If this or the above step fails, MCMAN will move on and check if a PSX memory card is inserted.
 * - Finally, the superblock (first block on the card) is read to make sure the card is formatted correctly.
 *   If this fails, the card will be detected, but MCMAN (memory card manager) will return an "unformatted" error.
 *
 * Once the card has been detected successfully, handling read/write/erase commands is quite simple.
 */

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
    cmd_params = 0;
    response_read_pos = 0;
    response_write_pos = 0;
    response_size = 0;
    auth_f0_do_xor = false;

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
            case 0x12:
                //Sent on write/erase
                //*RECV3 = 0x8C;
                cmd_length = 2;
                response_end();
                break;
            case 0x21:
                //Erase sector
                cmd_length = 7;
                response_end();
                break;
            case 0x22:
                //Write sector
                cmd_length = 7;
                response_end();
                break;
            case 0x23:
                //Read sector
                cmd_length = 7;
                response_end();
                break;
            case 0x26:
                //Get specs - currently hardcoded for standard Sony 8 MB memory cards
            {
                cmd_length = 11;
                write_response(0x2B);

                //Sector size in bytes (512)
                write_response(0x00);
                write_response(0x02);

                //Erase block pages (16)
                write_response(0x10);
                write_response(0x00);

                //Total size in sectors (0x4000)
                write_response(0x00);
                write_response(0x40);
                write_response(0x00);
                write_response(0x00);

                //XOR checksum (0x52)
                write_response(0x52);

                write_response(terminator);
            }
                break;
            case 0x27:
                //Set terminator
                cmd_length = 3;
                response_end();
                break;
            case 0x28:
                //Get terminator
                write_response(0x2B);
                write_response(terminator);
                write_response(0x55);
                cmd_length = 3;
                break;
            case 0x42:
                //Write
                cmd_length = 132;
                write_response(0x2B);
                write_response(terminator);
                break;
            case 0x43:
                //Read
                cmd_length = 132;
                write_response(0x2B);
                write_response(terminator);
                break;
            case 0x81:
                //Read/write end
                cmd_length = 2;
                response_end();
                break;
            case 0x82:
                //Erase sector
                cmd_length = 2;
                response_end();

                for (unsigned int i = 0; i < 528 * 16; i++)
                    mem[mem_addr + i] = 0xFF;
                break;
            case 0xBF:
                //Used when detecting the card...
                cmd_length = 3;
                response_end();
                break;
            case 0xF0:
                //Some XOR authentication thing
                cmd_length = 3;
                break;
            case 0xF3:
            case 0xF7:
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
            case 0x21:
                sector_op(data);
                break;
            case 0x22:
                sector_op(data);
                break;
            case 0x23:
                sector_op(data);
                break;
            case 0x27:
                if (cmd_params == 0)
                {
                    terminator = data;
                    response_end();
                }
                break;
            case 0x42:
                write_mem(data);
                break;
            case 0x43:
                if (cmd_params == 0)
                {
                    read_mem(mem_addr, data);
                    mem_addr += data;
                }
                break;
            case 0xF0:
                do_auth_f0(data);
                break;
            default:
                break;
        }
        cmd_params++;
        return read_response();
    }
}

uint8_t Memcard::read_response()
{
    if (response_read_pos >= sizeof(response_buffer))
        Errors::die("[Memcard] Reading more response data than is available!");
    uint8_t value = response_buffer[response_read_pos];
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

void Memcard::do_auth_f0(uint8_t value)
{
    if (cmd_params == 0)
    {
        switch (value)
        {
            case 0x06:
            case 0x07:
            case 0x0B:
                cmd_length = 12;
                auth_f0_do_xor = false;
                response_end();
                break;
            case 0x01:
            case 0x02:
            case 0x04:
            case 0x0F:
            case 0x11:
            case 0x13:
                auth_f0_do_xor = true;
                auth_f0_checksum = 0;
                write_response(0x00);
                break;
            default:
                auth_f0_do_xor = false;
                response_end();
        }
    }
    else if (auth_f0_do_xor)
    {
        switch (cmd_params)
        {
            case 1:
                write_response(0x2B);
                break;
            case 10:
                write_response(auth_f0_checksum);
                write_response(terminator);
                break;
            default:
                auth_f0_checksum ^= value;
                write_response(0x00);
        }
    }
}

void Memcard::sector_op(uint8_t value)
{
    switch (cmd_params)
    {
        case 0:
            mem_addr = value;
            break;
        case 1:
            mem_addr |= value << 8;
            break;
        case 2:
            mem_addr |= value << 16;
            break;
        case 3:
            mem_addr |= value << 24;
            mem_addr *= (512 + 16);

            if (cmd == 0x21)
                printf("[Memcard] Erasing $%08X...\n", mem_addr);
            else if (cmd == 0x22)
                printf("[Memcard] Writing $%08X...\n", mem_addr);
            else if (cmd == 0x23)
                printf("[Memcard] Reading $%08X...\n", mem_addr);
            break;
    }
}

void Memcard::read_mem(uint32_t addr, uint8_t size)
{
    for (unsigned int i = 0; i < size; i++)
        write_response(mem[addr + i]);

    write_response(do_checksum(&mem[addr], size));
    write_response(terminator);
}

void Memcard::write_mem(uint8_t data)
{
    if (cmd_params == 0)
    {
        mem_write_size = data;
        response_buffer[data + 3] = terminator;
    }
    else if (cmd_params - 1 < mem_write_size)
    {
        mem[mem_addr] = data;
        mem_addr++;
    }
}

uint8_t Memcard::do_checksum(uint8_t *buff, unsigned int size)
{
    uint8_t checksum = 0;

    for (unsigned int i = 0; i < size; i++)
        checksum ^= buff[i];

    return checksum;
}
