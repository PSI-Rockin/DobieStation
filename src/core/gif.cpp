#include <cstdio>
#include "gif.hpp"
#include "gs.hpp"

GraphicsInterface::GraphicsInterface(GraphicsSynthesizer *gs) : gs(gs)
{

}

void GraphicsInterface::reset()
{
    current_tag.data_left = 0;
    active_path = 0;
    path_queue = 0;
}

uint32_t GraphicsInterface::read_STAT()
{
    uint32_t reg = 0;

    //VIFMASK
    reg |= path3_vif_masked << 1;

    reg |= ((path_queue & (1 << 3)) != 0) << 6;
    reg |= ((path_queue & (1 << 2)) != 0) << 7;
    reg |= ((path_queue & (1 << 1)) != 0) << 8;

    //OPH
    reg |= (active_path != 0) << 9;

    //APATH
    reg |= active_path << 10;

    //Quadword count - since we don't emulate the FIFO, hack it to 16 if there's a transfer happening
    reg |= ((active_path != 0) * 16) << 24;
    printf("[GIF] Read GIF_STAT: $%08X\n", reg);
    return reg;
}

void GraphicsInterface::process_PACKED(uint128_t data)
{
    uint64_t data1 = data._u64[0];
    uint64_t data2 = data._u64[1];
    //printf("[GIF] PACKED: $%08X_%08X_%08X_%08X\n", data._u32[3], data._u32[2], data._u32[1], data._u32[0]);
    uint64_t reg_offset = (current_tag.reg_count - current_tag.regs_left) << 2;
    uint8_t reg = (current_tag.regs >> reg_offset) & 0xF;
    switch (reg)
    {
        case 0x0:
            //PRIM
            gs->write64(0, data1);
            break;
        case 0x1:
            //RGBAQ - set RGBA
            //Q is taken from the ST command
        {
            uint8_t r = data1 & 0xFF;
            uint8_t g = (data1 >> 32) & 0xFF;
            uint8_t b = data2 & 0xFF;
            uint8_t a = (data2 >> 32) & 0xFF;
            gs->set_RGBA(r, g, b, a);
        }
            break;
        case 0x2:
        {
            //ST - set ST coordinates and Q
            uint32_t s = data1 & 0xFFFFFFFF;
            uint32_t t = data1 >> 32;
            uint32_t q = data2 & 0xFFFFFFFF;
            gs->set_STQ(s, t, q);
        }
            break;
        case 0x3:
            //UV - set UV coordinates
        {
            uint16_t u = data1 & 0x3FFF;
            uint16_t v = (data1 >> 32) & 0x3FFF;
            gs->set_UV(u, v);
        }
            break;
        case 0x4:
            //XYZF2 - set XYZ and fog coefficient. Optionally disable drawing kick through bit 111
        {
            uint32_t x = data1 & 0xFFFF;
            uint32_t y = (data1 >> 32) & 0xFFFF;
            uint32_t z = (data2 >> 4) & 0xFFFFFF;
            bool disable_drawing = (data2 >> (111 - 64)) & 0x1;
            gs->set_XYZ(x, y, z, !disable_drawing);
        }
            break;
        case 0x5:
        //XYZ2 - set XYZ. Optionally disable drawing kick through bit 111
        {
            uint32_t x = data1 & 0xFFFF;
            uint32_t y = (data1 >> 32) & 0xFFFF;
            uint32_t z = data2 & 0xFFFFFFFF;
            bool disable_drawing = (data2 >> (111 - 64)) & 0x1;
            gs->set_XYZ(x, y, z, !disable_drawing);
        }
            break;
        case 0xE:
        {
            //A+D: output data to address
            uint32_t addr = data2 & 0xFF;
            gs->write64(addr, data1);
        }
            break;
        case 0xF:
            //NOP
            break;
        default:
            gs->write64(reg, data1);
            break;
    }
}

void GraphicsInterface::process_REGLIST(uint128_t data)
{
    //printf("[GIF] Reglist: $%08X_%08X_%08X_%08X\n", data._u32[3], data._u32[2], data._u32[1], data._u32[0]);
    for (int i = 0; i < 2; i++)
    {
        uint64_t reg_offset = (current_tag.reg_count - current_tag.regs_left) << 2;
        uint8_t reg = (current_tag.regs >> reg_offset) & 0xF;
        gs->write64(reg, data._u64[i]);

        current_tag.regs_left--;
        if (!current_tag.regs_left)
        {
            current_tag.regs_left = current_tag.reg_count;
            current_tag.data_left--;

            //If NREGS * NLOOP is odd, discard the last 64 bits of data
            if (!current_tag.data_left && i == 0)
                return;
        }
    }
}

void GraphicsInterface::feed_GIF(uint128_t data)
{
    //printf("[GIF] Data: $%08X_%08X_%08X_%08X\n", data._u32[3], data._u32[2], data._u32[1], data._u32[0]);
    uint64_t data1 = data._u64[0];
    uint64_t data2 = data._u64[1];
    if (!current_tag.data_left)
    {
        //Read new GIFtag
        current_tag.NLOOP = data1 & 0x7FFF;
        current_tag.end_of_packet = data1 & (1 << 15);
        current_tag.output_PRIM = (data1 >> 46) & 0x1;
        current_tag.PRIM = (data1 >> 47) & 0x7FF;
        current_tag.format = (data1 >> 58) & 0x3;
        current_tag.reg_count = data1 >> 60;
        if (!current_tag.reg_count)
            current_tag.reg_count = 16;
        current_tag.regs = data2;
        current_tag.regs_left = current_tag.reg_count;
        current_tag.data_left = current_tag.NLOOP;

        //Q is initialized to 1.0 upon reading a GIFtag
        gs->set_Q(1.0f);

        //Ignore zeroed out packets
        /*if (data1)
        {
            printf("[GIF] New primitive!\n");
            printf("NLOOP: $%04X\n", current_tag.NLOOP);
            printf("EOP: %d\n", current_tag.end_of_packet);
            printf("Output PRIM: %d PRIM: $%04X\n", current_tag.output_PRIM, current_tag.PRIM);
            printf("Format: %d\n", current_tag.format);
            printf("Reg count: %d\n", current_tag.reg_count);
            printf("Regs: $%08X_$%08X\n", current_tag.regs >> 32, current_tag.regs & 0xFFFFFFFF);
        }*/

        if (current_tag.output_PRIM && current_tag.format != 1)
            gs->write64(0, current_tag.PRIM);
    }
    else
    {
        switch (current_tag.format)
        {
            case 0:
                process_PACKED(data);
                current_tag.regs_left--;
                if (!current_tag.regs_left)
                {
                    current_tag.regs_left = current_tag.reg_count;
                    current_tag.data_left--;
                }
                break;
            case 1:
                process_REGLIST(data);
                break;
            case 2:
            case 3:
                gs->write64(0x54, data1);
                gs->write64(0x54, data2);
                current_tag.data_left--;
                break;
            default:
                printf("[GS] Unrecognized GIFtag format %d\n", current_tag.format);
                break;
        }
        if (!current_tag.data_left && current_tag.end_of_packet)
            gs->assert_FINISH();
    }
}

void GraphicsInterface::set_path3_vifmask(int value)
{
    path3_vif_masked = value;
}

void GraphicsInterface::request_PATH(int index)
{
    printf("[GIF] PATH%d requested\n", index);
    if (!active_path)
    {
        active_path = index;
        printf("[GIF] Active!\n");
    }
    else
        path_queue |= 1 << index;
}

void GraphicsInterface::deactivate_PATH(int index)
{
    printf("[GIF] PATH%d deactivated\n", index);
    path_queue &= ~(1 << index);
    if (active_path == index)
    {
        active_path = 0;

        for (int path = 1; path <= 3; path++)
        {
            int bit = 1 << path;
            if (path_queue & bit)
            {
                path_queue &= ~bit;
                active_path = path;
                break;
            }
        }
    }
}

bool GraphicsInterface::send_PATH(int index, uint128_t quad)
{
    feed_GIF(quad);
    return true;
}

//Returns true if an EOP transfer has ended - this terminates the XGKICK command
bool GraphicsInterface::send_PATH1(uint128_t quad)
{
    //printf("[GIF] Send PATH1 $%08X_%08X_%08X_%08X\n", quad._u32[3], quad._u32[2], quad._u32[1], quad._u32[0]);
    feed_GIF(quad);
    return !current_tag.data_left && current_tag.end_of_packet;
}

void GraphicsInterface::send_PATH2(uint32_t data[])
{
    uint128_t blorp;
    for (int i = 0; i < 4; i++)
        blorp._u32[i] = data[i];
    feed_GIF(blorp);
}

void GraphicsInterface::send_PATH3(uint128_t data)
{
    //printf("[GIF] Send PATH3 $%08X_%08X_%08X_%08X\n", data._u32[3], data._u32[2], data._u32[1], data._u32[0]);
    feed_GIF(data);
}
