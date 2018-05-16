#include <cstdio>
#include <cstdlib>
#include "vif.hpp"

#include "../gif.hpp"

VectorInterface::VectorInterface(GraphicsInterface* gif) : gif(gif)
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
                case 0x06:
                    printf("[VIF] MSKPATH3: %d\n", (value >> 15) & 0x1);
                    command_len = 1;
                    break;
                case 0x13:
                    printf("[VIF] FLUSHA\n");
                    command_len = 1;
                    break;
                case 0x50:
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
                case 0x50:
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

void VectorInterface::feed_DMA(uint128_t quad)
{
    printf("[VIF] Feed DMA: $%08X_%08X_%08X_%08X\n", quad._u32[3], quad._u32[2], quad._u32[1], quad._u32[0]);
    for (int i = 0; i < 4; i++)
        FIFO.push(quad._u32[i]);
}
