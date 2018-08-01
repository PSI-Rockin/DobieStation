#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <fstream>

#include "gsthread.hpp"
#include "gsmem.hpp"
#include "errors.hpp"

using namespace std;

//Swizzling tables - we declare these outside the class to prevent a stack overflow
//Globals are allocated in a different memory segment of the executable
static uint32_t page_PSMCT32[32][32][64];
static uint32_t page_PSMCT32Z[32][32][64];
static uint32_t page_PSMCT16[32][64][64];
static uint32_t page_PSMCT16S[32][64][64];
static uint32_t page_PSMCT16Z[32][64][64];
static uint32_t page_PSMCT16SZ[32][64][64];
static uint32_t page_PSMCT8[32][64][128];
static uint32_t page_PSMCT4[32][128][128];

#define printf(fmt, ...)(0)

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

const unsigned int GraphicsSynthesizerThread::max_vertices[8] = {1, 2, 2, 3, 3, 3, 2, 0};

GraphicsSynthesizerThread::GraphicsSynthesizerThread()
{
    frame_complete = false;
    local_mem = nullptr;

    //Initialize swizzling tables
    for (int block = 0; block < 32; block++)
    {
        for (int y = 0; y < 32; y++)
        {
            for (int x = 0; x < 64; x++)
            {
                uint32_t column = columnTable32[y & 0x7][x & 0x7];
                page_PSMCT32[block][y][x] = (blockid_PSMCT32(block, 0, x, y) << 6) + column;
                page_PSMCT32Z[block][y][x] = (blockid_PSMCT32Z(block, 0, x, y) << 6) + column;
            }
        }

        for (int y = 0; y < 64; y++)
        {
            for (int x = 0; x < 64; x++)
            {
                uint32_t column = columnTable16[y & 0x7][x & 0xF];
                page_PSMCT16[block][y][x] = (blockid_PSMCT16(block, 0, x, y) << 7) + column;
                page_PSMCT16S[block][y][x] = (blockid_PSMCT16S(block, 0, x, y) << 7) + column;
                page_PSMCT16Z[block][y][x] = (blockid_PSMCT16Z(block, 0, x, y) << 7) + column;
                page_PSMCT16SZ[block][y][x] = (blockid_PSMCT16SZ(block, 0, x, y) << 7) + column;
            }
        }

        for (int y = 0; y < 64; y++)
        {
            for (int x = 0; x < 128; x++)
                page_PSMCT8[block][y][x] = (blockid_PSMCT8(block, 0, x, y) << 8) + columnTable8[y & 0xF][x & 0xF];
        }

        for (int y = 0; y < 128; y++)
        {
            for (int x = 0; x < 128; x++)
                page_PSMCT4[block][y][x] = (blockid_PSMCT4(block, 0, x, y) << 9) + columnTable4[y & 0xF][x & 0x1F];
        }
    }
}

GraphicsSynthesizerThread::~GraphicsSynthesizerThread()
{
    if (local_mem)
        delete[] local_mem;
}

void GraphicsSynthesizerThread::event_loop(gs_fifo* fifo, gs_return_fifo* return_fifo)
{
    GraphicsSynthesizerThread gs = GraphicsSynthesizerThread();
    gs.reset();
    try
    {
        while (true)
        {
            GS_message data;
            bool available = fifo->pop(data);
            if (available)
            {
                switch (data.type)
                {
                    case write64_t:
                    {
                        auto p = data.payload.write64_payload;
                        gs.write64(p.addr, p.value);
                        break;
                    }
                    case write64_privileged_t:
                    {
                        auto p = data.payload.write64_payload;
                        gs.reg.write64_privileged(p.addr, p.value);
                        break;
                    }
                    case write32_privileged_t:
                    {
                        auto p = data.payload.write32_payload;
                        gs.reg.write32_privileged(p.addr, p.value);
                        break;
                    }
                    case set_rgba_t:
                    {
                        auto p = data.payload.rgba_payload;
                        gs.set_RGBA(p.r, p.g, p.b, p.a);
                        break;
                    }
                    case set_stq_t:
                    {
                        auto p = data.payload.stq_payload;
                        gs.set_STQ(p.s, p.t, p.q);
                        break;
                    }
                    case set_uv_t:
                    {
                        auto p = data.payload.uv_payload;
                        gs.set_UV(p.u, p.v);
                        break;
                    }
                    case set_xyz_t:
                    {
                        auto p = data.payload.xyz_payload;
                        gs.set_XYZ(p.x, p.y, p.z, p.drawing_kick);
                        break;
                    }
                    case set_q_t:
                    {
                        auto p = data.payload.q_payload;
                        gs.set_Q(p.q);
                        break;
                    }
                    case set_crt_t:
                    {
                        auto p = data.payload.crt_payload;
                        gs.reg.set_CRT(p.interlaced, p.mode, p.frame_mode);
                        break;
                    }
                    case render_crt_t:
                    {
                        auto p = data.payload.render_payload;

                        while (!p.target_mutex->try_lock())
                        {
                            printf("[GS_t] buffer lock failed!\n");
                            std::this_thread::yield();
                        }
                        std::lock_guard<std::mutex> lock(*p.target_mutex, std::adopt_lock);
                        gs.render_CRT(p.target);
                        GS_return_message_payload return_payload;
                        return_payload.no_payload = { 0 };
                        return_fifo->push({ GS_return::render_complete_t,return_payload });
                        break;
                    }
                    case assert_finish_t:
                        gs.reg.assert_FINISH();
                        break;
                    case set_vblank_t:
                    {
                        auto p = data.payload.vblank_payload;
                        gs.reg.set_VBLANK(p.vblank);
                        break;
                    }
                    case memdump_t:
                        gs.memdump();
                        break;
                    case die_t:
                        return;
                    case loadstate_t:
                    {
                        gs.load_state(data.payload.loadstate_payload.state);
                        GS_return_message_payload return_payload;
                        return_payload.no_payload = { 0 };
                        return_fifo->push({ GS_return::loadstate_done_t,return_payload });
                    }
                        break;
                    case savestate_t:
                    {
                        gs.save_state(data.payload.savestate_payload.state);
                        GS_return_message_payload return_payload;
                        return_payload.no_payload = { 0 };
                        return_fifo->push({ GS_return::savestate_done_t,return_payload });
                    }
                        break;
                }
            }
            else
            {
                std::this_thread::yield();
            }
        }
    }
    catch (Emulation_error &e)
    {
        GS_return_message_payload return_payload;
        char* copied_string = new char[ERROR_STRING_MAX_LENGTH];
        strncpy(copied_string, e.what(), ERROR_STRING_MAX_LENGTH);
        return_payload.death_error_payload.error_str = { copied_string };
        return_fifo->push({ GS_return::death_error_t,return_payload });
    }
}

void GraphicsSynthesizerThread::reset()
{
    if (!local_mem)
        local_mem = new uint8_t[1024 * 1024 * 4];
    pixels_transferred = 0;
    num_vertices = 0;
    frame_count = 0;

    reg.reset();
    context1.reset();
    context2.reset();
    PSMCT24_color = 0;
    PSMCT24_unpacked_count = 0;
    current_ctx = &context1;
}

void GraphicsSynthesizerThread::memdump()
{
    ofstream file("memdump.bin", std::ios::binary);
    file.write((char*)local_mem, 1024 * 1024 * 4);
    file.close();
}

void GraphicsSynthesizerThread::render_CRT(uint32_t* target)
{
    printf("DISPLAY2: (%d, %d) wh: (%d, %d)\n", reg.DISPLAY2.x >> 2, reg.DISPLAY2.y, reg.DISPLAY2.width >> 2, reg.DISPLAY2.height);
    DISPLAY &currentDisplay = reg.DISPLAY1;
    DISPFB &currentFB = reg.DISPFB1;
    if (reg.PMODE.circuit2 == false)
    {
        currentDisplay = reg.DISPLAY1;
        currentFB = reg.DISPFB1;
    }
    else
    {
        currentDisplay = reg.DISPLAY2;
        currentFB = reg.DISPFB2;
    }
    int width = currentDisplay.width >> 2;
    for (int y = 0; y < currentDisplay.height; y++)
    {
        for (int x = 0; x < width; x++)
        {
            int pixel_x = x;
            int pixel_y = y;

            if (reg.SMODE2.frame_mode && reg.SMODE2.interlaced)
                pixel_y *= 2;
            if (pixel_x >= width || pixel_y >= currentDisplay.height)
                continue;
            uint32_t scaled_x = currentFB.x + x;
            uint32_t scaled_y = currentFB.y + y;
            scaled_x *= currentFB.width;
            scaled_x /= width;
            uint32_t value = read_PSMCT32_block(currentFB.frame_base * 4, currentFB.width, scaled_x, scaled_y);
            target[pixel_x + (pixel_y * width)] = value | 0xFF000000;

            if (reg.SMODE2.frame_mode && reg.SMODE2.interlaced)
                target[pixel_x + ((pixel_y + 1)  * width)] = value | 0xFF000000;
        }
    }
}

void GraphicsSynthesizerThread::dump_texture(uint32_t* target, uint32_t start_addr, uint32_t width)
{
    uint32_t dwidth = reg.DISPLAY2.width >> 2;
    printf("[GS_t] Dumping texture\n");
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
            target[x + (y * dwidth)] = value;
            target[x + (y * dwidth)] |= 0xFF000000;
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
    printf("[GS_t] Done dumping\n");
}

void GraphicsSynthesizerThread::write64(uint32_t addr, uint64_t value)
{
    if (reg.write64(addr, value))
        return;
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
            printf("[GS_t] PRIM: $%08X\n", value);
            break;
        case 0x0001:
        {
            RGBAQ.r = value & 0xFF;
            RGBAQ.g = (value >> 8) & 0xFF;
            RGBAQ.b = (value >> 16) & 0xFF;
            RGBAQ.a = (value >> 24) & 0xFF;

            uint32_t q = value >> 32;
            RGBAQ.q = *(float*)&q;
            printf("[GS_t] RGBAQ: $%08X_%08X\n", value >> 32, value);
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
            //XYZF2
            current_vtx.x = value & 0xFFFF;
            current_vtx.y = (value >> 16) & 0xFFFF;
            current_vtx.z = (value >> 32) & 0xFFFFFF;
            vertex_kick(true);
            break;
        case 0x0005:
            //XYZ2
            current_vtx.x = value & 0xFFFF;
            current_vtx.y = (value >> 16) & 0xFFFF;
            current_vtx.z = value >> 32;
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
            current_vtx.x = value & 0xFFFF;
            current_vtx.y = (value >> 16) & 0xFFFF;
            current_vtx.z = value >> 32;
            vertex_kick(false);
            break;
        case 0x000F:
            break;
        case 0x0014:
            context1.set_tex1(value);
            break;
        case 0x0015:
            context2.set_tex1(value);
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
            printf("PRMODECNT: $%08X\n", value);
            use_PRIM = value & 0x1;
            break;
        case 0x001B:
            printf("PRMODE: $%08X\n", value);
            break;
        case 0x001C:
            TEXCLUT.width = (value & 0x3F) * 64;
            TEXCLUT.x = ((value >> 6) & 0x3F) * 16;
            TEXCLUT.y = (value >> 12) & 0x3FF;
            printf("TEXCLUT: $%08X\n", value);
            printf("Width: %d\n", TEXCLUT.width);
            printf("X: %d\n", TEXCLUT.x);
            printf("Y: %d\n", TEXCLUT.y);
            break;
        case 0x003B:
            printf("TEXA: $%08X_%08X\n", value >> 32, value);
            TEXA.alpha0 = value & 0xFF;
            TEXA.trans_black = value & (1 << 15);
            TEXA.alpha1 = (value >> 32) & 0xFF;
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
                TRXPOS.int_source_x = TRXPOS.source_x;
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
            Errors::print_warning("[GS_t] Unrecognized write64 to reg $%04X: $%08X_%08X\n", addr, value >> 32, value);
    }
}

void GraphicsSynthesizerThread::set_RGBA(uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    RGBAQ.r = r;
    RGBAQ.g = g;
    RGBAQ.b = b;
    RGBAQ.a = a;
}

void GraphicsSynthesizerThread::set_STQ(uint32_t s, uint32_t t, uint32_t q)
{
    ST.s = *(float*)&s;
    ST.t = *(float*)&t;
    RGBAQ.q = *(float*)&q;
}

void GraphicsSynthesizerThread::set_UV(uint16_t u, uint16_t v)
{
    UV.u = u;
    UV.v = v;
    printf("UV: ($%04X, $%04X)\n", UV.u, UV.v);
}

void GraphicsSynthesizerThread::set_Q(float q)
{
    RGBAQ.q = q;
}

void GraphicsSynthesizerThread::set_XYZ(uint32_t x, uint32_t y, uint32_t z, bool drawing_kick)
{
    current_vtx.x = x;
    current_vtx.y = y;
    current_vtx.z = z;
    vertex_kick(drawing_kick);
}

uint32_t GraphicsSynthesizerThread::blockid_PSMCT32(uint32_t block, uint32_t width, uint32_t x, uint32_t y)
{
    return block + ((y & ~0x1F) * (width / 64)) + ((x >> 1) & ~0x1F) + blockTable32[(y >> 3) & 0x3][(x >> 3) & 0x7];
}

uint32_t GraphicsSynthesizerThread::blockid_PSMCT32Z(uint32_t block, uint32_t width, uint32_t x, uint32_t y)
{
    return block + ((y & ~0x1F) * (width / 64)) + ((x >> 1) & ~0x1F) + blockTable32Z[(y >> 3) & 0x3][(x >> 3) & 0x7];
}

uint32_t GraphicsSynthesizerThread::blockid_PSMCT16(uint32_t block, uint32_t width, uint32_t x, uint32_t y)
{
    return block + ((y >> 1) & ~0x1F) * (width / 64) + ((x >> 1) & ~0x1F) + blockTable16[(y >> 3) & 7][(x >> 4) & 3];
}

uint32_t GraphicsSynthesizerThread::blockid_PSMCT16S(uint32_t block, uint32_t width, uint32_t x, uint32_t y)
{
    return block + ((y >> 1) & ~0x1F) * (width / 64) + ((x >> 1) & ~0x1F) + blockTable16S[(y >> 3) & 7][(x >> 4) & 3];
}

uint32_t GraphicsSynthesizerThread::blockid_PSMCT16Z(uint32_t block, uint32_t width, uint32_t x, uint32_t y)
{
    return block + ((y >> 1) & ~0x1F) * (width / 64) + ((x >> 1) & ~0x1F) + blockTable16Z[(y >> 3) & 7][(x >> 4) & 3];
}

uint32_t GraphicsSynthesizerThread::blockid_PSMCT16SZ(uint32_t block, uint32_t width, uint32_t x, uint32_t y)
{
    return block + ((y >> 1) & ~0x1F) * (width / 64) + ((x >> 1) & ~0x1F) + blockTable16SZ[(y >> 3) & 7][(x >> 4) & 3];
}

uint32_t GraphicsSynthesizerThread::blockid_PSMCT8(uint32_t block, uint32_t width, uint32_t x, uint32_t y)
{
    return block + ((y >> 1) & ~0x1F) * (width / 128) + ((x >> 2) & ~0x1F) + blockTable8[(y >> 4) & 3][(x >> 4) & 7];
}

uint32_t GraphicsSynthesizerThread::blockid_PSMCT4(uint32_t block, uint32_t width, uint32_t x, uint32_t y)
{
    return block + ((y >> 2) & ~0x1F) * (width / 128) + ((x >> 2) & ~0x1F) + blockTable4[(y >> 4) & 7][(x >> 5) & 3];
}

uint32_t GraphicsSynthesizerThread::addr_PSMCT32(uint32_t block, uint32_t width, uint32_t x, uint32_t y)
{
    uint32_t page = ((block >> 5) + (y >> 5) * width + (x >> 6));
    uint32_t addr = (page << 11) + page_PSMCT32[block & 0x1F][y & 0x1F][x & 0x3F];
    return (addr << 2) & 0x003FFFFC;
}

uint32_t GraphicsSynthesizerThread::addr_PSMCT32Z(uint32_t block, uint32_t width, uint32_t x, uint32_t y)
{
    uint32_t page = ((block >> 5) + (y >> 5) * width + (x >> 6));
    uint32_t addr = (page << 11) + page_PSMCT32Z[block & 0x1F][y & 0x1F][x & 0x3F];
    return (addr << 2) & 0x003FFFFC;
}

uint32_t GraphicsSynthesizerThread::addr_PSMCT16(uint32_t block, uint32_t width, uint32_t x, uint32_t y)
{
    uint32_t page = ((block >> 5) + (y >> 6) * width + (x >> 6));
    uint32_t addr = (page << 12) + page_PSMCT16[block & 0x1F][y & 0x3F][x & 0x3F];
    return (addr << 1) & 0x003FFFFE;
}

uint32_t GraphicsSynthesizerThread::addr_PSMCT16S(uint32_t block, uint32_t width, uint32_t x, uint32_t y)
{
    uint32_t page = ((block >> 5) + (y >> 6) * width + (x >> 6));
    uint32_t addr = (page << 12) + page_PSMCT16S[block & 0x1F][y & 0x3F][x & 0x3F];
    return (addr << 1) & 0x003FFFFE;
}

uint32_t GraphicsSynthesizerThread::addr_PSMCT16Z(uint32_t block, uint32_t width, uint32_t x, uint32_t y)
{
    uint32_t page = ((block >> 5) + (y >> 6) * width + (x >> 6));
    uint32_t addr = (page << 12) + page_PSMCT16Z[block & 0x1F][y & 0x3F][x & 0x3F];
    return (addr << 1) & 0x003FFFFE;
}

uint32_t GraphicsSynthesizerThread::addr_PSMCT16SZ(uint32_t block, uint32_t width, uint32_t x, uint32_t y)
{
    uint32_t page = ((block >> 5) + (y >> 6) * width + (x >> 6));
    uint32_t addr = (page << 12) + page_PSMCT16SZ[block & 0x1F][y & 0x3F][x & 0x3F];
    return (addr << 1) & 0x003FFFFE;
}

uint32_t GraphicsSynthesizerThread::addr_PSMCT8(uint32_t block, uint32_t width, uint32_t x, uint32_t y)
{
    uint32_t page = ((block >> 5) + (y >> 6) * (width >> 1) + (x >> 7));
    uint32_t addr = (page << 13) + page_PSMCT8[block & 0x1F][y & 0x3F][x & 0x7F];
    return addr & 0x003FFFFF;
}

uint32_t GraphicsSynthesizerThread::addr_PSMCT4(uint32_t block, uint32_t width, uint32_t x, uint32_t y)
{
    uint32_t page = ((block >> 5) + (y >> 7) * (width >> 1) + (x >> 7));
    uint32_t addr = (page << 14) + page_PSMCT4[block & 0x1F][y & 0x7F][x & 0x7F];
    return addr & 0x007FFFFF;
}

uint32_t GraphicsSynthesizerThread::read_PSMCT32_block(uint32_t base, uint32_t width, uint32_t x, uint32_t y)
{
    uint32_t addr = addr_PSMCT32(base / 256, width / 64, x, y);
    return *(uint32_t*)&local_mem[addr];
}

uint32_t GraphicsSynthesizerThread::read_PSMCT32Z_block(uint32_t base, uint32_t width, uint32_t x, uint32_t y)
{
    uint32_t addr = addr_PSMCT32Z(base / 256, width / 64, x, y);
    return *(uint32_t*)&local_mem[addr];
}

uint16_t GraphicsSynthesizerThread::read_PSMCT16_block(uint32_t base, uint32_t width, uint32_t x, uint32_t y)
{
    uint32_t addr = addr_PSMCT16(base / 256, width / 64, x, y);
    return *(uint16_t*)&local_mem[addr];
}

uint16_t GraphicsSynthesizerThread::read_PSMCT16S_block(uint32_t base, uint32_t width, uint32_t x, uint32_t y)
{
    uint32_t addr = addr_PSMCT16S(base / 256, width / 64, x, y);
    return *(uint16_t*)&local_mem[addr];
}

uint16_t GraphicsSynthesizerThread::read_PSMCT16Z_block(uint32_t base, uint32_t width, uint32_t x, uint32_t y)
{
    uint32_t addr = addr_PSMCT16Z(base / 256, width / 64, x, y);
    return *(uint16_t*)&local_mem[addr];
}

uint16_t GraphicsSynthesizerThread::read_PSMCT16SZ_block(uint32_t base, uint32_t width, uint32_t x, uint32_t y)
{
    uint32_t addr = addr_PSMCT16SZ(base / 256, width / 64, x, y);
    return *(uint16_t*)&local_mem[addr];
}

uint8_t GraphicsSynthesizerThread::read_PSMCT8_block(uint32_t base, uint32_t width, uint32_t x, uint32_t y)
{
    uint32_t addr = addr_PSMCT8(base / 256, width / 64, x, y);
    return local_mem[addr];
}

uint8_t GraphicsSynthesizerThread::read_PSMCT4_block(uint32_t base, uint32_t width, uint32_t x, uint32_t y)
{
    uint32_t addr = addr_PSMCT4(base / 256, width / 64, x, y);
    if (addr & 0x1)
        return local_mem[addr >> 1] >> 4;
    return local_mem[addr >> 1] & 0xF;
}

void GraphicsSynthesizerThread::write_PSMCT32_block(uint32_t base, uint32_t width, uint32_t x, uint32_t y, uint32_t value)
{
    uint32_t addr = addr_PSMCT32(base / 256, width / 64, x, y);
    *(uint32_t*)&local_mem[addr] = value;
}

void GraphicsSynthesizerThread::write_PSMCT32Z_block(uint32_t base, uint32_t width, uint32_t x, uint32_t y, uint32_t value)
{
    uint32_t addr = addr_PSMCT32Z(base / 256, width / 64, x, y);
    *(uint32_t*)&local_mem[addr] = value;
}

void GraphicsSynthesizerThread::write_PSMCT24Z_block(uint32_t base, uint32_t width, uint32_t x, uint32_t y, uint32_t value)
{
    uint32_t addr = addr_PSMCT32Z(base / 256, width / 64, x, y);
    uint32_t old_mem = *(uint32_t*)&local_mem[addr];
    *(uint32_t*)&local_mem[addr] = (old_mem & 0xFF000000) | value;
}

void GraphicsSynthesizerThread::write_PSMCT16_block(uint32_t base, uint32_t width, uint32_t x, uint32_t y, uint16_t value)
{
    uint32_t addr = addr_PSMCT16(base / 256, width / 64, x, y);
    *(uint16_t*)&local_mem[addr] = value;
}

void GraphicsSynthesizerThread::write_PSMCT16S_block(uint32_t base, uint32_t width, uint32_t x, uint32_t y, uint16_t value)
{
    uint32_t addr = addr_PSMCT16S(base / 256, width / 64, x, y);
    *(uint16_t*)&local_mem[addr] = value;
}

void GraphicsSynthesizerThread::write_PSMCT16Z_block(uint32_t base, uint32_t width, uint32_t x, uint32_t y, uint16_t value)
{
    uint32_t addr = addr_PSMCT16Z(base / 256, width / 64, x, y);
    *(uint16_t*)&local_mem[addr] = value;
}

void GraphicsSynthesizerThread::write_PSMCT16SZ_block(uint32_t base, uint32_t width, uint32_t x, uint32_t y, uint16_t value)
{
    uint32_t addr = addr_PSMCT16SZ(base / 256, width / 64, x, y);
    *(uint16_t*)&local_mem[addr] = value;
}

void GraphicsSynthesizerThread::write_PSMCT8_block(uint32_t base, uint32_t width, uint32_t x, uint32_t y, uint8_t value)
{
    uint32_t addr = addr_PSMCT8(base / 256, width / 64, x, y);
    local_mem[addr] = value;
}

void GraphicsSynthesizerThread::write_PSMCT4_block(uint32_t base, uint32_t width, uint32_t x, uint32_t y, uint8_t value)
{
    uint32_t addr = addr_PSMCT4(base / 256, width / 64, x, y);
    if (addr & 0x1)
    {
        local_mem[addr >> 1] &= ~0xF0;
        local_mem[addr >> 1] |= value & 0xF0;
    }
    else
    {
        local_mem[addr >> 1] &= ~0xF;
        local_mem[addr >> 1] |= value & 0xF;
    }
}

//The "vertex kick" is the name given to the process of placing a vertex in the vertex queue.
//If drawing_kick is true, and enough vertices are available, then the polygon is rendered.
void GraphicsSynthesizerThread::vertex_kick(bool drawing_kick)
{
    for (int i = num_vertices; i > 0; i--)
        vtx_queue[i] = vtx_queue[i - 1];
    current_vtx.rgbaq = RGBAQ;
    current_vtx.uv = UV;
    current_vtx.s = ST.s;
    current_vtx.t = ST.t;
    vtx_queue[0] = current_vtx;
    //printf("Vkick: (%d, %d, %d)\n", current_vtx.x, current_vtx.y, current_vtx.z);

    num_vertices++;
    bool request_draw_kick = false;
    switch (PRIM.prim_type)
    {
        case 0: //Point
            num_vertices--;
            request_draw_kick = true;
            break;
        case 1: //Line
            if (num_vertices == 2)
            {
                num_vertices = 0;
                request_draw_kick = true;
            }
            break;
        case 2: //LineStrip
            if (num_vertices == 2)
            {
                num_vertices--;
                request_draw_kick = true;
            }
            break;
        case 3: //Triangle
            if (num_vertices == 3)
            {
                num_vertices = 0;
                request_draw_kick = true;
            }
            break;
        case 4: //TriangleStrip
            if (num_vertices == 3)
            {
                num_vertices--;
                request_draw_kick = true;
            }
            break;
        case 5: //TriangleFan
            if (num_vertices == 3)
            {
                num_vertices--;
                if (drawing_kick)
                    render_primitive();

                //Move first vertex back, so that all triangles can share it
                vtx_queue[1] = vtx_queue[2];
            }
            break;
        case 6: //Sprite
            if (num_vertices == 2)
            {
                num_vertices = 0;
                request_draw_kick = true;
            }
            break;
        default:
            Errors::die("[GS] Unrecognized primitive %d\n", PRIM.prim_type);
    }
    if (drawing_kick && request_draw_kick)
        render_primitive();
}

void GraphicsSynthesizerThread::render_primitive()
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

bool GraphicsSynthesizerThread::depth_test(int32_t x, int32_t y, uint32_t z)
{
    uint32_t base = current_ctx->zbuf.base_pointer;
    uint32_t width = current_ctx->frame.width;
    switch (current_ctx->test.depth_method)
    {
        case 0: //FAIL
            return false;
        case 1: //PASS
            return true;
        case 2: //GEQUAL
            switch (current_ctx->zbuf.format)
            {
                case 0x00:
                    return z >= read_PSMCT32Z_block(base, width, x, y);
                case 0x01:
                    z = min(z, 0xFFFFFFU);
                    return z >= (read_PSMCT32Z_block(base, width, x, y) & 0xFFFFFF);
                case 0x02:
                    z = min(z, 0xFFFFU);
                    return z >= read_PSMCT16Z_block(base, width, x, y);
                case 0x0A:
                    z = min(z, 0xFFFFU);
                    return z >= read_PSMCT16SZ_block(base, width, x, y);
                default:
                    Errors::die("[GS_t] Unrecognized zbuf format $%02X\n", current_ctx->zbuf.format);
            }
            break;
        case 3: //GREATER
            switch (current_ctx->zbuf.format)
            {
                case 0x00:
                    return z > read_PSMCT32Z_block(base, width, x, y);
                case 0x01:
                    z = min(z, 0xFFFFFFU);
                    return z > (read_PSMCT32Z_block(base, width, x, y) & 0xFFFFFF);
                case 0x02:
                    z = min(z, 0xFFFFU);
                    return z > read_PSMCT16Z_block(base, width, x, y);
                case 0x0A:
                    z = min(z, 0xFFFFU);
                    return z > read_PSMCT16SZ_block(base, width, x, y);
                default:
                    Errors::die("[GS_t] Unrecognized zbuf format $%02X\n", current_ctx->zbuf.format);
            }
            break;
    }
    return false;
}

void GraphicsSynthesizerThread::draw_pixel(int32_t x, int32_t y, uint32_t z, RGBAQ_REG& color, bool alpha_blending)
{
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

    bool pass_depth_test = true;
    if (test->depth_test)
        pass_depth_test = depth_test(x, y, z);
    else
        update_z = false;

    if (!pass_depth_test)
        return;

    uint32_t frame_color = read_PSMCT32_block(current_ctx->frame.base_pointer, current_ctx->frame.width, x, y);
    uint32_t final_color = 0;

    if (test->dest_alpha_test)
    {
        if (test->dest_alpha_method && !(frame_color & (1 << 31)))
            return;
        else if (!test->dest_alpha_method && (frame_color & (1 << 31)))
            return;
    }

    if (alpha_blending)
    {
        uint32_t r1, g1, b1;
        uint32_t r2, g2, b2;
        uint32_t cr, cg, cb;
        uint32_t alpha;

        switch (current_ctx->alpha.spec_A)
        {
            case 0:
                r1 = color.r;
                g1 = color.g;
                b1 = color.b;
                break;
            case 1:
                r1 = frame_color & 0xFF;
                g1 = (frame_color >> 8) & 0xFF;
                b1 = (frame_color >> 16) & 0xFF;
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
                r2 = frame_color & 0xFF;
                g2 = (frame_color >> 8) & 0xFF;
                b2 = (frame_color >> 16) & 0xFF;
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
                cr = frame_color & 0xFF;
                cg = (frame_color >> 8) & 0xFF;
                cb = (frame_color >> 16) & 0xFF;
                break;
            case 2:
            case 3:
                cr = 0;
                cg = 0;
                cb = 0;
                break;
        }

        int fb = (int)b1 - (int)b2;
        int fg = (int)g1 - (int)g2;
        int fr = (int)r1 - (int)r2;

        fb = (((fb * (int)alpha) >> 7) + cb);
        fg = (((fg * (int)alpha) >> 7) + cg);
        fr = (((fr * (int)alpha) >> 7) + cr);

        if (COLCLAMP)
        {
            if (fb < 0)
                fb = 0;
            if (fg < 0)
                fg = 0;
            if (fr < 0)
                fr = 0;
            if (fb > 0xFF)
                fb = 0xFF;
            if (fg > 0xFF)
                fg = 0xFF;
            if (fr > 0xFF)
                fr = 0xFF;
        }
        else
        {
            fb &= 0xFF;
            fg &= 0xFF;
            fr &= 0xFF;
        }

        final_color |= alpha << 24;
        final_color |= fb << 16;
        final_color |= fg << 8;
        final_color |= fr;
    }
    else
    {
        final_color |= color.a << 24;
        final_color |= color.b << 16;
        final_color |= color.g << 8;
        final_color |= color.r;
    }

    if (!update_frame)
        final_color = frame_color;
    uint8_t alpha = frame_color >> 24;
    if (update_alpha && current_ctx->frame.format != 1)
        alpha = final_color >> 24;
    final_color &= 0x00FFFFFF;
    final_color |= alpha << 24;

    //printf("[GS_t] Write $%08X (%d, %d)\n", final_color, x, y);
    write_PSMCT32_block(current_ctx->frame.base_pointer, current_ctx->frame.width, x, y, final_color);
    if (update_z)
    {
        switch (current_ctx->zbuf.format)
        {
            case 0x00:
                write_PSMCT32Z_block(current_ctx->zbuf.base_pointer, current_ctx->frame.width, x, y, z);
                break;
            case 0x01:
                write_PSMCT24Z_block(current_ctx->zbuf.base_pointer, current_ctx->frame.width, x, y, z & 0xFFFFFF);
                break;
            case 0x02:
                write_PSMCT16Z_block(current_ctx->zbuf.base_pointer, current_ctx->frame.width, x, y, z & 0xFFFF);
                break;
            case 0x0A:
                write_PSMCT16SZ_block(current_ctx->zbuf.base_pointer, current_ctx->frame.width, x, y, z & 0xFFFF);
                break;
        }
    }
}

void GraphicsSynthesizerThread::render_point()
{
    Vertex v1 = vtx_queue[0]; v1.to_relative(current_ctx->xyoffset);
    if (v1.x < current_ctx->scissor.x1 || v1.x > current_ctx->scissor.x2 ||
        v1.y < current_ctx->scissor.y1 || v1.y > current_ctx->scissor.y2)
    return;
    printf("[GS_t] Rendering point!\n");
    printf("Coords: (%d, %d, %d)\n", v1.x >> 4, v1.y >> 4, v1.z);
    RGBAQ_REG vtx_color, tex_color;
    
    vtx_color = v1.rgbaq;
    if (PRIM.texture_mapping)
    {
        uint32_t u, v;
        if (!PRIM.use_UV)
        {
            u = v1.s * current_ctx->tex0.tex_width;
            v = v1.t * current_ctx->tex0.tex_height;
            u <<= 4;
            v <<= 4;
        }
        else
        {
            u = (uint32_t) v1.uv.u;
            v = (uint32_t) v1.uv.v;
        }
        tex_lookup(u, v, vtx_color, tex_color);
        draw_pixel(v1.x, v1.y, v1.z, tex_color, PRIM.alpha_blend);
    }
    else
    {
        draw_pixel(v1.x, v1.y, v1.z, vtx_color, PRIM.alpha_blend);
    }
}

void GraphicsSynthesizerThread::render_line()
{
    printf("[GS_t] Rendering line!\n");
    Vertex v1 = vtx_queue[1]; v1.to_relative(current_ctx->xyoffset);
    Vertex v2 = vtx_queue[0]; v2.to_relative(current_ctx->xyoffset);

    //Transpose line if it's steep
    bool is_steep = false;
    if (abs(v2.x - v1.x) < abs(v2.y - v1.y))
    {
        swap(v1.x, v1.y);
        swap(v2.x, v2.y);
        is_steep = true;
    }

    //Make line left-to-right
    if (v1.x > v2.x)
    {
        swap(v1, v2);
    }
    
    int32_t min_x = max(v1.x, (int32_t)current_ctx->scissor.x1);
    int32_t min_y = max(v1.y, (int32_t)current_ctx->scissor.y1);
    int32_t max_x = min(v2.x, (int32_t)current_ctx->scissor.x2);
    int32_t max_y = min(v2.y, (int32_t)current_ctx->scissor.y2);
    
    RGBAQ_REG color = vtx_queue[0].rgbaq;
    RGBAQ_REG tex_color;

    printf("Coords: (%d, %d, %d) (%d, %d, %d)\n", v1.x >> 4, v1.y >> 4, v1.z, v2.x >> 4, v2.y >> 4, v2.z);

    for (int32_t x = min_x; x < max_x; x += 0x10)
    {
        uint32_t z = interpolate(x, v1.z, v1.x, v2.z, v2.x);
        float t = (x - v1.x)/(float)(v2.x - v1.x);
        int32_t y = v1.y*(1.-t) + v2.y*t;        
        //if (y < min_y || y > max_y)
            //continue;
        if (PRIM.gourand_shading)
        {
            color.r = interpolate(x, v1.rgbaq.r, v1.x, v2.rgbaq.r, v2.x);
            color.g = interpolate(x, v1.rgbaq.g, v1.x, v2.rgbaq.g, v2.x);
            color.b = interpolate(x, v1.rgbaq.b, v1.x, v2.rgbaq.b, v2.x);
            color.a = interpolate(x, v1.rgbaq.a, v1.x, v2.rgbaq.a, v2.x);
        }
        if (PRIM.texture_mapping)
        {
            uint32_t u, v;
            if (!PRIM.use_UV)
            {
                float tex_s, tex_t;
                tex_s = interpolate(x, v1.s, v1.x, v2.s, v2.x);
                tex_t = interpolate(y, v1.t, v1.y, v2.t, v2.y);
                u = (tex_s * current_ctx->tex0.tex_width) * 16.0;
                v = (tex_t * current_ctx->tex0.tex_height) * 16.0;
            }
            else
            {
                v = interpolate(y, v2.uv.v, v1.y, v2.uv.v, v2.y);
                u = interpolate(x, v1.uv.u, v1.x, v2.uv.u, v2.x);
            }
            tex_lookup(u, v, color, tex_color);
            color = tex_color;
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
int32_t GraphicsSynthesizerThread::orient2D(const Vertex &v1, const Vertex &v2, const Vertex &v3)
{
    return (v2.x - v1.x) * (v3.y - v1.y) - (v3.x - v1.x) * (v2.y - v1.y);
}

void GraphicsSynthesizerThread::render_triangle()
{
    printf("[GS_t] Rendering triangle!\n");

    Vertex v1 = vtx_queue[2]; v1.to_relative(current_ctx->xyoffset);
    Vertex v2 = vtx_queue[1]; v2.to_relative(current_ctx->xyoffset);
    Vertex v3 = vtx_queue[0]; v3.to_relative(current_ctx->xyoffset);

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
    
    //Automatic scissoring test
    min_x = max(min_x, (int32_t)current_ctx->scissor.x1);
    min_y = max(min_y, (int32_t)current_ctx->scissor.y1);
    max_x = min(max_x, (int32_t)current_ctx->scissor.x2);
    max_y = min(max_y, (int32_t)current_ctx->scissor.y2);

    //We'll process the pixels in blocks, set the blocksize
    const int32_t BLOCKSIZE = 1 << 4; // Must be power of 2

    //Round down to make starting corner's coordinates a multiple of BLOCKSIZE with bitwise magic
    min_x &= ~(BLOCKSIZE - 1);
    min_y &= ~(BLOCKSIZE - 1);

    //Calculate incremental steps for the weights
    //Reference: https://fgiesen.wordpress.com/2013/02/10/optimizing-the-basic-rasterizer/
    const int32_t A12 = v1.y - v2.y;
    const int32_t B12 = v2.x - v1.x;
    const int32_t A23 = v2.y - v3.y;
    const int32_t B23 = v3.x - v2.x;
    const int32_t A31 = v3.y - v1.y;
    const int32_t B31 = v1.x - v3.x;

    Vertex min_corner;
    min_corner.x = min_x; min_corner.y = min_y;
    int32_t w1_row = orient2D(v2, v3, min_corner);
    int32_t w2_row = orient2D(v3, v1, min_corner);
    int32_t w3_row = orient2D(v1, v2, min_corner);
    int32_t w1_row_block = w1_row;
    int32_t w2_row_block = w2_row;
    int32_t w3_row_block = w3_row;

    if (!PRIM.gourand_shading)
    {
        //Flatten the colors
        v1.rgbaq.r = v3.rgbaq.r;
        v2.rgbaq.r = v3.rgbaq.r;

        v1.rgbaq.g = v3.rgbaq.g;
        v2.rgbaq.g = v3.rgbaq.g;

        v1.rgbaq.b = v3.rgbaq.b;
        v2.rgbaq.b = v3.rgbaq.b;

        v1.rgbaq.a = v3.rgbaq.a;
        v2.rgbaq.a = v3.rgbaq.a;
    }

    RGBAQ_REG vtx_color, tex_color;

    //TODO: Parallelize this
    //Iterate through the bounding rectangle using BLOCKSIZE * BLOCKSIZE large blocks
    //This way we can throw out blocks which are totally outside the triangle way faster
    //Thanks you, Dolphin code for the idea
    //https://github.com/dolphin-emu/dolphin/blob/master/Source/Core/VideoBackends/Software/Rasterizer.cpp#L388
    for (int32_t y_block = min_y; y_block < max_y; y_block += BLOCKSIZE)
    {
        int32_t w1_block = w1_row_block;
        int32_t w2_block = w2_row_block;
        int32_t w3_block = w3_row_block;
        for (int32_t x_block = min_x; x_block < max_x; x_block += BLOCKSIZE)
        {
            //Store barycentric coordinates for the corners of a block
            //tl = top left, tr = top right, etc.
            int32_t w1_tl, w1_tr, w1_bl, w1_br;
            int32_t w2_tl, w2_tr, w2_bl, w2_br;
            int32_t w3_tl, w3_tr, w3_bl, w3_br;
            //Calculate the weight offsets for the corners
            w1_tl = w1_block; w2_tl = w2_block, w3_tl = w3_block;
            w1_tr = w1_tl + (BLOCKSIZE - 1) * A23;
            w2_tr = w2_tl + (BLOCKSIZE - 1) * A31;
            w3_tr = w3_tl + (BLOCKSIZE - 1) * A12;
            w1_bl = w1_tl + (BLOCKSIZE - 1) * B23;
            w2_bl = w2_tl + (BLOCKSIZE - 1) * B31;
            w3_bl = w3_tl + (BLOCKSIZE - 1) * B12;
            w1_br = w1_tl + (BLOCKSIZE - 1) * B23 + (BLOCKSIZE - 1) * A23;
            w2_br = w2_tl + (BLOCKSIZE - 1) * B31 + (BLOCKSIZE - 1) * A31;
            w3_br = w3_tl + (BLOCKSIZE - 1) * B12 + (BLOCKSIZE - 1) * A12;

            //Check if any of the corners are in the positive half-space of a weight
            bool w1_tl_check = w1_tl > 0;
            bool w1_tr_check = w1_tr > 0;
            bool w1_bl_check = w1_bl > 0;
            bool w1_br_check = w1_br > 0;
            bool w2_tl_check = w2_tl > 0;
            bool w2_tr_check = w2_tr > 0;
            bool w2_bl_check = w2_bl > 0;
            bool w2_br_check = w2_br > 0;
            bool w3_tl_check = w3_tl > 0;
            bool w3_tr_check = w3_tr > 0;
            bool w3_bl_check = w3_bl > 0;
            bool w3_br_check = w3_br > 0;


            //Combine all checks for a barycentric coordinate into one
            //If for one half-space checks for each corner --> all corners lie outside the triangle --> block can be skipped
            uint8_t w1_mask = (w1_tl_check << 0) | (w1_tr_check << 1) | (w1_bl_check << 2) | (w1_br_check << 3);
            uint8_t w2_mask = (w2_tl_check << 0) | (w2_tr_check << 1) | (w2_bl_check << 2) | (w2_br_check << 3);
            uint8_t w3_mask = (w3_tl_check << 0) | (w3_tr_check << 1) | (w3_bl_check << 2) | (w3_br_check << 3);
            //However in other cases, we process the block pixel-by-pixel as usual
            //TODO: In the case where all corners lie inside the triangle the code below could be slightly simplified
            if (w1_mask != 0 && w2_mask != 0 && w3_mask != 0)
            {
                //Process a BLOCKSIZE * BLOCKSIZE block as normal
                int32_t w1_row = w1_block;
                int32_t w2_row = w2_block;
                int32_t w3_row = w3_block;
                for (int32_t y = y_block; y < y_block + BLOCKSIZE; y += 0x10)
                {
                    int32_t w1 = w1_row;
                    int32_t w2 = w2_row;
                    int32_t w3 = w3_row;
                    for (int32_t x = x_block; x < x_block + BLOCKSIZE; x += 0x10)
                    {
                        //Is inside triangle?
                        if ((w1 | w2 | w3) >= 0)
                        {
                            //Interpolate Z
                            double z = (double) v1.z * w1 + (double) v2.z * w2 + (double) v3.z * w3;
                            z /= divider;

                            //Gourand shading calculations
                            float r = (float) v1.rgbaq.r * w1 + (float) v2.rgbaq.r * w2 + (float) v3.rgbaq.r * w3;
                            float g = (float) v1.rgbaq.g * w1 + (float) v2.rgbaq.g * w2 + (float) v3.rgbaq.g * w3;
                            float b = (float) v1.rgbaq.b * w1 + (float) v2.rgbaq.b * w2 + (float) v3.rgbaq.b * w3;
                            float a = (float) v1.rgbaq.a * w1 + (float) v2.rgbaq.a * w2 + (float) v3.rgbaq.a * w3;
                            float q = v1.rgbaq.q * w1 + v2.rgbaq.q * w2 + v3.rgbaq.q * w3;
                            vtx_color.r = r / divider;
                            vtx_color.g = g / divider;
                            vtx_color.b = b / divider;
                            vtx_color.a = a / divider;
                            vtx_color.q = q / divider;

                            if (PRIM.texture_mapping)
                            {
                                uint32_t u, v;
                                if (!PRIM.use_UV)
                                {
                                    float s, t, q;
                                    s = v1.s * w1 + v2.s * w2 + v3.s * w3;
                                    t = v1.t * w1 + v2.t * w2 + v3.t * w3;
                                    q = v1.rgbaq.q * w1 + v2.rgbaq.q * w2 + v3.rgbaq.q * w3;

                                    //We don't divide s and t by "divider" because dividing by Q effectively
                                    //cancels that out
                                    s /= q;
                                    t /= q;
                                    u = (s * current_ctx->tex0.tex_width) * 16.0;
                                    v = (t * current_ctx->tex0.tex_height) * 16.0;
                                }
                                else
                                {
                                    float temp_u = (float) v1.uv.u * w1 + (float) v2.uv.u * w2 + (float) v3.uv.u * w3;
                                    float temp_v = (float) v1.uv.v * w1 + (float) v2.uv.v * w2 + (float) v3.uv.v * w3;
                                    temp_u /= divider;
                                    temp_v /= divider;
                                    u = (uint32_t) temp_u;
                                    v = (uint32_t) temp_v;
                                }
                                tex_lookup(u, v, vtx_color, tex_color);
                                draw_pixel(x, y, (uint32_t) z, tex_color, PRIM.alpha_blend);
                            }
                            else
                            {
                                draw_pixel(x, y, (uint32_t) z, vtx_color, PRIM.alpha_blend);
                            }
                        }
                        //Horizontal step
                        w1 += A23 << 4;
                        w2 += A31 << 4;
                        w3 += A12 << 4;
                    }
                    //Vertical step
                    w1_row += B23 << 4;
                    w2_row += B31 << 4;
                    w3_row += B12 << 4;
                }
            }

            w1_block += BLOCKSIZE * A23;
            w2_block += BLOCKSIZE * A31;
            w3_block += BLOCKSIZE * A12;

        }
        w1_row_block += BLOCKSIZE * B23;
        w2_row_block += BLOCKSIZE * B31;
        w3_row_block += BLOCKSIZE * B12;
    }

}

void GraphicsSynthesizerThread::render_sprite()
{
    printf("[GS_t] Rendering sprite!\n");
    Vertex v1 = vtx_queue[1]; v1.to_relative(current_ctx->xyoffset);
    Vertex v2 = vtx_queue[0]; v2.to_relative(current_ctx->xyoffset);

    RGBAQ_REG vtx_color, tex_color;
    vtx_color = vtx_queue[0].rgbaq;

    if (v1.x > v2.x)
    {
        swap(v1, v2);
    }

    //Automatic scissoring test
    int32_t min_y = std::max(v1.y, (int32_t)current_ctx->scissor.y1);
    int32_t min_x = std::max(v1.x, (int32_t)current_ctx->scissor.x1);
    int32_t max_y = std::min(v2.y, (int32_t)current_ctx->scissor.y2);
    int32_t max_x = std::min(v2.x, (int32_t)current_ctx->scissor.x2);

    printf("Coords: (%d, %d) (%d, %d)\n", v1.x >> 4, v1.y >> 4, v2.x >> 4, v2.y >> 4);

    for (int32_t y = min_y; y < max_y; y += 0x10)
    {
        float pix_t = interpolate_f(y, v1.t, v1.y, v2.t, v2.y);
        uint16_t pix_v = interpolate(y, v1.uv.v, v1.y, v2.uv.v, v2.y);
        for (int32_t x = min_x; x < max_x; x += 0x10)
        {
            float pix_s = interpolate_f(x, v1.s, v1.x, v2.s, v2.x);
            uint16_t pix_u = interpolate(x, v1.uv.u, v1.x, v2.uv.u, v2.x);
            if (PRIM.texture_mapping)
            {
                if (!PRIM.use_UV)
                {
                    pix_v = (pix_t * current_ctx->tex0.tex_height)*16;
                    pix_u = (pix_s * current_ctx->tex0.tex_width)*16;
                }
                tex_lookup(pix_u, pix_v, vtx_color, tex_color);
                draw_pixel(x, y, v2.z, tex_color, PRIM.alpha_blend);
            }
            else
            {
                draw_pixel(x, y, v2.z, vtx_color, PRIM.alpha_blend);
            }
        }
    }
}

void GraphicsSynthesizerThread::write_HWREG(uint64_t data)
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
        //PSMCT16S
        case 0x0A:
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
        //PSMCT8H
        case 0x1B:
            ppd = 8;
            break;
        //PSMT4HL
        case 0x24:
            ppd = 16;
            break;
        //PSMT4HH
        case 0x2C:
            ppd = 16;
            break;
        //PSMCT24Z
        case 0x31:
            ppd = 3;
            break;
        default:
            Errors::die("[GS_t] Unrecognized BITBLTBUF dest format $%02X\n", BITBLTBUF.dest_format);
    }

    for (int i = 0; i < ppd; i++)
    {
        switch (BITBLTBUF.dest_format)
        {
            case 0x00:
                write_PSMCT32_block(BITBLTBUF.dest_base, BITBLTBUF.dest_width, TRXPOS.int_dest_x, TRXPOS.dest_y, (data >> (i * 32)) & 0xFFFFFFFF);
                pixels_transferred++;
                TRXPOS.int_dest_x++;
                //printf("[GS_t] Write to $%08X of ", BITBLTBUF.dest_base + (dest_addr * 4));
                //printf("$%08X\n", (data >> (i * 32)) & 0xFFFFFFFF);
                break;
            case 0x01:
                unpack_PSMCT24(data, i, false);
                break;
            case 0x02:
                write_PSMCT16_block(BITBLTBUF.dest_base, BITBLTBUF.dest_width, TRXPOS.int_dest_x, TRXPOS.dest_y, (data >> (i * 16)) & 0xFFFF);
                pixels_transferred++;
                TRXPOS.int_dest_x++;
                break;
            case 0xA:
                write_PSMCT16S_block(BITBLTBUF.dest_base, BITBLTBUF.dest_width, TRXPOS.int_dest_x, TRXPOS.dest_y, (data >> (i * 16)) & 0xFFFF);
                pixels_transferred++;
                TRXPOS.int_dest_x++;
                break;
            case 0x13:
                write_PSMCT8_block(BITBLTBUF.dest_base, BITBLTBUF.dest_width, TRXPOS.int_dest_x, TRXPOS.dest_y,
                                   (data >> (i * 8)) & 0xFF);
                //printf("[GS_t] Write to $%08X\n", dest_addr);
                pixels_transferred++;
                TRXPOS.int_dest_x++;
                break;
            case 0x14:
                write_PSMCT4_block(BITBLTBUF.dest_base, BITBLTBUF.dest_width, TRXPOS.int_dest_x, TRXPOS.dest_y,
                                   data >> ((i >> 1) << 3));
                pixels_transferred++;
                TRXPOS.int_dest_x++;
                break;
            case 0x1B:
            {
                uint32_t value = (data >> (i << 3)) & 0xFF;
                value <<= 24;
                value |= read_PSMCT32_block(BITBLTBUF.dest_base, BITBLTBUF.dest_width, TRXPOS.int_dest_x, TRXPOS.dest_y)
                        & 0x00FFFFFF;
                write_PSMCT32_block(BITBLTBUF.dest_base, BITBLTBUF.dest_width, TRXPOS.int_dest_x, TRXPOS.dest_y, value);
                //printf("[GS_t] Write to $%08X of ", BITBLTBUF.dest_base + (dest_addr * 4));
                pixels_transferred++;
                TRXPOS.int_dest_x++;
            }
                break;
            case 0x24:
            {
                uint32_t value;
                if (i & 0x1)
                {
                    value = (data >> ((i >> 1) << 3)) & 0xF0;
                    value <<= 20;
                }
                else
                {
                    value = (data >> ((i >> 1) << 3)) & 0xF;
                    value <<= 24;
                }
                value |= read_PSMCT32_block(BITBLTBUF.dest_base, BITBLTBUF.dest_width, TRXPOS.int_dest_x, TRXPOS.dest_y)
                        & 0xF0FFFFFF;
                write_PSMCT32_block(BITBLTBUF.dest_base, BITBLTBUF.dest_width, TRXPOS.int_dest_x, TRXPOS.dest_y, value);
                //printf("[GS_t] Write to $%08X of ", BITBLTBUF.dest_base + (dest_addr * 4));
                //printf("$%08X\n", value);
                pixels_transferred++;
                TRXPOS.int_dest_x++;
            }
                break;
            case 0x2C:
            {
                uint32_t value;
                if (i & 0x1)
                {
                    value = (data >> ((i >> 1) << 3)) & 0xF0;
                    value <<= 24;
                }
                else
                {
                    value = (data >> ((i >> 1) << 3)) & 0xF;
                    value <<= 28;
                }
                value |= read_PSMCT32_block(BITBLTBUF.dest_base, BITBLTBUF.dest_width, TRXPOS.int_dest_x, TRXPOS.dest_y)
                        & 0x0FFFFFFF;
                write_PSMCT32_block(BITBLTBUF.dest_base, BITBLTBUF.dest_width, TRXPOS.int_dest_x, TRXPOS.dest_y, value);
                //printf("[GS_t] Write to $%08X of ", BITBLTBUF.dest_base + (dest_addr * 4));
                //printf("$%08X\n", value);
                pixels_transferred++;
                TRXPOS.int_dest_x++;
            }
                break;
            case 0x31:
                unpack_PSMCT24(data, i, true);
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
        printf("[GS_t] HWREG transfer ended\n");
        TRXDIR = 3;
        pixels_transferred = 0;
    }
}

void GraphicsSynthesizerThread::unpack_PSMCT24(uint64_t data, int offset, bool z_format)
{
    int bytes_unpacked = 0;
    for (int i = offset * 24; bytes_unpacked < 3 && i < 64; i += 8)
    {
        PSMCT24_color |= ((data >> i) & 0xFF) << (PSMCT24_unpacked_count * 8);
        PSMCT24_unpacked_count++;
        bytes_unpacked++;
        if (PSMCT24_unpacked_count == 3)
        {
            if (z_format)
                write_PSMCT24Z_block(BITBLTBUF.dest_base, BITBLTBUF.dest_width, TRXPOS.int_dest_x,
                                     TRXPOS.dest_y, PSMCT24_color);
            else
            {
                PSMCT24_color |= read_PSMCT32_block(BITBLTBUF.dest_base, BITBLTBUF.dest_width, TRXPOS.int_dest_x,
                    TRXPOS.dest_y) & 0xFF000000;
                write_PSMCT32_block(BITBLTBUF.dest_base, BITBLTBUF.dest_width, TRXPOS.int_dest_x,
                    TRXPOS.dest_y, PSMCT24_color);
            }
            PSMCT24_color = 0;
            PSMCT24_unpacked_count = 0;
            TRXPOS.int_dest_x++;
            pixels_transferred++;
        }
    }
}

void GraphicsSynthesizerThread::host_to_host()
{
    uint32_t max_pixels = TRXREG.width * TRXREG.height;

    while (pixels_transferred < max_pixels)
    {
        uint32_t data;
        switch (BITBLTBUF.source_format)
        {
            case 0x00:
                data = read_PSMCT32_block(BITBLTBUF.source_base, BITBLTBUF.source_width,
                                          TRXPOS.int_source_x, TRXPOS.source_y);
                write_PSMCT32_block(BITBLTBUF.dest_base, BITBLTBUF.dest_width, TRXPOS.int_dest_x, TRXPOS.dest_y, data);
                pixels_transferred++;
                TRXPOS.int_dest_x++;
                TRXPOS.int_source_x++;
                break;
            case 0x14:
                data = read_PSMCT4_block(BITBLTBUF.source_base, BITBLTBUF.source_width,
                                          TRXPOS.int_source_x, TRXPOS.source_y);
                write_PSMCT4_block(BITBLTBUF.dest_base, BITBLTBUF.dest_width, TRXPOS.int_dest_x, TRXPOS.dest_y, data);
                pixels_transferred++;
                TRXPOS.int_dest_x++;
                TRXPOS.int_source_x++;
                break;
            default:
                Errors::die("[GS_t] Unrecognized host-to-host format $%02X", BITBLTBUF.source_format);
        }

        if (TRXPOS.int_dest_x - TRXPOS.dest_x == TRXREG.width)
        {
            TRXPOS.int_dest_x = TRXPOS.dest_x;
            TRXPOS.dest_y++;
        }

        if (TRXPOS.int_source_x - TRXPOS.source_x == TRXREG.width)
        {
            TRXPOS.int_source_x = TRXPOS.source_x;
            TRXPOS.source_y++;
        }
    }
    pixels_transferred = 0;
    TRXDIR = 3;
}

void GraphicsSynthesizerThread::tex_lookup(int16_t u, int16_t v, const RGBAQ_REG& vtx_color, RGBAQ_REG& tex_color)
{
    double LOD;
    if (current_ctx->tex1.LOD_method == 0)
        LOD = (log2(1.0 / fabs(vtx_color.q)) * pow(2, current_ctx->tex1.L)) + current_ctx->tex1.K;
    else
        LOD = current_ctx->tex1.K;

    if (LOD < 1.0 / pow(2, 7)) //ps2 precision limit
        LOD = 0.0;

    if (current_ctx->tex1.filter_larger && LOD <= 0.0)
    {
        RGBAQ_REG a, b, c, d;
        int16_t uu = (u - 8) >> 4;
        int16_t vv = (v - 8) >> 4;
        tex_lookup_int(uu, vv, vtx_color, a);
        tex_lookup_int(uu+1, vv, vtx_color, b);
        tex_lookup_int(uu, vv+1, vtx_color, c);
        tex_lookup_int(uu+1, vv+1, vtx_color, d);
        double alpha = ((u - 8) & 0xF) * (1.0 / ((double)0xF));
        double beta = ((v - 8) & 0xF) * (1.0 / ((double)0xF));
        double alpha_s = 1.0 - alpha;
        double beta_s = 1.0 - beta;
        tex_color.r = alpha_s * beta_s*a.r + alpha * beta_s*b.r + alpha_s * beta*c.r + alpha * beta*d.r;
        tex_color.g = alpha_s * beta_s*a.g + alpha * beta_s*b.g + alpha_s * beta*c.g + alpha * beta*d.g;
        tex_color.b = alpha_s * beta_s*a.b + alpha * beta_s*b.b + alpha_s * beta*c.b + alpha * beta*d.b;
        tex_color.a = alpha_s * beta_s*a.a + alpha * beta_s*b.a + alpha_s * beta*c.a + alpha * beta*d.a;
    }
    else
        tex_lookup_int(u >> 4, v >> 4, vtx_color, tex_color);

    switch (current_ctx->tex0.color_function)
    {
        case 0: //Modulate
            tex_color.r = ((uint16_t)tex_color.r * vtx_color.r) >> 7;
            tex_color.g = ((uint16_t)tex_color.g * vtx_color.g) >> 7;
            tex_color.b = ((uint16_t)tex_color.b * vtx_color.b) >> 7;
            tex_color.a = ((uint16_t)tex_color.a * vtx_color.a) >> 7;
            break;
        case 1: //Decal
            break;
    }
}

void GraphicsSynthesizerThread::tex_lookup_int(int16_t u, int16_t v, const RGBAQ_REG& vtx_color, RGBAQ_REG& tex_color)
{
    switch (current_ctx->clamp.wrap_s)
    {
        case 0:
            u %= current_ctx->tex0.tex_width;
            break;
        case 1:
            if (u > current_ctx->tex0.tex_width)
                u = current_ctx->tex0.tex_width;
            else if (u < 0)
                u = 0;
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
            else if (v < 0)
                v = 0;
            break;
    }

    uint32_t tex_base = current_ctx->tex0.texture_base;
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
            tex_color.a = TEXA.alpha0;
        }
            break;
        case 0x02:
        {
            uint16_t color = read_PSMCT16_block(tex_base, current_ctx->tex0.width, u, v);
            tex_color.r = (color & 0x1F) << 3;
            tex_color.g = ((color >> 5) & 0x1F) << 3;
            tex_color.b = ((color >> 10) & 0x1F) << 3;

            if (!(color & 0x7FFF) && TEXA.trans_black)
                tex_color.a = 0;
            else
            {
                if (color & (1 << 15))
                    tex_color.a = TEXA.alpha1;
                else
                    tex_color.a = TEXA.alpha0;
            }
            tex_color.a = ((color & (1 << 15)) != 0) << 7;
        }
            break;
        case 0x09: //Invalid format??? FFX uses it
            tex_color.r = 0;
            tex_color.g = 0;
            tex_color.b = 0;
            tex_color.a = 0;
            break;
        case 0x0A:
        {
            uint16_t color = read_PSMCT16S_block(tex_base, current_ctx->tex0.width, u, v);
            tex_color.r = (color & 0x1F) << 3;
            tex_color.g = ((color >> 5) & 0x1F) << 3;
            tex_color.b = ((color >> 10) & 0x1F) << 3;
            if (!(color & 0x7FFF) && TEXA.trans_black)
                tex_color.a = 0;
            else
            {
                if (color & (1 << 15))
                    tex_color.a = TEXA.alpha1;
                else
                    tex_color.a = TEXA.alpha0;
            }
        }
            break;
        case 0x13:
        {
            uint8_t entry = read_PSMCT8_block(tex_base, current_ctx->tex0.width, u, v);
            if (current_ctx->tex0.use_CSM2)
            {
                tex_color.r = entry;
                tex_color.g = entry;
                tex_color.b = entry;
                tex_color.a = (entry) ? 0x80 : 0x00;
            }
            else
            {
                clut_lookup(entry, tex_color, true);
            }
            //return get_word(addr);
        }
            break;
        case 0x14:
        {
            uint8_t entry = read_PSMCT4_block(tex_base, current_ctx->tex0.width, u, v);
            if (current_ctx->tex0.use_CSM2)
            {
                tex_color.r = entry << 4;
                tex_color.g = entry << 4;
                tex_color.b = entry << 4;
                tex_color.a = (entry) ? 0x80 : 0x00;
            }
            else
                clut_lookup(entry, tex_color, false);
        }
            break;
        case 0x1B:
        {
            uint8_t entry = read_PSMCT32_block(tex_base, current_ctx->tex0.width, u, v) >> 24;
            if (current_ctx->tex0.use_CSM2)
            {
                tex_color.r = entry;
                tex_color.g = entry;
                tex_color.b = entry;
                tex_color.a = (entry) ? 0x80 : 0x00;
            }
            else
                clut_lookup(entry, tex_color, true);
        }
            break;
        case 0x24:
        {
            //printf("[GS_t] Format $24: Read from $%08X\n", tex_base + (coord << 2));
            uint8_t entry = (read_PSMCT32_block(tex_base, current_ctx->tex0.width, u, v) >> 24) & 0xF;
            if (current_ctx->tex0.use_CSM2)
                clut_CSM2_lookup(entry, tex_color);
            else
                clut_lookup(entry, tex_color, false);
            break;
        }
            break;
        case 0x2C:
        {
            uint8_t entry = read_PSMCT32_block(tex_base, current_ctx->tex0.width, u, v) >> 28;
            if (current_ctx->tex0.use_CSM2)
                clut_CSM2_lookup(entry, tex_color);
            else
                clut_lookup(entry, tex_color, false);
        }
            break;
        case 0x31:
        {
            uint32_t color = read_PSMCT32Z_block(tex_base, current_ctx->tex0.width, u, v);
            tex_color.r = color & 0xFF;
            tex_color.g = (color >> 8) & 0xFF;
            tex_color.b = (color >> 16) & 0xFF;
            tex_color.a = TEXA.alpha0;
        }
            break;
        default:
            Errors::die("[GS_t] Unrecognized texture format $%02X\n", current_ctx->tex0.format);
    }
}

void GraphicsSynthesizerThread::clut_lookup(uint8_t entry, RGBAQ_REG &tex_color, bool eight_bit)
{
    uint32_t x, y;
    if (eight_bit)
    {
        x = entry & 0x7;
        if (entry & 0x10)
            x += 8;
        y = (entry & 0xE0) / 0x10;
        if (entry & 0x8)
            y++;
    }
    else
    {
        x = entry & 0x7;
        y = entry / 8;
    }
    switch (current_ctx->tex0.CLUT_format)
    {
        //PSMCT32
        case 0x00:
        case 0x01:
        {
            uint32_t color = read_PSMCT32_block(current_ctx->tex0.CLUT_base, 64, x, y);
            tex_color.r = color & 0xFF;
            tex_color.g = (color >> 8) & 0xFF;
            tex_color.b = (color >> 16) & 0xFF;
            tex_color.a = color >> 24;
        }
            break;
        //PSMCT16
        case 0x02:
        {
            uint16_t color = read_PSMCT16_block(current_ctx->tex0.CLUT_base, 64, x, y);
            tex_color.r = (color & 0x1F) << 3;
            tex_color.g = ((color >> 5) & 0x1F) << 3;
            tex_color.b = ((color >> 10) & 0x1F) << 3;
            if (!(color & 0x7FFF) && TEXA.trans_black)
                tex_color.a = 0;
            else
            {
                if (color & (1 << 15))
                    tex_color.a = TEXA.alpha1;
                else
                    tex_color.a = TEXA.alpha0;
            }
        }
            break;
        default:
            Errors::die("[GS_t] Unrecognized CLUT format $%02X\n", current_ctx->tex0.CLUT_format);
    }
}

void GraphicsSynthesizerThread::clut_CSM2_lookup(uint8_t entry, RGBAQ_REG &tex_color)
{
    uint16_t color = read_PSMCT16_block(current_ctx->tex0.CLUT_base, TEXCLUT.width, TEXCLUT.x + entry, TEXCLUT.y);
    tex_color.r = (color & 0x1F) << 3;
    tex_color.g = ((color >> 5) & 0x1F) << 3;
    tex_color.b = ((color >> 10) & 0x1F) << 3;
    tex_color.a = (color & 0x8000) ? 0x80 : 0x00;
}

void GraphicsSynthesizerThread::load_state(ifstream *state)
{
    state->read((char*)local_mem, 1024 * 1024 * 4);
    state->read((char*)&IMR, sizeof(IMR));
    state->read((char*)&context1, sizeof(context1));
    state->read((char*)&context2, sizeof(context2));

    int current_ctx_id = 1;
    state->read((char*)&current_ctx_id, sizeof(current_ctx_id));
    if (current_ctx_id == 1)
        current_ctx = &context1;
    else
        current_ctx = &context2;

    state->read((char*)&PRIM, sizeof(PRIM));
    state->read((char*)&RGBAQ, sizeof(RGBAQ));
    state->read((char*)&UV, sizeof(UV));
    state->read((char*)&ST, sizeof(ST));
    state->read((char*)&TEXA, sizeof(TEXA));
    state->read((char*)&TEXCLUT, sizeof(TEXCLUT));
    state->read((char*)&DTHE, sizeof(DTHE));
    state->read((char*)&COLCLAMP, sizeof(COLCLAMP));

    state->read((char*)&BITBLTBUF, sizeof(BITBLTBUF));
    state->read((char*)&TRXPOS, sizeof(TRXPOS));
    state->read((char*)&TRXREG, sizeof(TRXREG));
    state->read((char*)&TRXDIR, sizeof(TRXDIR));
    state->read((char*)&BUSDIR, sizeof(BUSDIR));
    state->read((char*)&pixels_transferred, sizeof(pixels_transferred));

    state->read((char*)&PSMCT24_color, sizeof(PSMCT24_color));
    state->read((char*)&PSMCT24_unpacked_count, sizeof(PSMCT24_unpacked_count));

    state->read((char*)&reg, sizeof(reg));
    state->read((char*)&current_vtx, sizeof(current_vtx));
    state->read((char*)&vtx_queue, sizeof(vtx_queue));
    state->read((char*)&num_vertices, sizeof(num_vertices));
}

void GraphicsSynthesizerThread::save_state(ofstream *state)
{
    state->write((char*)local_mem, 1024 * 1024 * 4);
    state->write((char*)&IMR, sizeof(IMR));
    state->write((char*)&context1, sizeof(context1));
    state->write((char*)&context2, sizeof(context2));

    int current_ctx_id = 1;
    if (current_ctx == &context2)
        current_ctx_id = 2;
    state->write((char*)&current_ctx_id, sizeof(current_ctx_id));

    state->write((char*)&PRIM, sizeof(PRIM));
    state->write((char*)&RGBAQ, sizeof(RGBAQ));
    state->write((char*)&UV, sizeof(UV));
    state->write((char*)&ST, sizeof(ST));
    state->write((char*)&TEXA, sizeof(TEXA));
    state->write((char*)&TEXCLUT, sizeof(TEXCLUT));
    state->write((char*)&DTHE, sizeof(DTHE));
    state->write((char*)&COLCLAMP, sizeof(COLCLAMP));

    state->write((char*)&BITBLTBUF, sizeof(BITBLTBUF));
    state->write((char*)&TRXPOS, sizeof(TRXPOS));
    state->write((char*)&TRXREG, sizeof(TRXREG));
    state->write((char*)&TRXDIR, sizeof(TRXDIR));
    state->write((char*)&BUSDIR, sizeof(BUSDIR));
    state->write((char*)&pixels_transferred, sizeof(pixels_transferred));

    state->write((char*)&PSMCT24_color, sizeof(PSMCT24_color));
    state->write((char*)&PSMCT24_unpacked_count, sizeof(PSMCT24_unpacked_count));

    state->write((char*)&reg, sizeof(reg));
    state->write((char*)&current_vtx, sizeof(current_vtx));
    state->write((char*)&vtx_queue, sizeof(vtx_queue));
    state->write((char*)&num_vertices, sizeof(num_vertices));
}
