#include <cstdio>
#include "gamepad.hpp"
#include "memcard.hpp"
#include "sio2.hpp"

#include "../emulator.hpp"
#include "../errors.hpp"

using namespace std;

SIO2::SIO2(Emulator* e, Gamepad* pad, Memcard* memcard) : e(e), pad(pad), memcard(memcard)
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
    port = 0;
}

uint8_t SIO2::read_serial()
{
    //printf("[SIO2] Read FIFO\n");
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
    //printf("[SIO2] SEND1: $%08X (%d)\n", value, index);
    send1[index] = value;
    if (index < 0 || index >= 4)
        Errors::die("SIO2 set_send1 index (%d) out of range (0-4)", index);
}

void SIO2::set_send2(int index, uint32_t value)
{
    //printf("[SIO2] SEND2: $%08X (%d)\n", value, index);
    send2[index] = value;
    if (index < 0 || index >= 4)
        Errors::die("SIO2 set_send2 index (%d) out of range (0-4)", index);
}

void SIO2::set_send3(int index, uint32_t value)
{
    //printf("[SIO2] SEND3: $%08X (%d)\n", value, index);
    send3[index] = value;
    if (index < 0 || index >= 16)
        Errors::die("SIO2 set_send3 index (%d) out of range (0-4)", index);
}

void SIO2::write_serial(uint8_t value)
{
    //printf("[SIO2] DATAIN: $%02X\n", value);

    if (!command_length)
    {
        if (send3[send3_port] != 0)
        {
            printf("[SIO2] Get new send3 port: $%08X\n", send3[send3_port]);
            command_length = (send3[send3_port] >> 8) & 0x1FF;
            printf("[SIO2] Command len: %d\n", command_length);

            port = send3[send3_port] & 0x1;
            printf("[SIO2] Port: %d\n", port);
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
				case 0x81:
					active_command = SIO_DEVICE::MEMCARD;
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
            if (port)
            {
                FIFO.push(0x00);
                return;
            }
            uint8_t reply;
            if (new_command)
            {
                new_command = false;
                reply = pad->start_transfer(value);
            }
            else
                reply = pad->write_SIO(value);
            //printf("[SIO2] PAD reply: $%02X\n", reply);
            FIFO.push(reply);
        }
            break;
		case SIO_DEVICE::MEMCARD:
			if (memcard->mode != Memcard::MODE::INIT)
			{
				memcard->op_start(FIFO, value);
			}
			else
			{
				switch (FIFO.size())
				{
				case 0:
					FIFO.push(0xff); // A dummy value that means nothing, just fills the space

					// Should we be resetting the status register? Hell do we even
					// HAVE the status register???
					break;
				case 1:
					switch (value)
					{
					case 0x21: // Page Erase
					case 0x22: // Page Write
					case 0x23: // Page Read
						RECV3 = 0x8c;
						memcard->mode = Memcard::MODE::PAGE;
						break;
					case 0x26: // Memcard info
						RECV3 = 0x83;
						break;
					case 0x27: // Set terminator
						RECV3 = 0x8b;
						break;
					case 0x28: // Get terminator
						RECV3 = 0x8b;
						break;
					case 0xf0: // Authentication?
						memcard->mode = Memcard::MODE::AUTHENTICATION;
						break;
					// PS1 modes left unimplemented for now
					case 0x52: // PS1 Read
					case 0x53: // PS1 State
					case 0x57: // PS1 Write
					case 0x58: // PS1 Pocketstation (State?)
						memcard->mode = Memcard::MODE::PS1;
						break;

					// Welcome to the mystery corner, where the documentation is
					//shit and even PCSX2 isn't too sure what it's code is doing!
					case 0x11: // Seems to happen on boots/memcard probing?
					case 0x12: // Seems to happen on writes/erases?
						RECV3 = 0x8c;
						// Do not break; waterfall down into the payload for the other mystery commands

					case 0x81: // Seems to happen after copies and deletes (via PS2 browser, maybe???)
					case 0xbf: // PCSX2 says "wtf?", possibly used on game boots.
					case 0xf3: // Seems to happen on resets?
					case 0xf7: // No one knows.
						memcard->push_terminators(FIFO);
					}
				case 2:
					switch (memcard->command)
					{
					case 0x27:
						memcard->terminator = value;
						memcard->push_terminators(FIFO);
						break;
					}
					break;
				}
			}
			
			break;
        case SIO_DEVICE::DUMMY:
            FIFO.push(0x00);
            RECV1 = 0x1D100;
            break;
        default:
            Errors::die("[SIO2] Unrecognized active command!\n");
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
    //printf("[SIO2] Read RECV1\n");
    return RECV1;
}

uint32_t SIO2::get_RECV2()
{
    //printf("[SIO2] Read RECV2\n");
    return 0xF;
}

uint32_t SIO2::get_RECV3()
{
    //printf("[SIO2] Read RECV3\n");
    return RECV3;
}


