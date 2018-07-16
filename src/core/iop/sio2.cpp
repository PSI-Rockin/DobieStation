#include "../logger.hpp"
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
    RECV1 = 0x1D100;
}

uint8_t SIO2::read_serial()
{
    Logger::log(Logger::SIO2, "Read FIFO\n");
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
    Logger::log(Logger::SIO2, "SEND1: $%08X (%d)\n", value, index);
    send1[index] = value;
    if (index < 0 || index >= 4)
        exit(1);
}

void SIO2::set_send2(int index, uint32_t value)
{
    Logger::log(Logger::SIO2, "SEND2: $%08X (%d)\n", value, index);
    send2[index] = value;
    if (index < 0 || index >= 4)
        exit(1);
}

void SIO2::set_send3(int index, uint32_t value)
{
    Logger::log(Logger::SIO2, "SEND3: $%08X (%d)\n", value, index);
    send3[index] = value;
    if (index < 0 || index >= 16)
        exit(1);
}

void SIO2::write_serial(uint8_t value)
{
    Logger::log(Logger::SIO2, "DATAIN: $%02X\n", value);

    if (!command_length)
    {
        if (send3[send3_port] != 0)
        {
            Logger::log(Logger::SIO2, "Get new send3 port: $%08X\n", send3[send3_port]);
            command_length = (send3[send3_port] >> 8) & 0x1FF;
            Logger::log(Logger::SIO2, "Command len: %d\n", command_length);
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
            RECV1 = 0x1100;
            uint8_t reply;
            if (new_command)
            {
                new_command = false;
                reply = pad->start_transfer(value);
            }
            else
                reply = pad->write_SIO(value);
            Logger::log(Logger::SIO2, "PAD reply: $%02X\n", reply);
            FIFO.push(reply);
        }
            break;
        case SIO_DEVICE::DUMMY:
            FIFO.push(0x00);
            RECV1 = 0x1D100;
            break;
        default:
            Logger::log(Logger::SIO2, "Unrecognized active command!\n");
            exit(1);
    }
}

void SIO2::set_control(uint32_t value)
{
    Logger::log(Logger::SIO2, "Set control: $%08X\n", value);

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
    Logger::log(Logger::SIO2, "Read RECV1\n");
    return RECV1;
}

uint32_t SIO2::get_RECV2()
{
    Logger::log(Logger::SIO2, "Read RECV2\n");
    return 0xF;
}

uint32_t SIO2::get_RECV3()
{
    Logger::log(Logger::SIO2, "Read RECV3\n");
    return 0;
}
