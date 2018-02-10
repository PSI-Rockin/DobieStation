#include <cstdio>
#include "gif.hpp"
#include "gs.hpp"

GraphicsInterface::GraphicsInterface(GraphicsSynthesizer *gs) : gs(gs)
{

}

void GraphicsInterface::reset()
{
    processing_GIF_prim = false;
    current_tag.data_left = 0;
}

void GraphicsInterface::process_PACKED(uint64_t data[])
{
    int reg_offset = (current_tag.reg_count - current_tag.regs_left) << 2;
    uint8_t reg = (current_tag.regs & (0xF << reg_offset)) >> reg_offset;
    switch (reg)
    {
        case 0x0:
            //PRIM
            gs->write64(0, data[0]);
            break;
        case 0x1:
            //RGBAQ - set RGBA
            //Q is taken from the ST command
        {
            uint8_t r = data[0] & 0xFF;
            uint8_t g = (data[0] >> 32) & 0xFF;
            uint8_t b = data[1] & 0xFF;
            uint8_t a = (data[1] >> 32) & 0xFF;
            gs->set_RGBA(r, g, b, a);
        }
            break;
        case 0x4:
            //XYZF2 - set XYZ and fog coefficient. Optionally disable drawing kick through bit 111
        {
            uint32_t x = data[0] & 0xFFFF;
            uint32_t y = (data[0] >> 32) & 0xFFFF;
            uint32_t z = (data[1] >> 4) & 0xFFFFFF;
            bool disable_drawing = data[1] & (1UL << (111 - 64));
            gs->set_XYZ(x, y, z, !disable_drawing);
        }
            break;
        case 0x5:
        //XYZ2 - set XYZ. Optionally disable drawing kick through bit 111
        {
            uint32_t x = data[0] & 0xFFFF;
            uint32_t y = (data[0] >> 32) & 0xFFFF;
            uint32_t z = (data[1] >> 4) & 0xFFFFFF;
            bool disable_drawing = data[1] & (1UL << (111 - 64));
            gs->set_XYZ(x, y, z, !disable_drawing);
        }
            break;
        case 0xE:
        {
            //A+D: output data to address
            uint32_t addr = data[1] & 0xFF;
            gs->write64(addr, data[0]);
        }
            break;
        case 0xF:
            //NOP
            break;
        default:
            printf("\nUnrecognized PACKED reg $%02X", reg);
            break;
    }
}

void GraphicsInterface::feed_GIF(uint64_t data[])
{
    //printf("\n[GS] $%08X_%08X_%08X_%08X", data[1] >> 32, data[1] & 0xFFFFFFFF, data[0] >> 32, data[0] & 0xFFFFFFFF);
    if (!current_tag.data_left)
    {
        //Read the GIFtag
        processing_GIF_prim = true;
        current_tag.NLOOP = data[0] & 0x7FFF;
        current_tag.end_of_packet = data[0] & (1 << 15);
        current_tag.output_PRIM = data[0] & (1UL << 46);
        current_tag.PRIM = (data[0] >> 47) & 0x7FF;
        current_tag.format = (data[0] >> 58) & 0x3;
        current_tag.reg_count = data[0] >> 60;
        if (!current_tag.reg_count)
            current_tag.reg_count = 16;
        current_tag.regs = data[1];
        current_tag.regs_left = current_tag.reg_count;
        current_tag.data_left = current_tag.NLOOP;

        //Q is initialized to 1.0 upon reading a GIFtag
        gs->set_Q(1.0f);

        /*printf("\n[GS] New primitive!");
        printf("\nNLOOP: $%04X", current_tag.NLOOP);
        printf("\nEOP: %d", current_tag.end_of_packet);
        printf("\nOutput PRIM: %d PRIM: $%04X", current_tag.output_PRIM, current_tag.PRIM);
        printf("\nFormat: %d", current_tag.format);
        printf("\nReg count: %d", current_tag.reg_count);
        printf("\nRegs: $%08X_$%08X", current_tag.regs >> 32, current_tag.regs & 0xFFFFFFFF);*/

        if (current_tag.output_PRIM)
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
            case 2:
                gs->write64(0x54, data[0]);
                gs->write64(0x54, data[1]);
                current_tag.data_left--;
                break;
            default:
                printf("\n[GS] Unrecognized GIFtag format %d", current_tag.format);
                break;
        }

    }
}

void GraphicsInterface::send_PATH3(uint64_t data[])
{
    feed_GIF(data);
}
