#include <cstdio>
#include "gamepad.hpp"
#include "sio2.hpp"

#include "../emulator.hpp"

using namespace std;

SIO2::SIO2(Emulator* e, Gamepad* pad) : e(e), pad(pad)
{

}

void SIO2::reset()
{
    queue<uint8_t> empty;
    FIFO.swap(empty);
    active_command = SIO_DEVICE::NONE;
    control = 0;
    new_command = false;
}

uint8_t SIO2::read_serial()
{
    printf("[SIO2] Read FIFO\n");
    uint8_t value = FIFO.front();
    FIFO.pop();
    return value;
}

uint32_t SIO2::get_control()
{
    return control;
}

void SIO2::set_send1(int index, uint32_t value)
{
    printf("[SIO2] SEND1: $%08X (%d)\n", value, index);
    send1[index] = value;
}

void SIO2::set_send2(int index, uint32_t value)
{
    printf("[SIO2] SEND2: $%08X (%d)\n", value, index);
    send2[index] = value;
}

void SIO2::set_send3(int index, uint32_t value)
{
    printf("[SIO2] SEND3: $%08X (%d)\n", value, index);
    send3[index] = value;
}

void SIO2::write_serial(uint8_t value)
{
    printf("[SIO2] DATAIN: $%02X\n", value);

    if (!command_length)
    {
        if (send3[send3_port] != 0)
        {
            printf("[SIO2] Get new send3 port: $%08X\n", send3[send3_port]);
            command_length = (send3[send3_port] >> 8) & 0x1FF;
            printf("[SIO2] Command len: %d\n", command_length);
            send3_port++;
            new_command = true;
        }
    }

    if (command_length)
        command_length--;

    write_device(value);
}

void SIO2::write_device(uint8_t value)
{
    switch (active_command)
    {
        case SIO_DEVICE::NONE:
            switch (value)
            {
                case 0x01:
                    active_command = SIO_DEVICE::PAD;
                    break;
                default:
                    active_command = SIO_DEVICE::DUMMY;
                    break;
            }
            //Send command code to the device
            write_device(value);
            break;
        case SIO_DEVICE::PAD:
        {
            uint8_t reply;
            if (new_command)
            {
                new_command = false;
                reply = pad->start_transfer(value);
            }
            else
                reply = pad->write_SIO(value);
            printf("[SIO2] PAD reply: $%02X\n", reply);
            FIFO.push(reply);
        }
            break;
        case SIO_DEVICE::DUMMY:
            FIFO.push(0xFF);
            break;
        default:
            printf("[SIO2] Unrecognized active command!\n");
            exit(1);
    }
}

void SIO2::set_control(uint32_t value)
{
    printf("[SIO2] Set control: $%08X\n", value);

    control = value & 0x1;

    if (value & 0x1)
    {
        e->iop_request_IRQ(17);
        control &= ~0x1;
    }
    else
    {
        active_command = SIO_DEVICE::NONE;
        send3_port = 0;
        command_length = 0;
    }
}

uint32_t SIO2::get_RECV1()
{
    printf("[SIO2] Read RECV1\n");
    return 0x0D102;
}

uint32_t SIO2::get_RECV2()
{
    printf("[SIO2] Read RECV2\n");
    return 0xF;
}
