#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "gs.hpp"

/**
~ GS notes ~

PRIM.prim_type:
0 - Point
1 - Line
2 - Line strip
3 - Triangle
4 - Triangle strip
5 - Triangle fan
6 - Sprite
7 - Prohibited
**/

const unsigned int GraphicsSynthesizer::max_vertices[8] = {1, 2, 2, 3, 3, 3, 2, 0};

GraphicsSynthesizer::GraphicsSynthesizer()
{
    local_mem = nullptr;
}

GraphicsSynthesizer::~GraphicsSynthesizer()
{
    if (local_mem)
        delete[] local_mem;
}

void GraphicsSynthesizer::reset()
{
    if (!local_mem)
        local_mem = new uint32_t[0xFFFFF];
    processing_GIF_prim = false;
    current_tag.data_left = 0;
    pixels_transferred = 0;
    transfer_bit_depth = 32;
    num_vertices = 0;
}

uint32_t GraphicsSynthesizer::read32_privileged(uint32_t addr)
{
    addr &= 0xFFFF;
    switch (addr)
    {
        case 0x1000:
            return 0x00000008;
        default:
            printf("\n[GS] Unrecognized privileged read32 from $%04X", addr);
            exit(1);
    }
}

void GraphicsSynthesizer::write32_privileged(uint32_t addr, uint32_t value)
{
    addr &= 0xFFFF;
    switch (addr)
    {
        case 0x1000:
            printf("\n[GS] Write32 to GS_CSR: $%08X", value);
            break;
        default:
            printf("\n[GS] Unrecognized privileged write32 to reg $%04X: $%08X", addr, value);
    }
}

void GraphicsSynthesizer::write64_privileged(uint32_t addr, uint64_t value)
{
    addr &= 0xFFFF;
    switch (addr)
    {
        case 0x0000:
            printf("\n[GS] Write PMODE: $%08X_%08X", value >> 32, value & 0xFFFFFFFF);
            PMODE.circuit1 = value & 0x1;
            PMODE.circuit2 = value & 0x2;
            PMODE.output_switching = (value >> 2) & 0x7;
            PMODE.use_ALP = value & (1 << 5);
            PMODE.out1_circuit2 = value & (1 << 6);
            PMODE.blend_with_bg = value & (1 << 7);
            PMODE.ALP = (value >> 8) & 0xFF;
            break;
        case 0x0020:
            printf("\n[GS] Write SMODE2: $%08X_%08X", value >> 32, value & 0xFFFFFFFF);
            SMODE2.interlaced = value & 0x1;
            SMODE2.frame_mode = value & 0x2;
            SMODE2.power_mode = (value >> 2) & 0x3;
            break;
        case 0x0090:
            printf("\n[GS] Write DISPFB2: $%08X_%08X", value >> 32, value & 0xFFFFFFFF);
            DISPFB2.frame_base = (value & 0x3FF) * 2048;
            DISPFB2.width = ((value >> 9) & 0x3F) * 64;
            DISPFB2.format = (value >> 14) & 0x1F;
            DISPFB2.x = (value >> 32) & 0x7FF;
            DISPFB2.y = (value >> 43) & 0x7FF;
            break;
        case 0x00A0:
            printf("\n[GS] Write DISPLAY2: $%08X_%08X", value >> 32, value & 0xFFFFFFFF);
            DISPLAY2.x = value & 0xFFF;
            DISPLAY2.y = (value >> 12) & 0x7FF;
            DISPLAY2.magnify_x = (value >> 23) & 0xF;
            DISPLAY2.magnify_y = (value >> 27) & 0x3;
            DISPLAY2.width = (value >> 32) & 0xFFF;
            DISPLAY2.height = (value >> 44) & 0x7FF;
            break;
        default:
            printf("\n[GS] Unrecognized privileged write64 to reg $%04X: $%08X_%08X", addr, value >> 32, value & 0xFFFFFFFF);
    }
}

void GraphicsSynthesizer::write64(uint32_t addr, uint64_t value)
{
    addr &= 0xFFFF;
    switch (addr)
    {
        case 0x0000:
            PRIM.prim_type = value & 0x7;
            PRIM.gourand_shading = value & (1 << 3);
            PRIM.texture_mapping = value & (1 << 4);
            PRIM.fog = value & (1 << 5);
            PRIM.alpha_blend = value & (1 << 6);
            PRIM.antialiasing = value & (1 << 7);
            PRIM.use_UV = value & (1 << 8);
            PRIM.use_context2 = value & (1 << 9);
            PRIM.fix_fragment_value = value & (1 << 10);
            num_vertices = 0;
            break;
        case 0x0001:
            RGBAQ.r = value & 0xFF;
            RGBAQ.g = (value >> 8) & 0xFF;
            RGBAQ.b = (value >> 16) & 0xFF;
            RGBAQ.a = (value >> 24) & 0xFF;
            RGBAQ.q = (float)(value >> 32);
            break;
        case 0x0005:
            //XYZ2
            current_vtx.coords[0] = value & 0xFFFF;
            current_vtx.coords[1] = (value >> 16) & 0xFFFF;
            current_vtx.coords[2] = value >> 32;
            vertex_kick(true);
            break;
        case 0x0018:
            XYOFFSET_1.x = value & 0xFFFF;
            XYOFFSET_1.y = (value >> 32) & 0xFFFF;
            break;
        case 0x001A:
            use_PRIM = value & 0x1;
            break;
        case 0x0040:
            SCISSOR_1.x1 = value & 0x7FF;
            SCISSOR_1.x2 = (value >> 16) & 0x7FF;
            SCISSOR_1.y1 = (value >> 32) & 0x7FF;
            SCISSOR_1.y2 = (value >> 48) & 0x7FF;
            break;
        case 0x0045:
            DTHE = value & 0x1;
            break;
        case 0x0046:
            COLCLAMP = value & 0x1;
            break;
        case 0x0047:
            TEST_1.alpha_test = value & 0x1;
            TEST_1.alpha_method = (value >> 1) & 0x7;
            TEST_1.alpha_ref = (value >> 4) & 0xFF;
            TEST_1.alpha_fail_method = (value >> 12) & 0x3;
            TEST_1.dest_alpha_test = value & (1 << 14);
            TEST_1.dest_alpha_method = value & (1 << 15);
            TEST_1.depth_test = value & (1 << 16);
            TEST_1.depth_method = (value >> 17) & 0x3;
            break;
        case 0x004C:
            FRAME_1.base_pointer = (value & 0x1FF) * 2048;
            FRAME_1.width = ((value >> 16) & 0x1F) * 64;
            FRAME_1.format = (value >> 24) & 0x3F;
            FRAME_1.mask = value >> 32;
            break;
        case 0x004E:
            ZBUF_1.base_pointer = (value & 0x1FF) * 2048;
            ZBUF_1.format = (value >> 24) & 0xF;
            ZBUF_1.no_update = value & (1UL << 32);
            break;
        case 0x0050:
            BITBLTBUF.source_base = (value & 0x3FFF) * 2048;
            BITBLTBUF.source_width = ((value >> 16) & 0x3F) * 64;
            BITBLTBUF.source_format = (value >> 24) & 0x3F;
            BITBLTBUF.dest_base = ((value >> 32) & 0x3FFF) * 2048;
            BITBLTBUF.dest_width = ((value >> 48) & 0x3F) * 64;
            BITBLTBUF.dest_format = (value >> 56) & 0x3F;
            break;
        case 0x0051:
            TRXPOS.source_x = value & 0x7FF;
            TRXPOS.source_y = (value >> 16) & 0x7FF;
            TRXPOS.dest_x = (value >> 32) & 0x7FF;
            TRXPOS.dest_y = (value >> 48) & 0x7FF;
            TRXPOS.trans_order = (value >> 59) & 0x3;
            break;
        case 0x0052:
            TRXREG.width = value & 0xFFF;
            TRXREG.height = (value >> 32) & 0xFFF;
            break;
        case 0x0053:
            TRXDIR = value & 0x3;
            //Start transmission
            if (TRXDIR != 3)
            {
                pixels_transferred = 0;
                transfer_addr = BITBLTBUF.dest_base + (TRXPOS.dest_x + (TRXPOS.dest_y * TRXREG.width)) * 2;
            }
            break;
        case 0x0054:
            if (TRXDIR == 0)
                write_HWREG(value);
            break;
        default:
            printf("\n[GS] Unrecognized write64 to reg $%04X: $%08X_%08X", addr, value >> 32, value & 0xFFFFFFFF);
    }
}

void GraphicsSynthesizer::feed_GIF(uint64_t data[])
{
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
        RGBAQ.q = 1.0f;

        /*printf("\n[GS] New primitive!");
        printf("\nNLOOP: $%04X", current_tag.NLOOP);
        printf("\nEOP: %d", current_tag.end_of_packet);
        printf("\nOutput PRIM: %d PRIM: $%04X", current_tag.output_PRIM, current_tag.PRIM);
        printf("\nFormat: %d", current_tag.format);
        printf("\nReg count: %d", current_tag.reg_count);
        printf("\nRegs: $%08X_$%08X", current_tag.regs >> 32, current_tag.regs & 0xFFFFFFFF);*/

        if (current_tag.output_PRIM)
            write64(0, current_tag.PRIM);
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
                write64(0x54, data[0]);
                write64(0x54, data[1]);
                current_tag.data_left--;
                break;
            default:
                printf("\n[GS] Unrecognized GIFtag format %d", current_tag.format);
                break;
        }

    }
    //printf("\n[GS] $%08X_%08X_%08X_%08X", data[1] >> 32, data[1] & 0xFFFFFFFF, data[0] >> 32, data[0] & 0xFFFFFFFF);
}

//The "vertex kick" is the name given to the process of placing a vertex in the vertex queue.
//If drawing_kick is true, and enough vertices are available, then the polygon is rendered.
void GraphicsSynthesizer::vertex_kick(bool drawing_kick)
{
    for (int i = num_vertices; i > 0; i--)
        vtx_queue[i] = vtx_queue[i - 1];
    current_vtx.rgbaq = RGBAQ;
    vtx_queue[0] = current_vtx;

    num_vertices++;
    bool request_draw_kick = false;
    switch (PRIM.prim_type)
    {
        case 3:
            if (num_vertices == 3)
            {
                num_vertices = 0;
                request_draw_kick = true;
            }
            break;
        case 6:
            if (num_vertices == 2)
            {
                num_vertices = 0;
                request_draw_kick = true;
            }
            break;
        default:
            printf("\n[GS] Unrecognized primitive %d\n", PRIM.prim_type);
            exit(1);
    }
    if (drawing_kick && request_draw_kick)
        render_primitive();
}

void GraphicsSynthesizer::render_primitive()
{
    switch (PRIM.prim_type)
    {
        case 3:
        case 4:
        case 5:
            render_triangle();
            break;
        case 6:
            render_sprite();
            break;
    }
}

void GraphicsSynthesizer::render_triangle()
{
    printf("\n[GS] Rendering triangle!");
}

void GraphicsSynthesizer::render_sprite()
{
    printf("\n[GS] Rendering sprite!");
}

void GraphicsSynthesizer::process_PACKED(uint64_t data[])
{
    int reg_offset = (current_tag.reg_count - current_tag.regs_left) << 2;
    uint8_t reg = (current_tag.regs & (0xF << reg_offset)) >> reg_offset;
    switch (reg)
    {
        case 0x1:
            //RGBAQ - set RGBA
            //Q is taken from the ST command
            RGBAQ.r = data[0] & 0xFF;
            RGBAQ.g = (data[0] >> 32) & 0xFF;
            RGBAQ.b = data[1] & 0xFF;
            RGBAQ.a = (data[1] >> 32) & 0xFF;
            break;
        case 0x4:
            //XYZF2 - set XYZ and fog coefficient. Optionally disable drawing kick through bit 111
            current_vtx.coords[0] = data[0] & 0xFFFF;
            current_vtx.coords[1] = (data[0] >> 32) & 0xFFFF;
            current_vtx.coords[2] = (data[1] >> 4) & 0xFFFFFF;
            vertex_kick(!(data[1] & (1UL << (111 - 64))));
            break;
        case 0xE:
        {
            //A+D: output data to address
            uint32_t addr = data[1] & 0xFF;
            write64(addr, data[0]);
        }
            break;
        default:
            printf("\nUnrecognized PACKED reg $%02X", reg);
            break;
    }
}

void GraphicsSynthesizer::send_PATH3(uint64_t data[])
{
    feed_GIF(data);
}

void GraphicsSynthesizer::write_HWREG(uint64_t data)
{
    //TODO: other pixel formats
    transfer_bit_depth = 32;
    int ppd = 2; //pixels per doubleword
    *(uint64_t*)&local_mem[transfer_addr] = data;

    //printf("\n[GS] Write to $%08X of $%08X_%08X", transfer_addr, data >> 32, data & 0xFFFFFFFF);
    uint32_t max_pixels = TRXREG.width * TRXREG.height;
    pixels_transferred += ppd;
    transfer_addr += ppd;
    if (pixels_transferred >= max_pixels)
    {
        //Deactivate the transmisssion
        printf("\n[GS] HWREG transfer ended");
        TRXDIR = 3;
        pixels_transferred = 0;
    }
}
