#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "gs.hpp"

using namespace std;

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

Color formats:
PSMCT32 - RGBA32
PSMCT24 - RGB24 (upper 8 bits unused)
PSMCT16 - two RGBA16 pixels
**/

template <typename T>
T interpolate(int32_t x, T u1, int32_t x1, T u2, int32_t x2)
{
    int64_t bark = (int64_t)u1 * (x2 - x);
    bark += (int64_t)u2 * (x - x1);
    return bark / (x2 - x1);
}

const unsigned int GraphicsSynthesizer::max_vertices[8] = {1, 2, 2, 3, 3, 3, 2, 0};

GraphicsSynthesizer::GraphicsSynthesizer()
{
    frame_complete = false;
    output_buffer = nullptr;
    local_mem = nullptr;
}

GraphicsSynthesizer::~GraphicsSynthesizer()
{
    if (local_mem)
        delete[] local_mem;
    if (output_buffer)
        delete[] output_buffer;
}

void GraphicsSynthesizer::reset()
{
    if (!local_mem)
        local_mem = new uint32_t[1024 * 1024];
    if (!output_buffer)
        output_buffer = new uint32_t[640 * 256];
    processing_GIF_prim = false;
    current_tag.data_left = 0;
    pixels_transferred = 0;
    transfer_bit_depth = 32;
    num_vertices = 0;
    context1.reset();
    context2.reset();
    current_ctx = &context1;
}

void GraphicsSynthesizer::start_frame()
{
    frame_complete = false;
}

bool GraphicsSynthesizer::is_frame_complete()
{
    return frame_complete;
}

void GraphicsSynthesizer::set_CRT(bool interlaced, int mode, bool frame_mode)
{
    SMODE2.interlaced = interlaced;
    CRT_mode = mode;
    SMODE2.frame_mode = frame_mode;
}

uint32_t* GraphicsSynthesizer::get_framebuffer()
{
    return output_buffer;
}

void GraphicsSynthesizer::render_CRT()
{
    printf("\nDISPLAY2: (%d, %d) wh: (%d, %d)", DISPLAY2.x >> 2, DISPLAY2.y, DISPLAY2.width >> 2, DISPLAY2.height);
    int width = DISPLAY2.width >> 2;
    for (int y = 0; y < DISPLAY2.height; y++)
    {
        for (int x = 0; x < width; x++)
        {
            int pixel_x = x;
            int pixel_y = y;
            if (pixel_x >= width || pixel_y >= DISPLAY2.height)
                continue;
            uint32_t scaled_x = x;
            uint32_t scaled_y = y;
            scaled_x *= DISPFB2.width;
            scaled_x /= width;
            output_buffer[pixel_x + (pixel_y * width)] = local_mem[DISPFB2.frame_base + scaled_x + (scaled_y * DISPFB2.width)];
            //printf("\n$%08X ($%08X)", output_buffer[pixel_x + (pixel_y * width)], DISPFB2.frame_base + scaled_x + (scaled_y * DISPFB2.width));
            output_buffer[pixel_x + (pixel_y * width)] |= 0xFF000000;
        }
    }
}

void GraphicsSynthesizer::get_resolution(int &w, int &h)
{
    w = 640;
    switch (CRT_mode)
    {
        case 0x2:
            h = 448;
            break;
        case 0x3:
            h = 512;
            break;
        case 0x1C:
            h = 480;
            break;
        default:
            h = 448;
    }
}

uint32_t GraphicsSynthesizer::read32_privileged(uint32_t addr)
{
    addr &= 0xFFFF;
    switch (addr)
    {
        case 0x1000:
            return 0x8;
        default:
            printf("\n[GS] Unrecognized privileged read32 from $%04X", addr);
            exit(1);
    }
}

uint64_t GraphicsSynthesizer::read64_privileged(uint32_t addr)
{
    addr &= 0xFFFF;
    switch (addr)
    {
        case 0x1000:
            return 0x8;
    }
}

void GraphicsSynthesizer::write32_privileged(uint32_t addr, uint32_t value)
{
    addr &= 0xFFFF;
    switch (addr)
    {
        case 0x1000:
            printf("\n[GS] Write32 to GS_CSR: $%08X", value);

            //Ugly VSYNC hack
            if (value & 0x8)
                frame_complete = true;
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
            DISPLAY2.width = ((value >> 32) & 0xFFF) + 1;
            DISPLAY2.height = ((value >> 44) & 0x7FF) + 1;
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
        case 0x0003:
            UV.u = value & 0x3FFF;
            UV.v = (value >> 16) & 0x3FFF;
            printf("\nUV: ($%04X, $%04X)", UV.u, UV.v);
            break;
        case 0x0005:
            //XYZ2
            current_vtx.coords[0] = value & 0xFFFF;
            current_vtx.coords[1] = (value >> 16) & 0xFFFF;
            current_vtx.coords[2] = value >> 32;
            vertex_kick(true);
            break;
        case 0x0006:
            context1.set_tex0(value);
            break;
        case 0x0018:
            context1.set_xyoffset(value);
            break;
        case 0x001A:
            use_PRIM = value & 0x1;
            break;
        case 0x003F:
            printf("\nTEXFLUSH");
            break;
        case 0x0040:
            context1.set_scissor(value);
            break;
        case 0x0042:
            context1.set_alpha(value);
            break;
        case 0x0045:
            DTHE = value & 0x1;
            break;
        case 0x0046:
            COLCLAMP = value & 0x1;
            break;
        case 0x0047:
            context1.set_test(value);
            break;
        case 0x004C:
            context1.set_frame(value);
            break;
        case 0x004E:
            context1.set_zbuf(value);
            break;
        case 0x0050:
            BITBLTBUF.source_base = (value & 0x3FFF) * 64;
            BITBLTBUF.source_width = ((value >> 16) & 0x3F) * 64;
            BITBLTBUF.source_format = (value >> 24) & 0x3F;
            BITBLTBUF.dest_base = ((value >> 32) & 0x3FFF) * 64;
            BITBLTBUF.dest_width = ((value >> 48) & 0x3F) * 64;
            BITBLTBUF.dest_format = (value >> 56) & 0x3F;
            break;
        case 0x0051:
            TRXPOS.source_x = value & 0x7FF;
            TRXPOS.source_y = (value >> 16) & 0x7FF;
            TRXPOS.dest_x = (value >> 32) & 0x7FF;
            TRXPOS.dest_y = (value >> 48) & 0x7FF;
            TRXPOS.trans_order = (value >> 59) & 0x3;
            printf("\nTRXPOS: $%08X_%08X", value >> 32, value);
            break;
        case 0x0052:
            TRXREG.width = value & 0xFFF;
            TRXREG.height = (value >> 32) & 0xFFF;
            printf("\nTRXREG (%d, %d)", TRXREG.width, TRXREG.height);
            break;
        case 0x0053:
            TRXDIR = value & 0x3;
            //Start transmission
            if (TRXDIR != 3)
            {
                pixels_transferred = 0;
                transfer_addr = BITBLTBUF.dest_base + (TRXPOS.dest_x + (TRXPOS.dest_y * BITBLTBUF.dest_width));
                printf("\nTransfer started!");
                printf("\nDest base: $%08X", BITBLTBUF.dest_base);
                printf("\nTRXPOS: (%d, %d)", TRXPOS.dest_x, TRXPOS.dest_y);
                printf("\nWidth: %d", BITBLTBUF.dest_width);
                printf("\nTransfer addr: $%08X", transfer_addr);
                if (TRXDIR == 2)
                {
                    //VRAM-to-VRAM transfer
                    //More than likely not instantaneous
                    host_to_host();
                }
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
}

//The "vertex kick" is the name given to the process of placing a vertex in the vertex queue.
//If drawing_kick is true, and enough vertices are available, then the polygon is rendered.
void GraphicsSynthesizer::vertex_kick(bool drawing_kick)
{
    for (int i = num_vertices; i > 0; i--)
        vtx_queue[i] = vtx_queue[i - 1];
    current_vtx.rgbaq = RGBAQ;
    current_vtx.uv = UV;
    vtx_queue[0] = current_vtx;

    num_vertices++;
    bool request_draw_kick = false;
    switch (PRIM.prim_type)
    {
        case 0:
            num_vertices--;
            request_draw_kick = true;
            break;
        case 1:
            if (num_vertices == 2)
            {
                num_vertices = 0;
                request_draw_kick = true;
            }
            break;
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
        case 0:
            render_point();
            break;
        case 1:
        case 2:
            render_line();
            break;
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

void GraphicsSynthesizer::draw_pixel(uint32_t x, uint32_t y, uint32_t color, uint32_t z, bool alpha_blending)
{
    TEST* test = &current_ctx->test;
    bool update_frame = true;
    bool update_alpha = true;
    bool update_z = !current_ctx->zbuf.no_update;
    if (test->alpha_test)
    {
        bool fail = false;
        uint8_t alpha = color >> 24;
        switch (test->alpha_method)
        {
            case 0: //NEVER
                fail = true;
                break;
            case 1: //ALWAYS
                break;
            case 2: //LESS
                if (alpha >= test->alpha_ref)
                    fail = true;
                break;
            case 3: //LEQUAL
                if (alpha > test->alpha_ref)
                    fail = true;
                break;
            case 4: //EQUAL
                if (alpha != test->alpha_ref)
                    fail = true;
                break;
            case 5: //GEQUAL
                if (alpha < test->alpha_ref)
                    fail = true;
                break;
            case 6: //GREATER
                if (alpha <= test->alpha_ref)
                    fail = true;
                break;
            case 7: //NOTEQUAL
                if (alpha == test->alpha_ref)
                    fail = true;
                break;
        }
        if (fail)
        {
            switch (test->alpha_fail_method)
            {
                case 0: //KEEP - Update nothing
                    return;
                case 1: //FB_ONLY - Only update framebuffer
                    update_z = false;
                    break;
                case 2: //ZB_ONLY - Only update z-buffer
                    update_frame = false;
                    break;
                case 3: //RGB_ONLY - Same as FB_ONLY, but ignore alpha
                    update_z = false;
                    update_alpha = false;
                    break;
            }
        }
    }

    uint32_t pos = (x >> 4) + ((y >> 4) * current_ctx->frame.width);
    uint32_t dest_z = local_mem[current_ctx->zbuf.base_pointer + pos];
    if (test->depth_test)
    {
        switch (test->depth_method)
        {
            case 0: //FAIL
                return;
            case 1: //PASS
                break;
            case 2: //GEQUAL
                if (z < dest_z)
                    return;
                break;
            case 3: //GREATER
                if (z <= dest_z)
                    return;
                break;
        }
    }

    if (alpha_blending)
    {
        uint8_t r1, g1, b1;
        uint8_t r2, g2, b2;
        uint8_t cr, cg, cb;
        uint32_t alpha;

        uint32_t frame_color = local_mem[current_ctx->frame.base_pointer + pos];

        switch (current_ctx->alpha.spec_A)
        {
            case 0:
                r1 = (color >> 16) & 0xFF;
                g1 = (color >> 8) & 0xFF;
                b1 = color & 0xFF;
                break;
            case 1:
                r1 = (frame_color >> 16) & 0xFF;
                g1 = (frame_color >> 8) & 0xFF;
                b1 = frame_color & 0xFF;
                break;
            case 2:
            case 3:
                r1 = 0;
                g1 = 0;
                b1 = 0;
                break;
        }

        switch (current_ctx->alpha.spec_B)
        {
            case 0:
                r2 = (color >> 16) & 0xFF;
                g2 = (color >> 8) & 0xFF;
                b2 = color & 0xFF;
                break;
            case 1:
                r2 = (frame_color >> 16) & 0xFF;
                g2 = (frame_color >> 8) & 0xFF;
                b2 = frame_color & 0xFF;
                break;
            case 2:
            case 3:
                r2 = 0;
                g2 = 0;
                b2 = 0;
                break;
        }

        switch (current_ctx->alpha.spec_C)
        {
            case 0:
                alpha = color >> 24;
                break;
            case 1:
                alpha = frame_color >> 24;
                break;
            case 2:
            case 3:
                alpha = current_ctx->alpha.fixed_alpha;
                break;
        }

        switch (current_ctx->alpha.spec_D)
        {
            case 0:
                cr = (color >> 16) & 0xFF;
                cg = (color >> 8) & 0xFF;
                cb = color & 0xFF;
                break;
            case 1:
                cr = (frame_color >> 16) & 0xFF;
                cg = (frame_color >> 8) & 0xFF;
                cb = frame_color & 0xFF;
                break;
            case 2:
                cr = 0;
                cg = 0;
                cb = 0;
                break;
        }

        uint32_t final_color = 0;
        final_color |= alpha << 24;
        final_color |= ((((r1 - r2) * alpha) >> 7) + cr) << 16;
        final_color |= ((((g1 - g2) * alpha) >> 7) + cg) << 8;
        final_color |= (((b1 - b2) * alpha) >> 7) + cb;
        color = final_color;
    }
    if (update_frame)
    {
        uint8_t alpha = local_mem[current_ctx->frame.base_pointer + pos] >> 24;
        if (update_alpha)
            alpha = color >> 24;
        color &= 0x00FFFFFF;
        local_mem[current_ctx->frame.base_pointer + pos] = color | (alpha << 24);
    }
    if (update_z)
        local_mem[current_ctx->zbuf.base_pointer + pos] = z;
}

void GraphicsSynthesizer::render_point()
{
    printf("\n[GS] Rendering point!");
    uint32_t point[3];
    point[0] = vtx_queue[0].coords[0] - current_ctx->xyoffset.x;
    point[1] = vtx_queue[0].coords[1] - current_ctx->xyoffset.y;
    point[2] = vtx_queue[0].coords[2];
    uint32_t color = 0x00000000;
    color |= vtx_queue[0].rgbaq.a << 24;
    color |= vtx_queue[0].rgbaq.r << 16;
    color |= vtx_queue[0].rgbaq.g << 8;
    color |= vtx_queue[0].rgbaq.b;
    SCISSOR* scissor = &current_ctx->scissor;
    printf("\nCoords: (%d, %d, %d)", point[0] >> 4, point[1] >> 4, point[2]);
    if (point[0] >= scissor->x1 && point[0] <= scissor->x2 && point[1] >= scissor->y1 && point[1] <= scissor->y2)
        draw_pixel(point[0], point[1], color, point[2], PRIM.alpha_blend);
    else
    {
        printf("\nScissor: (%d, %d) (%d, %d)", scissor->x1, scissor->y1, scissor->x2, scissor->y2);
    }
}

void GraphicsSynthesizer::render_line()
{
    printf("\n[GS] Rendering line!");
    int16_t x1, x2, y1, y2;
    x1 = vtx_queue[1].coords[0] - current_ctx->xyoffset.x;
    x2 = vtx_queue[0].coords[0] - current_ctx->xyoffset.x;
    y1 = vtx_queue[1].coords[1] - current_ctx->xyoffset.y;
    y2 = vtx_queue[0].coords[1] - current_ctx->xyoffset.y;

    printf("\nCoords: (%d, %d) (%d, %d)", x1 >> 4, y1 >> 4, x2 >> 4, y2 >> 4);
}

void GraphicsSynthesizer::render_triangle()
{
    printf("\n[GS] Rendering triangle!");
    int32_t point[3];
    uint32_t color = 0xFF000000;
    color |= vtx_queue[0].rgbaq.r;
    color |= vtx_queue[0].rgbaq.g << 8;
    color |= vtx_queue[0].rgbaq.b << 16;
    color |= vtx_queue[0].rgbaq.a << 24;
    for (int i = 2; i >= 0; i--)
    {
        point[0] = vtx_queue[i].coords[0] - current_ctx->xyoffset.x;
        point[1] = vtx_queue[i].coords[1] - current_ctx->xyoffset.y;
        point[2] = vtx_queue[i].coords[2];
        //local_mem[(point[0] >> 4) + ((point[1] * current_ctx->frame.width) >> 4)] = color;
    }
}

void GraphicsSynthesizer::render_sprite()
{
    printf("\n[GS] Rendering sprite!");
    int16_t x1, x2, y1, y2;
    int16_t u1, u2, v1, v2;
    x1 = vtx_queue[1].coords[0] - current_ctx->xyoffset.x;
    x2 = vtx_queue[0].coords[0] - current_ctx->xyoffset.x;
    y1 = vtx_queue[1].coords[1] - current_ctx->xyoffset.y;
    y2 = vtx_queue[0].coords[1] - current_ctx->xyoffset.y;
    u1 = vtx_queue[1].uv.u;
    u2 = vtx_queue[0].uv.u;
    v1 = vtx_queue[1].uv.v;
    v2 = vtx_queue[0].uv.v;
    uint32_t z = vtx_queue[0].coords[2];

    if (x1 > x2)
    {
        swap(x1, x2);
        swap(y1, y2);
        swap(u1, u2);
        swap(v1, v2);
    }

    printf("\nCoords: ($%08X, $%08X) ($%08X, $%08X)", x1, y1, x2, y2);

    for (int32_t y = y1; y < y2; y += 0x10)
    {
        if (y < current_ctx->scissor.y1 || y > current_ctx->scissor.y2)
            continue;
        uint16_t pix_v = interpolate(y, v1, y1, v2, y2) >> 4;
        for (int32_t x = x1; x < x2; x += 0x10)
        {
            if (x < current_ctx->scissor.x1 || x > current_ctx->scissor.x2)
                continue;
            uint16_t pix_u = interpolate(x, u1, x1, u2, x2) >> 4;
            uint32_t tex_coord = current_ctx->tex0.texture_base + pix_u;
            tex_coord += (uint32_t)pix_v * current_ctx->tex0.tex_width;
            if (PRIM.texture_mapping)
                draw_pixel(x, y, local_mem[tex_coord], z, PRIM.alpha_blend);
            else
                draw_pixel(x, y, 0x80000000, z, PRIM.alpha_blend);
        }
    }
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

void GraphicsSynthesizer::host_to_host()
{
    transfer_bit_depth = 32;
    int ppd = 2; //pixels per doubleword
    uint32_t source_addr = BITBLTBUF.source_base + (TRXPOS.source_x + (TRXPOS.source_y * BITBLTBUF.source_width));
    uint32_t max_pixels = TRXREG.width * TRXREG.height;
    printf("\nTRXPOS Source: (%d, %d) Dest: (%d, %d)", TRXPOS.source_x, TRXPOS.source_y, TRXPOS.dest_x, TRXPOS.dest_y);
    printf("\nTRXREG: (%d, %d)", TRXREG.width, TRXREG.height);
    printf("\nBase: $%08X", BITBLTBUF.source_base);
    printf("\nSource addr: $%08X Dest addr: $%08X", source_addr, transfer_addr);
    while (pixels_transferred < max_pixels)
    {
        //printf("\nTransfer from $%08X to $%08X", source_addr, transfer_addr);
        uint64_t borp = *(uint64_t*)&local_mem[source_addr];
        *(uint64_t*)&local_mem[transfer_addr] = borp;
        pixels_transferred += ppd;
        transfer_addr += ppd;
        source_addr += ppd;
    }
    pixels_transferred = 0;
    TRXDIR = 3;
}
