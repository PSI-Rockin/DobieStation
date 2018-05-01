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

float interpolate_f(int32_t x, float u1, int32_t x1, float u2, int32_t x2)
{
    float bark = u1 * (x2 - x);
    bark += u2 * (x - x1);
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
        local_mem = new uint8_t[1024 * 1024 * 4];
    if (!output_buffer)
        output_buffer = new uint32_t[640 * 448];
    is_odd_frame = false;
    pixels_transferred = 0;
    num_vertices = 0;
    frame_count = 0;
    DISPLAY1.x = 0;
    DISPLAY1.y = 0;
    DISPLAY1.width = 0;
    DISPLAY1.height = 0;
    DISPLAY2.x = 0;
    DISPLAY2.y = 0;
    DISPLAY2.width = 0;
    DISPLAY2.height = 0;
    DISPFB1.frame_base = 0;
    DISPFB1.width = 0;
    DISPFB1.x = 0;
    DISPFB1.y = 0;
    DISPFB2.frame_base = 0;
    DISPFB2.width = 0;
    DISPFB2.x = 0;
    DISPFB2.y = 0;
    context1.reset();
    context2.reset();
    PSMCT24_color = 0;
    PSMCT24_unpacked_count = 0;
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
        frame_count++;
        is_odd_frame = !is_odd_frame;
    }
    VBLANK_generated = is_VBLANK;
}

void GraphicsSynthesizer::render_CRT()
{
    printf("DISPLAY2: (%d, %d) wh: (%d, %d)\n", DISPLAY2.x >> 2, DISPLAY2.y, DISPLAY2.width >> 2, DISPLAY2.height);
    int width = DISPLAY2.width >> 2;
    int y = 0;
    if (SMODE2.interlaced && !SMODE2.frame_mode)
        y = is_odd_frame;
    for (y; y < DISPLAY2.height; y++)
    {
        for (int x = 0; x < width; x++)
        {
            int pixel_x = x;
            int pixel_y = y;
            if (pixel_x >= width || pixel_y >= DISPLAY2.height)
                continue;
            uint32_t scaled_x = DISPFB2.x + x;
            uint32_t scaled_y = DISPFB2.y + y;
            scaled_x *= DISPFB2.width;
            scaled_x /= width;
            uint32_t value = read_PSMCT32_block(DISPFB2.frame_base * 4, DISPFB2.width, scaled_x, scaled_y);
            output_buffer[pixel_x + (pixel_y * width)] = value | 0xFF000000;
        }
        if (SMODE2.interlaced && !SMODE2.frame_mode)
            y++;
    }
}

void GraphicsSynthesizer::dump_texture(uint32_t start_addr, uint32_t width)
{
    uint32_t dwidth = DISPLAY2.width >> 2;
    printf("[GS] Dumping texture\n");
    int max_pixels = width * 256 / 2;
    int p = 0;
    while (p < max_pixels)
    {
        int x = p % width;
        int y = p / width;
        uint32_t addr = (x + (y * width));
        for (int i = 0; i < 2; i++)
        {
            uint8_t entry;
            entry = (local_mem[start_addr + (addr / 2)] >> (i * 4)) & 0xF;
            uint32_t value = (entry << 4) | (entry << 12) | (entry << 20);
            output_buffer[x + (y * dwidth)] = value;
            output_buffer[x + (y * dwidth)] |= 0xFF000000;
            x++;
        }
        p += 2;
    }
    /*for (int y = 0; y < 256; y++)
    {
        for (int x = 0; x < width; x++)
        {
            int pixel_x = x;
            int pixel_y = y;
            if (pixel_x >= width || pixel_y >= 256)
                continue;
            uint32_t addr = (pixel_x + (pixel_y * width));
            for (int i = 0; i < 2; i++)
            {
                uint8_t entry;
                entry = (local_mem[start_addr + (addr / 2)] >> (i * 4)) & 0xF;
                uint32_t value = (entry << 4) | (entry << 12) | (entry << 20);
                output_buffer[pixel_x + (pixel_y * dwidth)] = value;
                output_buffer[pixel_x + (pixel_y * dwidth)] |= 0xFF000000;
                pixel_x++;
            }
            x++;
        }
    }*/
    printf("[GS] Done dumping\n");
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

void GraphicsSynthesizer::get_inner_resolution(int &w, int &h)
{
    w = DISPLAY2.width >> 2;
    h = DISPLAY2.height;
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
            reg |= is_odd_frame << 13;
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
            reg |= is_odd_frame << 13;
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
            break;
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
            if (PRIM.use_context2)
                current_ctx = &context2;
            else
                current_ctx = &context1;
            PRIM.fix_fragment_value = value & (1 << 10);
            num_vertices = 0;
            printf("[GS] PRIM: $%08X\n", value);
            break;
        case 0x0001:
        {
            RGBAQ.r = value & 0xFF;
            RGBAQ.g = (value >> 8) & 0xFF;
            RGBAQ.b = (value >> 16) & 0xFF;
            RGBAQ.a = (value >> 24) & 0xFF;

            uint32_t q = value >> 32;
            RGBAQ.q = *(float*)&q;
            printf("[GS] RGBAQ: $%08X_%08X\n", value >> 32, value);
        }
            break;
        case 0x0002:
        {
            //Truncate the lower eight bits of the mantissa
            uint32_t s = value & 0xFFFFFF00;
            ST.s = *(float*)&s;
            uint32_t t = (value >> 32) & 0xFFFFFF00;
            ST.t = *(float*)&t;
        }
            printf("ST: (%f, %f)\n", ST.s, ST.t);
            break;
        case 0x0003:
            UV.u = value & 0x3FFF;
            UV.v = (value >> 16) & 0x3FFF;
            printf("UV: ($%04X, $%04X)\n", UV.u, UV.v);
            break;
        case 0x0004:
            current_vtx.coords[0] = value & 0xFFFF;
            current_vtx.coords[1] = (value >> 16) & 0xFFFF;
            current_vtx.coords[2] = value >> 32;
            vertex_kick(true);
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
        case 0x0008:
            context1.set_clamp(value);
            break;
        case 0x0009:
            context2.set_clamp(value);
            break;
        case 0x000D:
            //XYZ3
            current_vtx.coords[0] = value & 0xFFFF;
            current_vtx.coords[1] = (value >> 16) & 0xFFFF;
            current_vtx.coords[2] = value >> 32;
            vertex_kick(false);
            break;
        case 0x000F:
            break;
        case 0x0016:
            context1.set_tex2(value);
            break;
        case 0x0017:
            context2.set_tex2(value);
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
        case 0x001C:
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
            BITBLTBUF.source_base = (value & 0x3FFF) * 64 * 4;
            BITBLTBUF.source_width = ((value >> 16) & 0x3F) * 64;
            BITBLTBUF.source_format = (value >> 24) & 0x3F;
            BITBLTBUF.dest_base = ((value >> 32) & 0x3FFF) * 64 * 4;
            BITBLTBUF.dest_width = ((value >> 48) & 0x3F) * 64;
            BITBLTBUF.dest_format = (value >> 56) & 0x3F;
            printf("BITBLTBUF: $%08X_%08X\n", value >> 32, value);
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
                printf("Transfer started!\n");
                printf("Dest base: $%08X\n", BITBLTBUF.dest_base);
                printf("TRXPOS: (%d, %d)\n", TRXPOS.dest_x, TRXPOS.dest_y);
                printf("TRXREG (%d, %d)\n", TRXREG.width, TRXREG.height);
                printf("Format: $%02X\n", BITBLTBUF.dest_format);
                printf("Width: %d\n", BITBLTBUF.dest_width);
                TRXPOS.int_dest_x = TRXPOS.dest_x;
                TRXPOS.int_source_x = TRXPOS.int_source_x;
                //printf("Transfer addr: $%08X\n", transfer_addr);
                if (TRXDIR == 2)
                {
                    //VRAM-to-VRAM transfer
                    //More than likely not instantaneous
                    host_to_host();
                    TRXDIR = 3;
                }
            }
            break;
        case 0x0054:
            if (TRXDIR == 0)
                write_HWREG(value);
            break;
        default:
            printf("[GS] Unrecognized write64 to reg $%04X: $%08X_%08X\n", addr, value >> 32, value & 0xFFFFFFFF);
            //exit(1);
    }
}

void GraphicsSynthesizer::set_RGBA(uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    RGBAQ.r = r;
    RGBAQ.g = g;
    RGBAQ.b = b;
    RGBAQ.a = a;
}

void GraphicsSynthesizer::set_UV(uint16_t u, uint16_t v)
{
    UV.u = u;
    UV.v = v;
    printf("UV: ($%04X, $%04X)\n", UV.u, UV.v);
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

uint32_t GraphicsSynthesizer::read_PSMCT32_block(uint32_t base, uint32_t width, uint32_t x, uint32_t y)
{
    const static int blocks[4][8] =
    {
        {0, 1, 4, 5, 16, 17, 20, 21},
        {2, 3, 6, 7, 18, 19, 22, 23},
        {8, 9, 12, 13, 24, 25, 28, 29},
        {10, 11, 14, 15, 26, 27, 30, 31}
    };
    const static int pixels[2][8]
    {
        {0, 1, 4, 5, 8, 9, 12, 13},
        {2, 3, 6, 7, 10, 11, 14, 15}
    };
    uint32_t page = base / (2048 * 4);
    page += (x / 64) % (width / 64);
    page += (y / 32) * (width / 64);
    uint32_t block = (base / 256) % 32;
    block += blocks[(y / 8) % 4][(x / 8) % 8];
    uint32_t column = (y / 2) % 4;
    uint32_t pixel = pixels[y & 0x1][x % 8];
    uint32_t addr = (page * 2048 * 4);
    addr += (block * 256);
    addr += (column * 64);
    addr += (pixel * 4);
    return *(uint32_t*)&local_mem[addr];
}

void GraphicsSynthesizer::write_PSMCT32_block(uint32_t base, uint32_t width, uint32_t x, uint32_t y, uint32_t value)
{
    const static int blocks[4][8] =
    {
        {0, 1, 4, 5, 16, 17, 20, 21},
        {2, 3, 6, 7, 18, 19, 22, 23},
        {8, 9, 12, 13, 24, 25, 28, 29},
        {10, 11, 14, 15, 26, 27, 30, 31}
    };
    const static int pixels[2][8]
    {
        {0, 1, 4, 5, 8, 9, 12, 13},
        {2, 3, 6, 7, 10, 11, 14, 15}
    };
    uint32_t page = base / (2048 * 4);
    page += (x / 64) % (width / 64);
    page += (y / 32) * (width / 64);
    uint32_t block = (base / 256) % 32;
    block += blocks[(y / 8) % 4][(x / 8) % 8];
    uint32_t pixel = pixels[y & 0x1][x % 8];
    uint32_t column = (y / 2) % 4;
    //printf("[GS] Write PSMCT32 (Base: $%08X Width: %d X: %d Y: %d)\n", base, width, x, y);
    //printf("Page: %d Block: %d Column: %d Pixel: %d\n", page, block, column, pixel);

    uint32_t addr = (page * 2048 * 4);
    addr += (block * 256);
    addr += (column * 64);
    addr += (pixel * 4);
    //printf("Write to one-dimensional addr $%08X\n", addr);
    *(uint32_t*)&local_mem[addr] = value;
}

//The "vertex kick" is the name given to the process of placing a vertex in the vertex queue.
//If drawing_kick is true, and enough vertices are available, then the polygon is rendered.
void GraphicsSynthesizer::vertex_kick(bool drawing_kick)
{
    for (int i = num_vertices; i > 0; i--)
        vtx_queue[i] = vtx_queue[i - 1];
    current_vtx.rgbaq = RGBAQ;
    current_vtx.uv = UV;
    current_vtx.s = ST.s;
    current_vtx.t = ST.t;
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
            printf("[GS] Unrecognized primitive %d\n", PRIM.prim_type);
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

void GraphicsSynthesizer::draw_pixel(int32_t x, int32_t y, uint32_t z, RGBAQ_REG& color, bool alpha_blending)
{
    SCISSOR* s = &current_ctx->scissor;
    if (x < s->x1 || x > s->x2 || y < s->y1 || y > s->y2)
        return;
    x >>= 4;
    y >>= 4;
    TEST* test = &current_ctx->test;
    bool update_frame = true;
    bool update_alpha = true;
    bool update_z = !current_ctx->zbuf.no_update;
    if (test->alpha_test)
    {
        bool fail = false;
        switch (test->alpha_method)
        {
            case 0: //NEVER
                fail = true;
                break;
            case 1: //ALWAYS
                break;
            case 2: //LESS
                if (color.a >= test->alpha_ref)
                    fail = true;
                break;
            case 3: //LEQUAL
                if (color.a > test->alpha_ref)
                    fail = true;
                break;
            case 4: //EQUAL
                if (color.a != test->alpha_ref)
                    fail = true;
                break;
            case 5: //GEQUAL
                if (color.a < test->alpha_ref)
                    fail = true;
                break;
            case 6: //GREATER
                if (color.a <= test->alpha_ref)
                    fail = true;
                break;
            case 7: //NOTEQUAL
                if (color.a == test->alpha_ref)
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

    uint32_t pos = (x + (y * current_ctx->frame.width)) * 4;
    uint32_t dest_z = get_word(current_ctx->zbuf.base_pointer + pos);
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

    uint32_t frame_color = read_PSMCT32_block(current_ctx->frame.base_pointer, current_ctx->frame.width, x, y);
    uint32_t final_color = 0;

    if (alpha_blending)
    {
        uint8_t r1, g1, b1;
        uint8_t r2, g2, b2;
        uint8_t cr, cg, cb;
        uint32_t alpha;

        switch (current_ctx->alpha.spec_A)
        {
            case 0:
                r1 = color.r;
                g1 = color.g;
                b1 = color.b;
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
                r2 = color.r;
                g2 = color.g;
                b2 = color.b;
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
                alpha = color.a;
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
                cr = color.r;
                cg = color.g;
                cb = color.b;
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

        final_color |= alpha << 24;
        final_color |= ((((b1 - b2) * alpha) >> 7) + cb) << 16;
        final_color |= ((((g1 - g2) * alpha) >> 7) + cg) << 8;
        final_color |= (((r1 - r2) * alpha) >> 7) + cr;
    }
    else
    {
        final_color |= color.a << 24;
        final_color |= color.b << 16;
        final_color |= color.g << 8;
        final_color |= color.r;
    }
    if (update_frame)
    {
        uint8_t alpha = frame_color >> 24;
        if (update_alpha)
            alpha = final_color >> 24;
        final_color &= 0x00FFFFFF;
        final_color |= alpha << 24;
        uint32_t pos = (x + (y * current_ctx->frame.width)) * 4;
        //if (final_color & 0xFFFFFF)
            //printf("Draw pixel: $%08X ($%08X)\n", final_color, current_ctx->frame.base_pointer + pos);
        write_PSMCT32_block(current_ctx->frame.base_pointer, current_ctx->frame.width, x, y, final_color);
    }
    if (update_z)
        set_word(current_ctx->zbuf.base_pointer + pos, z);
}

void GraphicsSynthesizer::render_point()
{
    printf("[GS] Rendering point!\n");
    uint32_t point[3];
    point[0] = vtx_queue[0].coords[0] - current_ctx->xyoffset.x;
    point[1] = vtx_queue[0].coords[1] - current_ctx->xyoffset.y;
    point[2] = vtx_queue[0].coords[2];
    printf("Coords: (%d, %d, %d)\n", point[0] >> 4, point[1] >> 4, point[2]);
    draw_pixel(point[0], point[1], point[2], vtx_queue[0].rgbaq, PRIM.alpha_blend);
}

void GraphicsSynthesizer::render_line()
{
    printf("[GS] Rendering line!\n");
    int32_t x1, x2, y1, y2;

    int32_t u1, u2, v1, v2;
    uint32_t z1, z2;
    uint8_t r1, g1, b1, a1;
    uint8_t r2, g2, b2, a2;
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
    r1 = vtx_queue[1].rgbaq.r;
    g1 = vtx_queue[1].rgbaq.g;
    b1 = vtx_queue[1].rgbaq.b;
    a1 = vtx_queue[1].rgbaq.a;
    r2 = vtx_queue[0].rgbaq.r;
    g2 = vtx_queue[0].rgbaq.g;
    b2 = vtx_queue[0].rgbaq.b;
    a2 = vtx_queue[0].rgbaq.a;

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
        swap(r1, r2);
        swap(g1, g2);
        swap(b1, b2);
        swap(a1, a2);
    }

    RGBAQ_REG color = vtx_queue[0].rgbaq;

    printf("Coords: (%d, %d, %d) (%d, %d, %d)\n", x1 >> 4, y1 >> 4, z1, x2 >> 4, y2 >> 4, z2);

    for (int32_t x = x1; x < x2; x += 0x10)
    {
        uint32_t z = interpolate(x, z1, x1, z2, x2);
        float t = (x - x1)/(float)(x2 - x1);
        int32_t y = y1*(1.-t) + y2*t;
        uint16_t pix_v = interpolate(y, v1, y1, v2, y2) >> 4;
        uint16_t pix_u = interpolate(x, u1, x1, u2, x2) >> 4;
        uint32_t tex_coord = current_ctx->tex0.texture_base + pix_u;
        tex_coord += (uint32_t)pix_v * current_ctx->tex0.tex_width;
        if (PRIM.gourand_shading)
        {
            color.r = interpolate(x, r1, x1, r2, x2);
            color.g = interpolate(x, g1, x1, g2, x2);
            color.b = interpolate(x, b1, x1, b2, x2);
            color.a = interpolate(x, a1, x1, a2, x2);
        }
        if (is_steep)
            draw_pixel(y, x, z, color, PRIM.alpha_blend);
        else
            draw_pixel(x, y, z, color, PRIM.alpha_blend);
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

    int32_t x1, x2, x3, y1, y2, y3, z1, z2, z3;
    uint8_t r1, r2, r3, g1, g2, g3, b1, b2, b3, a1, a2, a3;
    uint16_t u1, u2, u3, tex_v1, tex_v2, tex_v3;
    float s1, s2, s3, t1, t2, t3;
    x1 = vtx_queue[2].coords[0] - current_ctx->xyoffset.x;
    y1 = vtx_queue[2].coords[1] - current_ctx->xyoffset.y;
    z1 = vtx_queue[2].coords[2];
    x2 = vtx_queue[1].coords[0] - current_ctx->xyoffset.x;
    y2 = vtx_queue[1].coords[1] - current_ctx->xyoffset.y;
    z2 = vtx_queue[1].coords[2];
    x3 = vtx_queue[0].coords[0] - current_ctx->xyoffset.x;
    y3 = vtx_queue[0].coords[1] - current_ctx->xyoffset.y;
    z3 = vtx_queue[0].coords[2];
    r1 = vtx_queue[2].rgbaq.r;
    g1 = vtx_queue[2].rgbaq.g;
    b1 = vtx_queue[2].rgbaq.b;
    a1 = vtx_queue[2].rgbaq.a;
    r2 = vtx_queue[1].rgbaq.r;
    g2 = vtx_queue[1].rgbaq.g;
    b2 = vtx_queue[1].rgbaq.b;
    a2 = vtx_queue[1].rgbaq.a;
    r3 = vtx_queue[0].rgbaq.r;
    g3 = vtx_queue[0].rgbaq.g;
    b3 = vtx_queue[0].rgbaq.b;
    a3 = vtx_queue[0].rgbaq.a;
    u1 = vtx_queue[2].uv.u;
    tex_v1 = vtx_queue[2].uv.v;
    s1 = vtx_queue[2].s;
    t1 = vtx_queue[2].t;
    u2 = vtx_queue[1].uv.u;
    tex_v2 = vtx_queue[1].uv.v;
    s2 = vtx_queue[1].s;
    t2 = vtx_queue[1].t;
    u3 = vtx_queue[0].uv.u;
    tex_v3 = vtx_queue[0].uv.v;
    s3 = vtx_queue[0].s;
    t3 = vtx_queue[0].t;
    Point v1(x1, y1, z1, r1, g1, b1, a1, u1, tex_v1, s1, t1);
    Point v2(x2, y2, z2, r2, g2, b2, a2, u2, tex_v2, s2, t2);
    Point v3(x3, y3, z3, r3, g3, b3, a3, u3, tex_v3, s3, t3);

    //The triangle rasterization code uses an approach with barycentric coordinates
    //Clear explanation can be read below:
    //https://fgiesen.wordpress.com/2013/02/06/the-barycentric-conspirac/

    //Order by counter-clockwise winding order
    if (orient2D(v1, v2, v3) < 0)
        swap(v2, v3);

    int32_t divider = orient2D(v1, v2, v3);
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

    if (!PRIM.gourand_shading)
    {
        //Flatten the colors
        v1.r = v3.r;
        v2.r = v3.r;

        v1.g = v3.g;
        v2.g = v3.g;

        v1.b = v3.b;
        v2.b = v3.b;

        v1.a = v3.a;
        v2.a = v3.a;
    }

    RGBAQ_REG vtx_color, tex_color;

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
                //w1, w2, w3 will always sum up to the double of area of the triangle
                //Interpolate Z
                float z = (float) v1.z * w1 + (float) v2.z * w2 + (float) v3.z * w3;
                z /= divider;

                //Gourand shading calculations
                float r = (float) v1.r * w1 + (float) v2.r * w2 + (float) v3.r * w3;
                float g = (float) v1.g * w1 + (float) v2.g * w2 + (float) v3.g * w3;
                float b = (float) v1.b * w1 + (float) v2.b * w2 + (float) v3.b * w3;
                float a = (float) v1.a * w1 + (float) v2.a * w2 + (float) v3.a * w3;
                vtx_color.r = r / divider;
                vtx_color.g = g / divider;
                vtx_color.b = b / divider;
                vtx_color.a = a / divider;

                if (PRIM.texture_mapping)
                {
                    uint32_t u, v;
                    if (!PRIM.use_UV)
                    {
                        float s, t;
                        s = v1.s * w1 + v2.s * w2 + v3.s * w3;
                        t = v1.t * w1 + v2.t * w2 + v3.t * w3;
                        s /= divider;
                        t /= divider;
                        u = s * current_ctx->tex0.tex_width;
                        v = t * current_ctx->tex0.tex_height;
                    }
                    else
                    {
                        float temp_u = (float) v1.u * w1 + (float) v2.u * w2 + (float) v3.u * w3;
                        float temp_v = (float) v1.v * w1 + (float) v2.v * w2 + (float) v3.v * w3;
                        temp_u /= divider;
                        temp_v /= divider;
                        u = (uint32_t)temp_u >> 4;
                        v = (uint32_t)temp_v >> 4;
                    }
                    tex_lookup(u, v, vtx_color, tex_color);
                    draw_pixel(x, y, (uint32_t)z, tex_color, PRIM.alpha_blend);
                }
                else
                {
                    draw_pixel(x, y, (uint32_t)z, vtx_color, PRIM.alpha_blend);
                }
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
    printf("[GS] Rendering sprite!\n");
    int32_t x1, x2, y1, y2;
    int32_t u1, u2, v1, v2;
    uint8_t r1, r2, r3, g1, g2, g3, b1, b2, b3, a1, a2, a3;
    float s1, s2, t1, t2, q1, q2;
    x1 = vtx_queue[1].coords[0] - current_ctx->xyoffset.x;
    x2 = vtx_queue[0].coords[0] - current_ctx->xyoffset.x;
    y1 = vtx_queue[1].coords[1] - current_ctx->xyoffset.y;
    y2 = vtx_queue[0].coords[1] - current_ctx->xyoffset.y;
    u1 = vtx_queue[1].uv.u;
    u2 = vtx_queue[0].uv.u;
    v1 = vtx_queue[1].uv.v;
    v2 = vtx_queue[0].uv.v;
    s1 = vtx_queue[1].s;
    s2 = vtx_queue[0].s;
    t1 = vtx_queue[1].t;
    t2 = vtx_queue[0].t;
    q1 = vtx_queue[1].rgbaq.q;
    q2 = vtx_queue[0].rgbaq.q;
    uint32_t z = vtx_queue[0].coords[2];

    RGBAQ_REG vtx_color, tex_color;
    vtx_color = vtx_queue[0].rgbaq;

    if (x1 > x2)
    {
        swap(x1, x2);
        swap(y1, y2);
        swap(u1, u2);
        swap(v1, v2);
        swap(s1, s2);
        swap(t1, t2);
    }

    printf("Coords: (%d, %d) (%d, %d)\n", x1 >> 4, y1 >> 4, x2 >> 4, y2 >> 4);

    for (int32_t y = y1; y < y2; y += 0x10)
    {
        float pix_t = interpolate_f(y, t1, y1, t2, y2);
        uint16_t pix_v = interpolate(y, v1, y1, v2, y2) >> 4;
        for (int32_t x = x1; x < x2; x += 0x10)
        {
            float pix_s = interpolate_f(x, s1, x1, s2, x2);
            uint16_t pix_u = interpolate(x, u1, x1, u2, x2) >> 4;
            if (PRIM.texture_mapping)
            {
                if (!PRIM.use_UV)
                {
                    pix_v = pix_t * current_ctx->tex0.tex_height;
                    pix_u = pix_s * current_ctx->tex0.tex_width;
                }
                tex_lookup(pix_u, pix_v, vtx_color, tex_color);
                draw_pixel(x, y, z, tex_color, PRIM.alpha_blend);
            }
            else
            {
                draw_pixel(x, y, z, vtx_color, PRIM.alpha_blend);
            }
        }
    }
}

void GraphicsSynthesizer::write_HWREG(uint64_t data)
{
    int ppd; //pixels per doubleword (64-bits)

    switch (BITBLTBUF.dest_format)
    {
        //PSMCT32
        case 0x00:
            ppd = 2;
            break;
        //PSMCT24
        case 0x01:
            ppd = 3;
            break;
        //PSMCT16
        case 0x02:
            ppd = 4;
            break;
        //PSMCT8
        case 0x13:
            ppd = 8;
            break;
        //PSMCT4
        case 0x14:
            ppd = 16;
            break;
        //PSMT4HL
        case 0x24:
            ppd = 2;
            break;
        //PSMT4HH
        case 0x2C:
            ppd = 2;
            break;
        default:
            printf("[GS] Unrecognized BITBLTBUF dest format $%02X\n", BITBLTBUF.dest_format);
            exit(1);
    }

    for (int i = 0; i < ppd; i++)
    {
        uint32_t dest_addr = TRXPOS.int_dest_x + (TRXPOS.dest_y * BITBLTBUF.dest_width);
        switch (BITBLTBUF.dest_format)
        {
            case 0x00:
            case 0x24:
            case 0x2C:
                write_PSMCT32_block(BITBLTBUF.dest_base, BITBLTBUF.dest_width, TRXPOS.int_dest_x, TRXPOS.dest_y, (data >> (i * 32)) & 0xFFFFFFFF);
                //*(uint32_t*)&local_mem[BITBLTBUF.dest_base + (dest_addr * 4)] = (data >> (i * 32)) & 0xFFFFFFFF;
                printf("[GS] Write to $%08X of ", BITBLTBUF.dest_base + (dest_addr * 4));
                printf("$%08X\n", (data >> (i * 32)) & 0xFFFFFFFF);
                pixels_transferred++;
                TRXPOS.int_dest_x++;
                break;
            case 0x01:
                unpack_PSMCT24(BITBLTBUF.dest_base + (dest_addr * 4), data, i);
                break;
            case 0x02:
                *(uint16_t*)&local_mem[BITBLTBUF.dest_base + (dest_addr * 2)] = (data >> (i * 16)) & 0xFFFF;
                pixels_transferred++;
                TRXPOS.int_dest_x++;
                break;
            case 0x13:
                dest_addr += BITBLTBUF.dest_base;
                local_mem[dest_addr] = (data >> (i * 8)) & 0xFF;
                //printf("[GS] Write to $%08X\n", dest_addr);
                pixels_transferred++;
                TRXPOS.int_dest_x++;
                break;
            case 0x14:
                dest_addr /= 2;
                dest_addr += BITBLTBUF.dest_base;
                if (i & 0x1)
                {
                    local_mem[dest_addr] &= ~0xF0;
                    local_mem[dest_addr] |= (data >> ((i >> 1) << 3)) & 0xF0;
                    //printf("[GS] Write to $%08X:1: $%02X\n", dest_addr, local_mem[dest_addr]);
                }
                else
                {
                    local_mem[dest_addr] &= ~0xF;
                    local_mem[dest_addr] |= (data >> ((i >> 1) << 3)) & 0xF;
                    //printf("[GS] Write to $%08X:0: $%02X\n", dest_addr, local_mem[dest_addr]);
                }
                pixels_transferred++;
                TRXPOS.int_dest_x++;
                break;
        }
        if (TRXPOS.int_dest_x - TRXPOS.dest_x == TRXREG.width)
        {
            TRXPOS.int_dest_x = TRXPOS.dest_x;
            TRXPOS.dest_y++;
        }
    }

    uint32_t max_pixels = TRXREG.width * TRXREG.height;
    if (pixels_transferred >= max_pixels)
    {
        //Deactivate the transmisssion
        printf("[GS] HWREG transfer ended\n");
        TRXDIR = 3;
        pixels_transferred = 0;
    }
}

void GraphicsSynthesizer::unpack_PSMCT24(uint32_t dest_addr, uint64_t data, int offset)
{
    int bytes_unpacked = 0;
    for (int i = offset * 24; bytes_unpacked < 3 && i < 64; i += 8)
    {
        PSMCT24_color |= ((data >> i) & 0xFF) << (PSMCT24_unpacked_count * 8);
        PSMCT24_unpacked_count++;
        bytes_unpacked++;
        if (PSMCT24_unpacked_count == 3)
        {
            write_PSMCT32_block(BITBLTBUF.dest_base, BITBLTBUF.dest_width, TRXPOS.int_dest_x, TRXPOS.dest_y, PSMCT24_color);
            PSMCT24_color = 0;
            PSMCT24_unpacked_count = 0;
            TRXPOS.int_dest_x++;
            pixels_transferred++;
        }
    }
}

void GraphicsSynthesizer::host_to_host()
{
    exit(1);
    int ppd = 2; //pixels per doubleword
    uint32_t source_addr = (TRXPOS.source_x + (TRXPOS.source_y * BITBLTBUF.source_width)) << 1;
    source_addr += BITBLTBUF.source_base;
    uint32_t max_pixels = TRXREG.width * TRXREG.height;
    printf("TRXPOS Source: (%d, %d) Dest: (%d, %d)\n", TRXPOS.source_x, TRXPOS.source_y, TRXPOS.dest_x, TRXPOS.dest_y);
    printf("TRXREG: (%d, %d)\n", TRXREG.width, TRXREG.height);
    printf("Base: $%08X\n", BITBLTBUF.source_base);
    /*printf("Source addr: $%08X Dest addr: $%08X\n", source_addr, transfer_addr);
    while (pixels_transferred < max_pixels)
    {
        printf("Host-host transfer from $%08X to $%08X\n", source_addr, transfer_addr);
        uint64_t borp = *(uint64_t*)&local_mem[source_addr];
        *(uint64_t*)&local_mem[transfer_addr] = borp;
        pixels_transferred += ppd;
        transfer_addr += 8;
        source_addr += 8;
    }*/
    pixels_transferred = 0;
    TRXDIR = 3;
}

void GraphicsSynthesizer::tex_lookup(uint16_t u, uint16_t v, RGBAQ_REG& vtx_color, RGBAQ_REG& tex_color)
{
    switch (current_ctx->clamp.wrap_s)
    {
        case 0:
            u %= current_ctx->tex0.tex_width;
            break;
        case 1:
            if (u > current_ctx->tex0.tex_width)
                u = current_ctx->tex0.tex_width;
            break;
    }
    switch (current_ctx->clamp.wrap_t)
    {
        case 0:
            v %= current_ctx->tex0.tex_height;
            break;
        case 1:
            if (v > current_ctx->tex0.tex_height)
                v = current_ctx->tex0.tex_height;
            break;
    }

    uint32_t coord = u + (v * current_ctx->tex0.width);
    uint32_t tex_base = current_ctx->tex0.texture_base;
    uint32_t clut_base = current_ctx->tex0.CLUT_base;
    switch (current_ctx->tex0.format)
    {
        case 0x00:
        {
            uint32_t color = read_PSMCT32_block(tex_base, current_ctx->tex0.width, u, v);
            tex_color.r = color & 0xFF;
            tex_color.g = (color >> 8) & 0xFF;
            tex_color.b = (color >> 16) & 0xFF;
            tex_color.a = color >> 24;
        }
            break;
        case 0x01:
        {
            uint32_t color = read_PSMCT32_block(tex_base, current_ctx->tex0.width, u, v);
            tex_color.r = color & 0xFF;
            tex_color.g = (color >> 8) & 0xFF;
            tex_color.b = (color >> 16) & 0xFF;
            tex_color.a = 0x80;
        }
            break;
        case 0x02:
        {
            uint16_t color = local_mem[tex_base + (coord << 1)];
            tex_color.r = (color & 0x1F) << 3;
            tex_color.g = ((color >> 5) & 0x1F) << 11;
            tex_color.b = ((color >> 10) & 0x1F) << 19;
            tex_color.a = ((color & (1 << 15)) != 0) << 7;
        }
            break;
        case 0x13:
        {
            uint8_t entry;
            entry = local_mem[tex_base + coord];
            uint32_t color;
            if (current_ctx->tex0.use_CSM2)
                color = 0;
            else
            {
                int x = (entry & 0x7);
                x += (entry & 0x10) / 2;
                int y = (entry & 0x8) != 0;
                color = read_PSMCT32_block(current_ctx->tex0.CLUT_base, 64, x, y);
            }
            tex_color.r = 0xFF;
            tex_color.g = 0xFF;
            tex_color.b = 0xFF;
            tex_color.a = 0x80;
            //return get_word(addr);
        }
            break;
        case 0x14:
        {
            uint8_t entry;
            if (coord & 0x1)
                entry = local_mem[tex_base + (coord >> 1)] >> 4;
            else
                entry = local_mem[tex_base + (coord >> 1)] & 0xF;
            //printf("[GS] 4-bit entry: %d\n", entry);
            //return 0x80000000 | (entry << 4) | (entry << 12) | (entry << 20);
            /*printf("[GS] CLUT base: $%08X\n", clut_base);
            printf("Coord: $%08X\n", coord >> 1);
            printf("Entry: %d\n", entry);
            printf("Offset: %d\n", current_ctx->tex0.CLUT_offset);
            printf("Format: %d\n", current_ctx->tex0.CLUT_format);*/
            uint32_t color = 0;
            if (current_ctx->tex0.use_CSM2)
                color = 0;
            else
            {
                color = read_PSMCT32_block(current_ctx->tex0.CLUT_base, 64, entry & 0x7, entry / 8);
            }
            //printf("[GS] Read 4-bit tex entry: %d\n", entry);
            //printf("[GS] Color: $%08X\n", color);
            tex_color.r = color & 0xFF;
            tex_color.g = (color >> 8) & 0xFF;
            tex_color.b = (color >> 16) & 0xFF;
            tex_color.a = color >> 24;
            //printf("CLUT addr: $%08X\n", addr);
            //printf("CLUT color: $%08X\n", color);
        }
            break;
        case 0x24:
        {
            printf("[GS] Format $24: Read from $%08X\n", tex_base + (coord << 2));
            uint8_t entry = (read_PSMCT32_block(tex_base, current_ctx->tex0.width, u, v) >> 24) & 0xF;
            printf("[GS] Format $24: Read entry %d\n", entry);
            entry = 0xF;
            tex_color.r = entry << 4;
            tex_color.g = entry << 4;
            tex_color.b = entry << 4;
            tex_color.a = 0x80;
            break;
            /*uint32_t addr;
            if (current_ctx->tex0.use_CSM2)
                addr = clut_base + (entry << 2);
            else
            {
                addr = clut_base + ((entry & 0x7) << 2);
                addr += (entry & 0x8) * 32; //increase distance by 64 words (256 bytes) if entry > 7
            }
            return get_word(addr);*/
        }
            break;
        case 0x2C:
        {
            uint8_t entry = read_PSMCT32_block(tex_base, current_ctx->tex0.width, u, v) >> 28;
            tex_color.r = entry << 4;
            tex_color.g = entry << 4;
            tex_color.b = entry << 4;
            tex_color.a = 0x80;
            break;
            /*uint32_t addr;
            if (current_ctx->tex0.use_CSM2)
                addr = clut_base + (entry << 2);
            else
            {
                addr = clut_base + ((entry & 0x7) << 2);
                addr += (entry & 0x8) * 32; //increase distance by 64 words (256 bytes) if entry > 7
            }
            return get_word(addr);*/
        }
            break;
        default:
            printf("[GS] Unrecognized texture format $%02X\n", current_ctx->tex0.format);
            exit(1);
    }

    switch (current_ctx->tex0.color_function)
    {
        //Modulate
        case 0:
            tex_color.r = ((uint16_t)tex_color.r * vtx_color.r) >> 7;
            tex_color.g = ((uint16_t)tex_color.g * vtx_color.g) >> 7;
            tex_color.b = ((uint16_t)tex_color.b * vtx_color.b) >> 7;
            tex_color.a = ((uint16_t)tex_color.a * vtx_color.a) >> 7;
            break;
        //Decal
        case 1:
            break;
    }
}
