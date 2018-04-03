#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "ee/intc.hpp"

#include "gs.hpp"
using namespace std;

/**
  * ~ GS notes ~
  * PRIM.prim_type:
  * 0 - Point
  * 1 - Line
  * 2 - Line strip
  * 3 - Triangle
  * 4 - Triangle strip
  * 5 - Triangle fan
  * 6 - Sprite
  * 7 - Prohibited
  *
  * Color formats:
  * PSMCT32 - RGBA32
  * PSMCT24 - RGB24 (upper 8 bits unused)
  * PSMCT16 - RGBA16 * 2 pixels
  **/

template <typename T>
T interpolate(int32_t x, T u1, int32_t x1, T u2, int32_t x2)
{
    int64_t bark = (int64_t)u1 * (x2 - x);
    bark += (int64_t)u2 * (x - x1);
    if (!(x2 - x1))
        return u1;
    return bark / (x2 - x1);
}

const unsigned int GraphicsSynthesizer::max_vertices[8] = {1, 2, 2, 3, 3, 3, 2, 0};

GraphicsSynthesizer::GraphicsSynthesizer(INTC* intc) : intc(intc)
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
        output_buffer = new uint32_t[640 * 448];
    pixels_transferred = 0;
    transfer_bit_depth = 32;
    num_vertices = 0;
    DISPLAY2.x = 0;
    DISPLAY2.y = 0;
    DISPLAY2.width = 0 << 2;
    DISPLAY2.height = 0;
    DISPFB2.frame_base = 0;
    DISPFB2.width = 0;
    DISPFB2.x = 0;
    DISPFB2.y = 0;
    context1.reset();
    context2.reset();
    current_ctx = &context1;
    VBLANK_enabled = false;
    VBLANK_generated = false;
    set_CRT(false, 0x2, false);
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

void GraphicsSynthesizer::set_VBLANK(bool is_VBLANK)
{
    if (is_VBLANK)
    {
        printf("[GS] VBLANK start\n");
        intc->assert_IRQ(2);
    }
    else
    {
        printf("[GS] VBLANK end\n");
        intc->assert_IRQ(3);
    }
    VBLANK_generated = is_VBLANK;
}

void GraphicsSynthesizer::render_CRT()
{
    printf("DISPLAY2: (%d, %d) wh: (%d, %d)\n", DISPLAY2.x >> 2, DISPLAY2.y, DISPLAY2.width >> 2, DISPLAY2.height);
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
            uint32_t value = local_mem[DISPFB2.frame_base + scaled_x + (scaled_y * DISPFB2.width)];
            output_buffer[pixel_x + (pixel_y * width)] = value;
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
        {
            uint32_t reg = 0;
            reg |= VBLANK_generated << 3;
            return reg;
        }
        default:
            printf("[GS] Unrecognized privileged read32 from $%04X\n", addr);
            return 0;
    }
}

uint64_t GraphicsSynthesizer::read64_privileged(uint32_t addr)
{
    addr &= 0xFFFF;
    switch (addr)
    {
        case 0x1000:
        {
            uint64_t reg = 0;
            reg |= VBLANK_generated << 3;
            return reg;
        }
        default:
            printf("[GS] Unrecognized privileged read64 from $%04X\n", addr);
            return 0;
    }
}

void GraphicsSynthesizer::write32_privileged(uint32_t addr, uint32_t value)
{
    addr &= 0xFFFF;
    switch (addr)
    {
        case 0x0070:
            printf("[GS] Write DISPFB1: $%08X\n", value);
            DISPFB1.frame_base = (value & 0x3FF) * 2048;
            DISPFB1.width = ((value >> 9) & 0x3F) * 64;
            DISPFB1.format = (value >> 14) & 0x1F;
            break;
        case 0x1000:
            printf("[GS] Write32 to GS_CSR: $%08X\n", value);
            if (value & 0x8)
            {
                VBLANK_enabled = true;
                VBLANK_generated = false;
            }
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
            printf("[GS] Write PMODE: $%08X_%08X\n", value >> 32, value & 0xFFFFFFFF);
            PMODE.circuit1 = value & 0x1;
            PMODE.circuit2 = value & 0x2;
            PMODE.output_switching = (value >> 2) & 0x7;
            PMODE.use_ALP = value & (1 << 5);
            PMODE.out1_circuit2 = value & (1 << 6);
            PMODE.blend_with_bg = value & (1 << 7);
            PMODE.ALP = (value >> 8) & 0xFF;
            break;
        case 0x0020:
            printf("[GS] Write SMODE2: $%08X_%08X\n", value >> 32, value & 0xFFFFFFFF);
            SMODE2.interlaced = value & 0x1;
            SMODE2.frame_mode = value & 0x2;
            SMODE2.power_mode = (value >> 2) & 0x3;
            break;
        case 0x0070:
            printf("[GS] Write DISPFB1: $%08X_%08X\n", value >> 32, value & 0xFFFFFFFF);
            DISPFB1.frame_base = (value & 0x3FF) * 2048;
            DISPFB1.width = ((value >> 9) & 0x3F) * 64;
            DISPFB1.format = (value >> 14) & 0x1F;
            DISPFB1.x = (value >> 32) & 0x7FF;
            DISPFB1.y = (value >> 43) & 0x7FF;
            break;
        case 0x0080:
            printf("[GS] Write DISPLAY1: $%08X_%08X\n", value >> 32, value & 0xFFFFFFFF);
            DISPLAY1.x = value & 0xFFF;
            DISPLAY1.y = (value >> 12) & 0x7FF;
            DISPLAY1.magnify_x = (value >> 23) & 0xF;
            DISPLAY1.magnify_y = (value >> 27) & 0x3;
            DISPLAY1.width = ((value >> 32) & 0xFFF) + 1;
            DISPLAY1.height = ((value >> 44) & 0x7FF) + 1;
        case 0x0090:
            printf("[GS] Write DISPFB2: $%08X_%08X\n", value >> 32, value & 0xFFFFFFFF);
            DISPFB2.frame_base = (value & 0x3FF) * 2048;
            DISPFB2.width = ((value >> 9) & 0x3F) * 64;
            DISPFB2.format = (value >> 14) & 0x1F;
            DISPFB2.x = (value >> 32) & 0x7FF;
            DISPFB2.y = (value >> 43) & 0x7FF;
            break;
        case 0x00A0:
            printf("[GS] Write DISPLAY2: $%08X_%08X\n", value >> 32, value & 0xFFFFFFFF);
            DISPLAY2.x = value & 0xFFF;
            DISPLAY2.y = (value >> 12) & 0x7FF;
            DISPLAY2.magnify_x = (value >> 23) & 0xF;
            DISPLAY2.magnify_y = (value >> 27) & 0x3;
            DISPLAY2.width = ((value >> 32) & 0xFFF) + 1;
            DISPLAY2.height = ((value >> 44) & 0x7FF) + 1;
            break;
        case 0x1000:
            printf("[GS] Write64 to GS_CSR: $%08X_%08X\n", value >> 32, value & 0xFFFFFFFF);
            if (value & 0x8)
            {
                VBLANK_enabled = true;
                VBLANK_generated = false;
            }
            break;
        default:
            printf("[GS] Unrecognized privileged write64 to reg $%04X: $%08X_%08X\n", addr, value >> 32, value & 0xFFFFFFFF);
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
            printf("UV: ($%04X, $%04X)\n", UV.u, UV.v);
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
        case 0x0007:
            context2.set_tex0(value);
            break;
        case 0x000D:
            //XYZ3
            current_vtx.coords[0] = value & 0xFFFF;
            current_vtx.coords[1] = (value >> 16) & 0xFFFF;
            current_vtx.coords[2] = value >> 32;
            vertex_kick(false);
            break;
        case 0x0018:
            context1.set_xyoffset(value);
            break;
        case 0x0019:
            context2.set_xyoffset(value);
            break;
        case 0x001A:
            use_PRIM = value & 0x1;
            break;
        case 0x003F:
            printf("TEXFLUSH\n");
            break;
        case 0x0040:
            context1.set_scissor(value);
            break;
        case 0x0041:
            context2.set_scissor(value);
            break;
        case 0x0042:
            context1.set_alpha(value);
            break;
        case 0x0043:
            context2.set_alpha(value);
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
        case 0x0048:
            context2.set_test(value);
            break;
        case 0x004C:
            context1.set_frame(value);
            break;
        case 0x004D:
            context2.set_frame(value);
            break;
        case 0x004E:
            context1.set_zbuf(value);
            break;
        case 0x004F:
            context2.set_zbuf(value);
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
            printf("TRXPOS: $%08X_%08X\n", value >> 32, value);
            break;
        case 0x0052:
            TRXREG.width = value & 0xFFF;
            TRXREG.height = (value >> 32) & 0xFFF;
            printf("TRXREG (%d, %d)\n", TRXREG.width, TRXREG.height);
            break;
        case 0x0053:
            TRXDIR = value & 0x3;
            //Start transmission
            if (TRXDIR != 3)
            {
                pixels_transferred = 0;
                transfer_addr = BITBLTBUF.dest_base + (TRXPOS.dest_x + (TRXPOS.dest_y * BITBLTBUF.dest_width));
                printf("Transfer started!\n");
                printf("Dest base: $%08X\n", BITBLTBUF.dest_base);
                printf("TRXPOS: (%d, %d)\n", TRXPOS.dest_x, TRXPOS.dest_y);
                printf("Width: %d\n", BITBLTBUF.dest_width);
                printf("Transfer addr: $%08X\n", transfer_addr);
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
            exit(1);
    }
}

void GraphicsSynthesizer::set_RGBA(uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    RGBAQ.r = r;
    RGBAQ.g = g;
    RGBAQ.b = b;
    RGBAQ.a = a;
}

void GraphicsSynthesizer::set_Q(float q)
{
    RGBAQ.q = q;
}

void GraphicsSynthesizer::set_XYZ(uint32_t x, uint32_t y, uint32_t z, bool drawing_kick)
{
    current_vtx.coords[0] = x;
    current_vtx.coords[1] = y;
    current_vtx.coords[2] = z;
    vertex_kick(drawing_kick);
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
        case 4:
            if (num_vertices == 3)
            {
                num_vertices--;
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

void GraphicsSynthesizer::draw_pixel(int32_t x, int32_t y, uint32_t color, uint32_t z, bool alpha_blending)
{
    SCISSOR* s = &current_ctx->scissor;
    if (x < s->x1 || x > s->x2 || y < s->y1 || y > s->y2)
        return;
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

    uint32_t color = 0;
    color |= vtx_queue[0].rgbaq.a << 24;
    color |= vtx_queue[0].rgbaq.r << 16;
    color |= vtx_queue[0].rgbaq.g << 8;
    color |= vtx_queue[0].rgbaq.b;
    printf("\nCoords: (%d, %d, %d)", point[0] >> 4, point[1] >> 4, point[2]);
    draw_pixel(point[0], point[1], color, point[2], PRIM.alpha_blend);
}

void GraphicsSynthesizer::render_line()
{
    printf("\n[GS] Rendering line!");
    int32_t x1, x2, y1, y2;
    int32_t u1, u2, v1, v2;
    uint32_t z1, z2;
    x1 = vtx_queue[1].coords[0] - current_ctx->xyoffset.x;
    x2 = vtx_queue[0].coords[0] - current_ctx->xyoffset.x;
    y1 = vtx_queue[1].coords[1] - current_ctx->xyoffset.y;
    y2 = vtx_queue[0].coords[1] - current_ctx->xyoffset.y;
    z1 = vtx_queue[1].coords[2];
    z2 = vtx_queue[0].coords[2];
    u1 = vtx_queue[1].uv.u;
    u2 = vtx_queue[0].uv.u;
    v1 = vtx_queue[1].uv.v;
    v2 = vtx_queue[0].uv.v;

    uint32_t color = 0;
    color |= vtx_queue[0].rgbaq.r;
    color |= vtx_queue[0].rgbaq.g << 8;
    color |= vtx_queue[0].rgbaq.b << 16;
    color |= vtx_queue[0].rgbaq.a << 24;

    //Transpose line if it's steep
    bool is_steep = false;
    if (abs(x2 - x1) < abs(y2 - y1))
    {
        swap(x1, y1);
        swap(x2, y2);
        is_steep = true;
    }

    //Make line left-to-right
    if (x1 > x2)
    {
        swap(x1, x2);
        swap(y1, y2);
        swap(z1, z2);
        swap(u1, u2);
        swap(v1, v2);
    }

    printf("\nCoords: (%d, %d, %d) (%d, %d, %d)", x1 >> 4, y1 >> 4, z1, x2 >> 4, y2 >> 4, z2);

    for (int32_t x = x1; x < x2; x += 0x10)
    {
        uint32_t z = interpolate(x, z1, x1, z2, x2);
        float t = (x - x1)/(float)(x2 - x1);
        int32_t y = y1*(1.-t) + y2*t;
        uint16_t pix_v = interpolate(y, v1, y1, v2, y2) >> 4;
        uint16_t pix_u = interpolate(x, u1, x1, u2, x2) >> 4;
        uint32_t tex_coord = current_ctx->tex0.texture_base + pix_u;
        tex_coord += (uint32_t)pix_v * current_ctx->tex0.tex_width;
        if (is_steep)
            draw_pixel(y, x, color, z, PRIM.alpha_blend);
        else
            draw_pixel(x, y, color, z, PRIM.alpha_blend);
    }
}


// Returns positive value if the points are in counter-clockwise order
// 0 if they it's on the same line
// Negative value if they are in a clockwise order
// Basicly the cross product of (v2 - v1) and (v3 - v1) vectors
int32_t GraphicsSynthesizer::orient2D(const Point &v1, const Point &v2, const Point &v3)
{
    return (v2.x - v1.x) * (v3.y - v1.y) - (v3.x - v1.x) * (v2.y - v1.y);
}

void GraphicsSynthesizer::render_triangle()
{
    printf("[GS] Rendering triangle!\n");
    uint32_t color = 0x00000000;
    color |= vtx_queue[0].rgbaq.r;
    color |= vtx_queue[0].rgbaq.g << 8;
    color |= vtx_queue[0].rgbaq.b << 16;
    color |= vtx_queue[0].rgbaq.a << 24;

    int32_t x1, x2, x3, y1, y2, y3, z1, z2, z3;
    x1 = vtx_queue[2].coords[0] - current_ctx->xyoffset.x;
    y1 = vtx_queue[2].coords[1] - current_ctx->xyoffset.y;
    z1 = vtx_queue[2].coords[2];
    x2 = vtx_queue[1].coords[0] - current_ctx->xyoffset.x;
    y2 = vtx_queue[1].coords[1] - current_ctx->xyoffset.y;
    z2 = vtx_queue[1].coords[2];
    x3 = vtx_queue[0].coords[0] - current_ctx->xyoffset.x;
    y3 = vtx_queue[0].coords[1] - current_ctx->xyoffset.y;
    z3 = vtx_queue[0].coords[2];
    Point v1(x1, y1, z1);
    Point v2(x2, y2, z2);
    Point v3(x3, y3, z3);

    //The triangle rasterization code uses an approach with barycentric coordinates
    //Clear explanation can be read below:
    //https://fgiesen.wordpress.com/2013/02/06/the-barycentric-conspirac/

    //Order by counter-clockwise winding order
    if (orient2D(v1, v2, v3) < 0)
        swap(v2, v3);

    //Calculate bounding box of triangle
    int32_t min_x = min({v1.x, v2.x, v3.x});
    int32_t min_y = min({v1.y, v2.y, v3.y});
    int32_t max_x = max({v1.x, v2.x, v3.x});
    int32_t max_y = max({v1.y, v2.y, v3.y});

    //Calculate incremental steps for the weights
    //Reference: https://fgiesen.wordpress.com/2013/02/10/optimizing-the-basic-rasterizer/
    int32_t A12 = v1.y - v2.y;
    int32_t B12 = v2.x - v1.x;
    int32_t A23 = v2.y - v3.y;
    int32_t B23 = v3.x - v2.x;
    int32_t A31 = v3.y - v1.y;
    int32_t B31 = v1.x - v3.x;

    Point min_corner(min_x, min_y);
    int32_t w1_row = orient2D(v2, v3, min_corner);
    int32_t w2_row = orient2D(v3, v1, min_corner);
    int32_t w3_row = orient2D(v1, v2, min_corner);

    //TODO: Parallelize this
    //Iterate through pixels in bounds
    for (int32_t y = min_y; y <= max_y; y++)
    {
        int32_t w1 = w1_row;
        int32_t w2 = w2_row;
        int32_t w3 = w3_row;
        for (int32_t x = min_x; x <= max_x; x++)
        {
            //Is inside triangle?
            if ((w1 | w2 | w3) >= 0)
            {
                //Interpolate Z
                float z = (float) v1.z * w1 + (float) v2.z * w2 + (float) v3.z * w3;
                z /= (float) (w1 + w2 + w3);
                draw_pixel(x, y, color, (int32_t) z, PRIM.alpha_blend);
            }
            //Horizontal step
            w1 += A23;
            w2 += A31;
            w3 += A12;
        }
        //Vertical step
        w1_row += B23;
        w2_row += B31;
        w3_row += B12;
    }
}

void GraphicsSynthesizer::render_sprite()
{
    printf("\n[GS] Rendering sprite!");
    int32_t x1, x2, y1, y2;
    int32_t u1, u2, v1, v2;
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
        uint16_t pix_v = interpolate(y, v1, y1, v2, y2) >> 4;
        for (int32_t x = x1; x < x2; x += 0x10)
        {
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
