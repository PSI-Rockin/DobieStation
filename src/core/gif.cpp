#include "logger.hpp"
#include "gif.hpp"
#include "gs.hpp"

GraphicsInterface::GraphicsInterface(GraphicsSynthesizer *gs) : gs(gs)
{

}

void GraphicsInterface::reset()
{
    processing_GIF_prim = false;
    current_tag.data_left = 0;
    active_paths = 0;
}

uint32_t GraphicsInterface::read_STAT()
{
    uint32_t reg = 0;

    //OPH
    if (active_paths)
    {
        reg |= 1 << 9;
        int path = 0;
        if (active_paths & (1 << 1))
            path = 1;
        else if (active_paths & (1 << 2))
            path = 2;
        else if (active_paths & (1 << 3))
            path = 3;
        reg |= path << 10;
    }
    return reg;
}

void GraphicsInterface::process_PACKED(uint128_t data)
{
    uint64_t data1 = data._u64[0];
    uint64_t data2 = data._u64[1];
    //Logger::log(Logger::GIF, "PACKED: $%08X_%08X_%08X_%08X\n", data._u32[3], data._u32[2], data._u32[1], data._u32[0]);
    uint64_t reg_offset = (current_tag.reg_count - current_tag.regs_left) << 2;
    uint8_t reg = (current_tag.regs & (0xFUL << reg_offset)) >> reg_offset;
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
        case 0x6:
        case 0x7:
            gs->write64(reg, data1);
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
            Logger::log(Logger::GIF, "Unrecognized PACKED reg $%02X\n", reg);
            break;
    }
}

void GraphicsInterface::process_REGLIST(uint128_t data)
{
    //Logger::log(Logger::GIF, "Reglist: $%08X_%08X_%08X_%08X\n", data._u32[3], data._u32[2], data._u32[1], data._u32[0]);
    for (int i = 0; i < 2; i++)
    {
        uint64_t reg_offset = (current_tag.reg_count - current_tag.regs_left) << 2;
        uint8_t reg = (current_tag.regs & (0xFUL << reg_offset)) >> reg_offset;
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
    //Logger::log(Logger::GIF, "Data: $%08X_%08X_%08X_%08X\n", data._u32[3], data._u32[2], data._u32[1], data._u32[0]);
    uint64_t data1 = data._u64[0];
    uint64_t data2 = data._u64[1];
    if (!current_tag.data_left)
    {
        //Read new GIFtag
        processing_GIF_prim = true;
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
        if (data1)
        {
            Logger::log(Logger::GIF, "New primitive!\n");
            Logger::log(Logger::GIF, "NLOOP: $%04X\n", current_tag.NLOOP);
            Logger::log(Logger::GIF, "EOP: %d\n", current_tag.end_of_packet);
            Logger::log(Logger::GIF, "Output PRIM: %d PRIM: $%04X\n", current_tag.output_PRIM, current_tag.PRIM);
            Logger::log(Logger::GIF, "Format: %d\n", current_tag.format);
            Logger::log(Logger::GIF, "Reg count: %d\n", current_tag.reg_count);
            Logger::log(Logger::GIF, "Regs: $%08X_$%08X\n", current_tag.regs >> 32, current_tag.regs & 0xFFFFFFFF);
        }

        if (current_tag.output_PRIM && current_tag.format != 1)
            gs->write64(0, current_tag.PRIM);
    }
    else
    {
        /*if (!current_tag.data_left)
        {
            processing_GIF_prim = false;
            return;
        }*/
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
                Logger::log(Logger::GS, "Unrecognized GIFtag format %d\n", current_tag.format);
                break;
        }
        if (!current_tag.data_left && current_tag.end_of_packet)
            gs->assert_FINISH();
    }
}

void GraphicsInterface::activate_PATH(int index)
{
    active_paths |= 1 << index;
}

void GraphicsInterface::deactivate_PATH(int index)
{
    active_paths &= ~(1 << index);
}

bool GraphicsInterface::send_PATH(int index, uint128_t quad)
{
    if (active_paths & (1 * (1 << (index - 1)) - 1))
        return false;
    feed_GIF(quad);
    return true;
}

//Returns true if an EOP transfer has ended - this terminates the XGKICK command
bool GraphicsInterface::send_PATH1(uint128_t quad)
{
    Logger::log(Logger::GIF,"Send PATH1 $%08X_%08X_%08X_%08X\n", quad._u32[3], quad._u32[2], quad._u32[1], quad._u32[0]);
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
    feed_GIF(data);
}
