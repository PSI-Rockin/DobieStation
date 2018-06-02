#include <cstdio>
#include <cstdlib>
#include "vif.hpp"

#include "../gif.hpp"

VectorInterface::VectorInterface(GraphicsInterface* gif, VectorUnit* vu) : gif(gif), vu(vu)
{

}

void VectorInterface::reset()
{
    std::queue<uint32_t> empty;
    FIFO.swap(empty);
    command_len = 0;
    command = 0;
    buffer_size = 0;
}

void VectorInterface::update()
{
    while (FIFO.size())
    {
        uint32_t value = FIFO.front();
        if (command_len == 0)
        {
            command = value >> 24;
            uint16_t imm = value & 0xFFFF;
            switch (command)
            {
                case 0x00:
                    printf("[VIF] NOP\n");
                    command_len = 1;
                    break;
                case 0x01:
                    printf("[VIF] Set CYCLE\n");
                    command_len = 1;
                    break;
                case 0x02:
                    printf("[VIF] Set OFFSET\n");
                    command_len = 1;
                    break;
                case 0x03:
                    printf("[VIF] Set BASE\n");
                    command_len = 1;
                    break;
                case 0x04:
                    printf("[VIF] Set ITOP\n");
                    command_len = 1;
                    break;
                case 0x05:
                    printf("[VIF] Set MODE\n");
                    command_len = 1;
                    break;
                case 0x06:
                    printf("[VIF] MSKPATH3: %d\n", (value >> 15) & 0x1);
                    command_len = 1;
                    break;
                case 0x07:
                    printf("[VIF] Set MARK\n");
                    command_len = 1;
                    break;
                case 0x10:
                    printf("[VIF] FLUSHE\n");
                    command_len = 1;
                    break;
                case 0x11:
                    printf("[VIF] FLUSH\n");
                    command_len = 1;
                    break;
                case 0x13:
                    printf("[VIF] FLUSHA\n");
                    command_len = 1;
                    break;
                case 0x20:
                    printf("[VIF] Set MASK\n");
                    command_len = 2;
                    break;
                case 0x30:
                    printf("[VIF] Set ROW\n");
                    command_len = 5;
                    break;
                case 0x31:
                    printf("[VIF] Set COL\n");
                    command_len = 5;
                    break;
                case 0x4A:
                    printf("[VIF] MPG: $%08X\n", value);
                    {
                        command_len = 1;
                        int num = (value >> 16) & 0xFF;
                        if (!num)
                            command_len += 512;
                        else
                            command_len += num << 1;
                        printf("Command len: %d\n", command_len);
                        mpg.addr = imm * 8;
                    }
                    break;
                case 0x50:
                case 0x51:
                    command_len = 1;
                    if (!imm)
                        command_len += 65536;
                    else
                        command_len += (imm * 4);
                    printf("[VIF] DIRECT: %d\n", command_len);
                    break;
                default:
                    printf("[VIF] Unrecognized command $%02X\n", command);
                    exit(1);
            }
        }
        else
        {
            switch (command)
            {
                case 0x20:
                    //STMASK
                    break;
                case 0x30:
                    //STROW
                    break;
                case 0x31:
                    //STCOL
                    break;
                case 0x4A:
                    if (FIFO.size() < 2)
                        return;
                    vu->write_instr(mpg.addr, value);
                    FIFO.pop();
                    value = FIFO.front();
                    vu->write_instr(mpg.addr + 4, value);
                    command_len--;
                    mpg.addr += 8;
                    break;
                case 0x50:
                case 0x51:
                    buffer[buffer_size] = value;
                    buffer_size++;
                    if (buffer_size == 4)
                    {
                        gif->send_PATH2(buffer);
                        buffer_size = 0;
                    }
                    break;
                default:
                    printf("[VIF] Unhandled data for command $%02X\n", command);
                    exit(1);
            }
        }
        command_len--;
        FIFO.pop();
    }
}

bool VectorInterface::transfer_DMAtag(uint128_t tag)
{
    //This should return false if the transfer stalls due to the FIFO filling up
    printf("[VIF] Transfer tag: $%08X_%08X_%08X_%08X\n", tag._u32[3], tag._u32[2], tag._u32[1], tag._u32[0]);
    for (int i = 2; i < 4; i++)
        FIFO.push(tag._u32[i]);
    return true;
}

void VectorInterface::feed_DMA(uint128_t quad)
{
    printf("[VIF] Feed DMA: $%08X_%08X_%08X_%08X\n", quad._u32[3], quad._u32[2], quad._u32[1], quad._u32[0]);
    for (int i = 0; i < 4; i++)
        FIFO.push(quad._u32[i]);
}
