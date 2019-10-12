#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <fstream>

#include "gsthread.hpp"
#include "gsmem.hpp"
#include "errors.hpp"

#ifdef _MSC_VER
#define _noinline(type) __declspec(noinline) type
#else
#define _noinline(type) type __attribute__ ((noinline))
#endif

#ifdef _WIN32
extern "C" void jit_draw_pixel_asm(uint8_t* func, int32_t x, int32_t y, uint32_t z, RGBAQ_REG* color);
extern "C" void jit_tex_lookup_asm(uint8_t* func, int16_t u, int16_t v, TexLookupInfo* tex_info);
#endif

using namespace std;

//Swizzling tables - we declare these outside the class to prevent a stack overflow
//Globals are allocated in a different memory segment of the executable

static SwizzleTable<32,32,64> page_PSMCT32;
static SwizzleTable<32,32,64> page_PSMCT32Z;
static SwizzleTable<32,64,64> page_PSMCT16;
static SwizzleTable<32,64,64> page_PSMCT16S;
static SwizzleTable<32,64,64> page_PSMCT16Z;
static SwizzleTable<32,64,64> page_PSMCT16SZ;
static SwizzleTable<32,64,128> page_PSMCT8;
static SwizzleTable<32,128,128> page_PSMCT4;

#define printf(fmt, ...)(0)

#define GS_JIT

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

template <typename T>
T stepsize(T u1, int32_t x1, T u2, int32_t x2, int64_t mult)
{
    if (!(x2 - x1))
        return ((u2 - u1) * mult);
    return ((u2 - u1) * mult)/(x2 - x1);
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
constexpr REG_64 GraphicsSynthesizerThread::abi_args[4];

GraphicsSynthesizerThread::GraphicsSynthesizerThread()
    : frame_complete(false), local_mem(nullptr), jit_draw_pixel_block("GS-pixel"), jit_tex_lookup_block("GS-texture"),
    emitter_dp(&jit_draw_pixel_block),
      emitter_tex(&jit_tex_lookup_block)
{
    //Initialize swizzling tables
    for (int block = 0; block < 32; block++)
    {
        for (int y = 0; y < 32; y++)
        {
            for (int x = 0; x < 64; x++)
            {
                uint32_t column = columnTable32[y & 0x7][x & 0x7];
                page_PSMCT32.get(block,y,x) = (blockid_PSMCT32(block, 0, x, y) << 6) + column;
                page_PSMCT32Z.get(block,y,x) = (blockid_PSMCT32Z(block, 0, x, y) << 6) + column;
            }
        }

        for (int y = 0; y < 64; y++)
        {
            for (int x = 0; x < 64; x++)
            {
                uint32_t column = columnTable16[y & 0x7][x & 0xF];
                page_PSMCT16.get(block,y,x) = (blockid_PSMCT16(block, 0, x, y) << 7) + column;
                page_PSMCT16S.get(block,y,x) = (blockid_PSMCT16S(block, 0, x, y) << 7) + column;
                page_PSMCT16Z.get(block,y,x) = (blockid_PSMCT16Z(block, 0, x, y) << 7) + column;
                page_PSMCT16SZ.get(block,y,x) = (blockid_PSMCT16SZ(block, 0, x, y) << 7) + column;
            }
        }

        for (int y = 0; y < 64; y++)
        {
            for (int x = 0; x < 128; x++)
                page_PSMCT8.get(block,y,x) = (blockid_PSMCT8(block, 0, x, y) << 8) + columnTable8[y & 0xF][x & 0xF];
        }

        for (int y = 0; y < 128; y++)
        {
            for (int x = 0; x < 128; x++)
                page_PSMCT4.get(block,y,x) = (blockid_PSMCT4(block, 0, x, y) << 9) + columnTable4[y & 15][x & 31];
        }
    }

    //Initialize lookup table used for LOD calculation
    for (int i = 0; i < 32768; i++)
    {
        uint32_t value = i * 0x10000;
        int exp = (value >> 23) & 0xFF;
        float calculation;
        if (exp == 0)
        {
            //arbitrary "large" value
            calculation = 1000.0;
        }
        else if (exp == 0xFF)
        {
            //arbitrary large negative value - do this to prevent NaNs
            calculation = -1000.0;
        }
        else
            calculation = log2(1.0f / *(float*)&value);

        //L has four possible values, so we need four different shifts
        log2_lookup[i][0] = calculation;
        log2_lookup[i][1] = ldexp(calculation, 1);
        log2_lookup[i][2] = ldexp(calculation, 2);
        log2_lookup[i][3] = ldexp(calculation, 3);
    }

    jit_draw_pixel_heap.flush_all_blocks();
    jit_tex_lookup_heap.flush_all_blocks();

    thread = std::thread(&GraphicsSynthesizerThread::event_loop, this);
}

GraphicsSynthesizerThread::~GraphicsSynthesizerThread()
{
    delete[] local_mem;
    delete message_queue;
    delete return_queue;
}

void GraphicsSynthesizerThread::wait_for_return(GSReturn type, GSReturnMessage &data)
{
    printf("[GS] Waiting for return\n");

    while (true)
    {
        if (return_queue->pop(data))
        {
            if (data.type == death_error_t)
            {
                auto p = data.payload.death_error_payload;
                auto data = std::string(p.error_str);
                delete[] p.error_str;
                Errors::die(data.c_str());
                //There's probably a better way of doing this
                //but I don't know how to make RAII work across threads properly
            }

            if (data.type == type)
                return;
            else
            {
                if (return_queue->was_empty())
                {
                    //Last message in the queue, so we don't want this one so we need to sleep
                    return_queue->push(data); //Put it back on the queue, something else probably wants it
                    printf("[GS] Waiting for return message, pushed last message on to queue type %d expecting %d\n", data.type, type);
                    std::unique_lock<std::mutex> lk(data_mutex);
                    notifier.wait(lk, [this] {return recieve_data; });
                    recieve_data = false;
                }
                else
                    return_queue->push(data); //Put it back on the queue, something else probably wants it
            }
              //Errors::die("[GS] return message expected %d but was %d!\n", type, data.type);
        }
        else
        {
            printf("[GS] No Messages, waiting for return message\n");
            std::unique_lock<std::mutex> lk(data_mutex);
            notifier.wait(lk, [this] {return recieve_data;});
            recieve_data = false;
        }
    }
}

void GraphicsSynthesizerThread::send_message(GSMessage message)
{
    //printf("[GS] Notifying gs thread of new data\n");
    message_queue->push(message);
    send_data = true;
}

void GraphicsSynthesizerThread::wake_thread()
{
    printf("[GS] Waking GS Thread\n");
    std::unique_lock<std::mutex> lk(data_mutex);
    notifier.notify_one();
}

void GraphicsSynthesizerThread::reset_fifos()
{
    if (!message_queue)
        message_queue = new gs_fifo();
    if (!return_queue)
        return_queue = new gs_return_fifo();

    GSReturnMessage data;
    while (return_queue->pop(data));

    GSMessage data2;
    while (message_queue->pop(data2));
}

void GraphicsSynthesizerThread::exit()
{
    if (thread.joinable())
    {
        GSMessagePayload payload;
        payload.no_payload = {0};
        
        send_message({ GSCommand::die_t, payload });
        wake_thread();
        thread.join();
    }
}

void GraphicsSynthesizerThread::event_loop()
{
    printf("[GS_t] Starting GS Thread\n");

    reset();

    bool gsdump_recording = false;
    ofstream gsdump_file;

    try
    {
        while (true)
        {
            GSMessage data;

            if (message_queue->pop(data))
            {
                if (gsdump_recording)
                    gsdump_file.write((char*)&data, sizeof(data));

                switch (data.type)
                {
                    case write64_t:
                    {
                        auto p = data.payload.write64_payload;
                        write64(p.addr, p.value);
                        break;
                    }
                    case write64_privileged_t:
                    {
                        auto p = data.payload.write64_payload;
                        reg.write64_privileged(p.addr, p.value);
                        break;
                    }
                    case write32_privileged_t:
                    {
                        auto p = data.payload.write32_payload;
                        reg.write32_privileged(p.addr, p.value);
                        break;
                    }
                    case set_rgba_t:
                    {
                        auto p = data.payload.rgba_payload;
                        set_RGBA(p.r, p.g, p.b, p.a, p.q);
                        break;
                    }
                    case set_st_t:
                    {
                        auto p = data.payload.st_payload;
                        set_ST(p.s, p.t);
                        break;
                    }
                    case set_uv_t:
                    {
                        auto p = data.payload.uv_payload;
                        set_UV(p.u, p.v);
                        break;
                    }
                    case set_xyz_t:
                    {
                        auto p = data.payload.xyz_payload;
                        set_XYZ(p.x, p.y, p.z, p.drawing_kick);
                        break;
                    }
                    case set_xyzf_t:
                    {
                        auto p = data.payload.xyzf_payload;
                        set_XYZF(p.x, p.y, p.z, p.fog, p.drawing_kick);
                        break;
                    }
                        break;
                    case set_crt_t:
                    {
                        auto p = data.payload.crt_payload;
                        reg.set_CRT(p.interlaced, p.mode, p.frame_mode);
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
                        render_CRT(p.target);
                        GSReturnMessagePayload return_payload;
                        return_payload.no_payload = { 0 };
                        return_queue->push({ GSReturn::render_complete_t,return_payload });
                        std::unique_lock<std::mutex> lk(data_mutex);
                        recieve_data = true;
                        notifier.notify_one();
                        break;
                    }
                    case assert_finish_t:
                        reg.assert_FINISH();
                        break;
                    case assert_vsync_t:
                        reg.assert_VSYNC();
                        break;
                    case set_vblank_t:
                    {
                        auto p = data.payload.vblank_payload;
                        reg.set_VBLANK(p.vblank);
                        break;
                    }
                    case memdump_t:
                    {
                        auto p = data.payload.render_payload;

                        while (!p.target_mutex->try_lock())
                        {
                            printf("[GS_t] buffer lock failed!\n");
                            std::this_thread::yield();
                        }
                        std::lock_guard<std::mutex> lock(*p.target_mutex, std::adopt_lock);
                        uint16_t width, height;
                        memdump(p.target, width, height);
                        GSReturnMessagePayload return_payload;
                        return_payload.xy_payload = { width, height };
                        return_queue->push({ GSReturn::gsdump_render_partial_done_t,return_payload });
                        std::unique_lock<std::mutex> lk(data_mutex);
                        recieve_data = true;
                        notifier.notify_one();
                        break;
                    }
                    case die_t:
                        return;
                    case load_state_t:
                    {
                        load_state(data.payload.load_state_payload.state);
                        GSReturnMessagePayload return_payload;
                        return_payload.no_payload = { 0 };
                        return_queue->push({ GSReturn::load_state_done_t,return_payload });
                        std::unique_lock<std::mutex> lk(data_mutex);
                        recieve_data = true;
                        notifier.notify_one();
                        break;
                    }
                    case save_state_t:
                    {
                        save_state(data.payload.save_state_payload.state);
                        GSReturnMessagePayload return_payload;
                        return_payload.no_payload = { 0 };
                        return_queue->push({ GSReturn::save_state_done_t,return_payload });
                        recieve_data = true;
                        notifier.notify_one();
                        break;
                    }
                    case gsdump_t:
                    {
                        printf("gs dump! ");
                        if (!gsdump_recording)
                        {
                            printf("(start)\n");
                            gsdump_file.open("gsdump.gsd", ios::out | ios::binary);
                            if (!gsdump_file.is_open())
                                Errors::die("gs dump file failed to open");
                            gsdump_recording = true;
                            save_state(&gsdump_file);
                            gsdump_file.write((char*)&reg, sizeof(reg));//this is for the emuthread's gs faker
                        }
                        else
                        {
                            printf("(end)\n");
                            gsdump_file.close();
                            gsdump_recording = false;
                        }
                        break;
                    }
                    case request_local_host_tx:
                    {
                        GSReturnMessagePayload return_payload;
                        return_payload.data_payload.quad_data = local_to_host();
                        return_queue->push({ GSReturn::local_host_transfer, return_payload });
                        std::unique_lock<std::mutex> lk(data_mutex);
                        recieve_data = true;
                        notifier.notify_one();
                        break;
                    }
                    default:
                        Errors::die("corrupted command sent to GS thread");
                }
            }
            else
            {
                printf("GS Thread: No messages waiting, going to sleep\n");
                std::unique_lock<std::mutex> lk(data_mutex);
                notifier.wait(lk, [this] {return send_data;});
                send_data = false;
            }
        }
    }
    catch (Emulation_error &e)
    {
        GSReturnMessagePayload return_payload;
        char* copied_string = new char[ERROR_STRING_MAX_LENGTH];
        strncpy(copied_string, e.what(), ERROR_STRING_MAX_LENGTH);
        return_payload.death_error_payload.error_str = { copied_string };
        return_queue->push({ GSReturn::death_error_t, return_payload });
        recieve_data = true;
        notifier.notify_one();
    }
}

void GraphicsSynthesizerThread::reset()
{
    if (!local_mem)
        local_mem = new uint8_t[1024 * 1024 * 4];

    pixels_transferred = 0;
    num_vertices = 0;
    frame_count = 0;

    COLCLAMP = true;

    reg.reset();
    context1.reset();
    context2.reset();
    PSMCT24_color = 0;
    PSMCT24_unpacked_count = 0;
    current_ctx = &context1;
    current_PRMODE = &PRIM;
    PRIM.reset();
    PRMODE.reset();

    jit_draw_pixel_func = nullptr;
    jit_tex_lookup_func = nullptr;

    jit_tex_lookup_heap.flush_all_blocks();
    jit_draw_pixel_heap.flush_all_blocks();

    reset_fifos();
}

void GraphicsSynthesizerThread::memdump(uint32_t* target, uint16_t& width, uint16_t& height)
{
    SCISSOR s = current_ctx->scissor;
    width = min(static_cast<uint16_t>(s.x2 - s.x1), (uint16_t)current_ctx->frame.width);
    height = min(static_cast<uint16_t>(s.y2 - s.y1), (uint16_t)480);
    int yy = 0;
    for (int y = s.y1; y < s.y2; y++)
    {
        int xx = 0;
        for (int x = s.x1; x < s.x2; x++)
        {
            uint32_t value = read_PSMCT32_block(current_ctx->frame.base_pointer, current_ctx->frame.width, x, y);
            target[xx + (yy * width)] = value | 0xFF000000;
            xx++;
            if (xx > width) break;
        }
        yy++;
        if (yy > height) break;
    }
}

uint32_t convert_color_up(uint16_t col)
{
    uint32_t r = (col & 0x1F)<<3;
    uint32_t g = ((col>>5) & 0x1F)<<3;
    uint32_t b = ((col>>10) & 0x1F)<<3;
    uint32_t a = ((col >> 15) & 0x1) << 7;
    return (r | (g << 8) | (b << 16) | (a << 24));
}

uint16_t convert_color_down(uint32_t col)
{
    uint32_t r = (col & 0xFF) >> 3;
    uint32_t g = ((col >> 8) & 0xFF) >> 3;
    uint32_t b = ((col >> 16) & 0xFF) >> 3;
    uint32_t a = ((col >> 24) & 0xFF) >> 7;
    return (r | (g << 5) | (b << 10) | (a << 15));
}

uint32_t GraphicsSynthesizerThread::get_CRT_color(DISPFB &dispfb, uint32_t x, uint32_t y)
{
    switch (dispfb.format)
    {
        case 0x0:
            return read_PSMCT32_block(dispfb.frame_base * 4, dispfb.width, x, y);
        case 0x1://PSMCT24
            return (read_PSMCT32_block(dispfb.frame_base * 4, dispfb.width, x, y) & 0xFFFFFF) | (1 << 31);
        case 0x2:
            return convert_color_up(read_PSMCT16_block(dispfb.frame_base * 4, dispfb.width, x, y));
        case 0xA:
            return convert_color_up(read_PSMCT16S_block(dispfb.frame_base * 4, dispfb.width, x, y));
        default:
            Errors::die("Unknown framebuffer format (%x)", dispfb.format);
            return 0;
    }
}

void GraphicsSynthesizerThread::render_single_CRT(uint32_t *target, DISPFB &dispfb, DISPLAY &display)
{
    int width = display.width >> 2;
    for (int y = 0; y < display.height; y++)
    {
        for (int x = 0; x < width; x++)
        {
            int pixel_x = x;
            int pixel_y = y;

            if (reg.SMODE2.frame_mode && reg.SMODE2.interlaced)
                pixel_y *= 2;
            if (pixel_x >= width || pixel_y >= display.height)
                continue;
            uint32_t scaled_x = dispfb.x + x;
            uint32_t scaled_y = dispfb.y + y;
            scaled_x = (scaled_x * dispfb.width) / width;
            uint32_t value = get_CRT_color(dispfb, scaled_x, scaled_y);

            target[pixel_x + (pixel_y * width)] = value | 0xFF000000;

            if (reg.SMODE2.frame_mode && reg.SMODE2.interlaced)
                target[pixel_x + ((pixel_y + 1) * width)] = value | 0xFF000000;
        }
    }
}

void GraphicsSynthesizerThread::render_CRT(uint32_t* target)
{
    //Circuit 1 only
    if (reg.PMODE.circuit1 && !reg.PMODE.circuit2)
        render_single_CRT(target, reg.DISPFB1, reg.DISPLAY1);
    //Circuit 2 only
    else if (!reg.PMODE.circuit1 && reg.PMODE.circuit2)
        render_single_CRT(target, reg.DISPFB2, reg.DISPLAY2);
    //Circuits 1 and 2 (merge circuit)
    else
    {
        int width = reg.DISPLAY1.width >> 2;
        for (int y = 0; y < reg.DISPLAY1.height; y++)
        {
            for (int x = 0; x < width; x++)
            {
                int pixel_x = x;
                int pixel_y = y;

                if (reg.SMODE2.frame_mode && reg.SMODE2.interlaced)
                    pixel_y *= 2;
                if (pixel_x >= width || pixel_y >= reg.DISPLAY1.height)
                    continue;
                uint32_t scaled_x1 = reg.DISPFB1.x + x;
                uint32_t scaled_x2 = reg.DISPFB2.x + x;
                uint32_t scaled_y1 = reg.DISPFB1.y + y;
                uint32_t scaled_y2 = reg.DISPFB2.y + y;

                scaled_x1 = (scaled_x1 * reg.DISPFB1.width) / width;
                scaled_x2 = (scaled_x2 * reg.DISPFB2.width) / width;

                uint32_t output1 = get_CRT_color(reg.DISPFB1, scaled_x1, scaled_y1);
                uint32_t output2 = get_CRT_color(reg.DISPFB2, scaled_x2, scaled_y2);

                if (reg.PMODE.blend_with_bg)
                    output2 = reg.BGCOLOR;

                int alpha = (output1 >> 24) * 2;
                if (reg.PMODE.use_ALP)
                    alpha = reg.PMODE.ALP;

                if (alpha > 0xFF)
                    alpha = 0xFF;

                uint32_t r1, g1, b1, r2, g2, b2, r, g, b;
                uint32_t final_color;
                r1 = output1 & 0xFF; r2 = output2 & 0xFF;
                g1 = (output1 >> 8) & 0xFF; g2 = (output2 >> 8) & 0xFF;
                b1 = (output1 >> 16) & 0xFF; b2 = (output2 >> 16) & 0xFF;

                r = ((r1 * alpha) + (r2 * (0xFF - alpha))) >> 8;
                if (r > 0xFF)
                    r = 0xFF;

                g = ((g1 * alpha) + (g2 * (0xFF - alpha))) >> 8;
                if (g > 0xFF)
                    g = 0xFF;

                b = ((b1 * alpha) + (b2 * (0xFF - alpha))) >> 8;
                if (b > 0xFF)
                    b = 0xFF;

                final_color = 0xFF000000 | r | (g << 8) | (b << 16);

                target[pixel_x + (pixel_y * width)] = final_color;

                if (reg.SMODE2.frame_mode && reg.SMODE2.interlaced)
                    target[pixel_x + ((pixel_y + 1) * width)] = final_color;
            }
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
            prim_type = value & 0x7;
            PRIM.gourand_shading = value & (1 << 3);
            PRIM.texture_mapping = value & (1 << 4);
            PRIM.fog = value & (1 << 5);
            PRIM.alpha_blend = value & (1 << 6);
            PRIM.antialiasing = value & (1 << 7);
            PRIM.use_UV = value & (1 << 8);
            PRIM.use_context2 = value & (1 << 9);

            if (current_PRMODE == &PRIM)
            {
                if (PRIM.use_context2)
                    current_ctx = &context2;
                else
                    current_ctx = &context1;
            }
            PRIM.fix_fragment_value = value & (1 << 10);
            num_vertices = 0;
            printf("[GS_t] PRIM: $%08X\n", value);
            update_draw_pixel_state();
            update_tex_lookup_state();
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
            FOG = (value >> 56) & 0xFF;
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
            reload_clut(context1);
            if (current_ctx == &context1)
                update_tex_lookup_state();
            break;
        case 0x0007:
            context2.set_tex0(value);
            reload_clut(context2);
            if (current_ctx == &context2)
                update_tex_lookup_state();
            break;
        case 0x0008:
            context1.set_clamp(value);
            if (current_ctx == &context1)
                update_tex_lookup_state();
            break;
        case 0x0009:
            context2.set_clamp(value);
            if (current_ctx == &context2)
                update_tex_lookup_state();
            break;
        case 0x000A:
            FOG = (value >> 56) & 0xFF;
            break;
        case 0x000C:
            //XYZ3F
            current_vtx.x = value & 0xFFFF;
            current_vtx.y = (value >> 16) & 0xFFFF;
            current_vtx.z = (value >> 32) & 0xFFFFFF;
            FOG = (value >> 56) & 0xFF;
            vertex_kick(false);
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
            if (current_ctx == &context1)
                update_tex_lookup_state();
            break;
        case 0x0015:
            context2.set_tex1(value);
            if (current_ctx == &context2)
                update_tex_lookup_state();
            break;
        case 0x0016:
            context1.set_tex2(value);
            reload_clut(context1);
            if (current_ctx == &context1)
                update_tex_lookup_state();
            break;
        case 0x0017:
            context2.set_tex2(value);
            reload_clut(context2);
            if (current_ctx == &context2)
                update_tex_lookup_state();
            break;
        case 0x0018:
            context1.set_xyoffset(value);
            break;
        case 0x0019:
            context2.set_xyoffset(value);
            break;
        case 0x001A:
            printf("PRMODECNT: $%08X\n", value);
            {
                if (value & 0x1)
                    current_PRMODE = &PRIM;
                else
                    current_PRMODE = &PRMODE;

                if (current_PRMODE->use_context2)
                    current_ctx = &context2;
                else
                    current_ctx = &context1;
            }
            update_draw_pixel_state();
            update_tex_lookup_state();
            break;
        case 0x001B:
            printf("PRMODE: $%08X\n", value);
            PRMODE.gourand_shading = value & (1 << 3);
            PRMODE.texture_mapping = value & (1 << 4);
            PRMODE.fog = value & (1 << 5);
            PRMODE.alpha_blend = value & (1 << 6);
            PRMODE.antialiasing = value & (1 << 7);
            PRMODE.use_UV = value & (1 << 8);
            PRMODE.use_context2 = value & (1 << 9);

            if (current_PRMODE == &PRMODE)
            {
                if (PRMODE.use_context2)
                    current_ctx = &context2;
                else
                    current_ctx = &context1;
            }
            PRMODE.fix_fragment_value = value & (1 << 10);
            update_draw_pixel_state();
            update_tex_lookup_state();
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
        case 0x0022:
            SCANMSK = value & 0x3;
            break;
        case 0x0034:
            context1.set_miptbl1(value);
            break;
        case 0x0035:
            context2.set_miptbl1(value);
            break;
        case 0x0036:
            context1.set_miptbl2(value);
            break;
        case 0x0037:
            context2.set_miptbl2(value);
            break;
        case 0x003B:
            printf("TEXA: $%08X_%08X\n", value >> 32, value);
            TEXA.alpha0 = value & 0xFF;
            TEXA.trans_black = value & (1 << 15);
            TEXA.alpha1 = (value >> 32) & 0xFF;
            update_tex_lookup_state();
            break;
        case 0x003D:
            printf("FOGCOL: $%08X\n", value);
            FOGCOL.r = value & 0xFF;
            FOGCOL.g = (value >> 8) & 0xFF;
            FOGCOL.b = (value >> 16) & 0xFF;
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
            if (current_ctx == &context1)
                update_draw_pixel_state();
            break;
        case 0x0043:
            context2.set_alpha(value);
            if (current_ctx == &context2)
                update_draw_pixel_state();
            break;
        case 0x0044:
            for (int y = 0; y < 4; y++)
            {
                for (int x = 0; x < 4; x++)
                {
                    uint8_t element = value >> ((x + (y * 4)) * 4);
                    element &= 0x7;
                    dither_mtx[y][x] = element;
                }
            }
            break;
        case 0x0045:
            DTHE = value & 0x1;
            update_draw_pixel_state();
            break;
        case 0x0046:
            COLCLAMP = value & 0x1;
            update_draw_pixel_state();
            break;
        case 0x0047:
            context1.set_test(value);
            if (current_ctx == &context1)
                update_draw_pixel_state();
            break;
        case 0x0048:
            context2.set_test(value);
            if (current_ctx == &context2)
                update_draw_pixel_state();
            break;
        case 0x0049:
            printf("[GS_t] PABE: $%02X\n", value);
            PABE = value & 0x1;
            update_draw_pixel_state();
            break;
        case 0x004A:
            context1.FBA = value & 0x1;
            if (current_ctx == &context1)
                update_draw_pixel_state();
            break;
        case 0x004B:
            context2.FBA = value & 0x1;
            if (current_ctx == &context2)
                update_draw_pixel_state();
            break;
        case 0x004C:
            context1.set_frame(value);
            if (current_ctx == &context1)
                update_draw_pixel_state();
            break;
        case 0x004D:
            context2.set_frame(value);
            if (current_ctx == &context2)
                update_draw_pixel_state();
            break;
        case 0x004E:
            context1.set_zbuf(value);
            if (current_ctx == &context1)
                update_draw_pixel_state();
            break;
        case 0x004F:
            context2.set_zbuf(value);
            if (current_ctx == &context2)
                update_draw_pixel_state();
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
                TRXPOS.int_dest_y = TRXPOS.dest_y;
                TRXPOS.int_source_y = TRXPOS.dest_y;
                PSMCT24_unpacked_count = 0;
                PSMCT24_color = 0;
                //printf("Transfer addr: $%08X\n", transfer_addr);
                if (TRXDIR == 2)
                {
                    //VRAM-to-VRAM transfer
                    //More than likely not instantaneous
                    local_to_local();
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

void GraphicsSynthesizerThread::set_RGBA(uint8_t r, uint8_t g, uint8_t b, uint8_t a, float q)
{
    RGBAQ.r = r;
    RGBAQ.g = g;
    RGBAQ.b = b;
    RGBAQ.a = a;
    RGBAQ.q = q;
}

void GraphicsSynthesizerThread::set_ST(uint32_t s, uint32_t t)
{
    ST.s = *(float*)&s;
    ST.t = *(float*)&t;
    printf("ST: (%f, %f) ($%08X $%08X)\n", ST.s, ST.t, s, t);
}

void GraphicsSynthesizerThread::set_UV(uint16_t u, uint16_t v)
{
    UV.u = u;
    UV.v = v;
    printf("UV: ($%04X, $%04X)\n", UV.u, UV.v);
}

void GraphicsSynthesizerThread::set_XYZ(uint32_t x, uint32_t y, uint32_t z, bool drawing_kick)
{
    current_vtx.x = x;
    current_vtx.y = y;
    current_vtx.z = z;
    vertex_kick(drawing_kick);
}

void GraphicsSynthesizerThread::set_XYZF(uint32_t x, uint32_t y, uint32_t z, uint8_t fog, bool drawing_kick)
{
    current_vtx.x = x;
    current_vtx.y = y;
    current_vtx.z = z;
    FOG = fog;
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
    return block + ((y >> 2) & ~0x1f) * (width >> 7) + ((x >> 2) & ~0x1f) + blockTable4[(y >> 4) & 7][(x >> 5) & 3];
}

uint32_t addr_PSMCT32(uint32_t block, uint32_t width, uint32_t x, uint32_t y)
{
    uint32_t page = ((block >> 5) + (y >> 5) * width + (x >> 6));
    //uint32_t addr = (page << 11) + page_PSMCT32[block & 0x1F][y & 0x1F][x & 0x3F];
    uint32_t addr = (page << 11) + page_PSMCT32.get(block & 0x1F, y & 0x1F, x & 0x3F);
    return (addr << 2) & 0x003FFFFC;
}

uint32_t addr_PSMCT32Z(uint32_t block, uint32_t width, uint32_t x, uint32_t y)
{
    uint32_t page = ((block >> 5) + (y >> 5) * width + (x >> 6));
    uint32_t addr = (page << 11) + page_PSMCT32Z.get(block & 0x1F, y & 0x1F, x & 0x3F);
    return (addr << 2) & 0x003FFFFC;
}

uint32_t addr_PSMCT16(uint32_t block, uint32_t width, uint32_t x, uint32_t y)
{
    uint32_t page = ((block >> 5) + (y >> 6) * width + (x >> 6));
    uint32_t addr = (page << 12) + page_PSMCT16.get(block & 0x1F, y & 0x3F, x & 0x3F);
    return (addr << 1) & 0x003FFFFE;
}

uint32_t addr_PSMCT16S(uint32_t block, uint32_t width, uint32_t x, uint32_t y)
{
    uint32_t page = ((block >> 5) + (y >> 6) * width + (x >> 6));
    uint32_t addr = (page << 12) + page_PSMCT16S.get(block & 0x1F, y & 0x3F, x & 0x3F);
    return (addr << 1) & 0x003FFFFE;
}

uint32_t addr_PSMCT16Z(uint32_t block, uint32_t width, uint32_t x, uint32_t y)
{
    uint32_t page = ((block >> 5) + (y >> 6) * width + (x >> 6));
    uint32_t addr = (page << 12) + page_PSMCT16Z.get(block & 0x1F, y & 0x3F, x & 0x3F);
    return (addr << 1) & 0x003FFFFE;
}

uint32_t addr_PSMCT16SZ(uint32_t block, uint32_t width, uint32_t x, uint32_t y)
{
    uint32_t page = ((block >> 5) + (y >> 6) * width + (x >> 6));
    uint32_t addr = (page << 12) + page_PSMCT16SZ.get(block & 0x1F, y & 0x3F, x & 0x3F);
    return (addr << 1) & 0x003FFFFE;
}

uint32_t addr_PSMCT8(uint32_t block, uint32_t width, uint32_t x, uint32_t y)
{
    uint32_t page = ((block >> 5) + (y >> 6) * (width >> 1) + (x >> 7));
    uint32_t addr = (page << 13) + page_PSMCT8.get(block & 0x1F, y & 0x3F, x & 0x7F);
    return addr & 0x003FFFFF;
}

uint32_t addr_PSMCT4(uint32_t block, uint32_t width, uint32_t x, uint32_t y)
{
    uint32_t page = ((block >> 5) + (y >> 7) * (width >> 1) + (x >> 7));
    uint32_t addr = (page << 14) + page_PSMCT4.get(block & 0x1F, y & 0x7F, x & 0x7F);
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
    return (local_mem[addr >> 1] >> ((addr & 1) << 2)) & 0x0f;
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

void GraphicsSynthesizerThread::write_PSMCT24_block(uint32_t base, uint32_t width, uint32_t x, uint32_t y, uint32_t value)
{
    uint32_t addr = addr_PSMCT32(base / 256, width / 64, x, y);
    uint32_t old_mem = *(uint32_t*)&local_mem[addr];
    value &= 0xFFFFFF;
    *(uint32_t*)&local_mem[addr] = (old_mem & 0xFF000000) | value;
}

void GraphicsSynthesizerThread::write_PSMCT24Z_block(uint32_t base, uint32_t width, uint32_t x, uint32_t y, uint32_t value)
{
    uint32_t addr = addr_PSMCT32Z(base / 256, width / 64, x, y);
    uint32_t old_mem = *(uint32_t*)&local_mem[addr];
    value &= 0xFFFFFF;
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
    int shift = (addr & 1) << 2;
    addr >>= 1;

    local_mem[addr] = (uint8_t)((local_mem[addr] & (0xf0 >> shift)) | ((value & 0x0f) << shift));
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
    current_vtx.fog = FOG;
    vtx_queue[0] = current_vtx;
    //printf("Vkick: (%d, %d, %d)\n", current_vtx.x, current_vtx.y, current_vtx.z);

    num_vertices++;
    bool request_draw_kick = false;
    switch (prim_type)
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
            Errors::die("[GS] Unrecognized primitive %d\n", prim_type);
    }
    if (drawing_kick && request_draw_kick)
        render_primitive();
}

void GraphicsSynthesizerThread::render_primitive()
{
#ifdef GS_JIT
    jit_draw_pixel_func = get_jitted_draw_pixel(draw_pixel_state);
    jit_tex_lookup_func = get_jitted_tex_lookup(tex_lookup_state);
#endif
    switch (prim_type)
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
            render_triangle2();
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

uint32_t GraphicsSynthesizerThread::lookup_frame_color(int32_t x, int32_t y)
{
    if (frame_color_looked_up)
    {
        return frame_color;
    }

    switch (current_ctx->frame.format)
    {
        case 0x0:
            frame_color = read_PSMCT32_block(current_ctx->frame.base_pointer, current_ctx->frame.width, x, y);
            break;
        case 0x1://24
            frame_color = read_PSMCT32_block(current_ctx->frame.base_pointer, current_ctx->frame.width, x, y)
                & 0xFFFFFF;
            frame_color |= 1 << 31;
            break;
        case 0x2:
            frame_color = convert_color_up(read_PSMCT16_block(current_ctx->frame.base_pointer, current_ctx->frame.width, x, y));
            break;
        case 0xA:
            frame_color = convert_color_up(read_PSMCT16S_block(current_ctx->frame.base_pointer, current_ctx->frame.width, x, y));
            break;
        case 0x30:
            frame_color = read_PSMCT32Z_block(current_ctx->frame.base_pointer, current_ctx->frame.width, x, y);
            break;
        case 0x31://24Z
            frame_color = read_PSMCT32Z_block(current_ctx->frame.base_pointer, current_ctx->frame.width, x, y)
                & 0xFFFFFF;
            frame_color |= 1 << 31;
            break;
        case 0x32:
            frame_color = convert_color_up(read_PSMCT16Z_block(current_ctx->frame.base_pointer, current_ctx->frame.width, x, y));
            break;
        case 0x3A:
            frame_color = convert_color_up(read_PSMCT16SZ_block(current_ctx->frame.base_pointer, current_ctx->frame.width, x, y));
            break;
        default:
            Errors::die("Unknown FRAME format (%x) read attempted", current_ctx->frame.format);
            break;
    }
    frame_color_looked_up = true;

    return frame_color;
}

void GraphicsSynthesizerThread::draw_pixel(int32_t x, int32_t y, uint32_t z, RGBAQ_REG& color)
{
    frame_color_looked_up = false;
    x >>= 4;
    y >>= 4;

    //SCANMSK prohibits drawing on even or odd y-coordinates
    if (SCANMSK == 2 && (y & 0x1) == 0)
        return;
    else if (SCANMSK == 3 && (y & 0x1) == 1)
        return;
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

    uint32_t final_color = 0;

    if (test->dest_alpha_test && !(current_ctx->frame.format & 0x1))
    {
        bool alpha = lookup_frame_color(x, y) & (1 << 31);
        if (test->dest_alpha_method ^ alpha)
            return;
    }

    //PABE - MSB of source alpha must be set to enable alpha blending
    if (current_PRMODE->alpha_blend && (!PABE || (color.a & 0x80)))
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
                r1 = lookup_frame_color(x, y) & 0xFF;
                g1 = (lookup_frame_color(x, y) >> 8) & 0xFF;
                b1 = (lookup_frame_color(x, y) >> 16) & 0xFF;
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
                r2 = lookup_frame_color(x, y) & 0xFF;
                g2 = (lookup_frame_color(x, y) >> 8) & 0xFF;
                b2 = (lookup_frame_color(x, y) >> 16) & 0xFF;
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
                alpha = lookup_frame_color(x, y) >> 24;
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
                cr = lookup_frame_color(x, y) & 0xFF;
                cg = (lookup_frame_color(x, y) >> 8) & 0xFF;
                cb = (lookup_frame_color(x, y) >> 16) & 0xFF;
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

        //Color values are 9-bit after an alpha blending operation
        fb = (((fb * (int)alpha) >> 7) + cb);
        fg = (((fg * (int)alpha) >> 7) + cg);
        fr = (((fr * (int)alpha) >> 7) + cr);

        //Dithering
        if (DTHE)
        {
            uint8_t dither = dither_mtx[y % 4][x % 4];
            uint8_t dither_amount = dither & 0x3;
            if (dither & 0x4)
            {
                fb -= dither_amount;
                fg -= dither_amount;
                fr -= dither_amount;
            }
            else
            {
                fb += dither_amount;
                fg += dither_amount;
                fr += dither_amount;
            }
        }

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
        //Dithering
        if (DTHE)
        {
            uint8_t dither = dither_mtx[y % 4][x % 4];
            uint8_t dither_amount = dither & 0x3;
            if (dither & 0x4)
            {
                color.b -= dither_amount;
                color.g -= dither_amount;
                color.r -= dither_amount;
            }
            else
            {
                color.b += dither_amount;
                color.g += dither_amount;
                color.r += dither_amount;
            }

            if (COLCLAMP)
            {
                if (color.b < 0)
                    color.b = 0;
                if (color.g < 0)
                    color.g = 0;
                if (color.r < 0)
                    color.r = 0;
                if (color.b > 0xFF)
                    color.b = 0xFF;
                if (color.g > 0xFF)
                    color.g = 0xFF;
                if (color.r > 0xFF)
                    color.r = 0xFF;
            }
            else
            {
                color.b &= 0xFF;
                color.g &= 0xFF;
                color.r &= 0xFF;
            }
        }
        final_color |= color.a << 24;
        final_color |= color.b << 16;
        final_color |= color.g << 8;
        final_color |= color.r;
    }

    if (update_frame)
    {
        if (!update_alpha)
        {
            uint8_t alpha = lookup_frame_color(x, y) >> 24;
            final_color &= 0x00FFFFFF;
            final_color |= alpha << 24;
        }

        //FBA performs "alpha correction" - MSB of alpha is always set when writing to frame buffer
        final_color |= current_ctx->FBA << 31;

        uint32_t mask = current_ctx->frame.mask;
        final_color = (final_color & ~mask) | (lookup_frame_color(x, y) & mask);

        //printf("[GS_t] Write $%08X (%d, %d)\n", final_color, x, y);
        switch (current_ctx->frame.format)
        {
            case 0x0:
                write_PSMCT32_block(current_ctx->frame.base_pointer, current_ctx->frame.width, x, y, final_color);
                break;
            case 0x1:
                write_PSMCT24_block(current_ctx->frame.base_pointer, current_ctx->frame.width, x, y, final_color);
                break;
            case 0x2:
                write_PSMCT16_block(current_ctx->frame.base_pointer, current_ctx->frame.width, x, y, convert_color_down(final_color));
                break;
            case 0xA:
                write_PSMCT16S_block(current_ctx->frame.base_pointer, current_ctx->frame.width, x, y, convert_color_down(final_color));
                break;
            case 0x30:
                write_PSMCT32Z_block(current_ctx->frame.base_pointer, current_ctx->frame.width, x, y, final_color);
                break;
            case 0x31:
                write_PSMCT24Z_block(current_ctx->frame.base_pointer, current_ctx->frame.width, x, y, final_color);
                break;
            case 0x32:
                write_PSMCT16Z_block(current_ctx->frame.base_pointer, current_ctx->frame.width, x, y, convert_color_down(final_color));
                break;
            case 0x3A:
                write_PSMCT16SZ_block(current_ctx->frame.base_pointer, current_ctx->frame.width, x, y, convert_color_down(final_color));
                break;
            default:
                Errors::die("Unknown FRAME format (%x) write attempted", current_ctx->frame.format);
                break;
        }
    }
    
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

_noinline(void) GraphicsSynthesizerThread::jit_draw_pixel(int32_t x, int32_t y,
                                               uint32_t z, RGBAQ_REG &color)
{
#ifdef _MSC_VER
    jit_draw_pixel_asm(jit_draw_pixel_func, x, y, z, &color);
#else
    __asm__(
        "pushq %%r12\n\t"
        "pushq %%r13\n\t"
        "pushq %%r14\n\t"
        "pushq %%r15\n\t"
        "subq $8, %%rsp\n\t"
        "mov %0, %%r12d\n\t"
        "mov %1, %%r13d\n\t"
        "mov %2, %%r14d\n\t"
        "mov %3, %%r15\n\t"
        "callq *%4\n\t"
        "addq $8, %%rsp\n\t"
        "popq %%r15\n\t"
        "popq %%r14\n\t"
        "popq %%r13\n\t"
        "popq %%r12\n\t"
                :
                : "r" (x), "r" (y), "r" (z), "X" (*(uint64_t*)(&color)), "r" (jit_draw_pixel_func)

    );
#endif
}

void GraphicsSynthesizerThread::render_point()
{
    Vertex v1 = vtx_queue[0]; v1.to_relative(current_ctx->xyoffset);
    if (v1.x < current_ctx->scissor.x1 || v1.x > current_ctx->scissor.x2 ||
        v1.y < current_ctx->scissor.y1 || v1.y > current_ctx->scissor.y2)
        return;
    printf("[GS_t] Rendering point!\n");
    printf("Coords: (%d, %d, %d)\n", v1.x >> 4, v1.y >> 4, v1.z);
    TexLookupInfo tex_info;
    tex_info.new_lookup = true;
    
    tex_info.vtx_color = v1.rgbaq;
    tex_info.fog = v1.fog;
    tex_info.tex_base = current_ctx->tex0.texture_base;
    tex_info.buffer_width = current_ctx->tex0.width;
    tex_info.tex_width = current_ctx->tex0.tex_width;
    tex_info.tex_height = current_ctx->tex0.tex_height;

    if (current_PRMODE->texture_mapping)
    {
        int32_t u, v;
        calculate_LOD(tex_info);
        if (!current_PRMODE->use_UV)
        {
            u = (v1.s * tex_info.tex_width) / v1.rgbaq.q;
            v = (v1.t * tex_info.tex_height) / v1.rgbaq.q;
            u <<= 4;
            v <<= 4;
        }
        else
        {
            u = (uint32_t) v1.uv.u;
            v = (uint32_t) v1.uv.v;
        }
        tex_lookup(u, v, tex_info);
        draw_pixel(v1.x, v1.y, v1.z, tex_info.tex_color);
    }
    else
    {
        draw_pixel(v1.x, v1.y, v1.z, tex_info.vtx_color);
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
    //int32_t min_y = max(v1.y, (int32_t)current_ctx->scissor.y1);
    int32_t max_x = min(v2.x, (int32_t)current_ctx->scissor.x2);
    //int32_t max_y = min(v2.y, (int32_t)current_ctx->scissor.y2);
    
    TexLookupInfo tex_info;
    tex_info.new_lookup = true;
    tex_info.vtx_color = vtx_queue[0].rgbaq;
    tex_info.tex_base = current_ctx->tex0.texture_base;
    tex_info.buffer_width = current_ctx->tex0.width;
    tex_info.tex_width = current_ctx->tex0.tex_width;
    tex_info.tex_height = current_ctx->tex0.tex_height;

    printf("Coords: (%d, %d, %d) (%d, %d, %d)\n", v1.x >> 4, v1.y >> 4, v1.z, v2.x >> 4, v2.y >> 4, v2.z);

    for (int32_t x = min_x; x < max_x; x += 0x10)
    {
        uint32_t z = interpolate(x, v1.z, v1.x, v2.z, v2.x);
        float t = (x - v1.x)/(float)(v2.x - v1.x);
        int32_t y = v1.y*(1.-t) + v2.y*t;        
        //if (y < min_y || y > max_y)
            //continue;
        tex_info.fog = interpolate(x, v1.fog, v1.x, v2.fog, v2.x);
        if (current_PRMODE->gourand_shading)
        {
            tex_info.vtx_color.r = interpolate(x, v1.rgbaq.r, v1.x, v2.rgbaq.r, v2.x);
            tex_info.vtx_color.g = interpolate(x, v1.rgbaq.g, v1.x, v2.rgbaq.g, v2.x);
            tex_info.vtx_color.b = interpolate(x, v1.rgbaq.b, v1.x, v2.rgbaq.b, v2.x);
            tex_info.vtx_color.a = interpolate(x, v1.rgbaq.a, v1.x, v2.rgbaq.a, v2.x);
        }
        if (current_PRMODE->texture_mapping)
        {
            int32_t u, v;
            calculate_LOD(tex_info);
            if (!current_PRMODE->use_UV)
            {
                float tex_s, tex_t;
                tex_s = interpolate_f(x, v1.s, v1.x, v2.s, v2.x);
                tex_t = interpolate_f(y, v1.t, v1.y, v2.t, v2.y);
                u = (tex_s * tex_info.tex_width) * 16.0;
                v = (tex_t * tex_info.tex_height) * 16.0;
            }
            else
            {
                v = interpolate(y, v2.uv.v, v1.y, v2.uv.v, v2.y);
                u = interpolate(x, v1.uv.u, v1.x, v2.uv.u, v2.x);
            }
            tex_lookup(u, v, tex_info);
            tex_info.vtx_color = tex_info.tex_color;
        }
        if (is_steep)
            draw_pixel(y, x, z, tex_info.vtx_color);
        else
            draw_pixel(x, y, z, tex_info.vtx_color);
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

// this is garbage, possible to go way faster.
static void order3(int32_t a, int32_t b, int32_t c, uint8_t* order)
{
    if(a > b) {
        if(b > c) {
            order[0] = 2;
            order[1] = 1;
            order[2] = 0;
        } else {
            // a > b, b < c
            order[0] = 1;
            if(a > c) {
                order[2] = 0;
                order[1] = 2;
            } else {
                // c > a
                order[2] = 2;
                order[1] = 0;
            }
        }
    } else {
        // b > a
        if(a > c) {
            order[0] = 2;
            order[1] = 0;
            order[2] = 1;
        } else {
            // b > a, c > a
            order[0] = 0; // a
            if(b > c) {
                order[1] = 2;
                order[2] = 1;
            } else {
                order[1] = 1;
                order[2] = 2;
            }
        }
    }
}

void GraphicsSynthesizerThread::render_triangle2() {
    // This is a "scanline" algorithm which reduces flops/pixel
    //  at the cost of a longer setup time.

    // per-pixel work:
    //         add   mult   div
    // stupid  13     7      1
    // old      5     3      1
    // scan     3     0      0
    //
    // per-line work:
    //         add   mult   div
    // stupid  0     0      0
    // old     3     0      0
    // scan    1     1      0   (possible to do 1 add, but rounding issues with floats occur)

    // it divides a triangle like this:

    //             * v0
    //
    //     v1  * ----
    //
    //
    //               * v2

    // where v0, v1, v2 are floating point pixel locations, ordered from low to high
    // (this triangles also has a positive area because the vertices are CCW)


    Vertex unsortedVerts[3]; // vertices in the order they were sent to GS
    unsortedVerts[0] = vtx_queue[2]; unsortedVerts[0].to_relative(current_ctx->xyoffset);
    unsortedVerts[1] = vtx_queue[1]; unsortedVerts[1].to_relative(current_ctx->xyoffset);
    unsortedVerts[2] = vtx_queue[0]; unsortedVerts[2].to_relative(current_ctx->xyoffset);

    if (!current_PRMODE->gourand_shading)
    {
        //Flatten the colors
        unsortedVerts[0].rgbaq.r = unsortedVerts[2].rgbaq.r;
        unsortedVerts[1].rgbaq.r = unsortedVerts[2].rgbaq.r;

        unsortedVerts[0].rgbaq.g = unsortedVerts[2].rgbaq.g;
        unsortedVerts[1].rgbaq.g = unsortedVerts[2].rgbaq.g;

        unsortedVerts[0].rgbaq.b = unsortedVerts[2].rgbaq.b;
        unsortedVerts[1].rgbaq.b = unsortedVerts[2].rgbaq.b;

        unsortedVerts[0].rgbaq.a = unsortedVerts[2].rgbaq.a;
        unsortedVerts[1].rgbaq.a = unsortedVerts[2].rgbaq.a;
    }

    TexLookupInfo tex_info;
    tex_info.new_lookup = true;
    tex_info.tex_base = current_ctx->tex0.texture_base;
    tex_info.buffer_width = current_ctx->tex0.width;
    tex_info.tex_width = current_ctx->tex0.tex_width;
    tex_info.tex_height = current_ctx->tex0.tex_height;


    // fast reject - some games like to spam triangles that don't have any pixels
    if(unsortedVerts[0].y == unsortedVerts[1].y && unsortedVerts[1].y == unsortedVerts[2].y)
    {
        return;
    }


    // sort the three vertices by their y coordinate (increasing)
    uint8_t order[3];
    order3(unsortedVerts[0].y, unsortedVerts[1].y, unsortedVerts[2].y, order);

    // convert all vertex data to floating point, converts position to floating point pixels
    VertexF v0(unsortedVerts[order[0]]);
    VertexF v1(unsortedVerts[order[1]]);
    VertexF v2(unsortedVerts[order[2]]);

    // COMMONLY USED VALUES

    // check if we only have a single triangle like this:
    //     v0  * ----*  v1         v1 *-----* v0
    //                        OR
    //             * v2                 * v2
    // the other orientations of single triangle (where v1 v2 is horizontal) works fine.
    bool lower_tri_only = (v0.y == v1.y);

    // the edge e21 is the edge from v1 -> v2.  So v1 + e21 = v2
    // edges (difference of the ENTIRE vertex properties, not just position)
    VertexF e21 = v2 - v1;
    VertexF e20 = v2 - v0;
    VertexF e10 = v1 - v0;

    // interpolating z (or any value) at point P in a triangle can be done by computing the barycentric coordinates
    // (w0, w1, w2) and using P_z = w0 * v1_z + w1 * v2_z + w2 * v3_z

    // derivative of P_z wrt x and y is constant everywhere
    // dP_z/dx = dw0/dx * v1_z + dw1/dx * v2_z + dw2/dx * v3_z

    // w0 = (v2_y - v3_y)*(P_x - v3_x) + (v3_x - v2_x) * (P_y - v3_y)
    //      ----------------------------------------------------------
    //      (v1_y - v0_y)*(v2_x - v0_x) + (v1_x - v0_x)*(v2_y - v0_y)

    // dw0/dx =           v2_y - v3_y
    //          ------------------------------
    //           the same denominator as above

    // The denominator of this fraction shows up everywhere, so we compute it once.
    float div = (e10.y * e20.x - e10.x * e20.y);

    // If the vertices of the triangle are CCW, the denominator will be negative
    // if the triangle is degenerate (has 0 area), it will be zero.
    bool reversed = div < 0.f;

    if(div == 0.f)
    {
        return;
    }

    // next we need to determine the horizontal scanlines that will have pixels.
    // GS pixel draw condition for scissor
    //   >= minimum value, <= maximum value (draw left/top, but not right/bottom)
    // Our scanline loop
    //   >= minimum value, < maximum value

    // MINIMUM SCISSOR
    // -------------------  y = 0.0 (pixel)
    //
    //  XXXXXXXXXXXXXXXXXX  scissor minimum (y = 0.125 to y = 0.875)
    //                           (round to y = 1.0 - the first scanline we should consider)
    // -------------------- y = 1.0 (pixel)
    int scissorY1 = (current_ctx->scissor.y1 + 15) / 16; // min y coordinate, round up because we don't draw px below scissor
    int scissorX1 = (current_ctx->scissor.x1 + 15) / 16;

    // MAXIMUM SCISSOR
    // -------------------  y = 3.0 (pixel)
    //
    //  XXXXXXXXXXXXXXXXXX  scissor maximum (y = 3.125 to y = 3.875)
    //                           (round to y = 4.0 - will do scanlines at y = 1, 2, 3)
    // -------------------- y = 4.0 (pixel)

    // however, if SCISSOR = 4, we should round that up to 5 because we do want to draw pixels on y = 4 (<= max value)
    int scissorY2 = (current_ctx->scissor.y2 + 16) / 16;
    int scissorX2 = (current_ctx->scissor.x2 + 16) / 16;

    // scissor triangle top/bottoms
    // we can get away with only checking min scissor for tops and max scissors for bottom
    // because it will give negative height triangles for completely scissored half tris
    // and the correct answer for half tris that aren't completely killed
    int upperTop = std::max((int)std::ceil(v0.y), scissorY1); // we draw this
    int upperBot = std::min((int)std::ceil(v1.y), scissorY2); // we don't draw this, (< max value, different from scissor)
    int lowerTop = std::max((int)std::ceil(v1.y), scissorY1); // we draw this
    int lowerBot = std::min((int)std::ceil(v2.y), scissorY2); // we don't draw this, (< max value, different from scissor)


    // compute the derivatives of the weights, like shown in the formula above
    float ndw2dy = e10.x / div; // n is negative
    float dw2dx  = e10.y / div;
    float dw1dy  = e20.x / div;
    float ndw1dx = e20.y / div; // also negative

    // derivatives wrt x and y would normally be computed as dz/dx = dw0/dx * z0 + dw1/dx * z1 + dw2/dx * z2
    // however, w0 + w1 + w2 = 1 so dw0/dx + dw1/dx + dw2/dx = 0,
    //   and we can use some clever rearranging and reuse of the edges to simplify this
    //   we can replace dw0/dx with (-dw1/dx - dw2/dx):

    // dz/dx = dw0/dx*z0 + dw1/dx*z1 + dw2/dx*z2
    // dz/dx = (-dw1/dx - dw2/dx)*z0 + dw1/dx*z1 + dw2/dx*z2
    // dz/dx = -dw1/dx*z0 - dw2/dx*z0 + dw1/dx*z1 + dw2/dx*z2
    // dz/dx = dw1/dx*(z1 - z0) + dw2/dx*(z2 - z0)

    // the value to step per field per x/y pixel
    VertexF dvdx = e20 * dw2dx - e10 * ndw1dx;
    VertexF dvdy = e10 * dw1dy - e20 * ndw2dy;

    // slopes of the edges
    float e20dxdy = e20.x / e20.y;
    float e21dxdy = e21.x / e21.y;
    float e10dxdy = e10.x / e10.y;

    // we need to know the left/right side slopes. They can be different if v1 is on the opposite side of e20
    float lowerLeftEdgeStep  = reversed ? e20dxdy : e21dxdy;
    float lowerRightEdgeStep = reversed ? e21dxdy : e20dxdy;

    // draw triangles
    if(lower_tri_only)
    {
        if(lowerTop < lowerBot) // if we weren't killed by scissoring
        {
            // we don't know which vertex is on the left or right, but the two configures have opposite sign areas:
            //     v0  * ----*  v1         v1 *-----* v0
            //                        OR
            //             * v2                 * v2
            VertexF& left_vertex = reversed ? v0 : v1;
            VertexF& right_vertex = reversed ? v1 : v0;
            render_half_triangle(left_vertex.x,    // upper edge left vertex, floating point pixels
                                 right_vertex.x,   // upper edge right vertex, floating point pixels
                                 upperTop,         // start scanline (included)
                                 lowerBot,         // end scanline   (not included)
                                 dvdx,             // derivative of values wrt x coordinate
                                 dvdy,             // derivative of values wrt y coordinate
                                 left_vertex,      // one point to interpolate from
                                 lowerLeftEdgeStep, // slope of left edge
                                 lowerRightEdgeStep,  // slope of right edge
                                 scissorX1,        // x scissor (integer pixels, do draw this px)
                                 scissorX2,        // x scissor (integer pixels, don't draw this px)
                                 tex_info);        // texture
        }
    }
    else
    {
        // again, left/right slopes
        float upperLeftEdgeStep = reversed ? e20dxdy : e10dxdy;
        float upperRightEdgeStep = reversed ? e10dxdy : e20dxdy;

        // upper triangle
        if(upperTop < upperBot) // if we weren't killed by scissoring
        {
            render_half_triangle(v0.x, v0.x,          // upper edge is just the highest point on triangle
                                 upperTop, upperBot,  // scanline bounds
                                 dvdx, dvdy,          // derivatives of values
                                 v0,                  // interpolate from this vertex
                                 upperLeftEdgeStep, upperRightEdgeStep, // slopes
                                 scissorX1, scissorX2,  // integer x scissor
                                 tex_info);
        }

        if(lowerTop < lowerBot)
        {
            //             * v0
            //
            //     v1  * ----
            //
            //
            //               * v2
            render_half_triangle(v0.x + upperLeftEdgeStep * e10.y, // one of our upper edge vertices isn't v0,v1,v2, but we don't know which. todo is this faster than branch?
                                 v0.x + upperRightEdgeStep * e10.y,
                                 lowerTop, lowerBot, dvdx, dvdy, v1,
                                 lowerLeftEdgeStep, lowerRightEdgeStep, scissorX1, scissorX2, tex_info);
        }

    }

}

/*!
 * Render a "half-triangle" which has a horizontal edge
 * @param x0 - the x coordinate of the upper left most point of the triangle. floating point pixels
 * @param x1 - the x coordinate of the upper right most point of the triangle (can be the same as x0), floating point px
 * @param y0 - the y coordinate of the first scanline which will contain the triangle (integer pixels)
 * @param y1 - the y coordinate of the last scanline which will contain the triangle (integer pixels)
 * @param x_step - the derivatives of all parameters wrt x
 * @param y_step - the derivatives of all parameters wrt y
 * @param init   - the vertex we interpolate from
 * @param step_x0 - how far to step to the left on each step down (floating point px)
 * @param step_x1 - how far to step to the right on each step down (floating point px)
 * @param scx1    - left x scissor (fp px)
 * @param scx2    - right x scissor (fp px)
 * @param tex_info - texture data
 */
void GraphicsSynthesizerThread::render_half_triangle(float x0, float x1, int y0, int y1, VertexF &x_step,
                                                     VertexF &y_step, VertexF &init, float step_x0, float step_x1,
                                                     float scx1, float scx2, TexLookupInfo& tex_info) {

    bool tmp_tex = current_PRMODE->texture_mapping;
    bool tmp_uv = !current_PRMODE->use_UV;

    for(int y = y0; y < y1; y++) // loop over scanlines of triangle
    {
        float height = y - init.y; // how far down we've made it
        VertexF vtx = init + y_step * height;       // interpolate to point (x_init, y)
        float x0l = x0 + step_x0 * height;          // start x coordinates of scanline from interpolation
        float x1l = x1 + step_x1 * height;          // end   x coordinate of scanline from interpolation
        x0l = std::max(scx1, std::ceil(x0l));       // round and scissor
        x1l = std::min(scx2, std::ceil(x1l));       // round and scissor
        int xStop = x1l;                            // integer start/stop pixels
        int xStart = x0l;

        if(xStop == xStart) continue;               // skip rows of zero length

        vtx += (x_step * (x0l - init.x));           // interpolate to point (x0l, y)

        for(int x = x0l; x < xStop; x++)            // loop over x pixels of scanline
        {
            //vtx = init + y_step * height + (x_step * (x - init.x));
            tex_info.vtx_color.r = vtx.r;           // set most recently interpolated stuff
            tex_info.vtx_color.g = vtx.g;
            tex_info.vtx_color.b = vtx.b;
            tex_info.vtx_color.a = vtx.a;
            tex_info.vtx_color.q = vtx.q;
            tex_info.fog = vtx.fog;
            if (tmp_tex)
            {
                int32_t u, v;
                calculate_LOD(tex_info);
                if (tmp_uv)
                {
                    float s, t, q;
                    s = vtx.s * 16.f;
                    t = vtx.t * 16.f;
                    q = vtx.q * 16.f;

                    s /= q;
                    t /= q;
                    u = (s * tex_info.tex_width) * 16.f;
                    v = (t * tex_info.tex_height) * 16.f;
                    //fprintf(stderr, "q: %f, u: %d, v: %d, a: %d\n", vtx.q, u,v, tex_info.vtx_color.a);
                }
                else
                {
                    u = (uint32_t) vtx.u;
                    v = (uint32_t) vtx.v;
                }
#ifdef GS_JIT
                jit_tex_lookup(u, v, &tex_info);
                jit_draw_pixel(x * 16, y * 16, (uint32_t)vtx.z, tex_info.tex_color);
#else
                tex_lookup(u, v, tex_info);
                draw_pixel(x * 16, y * 16, (uint32_t)vtx.z, tex_info.tex_color);
#endif

            }
            else
            {
#ifdef GS_JIT
                jit_draw_pixel(x * 16, y * 16, (uint32_t)vtx.z, tex_info.vtx_color);
#else
                draw_pixel(x * 16, y * 16, (uint32_t)vtx.z, tex_info.vtx_color);
#endif
            }

            vtx += x_step;                       // get values for the adjacent pixel
        }
    }

}

void GraphicsSynthesizerThread::render_triangle()
{
    printf("[GS_t] Rendering triangle!\n");

    Vertex v1 = vtx_queue[2]; v1.to_relative(current_ctx->xyoffset);
    Vertex v2 = vtx_queue[1]; v2.to_relative(current_ctx->xyoffset);
    Vertex v3 = vtx_queue[0]; v3.to_relative(current_ctx->xyoffset);

    if (!current_PRMODE->gourand_shading)
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
    max_x = min(max_x, (int32_t)current_ctx->scissor.x2 + 0x10);
    max_y = min(max_y, (int32_t)current_ctx->scissor.y2 + 0x10);

    if (max_y == min_y && min_x == max_x)
        return;
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

    const int32_t block_A23 = (BLOCKSIZE - 1) * A23;
    const int32_t block_A31 = (BLOCKSIZE - 1) * A31;
    const int32_t block_A12 = (BLOCKSIZE - 1) * A12;
    const int32_t block_B23 = (BLOCKSIZE - 1) * B23;
    const int32_t block_B31 = (BLOCKSIZE - 1) * B31;
    const int32_t block_B12 = (BLOCKSIZE - 1) * B12;

    Vertex min_corner;
    min_corner.x = min_x; min_corner.y = min_y;
    int32_t w1_row = orient2D(v2, v3, min_corner);
    int32_t w2_row = orient2D(v3, v1, min_corner);
    int32_t w3_row = orient2D(v1, v2, min_corner);
    int32_t w1_row_block = w1_row;
    int32_t w2_row_block = w2_row;
    int32_t w3_row_block = w3_row;



    TexLookupInfo tex_info;
    tex_info.new_lookup = true;
    tex_info.tex_base = current_ctx->tex0.texture_base;
    tex_info.buffer_width = current_ctx->tex0.width;
    tex_info.tex_width = current_ctx->tex0.tex_width;
    tex_info.tex_height = current_ctx->tex0.tex_height;

    bool tmp_tex = current_PRMODE->texture_mapping;
    bool tmp_uv = !current_PRMODE->use_UV;//allow for loop unswitching

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
            w1_tr = w1_tl + block_A23;
            w2_tr = w2_tl + block_A31;
            w3_tr = w3_tl + block_A12;
            w1_bl = w1_tl + block_B23;
            w2_bl = w2_tl + block_B31;
            w3_bl = w3_tl + block_B12;
            w1_br = w1_tl + block_B23 + block_A23;
            w2_br = w2_tl + block_B31 + block_A31;
            w3_br = w3_tl + block_B12 + block_A12;

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
                            float fog = (float) v1.fog * w1 + (float) v2.fog * w2 + (float) v3.fog * w3;
                            tex_info.vtx_color.r = r / divider;
                            tex_info.vtx_color.g = g / divider;
                            tex_info.vtx_color.b = b / divider;
                            tex_info.vtx_color.a = a / divider;
                            tex_info.vtx_color.q = q / divider;
                            tex_info.fog = fog / divider;

                            if (tmp_tex)
                            {
                                int32_t u, v;
                                calculate_LOD(tex_info);
                                if (tmp_uv)
                                {
                                    float s, t, q;
                                    s = v1.s * w1 + v2.s * w2 + v3.s * w3;
                                    t = v1.t * w1 + v2.t * w2 + v3.t * w3;
                                    q = v1.rgbaq.q * w1 + v2.rgbaq.q * w2 + v3.rgbaq.q * w3;

                                    //We don't divide s and t by "divider" because dividing by Q effectively
                                    //cancels that out
                                    s /= q;
                                    t /= q;
                                    u = (s * tex_info.tex_width) * 16.0;
                                    v = (t * tex_info.tex_height) * 16.0;
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
#ifdef GS_JIT
                                jit_tex_lookup(u, v, &tex_info);
                                jit_draw_pixel(x, y, (uint32_t)z, tex_info.tex_color);
#else
                                tex_lookup(u, v, tex_info);
                                draw_pixel(x, y, (uint32_t)z, tex_info.tex_color);
#endif
                            }
                            else
                            {
#ifdef GS_JIT
                                jit_draw_pixel(x, y, (uint32_t)z, tex_info.vtx_color);
#else
                                draw_pixel(x, y, (uint32_t)z, tex_info.vtx_color);
#endif
                            }
                        }
                        else
                            break;
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
    TexLookupInfo tex_info;
    tex_info.new_lookup = true;

    tex_info.vtx_color = vtx_queue[0].rgbaq;
    tex_info.tex_base = current_ctx->tex0.texture_base;
    tex_info.buffer_width = current_ctx->tex0.width;
    tex_info.tex_width = current_ctx->tex0.tex_width;
    tex_info.tex_height = current_ctx->tex0.tex_height;

    calculate_LOD(tex_info);

    if (v1.x > v2.x)
    {
        swap(v1, v2);
    }

    //Automatic scissoring test
    int32_t min_y = ((std::max(v1.y, (int32_t)current_ctx->scissor.y1) + 8) >> 4) << 4;
    int32_t min_x = ((std::max(v1.x, (int32_t)current_ctx->scissor.x1) + 8) >> 4) << 4;
    int32_t max_y = ((std::min(v2.y, (int32_t)current_ctx->scissor.y2 + 0x10) + 8) >> 4) << 4;
    int32_t max_x = ((std::min(v2.x, (int32_t)current_ctx->scissor.x2 + 0x10) + 8) >> 4) << 4;

    printf("Coords: (%d, %d) (%d, %d)\n", min_x >> 4, min_y >> 4, max_x >> 4, max_y >> 4);

    float pix_t = interpolate_f(min_y, v1.t, v1.y, v2.t, v2.y);
    int32_t pix_v = (int32_t)interpolate(min_y, v1.uv.v, v1.y, v2.uv.v, v2.y) << 16;
    float pix_s_init = interpolate_f(min_x, v1.s, v1.x, v2.s, v2.x);
    int32_t pix_u_init = (int32_t)interpolate(min_x, v1.uv.u, v1.x, v2.uv.u, v2.x) << 16;

    float pix_t_step = stepsize(v1.t, v1.y, v2.t, v2.y, 0x10);
    int32_t pix_v_step = stepsize((int32_t)v1.uv.v, v1.y, (int32_t)v2.uv.v, v2.y, 0x100000);
    float pix_s_step = stepsize(v1.s, v1.x, v2.s, v2.x, 0x10);
    int32_t pix_u_step = stepsize((int32_t)v1.uv.u, v1.x, (int32_t)v2.uv.u, v2.x, 0x100000);

    bool tmp_tex = current_PRMODE->texture_mapping;
    bool tmp_st = !current_PRMODE->use_UV;//allow for loop unswitching

    for (int32_t y = min_y; y < max_y; y += 0x10)
    {
        float pix_s = pix_s_init;
        uint32_t pix_u = pix_u_init;
        for (int32_t x = min_x; x < max_x; x += 0x10)
        {
            if (tmp_tex)
            {
                tex_info.fog = v2.fog;
                if (tmp_st)
                {
                    pix_v = (pix_t * tex_info.tex_height) * 16.0;
                    pix_u = (pix_s * tex_info.tex_width) * 16.0;
#ifdef GS_JIT
                    jit_tex_lookup(pix_u, pix_v, &tex_info);
#else
                    tex_lookup(pix_u, pix_v, tex_info);
#endif
                }
                else
                {
#ifdef GS_JIT
                    jit_tex_lookup(pix_u >> 16, pix_v >> 16, &tex_info);
#else
                    tex_lookup(pix_u >> 16, pix_v >> 16, tex_info);
#endif
                }

#ifdef GS_JIT
                jit_draw_pixel(x, y, v2.z, tex_info.tex_color);
#else
                draw_pixel(x, y, v2.z, tex_info.tex_color);
#endif
            }
            else
            {
#ifdef GS_JIT
                jit_draw_pixel(x, y, v2.z, tex_info.vtx_color);
#else
                draw_pixel(x, y, v2.z, tex_info.vtx_color);
#endif
            }
            pix_s += pix_s_step;
            pix_u += pix_u_step;
        }
        pix_t += pix_t_step;
        pix_v += pix_v_step;
    }
}

void GraphicsSynthesizerThread::write_HWREG(uint64_t data)
{
    int ppd = 0; //pixels per doubleword (64-bits)

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
            Errors::print_warning("[GS_t] Unrecognized BITBLTBUF dest format $%02X\n", BITBLTBUF.dest_format);
            return;
    }

    for (int i = 0; i < ppd; i++)
    {
        switch (BITBLTBUF.dest_format)
        {
            case 0x00:
                write_PSMCT32_block(BITBLTBUF.dest_base, BITBLTBUF.dest_width, TRXPOS.int_dest_x, TRXPOS.int_dest_y, (data >> (i * 32)) & 0xFFFFFFFF);
                pixels_transferred++;
                TRXPOS.int_dest_x++;
                //printf("[GS_t] Write to $%08X of ", BITBLTBUF.dest_base + (dest_addr * 4));
                //printf("$%08X\n", (data >> (i * 32)) & 0xFFFFFFFF);
                break;
            case 0x01:
                unpack_PSMCT24(data, i, false);
                break;
            case 0x02:
                write_PSMCT16_block(BITBLTBUF.dest_base, BITBLTBUF.dest_width, TRXPOS.int_dest_x, TRXPOS.int_dest_y, (data >> (i * 16)) & 0xFFFF);
                pixels_transferred++;
                TRXPOS.int_dest_x++;
                break;
            case 0xA:
                write_PSMCT16S_block(BITBLTBUF.dest_base, BITBLTBUF.dest_width, TRXPOS.int_dest_x, TRXPOS.int_dest_y, (data >> (i * 16)) & 0xFFFF);
                pixels_transferred++;
                TRXPOS.int_dest_x++;
                break;
            case 0x13:
                write_PSMCT8_block(BITBLTBUF.dest_base, BITBLTBUF.dest_width, TRXPOS.int_dest_x, TRXPOS.int_dest_y,
                                   (data >> (i * 8)) & 0xFF);
                //printf("[GS_t] Write to $%08X\n", dest_addr);
                pixels_transferred++;
                TRXPOS.int_dest_x++;
                break;
            case 0x14:
            {
                uint8_t value = (data >> ((i >> 1) << 3));
                if (TRXPOS.int_dest_x & 0x1)
                    value >>= 4;
                else
                    value &= 0xF;
                write_PSMCT4_block(BITBLTBUF.dest_base, BITBLTBUF.dest_width, TRXPOS.int_dest_x, TRXPOS.int_dest_y,
                                   value);
                pixels_transferred++;
                TRXPOS.int_dest_x++;
            }
                break;
            case 0x1B:
            {
                uint32_t value = (data >> (i << 3)) & 0xFF;
                value <<= 24;
                value |= read_PSMCT32_block(BITBLTBUF.dest_base, BITBLTBUF.dest_width, TRXPOS.int_dest_x, TRXPOS.int_dest_y)
                        & 0x00FFFFFF;
                write_PSMCT32_block(BITBLTBUF.dest_base, BITBLTBUF.dest_width, TRXPOS.int_dest_x, TRXPOS.int_dest_y, value);
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
                value |= read_PSMCT32_block(BITBLTBUF.dest_base, BITBLTBUF.dest_width, TRXPOS.int_dest_x, TRXPOS.int_dest_y)
                        & 0xF0FFFFFF;
                write_PSMCT32_block(BITBLTBUF.dest_base, BITBLTBUF.dest_width, TRXPOS.int_dest_x, TRXPOS.int_dest_y, value);
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
                value |= read_PSMCT32_block(BITBLTBUF.dest_base, BITBLTBUF.dest_width, TRXPOS.int_dest_x, TRXPOS.int_dest_y)
                        & 0x0FFFFFFF;
                write_PSMCT32_block(BITBLTBUF.dest_base, BITBLTBUF.dest_width, TRXPOS.int_dest_x, TRXPOS.int_dest_y, value);
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
            TRXPOS.int_dest_y++;
        }
    }

    int max_pixels = TRXREG.width * TRXREG.height;
    if (pixels_transferred >= max_pixels)
    {
        //Deactivate the transmisssion
        printf("[GS_t] HWREG transfer ended\n");
        TRXDIR = 3;
        pixels_transferred = 0;
    }
}

uint128_t GraphicsSynthesizerThread::local_to_host()
{
    int ppd = 0; //pixels per doubleword (64-bits)
    uint128_t return_data;
    return_data._u64[0] = 0;
    return_data._u64[1] = 0;
    if (TRXDIR == 3)
        return return_data;

    switch (BITBLTBUF.source_format)
    {
        //PSMCT32
        case 0x00:
            ppd = 2;
            break;
        //PSMCT24
        case 0x01:
            ppd = 1; //Does it all in one go
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
        case 0x31:
            ppd = 1; //Does it all in one go
            break;
        default:
            Errors::print_warning("[GS_t] GS Download Unrecognized BITBLTBUF source format $%02X\n", BITBLTBUF.source_format);
            return return_data;
    }
    uint64_t data = 0;
    for (int datapart = 0; datapart < 2; datapart++)
    {
        for (int i = 0; i < ppd; i++)
        {
            int datapart = i / (ppd / 2);

            switch (BITBLTBUF.source_format)
            {
                case 0x00:
                    data |= (uint64_t)(read_PSMCT32_block(BITBLTBUF.source_base, BITBLTBUF.source_width,
                        TRXPOS.int_source_x, TRXPOS.int_source_y) & 0xFFFFFFFF) << (i * 32);
                    pixels_transferred++;
                    TRXPOS.int_source_x++;
                    break;
                case 0x01:
                    data = pack_PSMCT24(false);
                    break;
                case 0x02:
                    data |= (uint64_t)(read_PSMCT16_block(BITBLTBUF.source_base, BITBLTBUF.source_width,
                        TRXPOS.int_source_x, TRXPOS.int_source_y) & 0xFFFF) << (i * 16);
                    pixels_transferred++;
                    TRXPOS.int_source_x++;
                    break;
                case 0x0A:
                    data |= (uint64_t)(read_PSMCT16S_block(BITBLTBUF.source_base, BITBLTBUF.source_width,
                        TRXPOS.int_source_x, TRXPOS.int_source_y) & 0xFFFF) << (i * 16);
                    pixels_transferred++;
                    TRXPOS.int_source_x++;
                    break;
                case 0x13:
                    data |= (uint64_t)(read_PSMCT8_block(BITBLTBUF.source_base, BITBLTBUF.source_width,
                        TRXPOS.int_source_x, TRXPOS.int_source_y) & 0xFF) << (i * 8);
                    pixels_transferred++;
                    TRXPOS.int_source_x++;
                    break;
                case 0x14:
                    data |= (uint64_t)(read_PSMCT4_block(BITBLTBUF.source_base, BITBLTBUF.source_width,
                        TRXPOS.int_source_x, TRXPOS.int_source_y) & 0xf) << (i * 4);
                    pixels_transferred++;
                    TRXPOS.int_source_x++;
                    break;
                case 0x1B:
                    data <<= 8;
                    data |= (uint64_t)((read_PSMCT32_block(BITBLTBUF.source_base, BITBLTBUF.source_width,
                        TRXPOS.int_source_x, TRXPOS.int_source_y) >> 24) & 0xFF) << (i * 8);
                    pixels_transferred++;
                    TRXPOS.int_source_x++;
                    break;
                case 0x31:
                    data = pack_PSMCT24(true);
                    break;
                default:
                    Errors::print_warning("[GS_t] GS Download Unrecognized BITBLTBUF source format $%02X\n", BITBLTBUF.source_format);
                    return return_data;
            }


            if (TRXPOS.int_source_x - TRXPOS.source_x == TRXREG.width)
            {
                TRXPOS.int_source_x = TRXPOS.source_x;
                TRXPOS.int_source_y++;
            }
        }

        return_data._u64[datapart] = data;
        data = 0;
    }

    int max_pixels = TRXREG.width * TRXREG.height;
    if (pixels_transferred >= max_pixels)
    {
        //Deactivate the transmisssion
        printf("[GS_t] Local to Host transfer ended\n");
        TRXDIR = 3;
        pixels_transferred = 0;
    }

    return return_data;
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
                                     TRXPOS.int_dest_y, PSMCT24_color);
            else
                write_PSMCT24_block(BITBLTBUF.dest_base, BITBLTBUF.dest_width, TRXPOS.int_dest_x,
                    TRXPOS.int_dest_y, PSMCT24_color);
            PSMCT24_color = 0;
            PSMCT24_unpacked_count = 0;
            TRXPOS.int_dest_x++;
            pixels_transferred++;
        }
    }
}

uint64_t GraphicsSynthesizerThread::pack_PSMCT24(bool z_format)
{
    int data_in_output = 0;
    uint64_t output_color = 0;

    while (data_in_output < 64)
    {
        if (PSMCT24_unpacked_count)
        {
            output_color |= (uint64_t)PSMCT24_color << data_in_output;
            data_in_output += PSMCT24_unpacked_count;

            //Data overflowed, so keep left over
            if (data_in_output > 64)
            {
                PSMCT24_color >>= PSMCT24_unpacked_count - (data_in_output - 64);                
                PSMCT24_unpacked_count = (data_in_output - 64);
            }
            else
            {
                PSMCT24_unpacked_count = 0;
                PSMCT24_color = 0;
            }

            int max_pixels = TRXREG.width * TRXREG.height;
            if (pixels_transferred >= max_pixels)
                break;
        }
        else
        {
            if (z_format)
            {
                PSMCT24_color = (uint64_t)(read_PSMCT32Z_block(BITBLTBUF.source_base, BITBLTBUF.source_width,
                    TRXPOS.int_source_x, TRXPOS.int_source_y) & 0xFFFFFF) << PSMCT24_unpacked_count;
            }
            else
            {
                PSMCT24_color = (uint64_t)(read_PSMCT32_block(BITBLTBUF.source_base, BITBLTBUF.source_width,
                    TRXPOS.int_source_x, TRXPOS.int_source_y) & 0xFFFFFF) << PSMCT24_unpacked_count;
            }
            PSMCT24_unpacked_count += 24;
            TRXPOS.int_source_x++;
            pixels_transferred++;

            if (TRXPOS.int_source_x - TRXPOS.source_x == TRXREG.width)
            {
                TRXPOS.int_source_x = TRXPOS.source_x;
                TRXPOS.int_source_y++;
            }
        }
    }

    return output_color;
}

void GraphicsSynthesizerThread::local_to_local()
{
    printf("[GS_t] Local to local transfer\n");
    printf("(%d, %d) -> (%d, %d)\n", TRXPOS.source_x, TRXPOS.source_y, TRXPOS.dest_x, TRXPOS.dest_y);
    printf("Trans order: %d\n", TRXPOS.trans_order);
    printf("Source: $%08X Dest: $%08X\n", BITBLTBUF.source_base, BITBLTBUF.dest_base);
    int max_pixels = TRXREG.width * TRXREG.height;

    uint16_t start_x = 0, start_y = 0;
    int x_step = 0, y_step = 0;

    switch (TRXPOS.trans_order)
    {
        case 0x00:
            //Upper-left to bottom-right
            start_x = TRXPOS.int_dest_x = TRXPOS.dest_x;
            start_y = TRXPOS.int_dest_y = TRXPOS.dest_y;
            x_step = 1;
            y_step = 1;
            break;
        case 0x01:
            //Bottom-left to upper-right
            start_x = TRXPOS.int_dest_x = TRXPOS.dest_x;
            start_y = TRXPOS.int_dest_y = TRXPOS.dest_y + TRXREG.height - 1;
            x_step = 1;
            y_step = -1;
            break;
        case 0x02:
            //Upper-right to bottom-left
            start_x = TRXPOS.int_dest_x = TRXPOS.dest_x + TRXREG.width - 1;
            start_y = TRXPOS.int_dest_y = TRXPOS.dest_y;
            x_step = -1;
            y_step = 1;
            break;
        case 0x03:
            //Bottom-right to upper-left
            start_x = TRXPOS.int_dest_x = TRXPOS.dest_x + TRXREG.width - 1;
            start_y = TRXPOS.int_dest_y = TRXPOS.dest_y + TRXREG.height - 1;
            x_step = -1;
            y_step = -1;
            break;
        default:
            Errors::die("[GS_t] Unrecognized local-to-local transmission order $%02X", TRXPOS.trans_order);
    }

    while (pixels_transferred < max_pixels)
    {
        uint32_t data;
        switch (BITBLTBUF.source_format)
        {
            case 0x00:
            case 0x01:
                data = read_PSMCT32_block(BITBLTBUF.source_base, BITBLTBUF.source_width,
                                          TRXPOS.int_source_x, TRXPOS.int_source_y);
                break;
            case 0x13:
                data = read_PSMCT8_block(BITBLTBUF.source_base, BITBLTBUF.source_width,
                                         TRXPOS.int_source_x, TRXPOS.int_source_y);
                break;
            case 0x14:
                data = read_PSMCT4_block(BITBLTBUF.source_base, BITBLTBUF.source_width,
                                         TRXPOS.int_source_x, TRXPOS.int_source_y);
                break;
            case 0x30:
            case 0x31:
                data = read_PSMCT32Z_block(BITBLTBUF.source_base, BITBLTBUF.source_width,
                                          TRXPOS.int_source_x, TRXPOS.int_source_y);
                break;
            default:
                Errors::die("[GS_t] Unrecognized local-to-local source format $%02X", BITBLTBUF.source_format);
        }

        switch (BITBLTBUF.dest_format)
        {
            case 0x00:
                write_PSMCT32_block(BITBLTBUF.dest_base, BITBLTBUF.dest_width,
                                    TRXPOS.int_dest_x, TRXPOS.int_dest_y, data);
                break;
            case 0x01:
                write_PSMCT24_block(BITBLTBUF.dest_base, BITBLTBUF.dest_width,
                                    TRXPOS.int_dest_x, TRXPOS.int_dest_y, data);
                break;
            case 0x13:
                write_PSMCT8_block(BITBLTBUF.dest_base, BITBLTBUF.dest_width,
                                    TRXPOS.int_dest_x, TRXPOS.int_dest_y, data);
                break;
            case 0x14:
                write_PSMCT4_block(BITBLTBUF.dest_base, BITBLTBUF.dest_width,
                                    TRXPOS.int_dest_x, TRXPOS.int_dest_y, data);
                break;
            case 0x24:
                data <<= 24;
                data |= read_PSMCT32_block(BITBLTBUF.dest_base, BITBLTBUF.dest_width,
                                           TRXPOS.int_dest_x, TRXPOS.int_dest_y) & 0xF0FFFFFF;
                write_PSMCT32_block(BITBLTBUF.dest_base, BITBLTBUF.dest_width,
                                    TRXPOS.int_dest_x, TRXPOS.int_dest_y, data);
                break;
            case 0x2C:
                data <<= 28;
                data |= read_PSMCT32_block(BITBLTBUF.dest_base, BITBLTBUF.dest_width,
                                           TRXPOS.int_dest_x, TRXPOS.int_dest_y) & 0x0FFFFFFF;
                write_PSMCT32_block(BITBLTBUF.dest_base, BITBLTBUF.dest_width,
                                    TRXPOS.int_dest_x, TRXPOS.int_dest_y, data);
                break;
            case 0x30:
                write_PSMCT32Z_block(BITBLTBUF.dest_base, BITBLTBUF.dest_width,
                                    TRXPOS.int_dest_x, TRXPOS.int_dest_y, data);
                break;
            case 0x31:
                write_PSMCT24Z_block(BITBLTBUF.dest_base, BITBLTBUF.dest_width,
                                    TRXPOS.int_dest_x, TRXPOS.int_dest_y, data);
                break;
            default:
                Errors::die("[GS_t] Unrecognized local-to-local dest format $%02X", BITBLTBUF.dest_format);
        }

        pixels_transferred++;
        TRXPOS.int_source_x++;
        TRXPOS.int_dest_x += x_step;

        if (pixels_transferred % TRXREG.width == 0)
        {
            TRXPOS.int_source_x = TRXPOS.source_x;
            TRXPOS.int_source_y++;

            TRXPOS.int_dest_x = start_x;
            TRXPOS.int_dest_y += y_step;
        }
    }
    pixels_transferred = 0;
    TRXDIR = 3;
}

uint8_t GraphicsSynthesizerThread::get_16bit_alpha(uint16_t color)
{
    if (color & (1 << 15))
        return TEXA.alpha1;
    if (!(color & 0xFFFF) && TEXA.trans_black)
        return 0;
    return TEXA.alpha0;
}

int16_t GraphicsSynthesizerThread::multiply_tex_color(int16_t tex_color, int16_t frag_color)
{
    int16_t temp_color;
    if (frag_color != 0x80)
    {
        temp_color = (tex_color * frag_color) >> 7;
    }
    else
    {
        temp_color = tex_color;
    }

    if (temp_color > 0xFF)
        temp_color = 0xFF;
    if (temp_color < 0)
        temp_color = 0;

    return temp_color;
}

void GraphicsSynthesizerThread::calculate_LOD(TexLookupInfo &info)
{
    float K = current_ctx->tex1.K;

    if (current_ctx->tex1.LOD_method == 0 && !PRIM.use_UV && info.vtx_color.q != 1.0)
    {
        uint32_t q_int = (*(uint32_t*)&info.vtx_color.q & 0x7FFFFFFF) >> 16;
        info.LOD = log2_lookup[q_int][current_ctx->tex1.L] + K;

        if (!(current_ctx->tex1.filter_smaller & 0x1))
            info.LOD = round(info.LOD + 0.5);
    }
    else
        info.LOD = round(K);

    //Mipmapping is only enabled when the max MIP level is > 0 and filtering is set to a MIPMAP type
    if (current_ctx->tex1.max_MIP_level && current_ctx->tex1.filter_smaller >= 2)
    {
        //Determine mipmap level
        info.mipmap_level = min((int8_t)info.LOD, (int8_t)current_ctx->tex1.max_MIP_level);

        if (info.mipmap_level < 0)
            info.mipmap_level = 0;

        if (info.mipmap_level > 0)
        {
            info.tex_base = current_ctx->tex0.texture_base;
            info.buffer_width = current_ctx->tex0.width;
            info.tex_width = current_ctx->tex0.tex_width;
            info.tex_height = current_ctx->tex0.tex_height;

            if (current_ctx->tex1.MTBA && info.mipmap_level < 4)
            {
                //Tex width and tex height must be equal for this mipmapping method to work
                //Cartoon Network Racing breaks otherwise
                if (info.tex_width < 32 || info.tex_width != info.tex_height)
                {
                    info.mipmap_level = 0;
                    return;
                }
                //Counted in bytes
                const static float format_sizes[] =
                {
                    //0x00
                    4, 4, 2, 0, 0, 0, 0, 0,

                    //0x08
                    0, 0, 2, 0, 0, 0, 0, 0,

                    //0x10
                    0, 0, 0, 1, 0.5, 0, 0, 0,

                    //0x18
                    0, 0, 0, 4, 0, 0, 0, 0,

                    //0x20
                    0, 0, 0, 0, 4, 0, 0, 0,

                    //0x28
                    0, 0, 0, 0, 4, 0, 0, 0,

                    //0x30
                    4, 4, 2, 0, 0, 0, 0, 0,

                    //0x38
                    0, 0, 2, 0, 0, 0, 0, 0
                };
                //Calculate the texture base based on a continuous memory region

                uint32_t offset;
                for (int i = 0; i < info.mipmap_level; i++)
                {
                    offset = (info.tex_width * info.tex_height) >> (i << 1);
                    info.tex_base += (offset * format_sizes[current_ctx->tex0.format]);
                }
            }
            else
                info.tex_base = current_ctx->miptbl.texture_base[info.mipmap_level - 1];
            info.buffer_width = current_ctx->miptbl.width[info.mipmap_level - 1];
            info.tex_width >>= info.mipmap_level;
            info.tex_height >>= info.mipmap_level;

            info.tex_width = max((int)info.tex_width, 1);
            info.tex_height = max((int)info.tex_height, 1);
        }
    }
    else
        info.mipmap_level = 0;
}

void GraphicsSynthesizerThread::tex_lookup(int16_t u, int16_t v, TexLookupInfo& info)
{
    bool bilinear_filter = false;

    //If UV is being used and MIPMAP is enabled, we need to bring down the UV size too
    if (current_PRMODE->use_UV)
    {
        u >>= info.mipmap_level;
        v >>= info.mipmap_level;
    }

    if (info.tex_height >= 8 && info.tex_width >= 8)
    {
        if (current_ctx->tex1.filter_larger && info.LOD < 0.0)
            bilinear_filter = true;

        //Bilinear filtering is used when set to 1 or 4 and above
        if ((current_ctx->tex1.filter_smaller == 0x1 || current_ctx->tex1.filter_smaller >= 4) && info.LOD >= 0.0)
            bilinear_filter = true;
    }

    if (bilinear_filter)
    {
        RGBAQ_REG a, b, c, d;
        int16_t uu = (u - 8) >> 4;
        int16_t vv = (v - 8) >> 4;

        tex_lookup_int(uu, vv, info, true);
        a = info.srctex_color;

        tex_lookup_int(uu + 1, vv, info, true);
        b = info.srctex_color;

        tex_lookup_int(uu, vv + 1, info, true);
        c = info.srctex_color;

        tex_lookup_int(uu + 1, vv + 1, info, true);
        d = info.srctex_color;

        double alpha = (double)((u - 8) & 0xF) * (1.0 / 16.0);
        double beta = (double)((v - 8) & 0xF) * (1.0 / 16.0);
        double alpha_s = 1.0 - alpha;
        double beta_s = 1.0 - beta;
        info.srctex_color.r = alpha_s * beta_s*a.r + alpha * beta_s*b.r + alpha_s * beta*c.r + alpha * beta*d.r;
        info.srctex_color.g = alpha_s * beta_s*a.g + alpha * beta_s*b.g + alpha_s * beta*c.g + alpha * beta*d.g;
        info.srctex_color.b = alpha_s * beta_s*a.b + alpha * beta_s*b.b + alpha_s * beta*c.b + alpha * beta*d.b;
        info.srctex_color.a = alpha_s * beta_s*a.a + alpha * beta_s*b.a + alpha_s * beta*c.a + alpha * beta*d.a;
    }
    else //If we already have looked up the texture at this location and messed with it, no point in doing it again
        tex_lookup_int(u >> 4, v >> 4, info);

    switch (current_ctx->tex0.color_function)
    {
        case 0: //Modulate
            info.tex_color.r = multiply_tex_color(info.srctex_color.r, info.vtx_color.r);
            info.tex_color.g = multiply_tex_color(info.srctex_color.g, info.vtx_color.g);
            info.tex_color.b = multiply_tex_color(info.srctex_color.b, info.vtx_color.b);
            if (current_ctx->tex0.use_alpha)
                info.tex_color.a = multiply_tex_color(info.srctex_color.a, info.vtx_color.a);
            else
                info.tex_color.a = info.vtx_color.a;
            break;
        case 1: //Decal
            if (!current_ctx->tex0.use_alpha)
                info.tex_color.a = info.vtx_color.a;
            else
                info.tex_color.a = info.srctex_color.a;
            info.tex_color.r = info.srctex_color.r;
            info.tex_color.g = info.srctex_color.g;
            info.tex_color.b = info.srctex_color.b;
            break;
        case 2: //Highlight
            info.tex_color.r = multiply_tex_color(info.srctex_color.r, info.vtx_color.r) + info.vtx_color.a;
            info.tex_color.g = multiply_tex_color(info.srctex_color.g, info.vtx_color.g) + info.vtx_color.a;
            info.tex_color.b = multiply_tex_color(info.srctex_color.b, info.vtx_color.b) + info.vtx_color.a;
            if (!current_ctx->tex0.use_alpha)
                info.tex_color.a = info.vtx_color.a;
            else
                info.tex_color.a = info.srctex_color.a + info.vtx_color.a;
            break;
        case 3: //Highlight2
            info.tex_color.r = multiply_tex_color(info.srctex_color.r, info.vtx_color.r) + info.vtx_color.a;
            info.tex_color.g = multiply_tex_color(info.srctex_color.g, info.vtx_color.g) + info.vtx_color.a;
            info.tex_color.b = multiply_tex_color(info.srctex_color.b, info.vtx_color.b) + info.vtx_color.a;
            if (!current_ctx->tex0.use_alpha)
                info.tex_color.a = info.vtx_color.a;
            else
                info.tex_color.a = info.srctex_color.a;
            break;
        default:
            Errors::die("[GS_t] Unrecognized texture color function $%02X", current_ctx->tex0.color_function);
    }

    if (current_PRMODE->fog)
    {
        uint16_t fog = info.fog;
        uint16_t fog2 = 0xFF - info.fog;
        info.tex_color.r = ((info.tex_color.r * fog) >> 8) + ((fog2 * FOGCOL.r) >> 8);
        info.tex_color.g = ((info.tex_color.g * fog) >> 8) + ((fog2 * FOGCOL.g) >> 8);
        info.tex_color.b = ((info.tex_color.b * fog) >> 8) + ((fog2 * FOGCOL.b) >> 8);
    }
}

void GraphicsSynthesizerThread::tex_lookup_int(int16_t u, int16_t v, TexLookupInfo& info, bool forced_lookup)
{
    switch (current_ctx->clamp.wrap_s)
    {
        case 0:
            u &= info.tex_width - 1;
            break;
        case 1:
            if (u >= info.tex_width)
                u = info.tex_width-1;
            else if (u < 0)
                u = 0;
            break;
        case 2:
            if (u > (current_ctx->clamp.max_u >> info.mipmap_level))
                u = current_ctx->clamp.max_u >> info.mipmap_level;
            else if (u < (current_ctx->clamp.min_u >> info.mipmap_level))
                u = current_ctx->clamp.min_u >> info.mipmap_level;
            break;
        case 3:
            //Mask should only apply to integer component
            u = (u & (current_ctx->clamp.min_u | 0xF)) | current_ctx->clamp.max_u;
            break;
    }
    switch (current_ctx->clamp.wrap_t)
    {
        case 0:
            v &= info.tex_height - 1;
            break;
        case 1:
            if (v >= info.tex_height)
                v = info.tex_height-1;
            else if (v < 0)
                v = 0;
            break;
        case 2:
            if (v > (current_ctx->clamp.max_v >> info.mipmap_level))
                v = current_ctx->clamp.max_v >> info.mipmap_level;
            else if (v < (current_ctx->clamp.min_v >> info.mipmap_level))
                v = (current_ctx->clamp.min_v >> info.mipmap_level);
            break;
        case 3:
            v = (v & (current_ctx->clamp.min_v | 0xF)) | current_ctx->clamp.max_v;
            break;
    }

    if (!info.new_lookup && !forced_lookup)
    {
        //if it's the same texture position, we already have the info, no need to look it up again
        if (u == info.lastu && v == info.lastv)
            return;
    }
    info.lastu = u;
    info.lastv = v;
    info.new_lookup = forced_lookup; //If we're forcing a lookup, it's bilinear filtering, so the src will get polluted

    uint32_t tex_base = info.tex_base;
    uint32_t width = info.buffer_width;
    switch (current_ctx->tex0.format)
    {
        case 0x00:
        {
            uint32_t color = read_PSMCT32_block(tex_base, width, u, v);
            info.srctex_color.r = color & 0xFF;
            info.srctex_color.g = (color >> 8) & 0xFF;
            info.srctex_color.b = (color >> 16) & 0xFF;
            info.srctex_color.a = color >> 24;
        }
            break;
        case 0x01:
        {
            uint32_t color = read_PSMCT32_block(tex_base, width, u, v);
            info.srctex_color.r = color & 0xFF;
            info.srctex_color.g = (color >> 8) & 0xFF;
            info.srctex_color.b = (color >> 16) & 0xFF;

            if (!(color & 0xFFFFFF) && TEXA.trans_black)
                info.srctex_color.a = 0;
            else
                info.srctex_color.a = TEXA.alpha0;
        }
            break;
        case 0x02:
        {
            uint16_t color = read_PSMCT16_block(tex_base, width, u, v);
            info.srctex_color.r = (color & 0x1F) << 3;
            info.srctex_color.g = ((color >> 5) & 0x1F) << 3;
            info.srctex_color.b = ((color >> 10) & 0x1F) << 3;
            info.srctex_color.a = get_16bit_alpha(color);
        }
            break;
        case 0x09: //Invalid format??? FFX uses it
            info.srctex_color.r = 0;
            info.srctex_color.g = 0;
            info.srctex_color.b = 0;
            info.srctex_color.a = 0;
            break;
        case 0x0A:
        {
            uint16_t color = read_PSMCT16S_block(tex_base, width, u, v);
            info.srctex_color.r = (color & 0x1F) << 3;
            info.srctex_color.g = ((color >> 5) & 0x1F) << 3;
            info.srctex_color.b = ((color >> 10) & 0x1F) << 3;
            info.srctex_color.a = get_16bit_alpha(color);
        }
            break;
        case 0x13:
        {
            uint8_t entry = read_PSMCT8_block(tex_base, width, u, v);
            if (current_ctx->tex0.use_CSM2)
                clut_CSM2_lookup(entry, info.srctex_color);
            else
                clut_lookup(entry, info.srctex_color);
        }
            break;
        case 0x14:
        {
            uint8_t entry = read_PSMCT4_block(tex_base, width, u, v);
            if (current_ctx->tex0.use_CSM2)
                clut_CSM2_lookup(entry, info.srctex_color);
            else
                clut_lookup(entry, info.srctex_color);
        }
            break;
        case 0x1B:
        {
            uint8_t entry = read_PSMCT32_block(tex_base, width, u, v) >> 24;
            if (current_ctx->tex0.use_CSM2)
                clut_CSM2_lookup(entry, info.srctex_color);
            else
                clut_lookup(entry, info.srctex_color);
        }
            break;
        case 0x24:
        {
            //printf("[GS_t] Format $24: Read from $%08X\n", tex_base + (coord << 2));
            uint8_t entry = (read_PSMCT32_block(tex_base, width, u, v) >> 24) & 0xF;
            if (current_ctx->tex0.use_CSM2)
                clut_CSM2_lookup(entry, info.srctex_color);
            else
                clut_lookup(entry, info.srctex_color);
            break;
        }
            break;
        case 0x2C:
        {
            uint8_t entry = read_PSMCT32_block(tex_base, width, u, v) >> 28;
            if (current_ctx->tex0.use_CSM2)
                clut_CSM2_lookup(entry, info.srctex_color);
            else
                clut_lookup(entry, info.srctex_color);
        }
            break;
        case 0x30:
        {
            uint32_t color = read_PSMCT32Z_block(tex_base, width, u, v);
            info.srctex_color.r = color & 0xFF;
            info.srctex_color.g = (color >> 8) & 0xFF;
            info.srctex_color.b = (color >> 16) & 0xFF;
            info.srctex_color.a = color >> 24;
        }
            break;
        case 0x31:
        {
            uint32_t color = read_PSMCT32Z_block(tex_base, width, u, v);
            info.srctex_color.r = color & 0xFF;
            info.srctex_color.g = (color >> 8) & 0xFF;
            info.srctex_color.b = (color >> 16) & 0xFF;
            if (!(color & 0xFFFFFF) && TEXA.trans_black)
                info.srctex_color.a = 0;
            else
                info.srctex_color.a = TEXA.alpha0;
        }
            break;
        case 0x32:
        {
            uint16_t color = read_PSMCT16Z_block(tex_base, width, u, v);
            info.srctex_color.r = (color & 0x1F) << 3;
            info.srctex_color.g = ((color >> 5) & 0x1F) << 3;
            info.srctex_color.b = ((color >> 10) & 0x1F) << 3;
            info.srctex_color.a = get_16bit_alpha(color);
        }
            break;
        case 0x3A:
        {
            uint16_t color = read_PSMCT16SZ_block(tex_base, width, u, v);
            info.srctex_color.r = (color & 0x1F) << 3;
            info.srctex_color.g = ((color >> 5) & 0x1F) << 3;
            info.srctex_color.b = ((color >> 10) & 0x1F) << 3;
            info.srctex_color.a = get_16bit_alpha(color);
        }
            break;
        default:
            Errors::die("[GS_t] Unrecognized texture format $%02X\n", current_ctx->tex0.format);
    }
}

_noinline(void) GraphicsSynthesizerThread::jit_tex_lookup(int16_t u, int16_t v, TexLookupInfo *info)
{
#ifdef _MSC_VER
    jit_tex_lookup_asm(jit_tex_lookup_func, u, v, info);
#else
    __asm__(
        "pushq %%r12\n\t"
        "pushq %%r13\n\t"
        "pushq %%r14\n\t"
        "pushq %%r15\n\t"
        "subq $8, %%rsp\n\t"
        "xor %%r12, %%r12\n\t"
        "xor %%r13, %%r13\n\t"
        "mov %0, %%r12w\n\t"
        "mov %1, %%r13w\n\t"
        "mov %2, %%r14\n\t"
        "callq *%3\n\t"
        "addq $8, %%rsp\n\t"
        "popq %%r15\n\t"
        "popq %%r14\n\t"
        "popq %%r13\n\t"
        "popq %%r12\n\t"
                :
                : "r" (u), "r" (v), "r" (info), "r" (jit_tex_lookup_func)

    );
#endif
}

void GraphicsSynthesizerThread::clut_lookup(uint8_t entry, RGBAQ_REG &tex_color)
{
    uint32_t clut_addr = current_ctx->tex0.CLUT_offset;

    switch (current_ctx->tex0.CLUT_format)
    {
        //PSMCT32
        case 0x00:
        case 0x01:
        {
            uint32_t color = *(uint32_t*)&clut_cache[((clut_addr << 1) + (entry << 2)) & 0x7FF];
            tex_color.r = color & 0xFF;
            tex_color.g = (color >> 8) & 0xFF;
            tex_color.b = (color >> 16) & 0xFF;
            tex_color.a = color >> 24;
        }
            break;
        //PSMCT16
        case 0x02:
        //PSMCT16S
        case 0x0A:
        {
            uint16_t color = *(uint16_t*)&clut_cache[((clut_addr + entry) << 1) & 0x3FF];
            tex_color.r = (color & 0x1F) << 3;
            tex_color.g = ((color >> 5) & 0x1F) << 3;
            tex_color.b = ((color >> 10) & 0x1F) << 3;
            tex_color.a = get_16bit_alpha(color);
        }
            break;
        default:
            Errors::die("[GS_t] Unrecognized CLUT format $%02X\n", current_ctx->tex0.CLUT_format);
    }
}

void GraphicsSynthesizerThread::clut_CSM2_lookup(uint8_t entry, RGBAQ_REG &tex_color)
{
    uint16_t color = *(uint16_t*)&clut_cache[entry << 1];
    tex_color.r = (color & 0x1F) << 3;
    tex_color.g = ((color >> 5) & 0x1F) << 3;
    tex_color.b = ((color >> 10) & 0x1F) << 3;
    tex_color.a = get_16bit_alpha(color);
}

void GraphicsSynthesizerThread::reload_clut(const GSContext& context)
{
    int eight_bit = false;
    switch (context.tex0.format)
    {
        case 0x13: //8-bit textures
        case 0x1B:
            eight_bit = true;
            break;
        case 0x14: //4-bit textures
        case 0x24:
        case 0x2C:
            eight_bit = false;
            break;
        default:
            //Not sure what to do here?
            //Okami loads a new CLUT with a texture format of PSMCT32, so we
            eight_bit = true;
    }

    uint32_t clut_addr = context.tex0.CLUT_base;
    int cache_addr = context.tex0.CLUT_offset;
    bool reload = false;

    switch (context.tex0.CLUT_control)
    {
        case 1: //Load new CLUT
            reload = true;
            break;
        case 2: //Load new CLUT and CBP0
            reload = true;
            CBP0 = clut_addr;
            break;
        case 3: //Load new CLUT and CBP1
            reload = true;
            CBP1 = clut_addr;
            break;
        case 4: //Load new CLUT if CBP != CBP0
            reload = clut_addr != CBP0;
            if (reload)
                CBP0 = clut_addr;
            break;
        case 5: //Load new CLUT if CBP != CBP1
            reload = clut_addr != CBP1;
            if (reload)
                CBP1 = clut_addr;
            break;
        default:
            return;
    }

    int entries = (eight_bit) ? 256 : 16;

    if (reload)
    {
        printf("[GS_t] Reloading CLUT cache!\n");
        for (int i = 0; i < entries; i++)
        {
            if (context.tex0.use_CSM2)
            {
                uint16_t value = read_PSMCT16_block(current_ctx->tex0.CLUT_base, TEXCLUT.width,
                                                    TEXCLUT.x + i, TEXCLUT.y);
                *(uint16_t*)&clut_cache[cache_addr] = value;
                cache_addr = (cache_addr + 2) & 0x3FF;
            }
            else
            {
                uint32_t x, y;
                if (eight_bit)
                {
                    x = i & 0x7;
                    if (i & 0x10)
                        x += 8;
                    y = (i & 0xE0) / 0x10;
                    if (i & 0x8)
                        y++;
                }
                else
                {
                    x = i & 0x7;
                    y = i / 8;
                }

                switch (context.tex0.CLUT_format)
                {
                    case 0x00:
                    case 0x01:
                    {
                        uint32_t value = read_PSMCT32_block(clut_addr, 64, x, y);
                        *(uint32_t*)&clut_cache[cache_addr] = value;
                        //printf("[GS_t] Reload cache $%04X: $%08X\n", cache_addr, value);
                    }
                        cache_addr += 4;
                        break;
                    case 0x02:
                    {
                        uint16_t value = read_PSMCT16_block(clut_addr, 64, x, y);
                        *(uint16_t*)&clut_cache[cache_addr] = value;
                    }
                        cache_addr += 2;
                        break;
                    case 0x0A:
                    {
                        uint16_t value = read_PSMCT16S_block(clut_addr, 64, x, y);
                        *(uint16_t*)&clut_cache[cache_addr] = value;
                    }
                        cache_addr += 2;
                        break;
                }
            }

            cache_addr &= 0x3FF;
        }
    }
}

void GraphicsSynthesizerThread::update_draw_pixel_state()
{
    draw_pixel_state = 0;

    draw_pixel_state |= current_ctx->test.alpha_ref;
    draw_pixel_state |= current_ctx->test.alpha_test << 8;
    draw_pixel_state |= current_ctx->test.alpha_method << 9;
    draw_pixel_state |= current_ctx->test.alpha_fail_method << 12;
    draw_pixel_state |= current_ctx->test.depth_test << 14;
    draw_pixel_state |= current_ctx->test.depth_method << 15;
    draw_pixel_state |= current_ctx->test.dest_alpha_test << 17;
    draw_pixel_state |= current_ctx->test.dest_alpha_method << 18;
    draw_pixel_state |= current_ctx->frame.format << 19;
    draw_pixel_state |= current_PRMODE->alpha_blend << 25;
    draw_pixel_state |= PABE << 26;
    draw_pixel_state |= current_ctx->alpha.spec_A << 27;
    draw_pixel_state |= (uint64_t)current_ctx->alpha.spec_B << 29UL;
    draw_pixel_state |= (uint64_t)current_ctx->alpha.spec_C << 31UL;
    draw_pixel_state |= (uint64_t)current_ctx->alpha.spec_D << 33UL;
    draw_pixel_state |= (uint64_t)DTHE << 34UL;
    draw_pixel_state |= (uint64_t)COLCLAMP << 35UL;
    draw_pixel_state |= (uint64_t)current_ctx->zbuf.format << 36UL;
    draw_pixel_state |= (uint64_t)SCANMSK << 40UL;
    draw_pixel_state |= (uint64_t)current_ctx->zbuf.no_update << 42UL;
    draw_pixel_state |= (uint64_t)current_ctx->alpha.fixed_alpha << 43UL;
    draw_pixel_state |= (uint64_t)(current_PRMODE == &PRIM) << 51UL;
    draw_pixel_state |= (uint64_t)(current_ctx == &context1) << 52UL;
    draw_pixel_state |= (uint64_t)(current_ctx->frame.mask != 0) << 53UL;
}

void GraphicsSynthesizerThread::update_tex_lookup_state()
{
    tex_lookup_state = 0;

    tex_lookup_state |= current_PRMODE == &PRIM;
    tex_lookup_state |= current_PRMODE->use_UV << 1;
    tex_lookup_state |= current_ctx->tex1.filter_larger << 2;
    tex_lookup_state |= current_ctx->tex1.filter_smaller << 3;
    tex_lookup_state |= current_ctx->clamp.wrap_s << 6;
    tex_lookup_state |= current_ctx->clamp.wrap_t << 8;
    tex_lookup_state |= current_ctx->tex0.format << 10;
    tex_lookup_state |= TEXA.alpha0 << 16;
    tex_lookup_state |= (uint64_t)TEXA.alpha1 << 24UL;
    tex_lookup_state |= (uint64_t)TEXA.trans_black << 32UL;
    tex_lookup_state |= (uint64_t)current_ctx->tex0.use_CSM2 << 33UL;
    tex_lookup_state |= (uint64_t)current_ctx->tex0.CLUT_format << 34UL;
    tex_lookup_state |= (uint64_t)(current_ctx == &context1) << 38UL;
    tex_lookup_state |= (uint64_t)current_ctx->tex0.color_function << 39UL;
    tex_lookup_state |= (uint64_t)current_ctx->tex0.use_alpha << 41UL;
    tex_lookup_state |= (uint64_t)current_PRMODE->fog << 42UL;
}

uint8_t* GraphicsSynthesizerThread::get_jitted_draw_pixel(uint64_t state)
{
    GSPixelJitBlockRecord* found_block = jit_draw_pixel_heap.find_block(state);
    if (!found_block)
    {
        printf("[GS_t] RECOMPILING DRAW PIXEL %llX\n", state);
        found_block = recompile_draw_pixel(state);
    }
    return (uint8_t*)found_block->code_start;
}

uint8_t* GraphicsSynthesizerThread::get_jitted_tex_lookup(uint64_t state)
{
    GSTextureJitBlockRecord* found_block = jit_tex_lookup_heap.find_block(state);
    if (!found_block)
    {
        printf("[GS_t] RECOMPILING TEX LOOKUP %llX\n", state);
        found_block = recompile_tex_lookup(state);
    }
    return (uint8_t*)found_block->code_start;
}

GSPixelJitBlockRecord* GraphicsSynthesizerThread::recompile_draw_pixel(uint64_t state)
{
    jit_draw_pixel_block.clear();

    //Prologue - create stack frame and save registers
    emitter_dp.PUSH(RBP);
    emitter_dp.SUB64_REG_IMM(0xF0, RSP);
    emitter_dp.MOV64_MR(RSP, RBP);

    emitter_dp.MOVAPS_TO_MEM(XMM0, RBP, 0);
    emitter_dp.MOVAPS_TO_MEM(XMM1, RBP, 0x10);
    emitter_dp.MOVAPS_TO_MEM(XMM2, RBP, 0x20);
    emitter_dp.MOVAPS_TO_MEM(XMM3, RBP, 0x30);
    emitter_dp.MOVAPS_TO_MEM(XMM4, RBP, 0x40);

    emitter_dp.MOV64_TO_MEM(RBX, RBP, 0x50);
    emitter_dp.MOV64_TO_MEM(RDI, RBP, 0x58);
    emitter_dp.MOV64_TO_MEM(RSI, RBP, 0x60);

    //R12 = x  R13 = y  R14 = z  R15 = color

    //Shift x and y to the right by 4 (remove fractional component)
    emitter_dp.SAR32_REG_IMM(4, R12);
    emitter_dp.SAR32_REG_IMM(4, R13);

    //SCANMSK test - stop drawing on odd or even y coordinate
    if (SCANMSK >= 2)
    {
        uint8_t* scanmsk_success_dest = nullptr;
        emitter_dp.TEST8_REG_IMM(0x1, R13);
        if (SCANMSK == 2) //Fail if even (result is 0)
            scanmsk_success_dest = emitter_dp.JCC_NEAR_DEFERRED(ConditionCode::NE);
        else //Fail if odd
            scanmsk_success_dest = emitter_dp.JCC_NEAR_DEFERRED(ConditionCode::E);

        //Return on failure
        jit_epilogue_draw_pixel();

        //Success
        emitter_dp.set_jump_dest(scanmsk_success_dest);
    }

    //RBX = bitfield for storing update variables (update_frame, update_z, update_alpha)
    //If a flag is set, the component will NOT be updated
    //Bit 0 = frame, bit 1 = z, bit 2 = alpha
    emitter_dp.XOR32_REG(RBX, RBX);
    if (current_ctx->frame.format & 0x1)
        emitter_dp.OR32_REG_IMM(0x4, RBX);

    //Alpha test
    if ((current_ctx->test.alpha_test) && current_ctx->test.alpha_method != 1)
        recompile_alpha_test();

    //Hack for SotC - removes overbloom (but breaks other games of course)
    //else if (current_ctx->test.alpha_fail_method != 1)
        //emitter_dp.OR32_REG_IMM(0x2, RBX);

    //Depth test
    if (current_ctx->test.depth_test)
        recompile_depth_test();

    emitter_dp.TEST32_REG_IMM(0x1, RBX);
    uint8_t* do_not_update_rgba = emitter_dp.JCC_NEAR_DEFERRED(ConditionCode::NE);

    //Get framebuffer address and store on stack
    emitter_dp.load_addr((uint64_t)&current_ctx->frame.base_pointer, abi_args[0]);
    emitter_dp.MOV32_FROM_MEM(abi_args[0], abi_args[0]);
    emitter_dp.SAR32_REG_IMM(8, abi_args[0]);
    emitter_dp.load_addr((uint64_t)&current_ctx->frame.width, abi_args[1]);
    emitter_dp.MOV32_FROM_MEM(abi_args[1], abi_args[1]);
    emitter_dp.SAR32_REG_IMM(6, abi_args[1]);
    emitter_dp.MOV64_MR(R12, abi_args[2]);
    emitter_dp.MOV64_MR(R13, abi_args[3]);

    switch (current_ctx->frame.format)
    {
        case 0x00:
        case 0x01:
            //PSMCT32/PSMCT24
            jit_call_func(emitter_dp, (uint64_t)&addr_PSMCT32);
            break;
        case 0x02:
            //PSMCT16
            jit_call_func(emitter_dp, (uint64_t)&addr_PSMCT16);
            break;
        case 0x0A:
            //PSMCT16S
            jit_call_func(emitter_dp, (uint64_t)&addr_PSMCT16S);
            break;
        case 0x30:
        case 0x31:
            //PSMCT32Z/PSMCT24Z
            jit_call_func(emitter_dp, (uint64_t)&addr_PSMCT32Z);
            break;
        case 0x32:
            //PSMCT16Z
            jit_call_func(emitter_dp, (uint64_t)&addr_PSMCT16Z);
            break;
        case 0x3A:
            //PSMCT16SZ
            jit_call_func(emitter_dp, (uint64_t)&addr_PSMCT16SZ);
            break;
        default:
            Errors::die("[GS_t] Unrecognized frame format $%02X in recompile_draw_pixel", current_ctx->frame.format);
    }

    //[RBP + 0xD0] = pointer to framebuffer pixel
    //[RBP + 0xD8] = framebuffer pixel
    emitter_dp.load_addr((uint64_t)local_mem, RCX);
    emitter_dp.ADD64_REG(RCX, RAX);
    emitter_dp.MOV64_TO_MEM(RAX, RBP, 0xD0);

    switch (current_ctx->frame.format)
    {
        case 0x00:
        case 0x01:
        case 0x30:
        case 0x31:
            emitter_dp.MOV32_FROM_MEM(RAX, RAX);
            break;
        case 0x02:
        case 0x0A:
        case 0x32:
        case 0x3A:
            emitter_dp.MOV32_FROM_MEM(RAX, RAX);

            //RCX = R, RDX = G, RSI = B, RDI = A
            emitter_dp.MOV32_REG(RAX, RCX);
            emitter_dp.MOV32_REG(RAX, RDX);
            emitter_dp.MOV32_REG(RAX, RSI);
            emitter_dp.MOV32_REG(RAX, RDI);
            emitter_dp.XOR32_REG(RAX, RAX);

            emitter_dp.AND32_REG_IMM(0x1F, RCX);
            emitter_dp.AND32_REG_IMM(0x1F << 5, RDX);
            emitter_dp.AND32_REG_IMM(0x1F << 10, RSI);
            emitter_dp.AND32_REG_IMM(1 << 15, RDI);

            emitter_dp.SHL32_REG_IMM(3, RCX);
            emitter_dp.SHL32_REG_IMM(6, RDX);
            emitter_dp.SHL32_REG_IMM(9, RSI);
            emitter_dp.SHL32_REG_IMM(16, RDI);

            emitter_dp.OR32_REG(RCX, RAX);
            emitter_dp.OR32_REG(RDX, RAX);
            emitter_dp.OR32_REG(RSI, RAX);
            emitter_dp.OR32_REG(RDI, RAX);
            break;
        default:
            Errors::die("[GS JIT] Unrecognized read framebuffer format $%02X", current_ctx->frame.format);
    }

    emitter_dp.MOV32_TO_MEM(RAX, RBP, 0xD8);

    //Dest alpha test
    if (current_ctx->test.dest_alpha_test && !(current_ctx->frame.format & 0x1))
    {
        emitter_dp.MOV32_FROM_MEM(RBP, RAX, 0xD8);
        emitter_dp.TEST32_EAX(1 << 31);

        ConditionCode pass_code;
        if (current_ctx->test.dest_alpha_method)
            pass_code = ConditionCode::NE;
        else
            pass_code = ConditionCode::E;

        uint8_t* pass_dest_alpha_test = emitter_dp.JCC_NEAR_DEFERRED(pass_code);

        jit_epilogue_draw_pixel();

        emitter_dp.set_jump_dest(pass_dest_alpha_test);
    }

    if (current_PRMODE->alpha_blend)
        recompile_alpha_blend();
    else
    {
        //Get color: R14 = color in RGBA32 format
        emitter_dp.MOVQ_TO_XMM(R15, XMM0);
        emitter_dp.PACKUSWB(XMM0, XMM0);
        emitter_dp.MOVD_FROM_XMM(XMM0, R14);
    }

    //TODO: Are we not supposed to apply FBA for RGB24? It makes sense not to.
    if (current_ctx->FBA && !(current_ctx->frame.format & 0x1))
        emitter_dp.OR32_REG_IMM(0x80000000, R14);

    //Don't bother applying FBMASK if it's set to 0
    if (current_ctx->frame.mask)
    {
        emitter_dp.MOV32_FROM_MEM(RBP, RAX, 0xD8);

        //color = (color & ~mask) | (frame_color & mask)
        emitter_dp.load_addr((uint64_t)&current_ctx->frame.mask, RCX);
        emitter_dp.MOV32_FROM_MEM(RCX, RCX);
        emitter_dp.AND32_REG(RCX, RAX);
        emitter_dp.NOT32(RCX);
        emitter_dp.AND32_REG(RCX, R14);
        emitter_dp.OR32_REG(RAX, R14);
    }

    //Update alpha?
    emitter_dp.TEST32_REG_IMM(0x4, RBX);
    uint8_t* update_alpha_dest = emitter_dp.JCC_NEAR_DEFERRED(ConditionCode::E);

    //color = (color & 0xFFFFFF) | (framebuffer_color & 0xFF000000)
    emitter_dp.MOV32_FROM_MEM(RBP, RDX, 0xD8);
    emitter_dp.AND32_REG_IMM(0x00FFFFFF, R14);
    emitter_dp.AND32_REG_IMM(0xFF000000, RDX);
    emitter_dp.OR32_REG(RDX, R14);

    emitter_dp.set_jump_dest(update_alpha_dest);

    //RCX = framebuffer address
    emitter_dp.MOV64_FROM_MEM(RBP, RCX, 0xD0);

    switch (current_ctx->frame.format)
    {
        case 0x00:
        case 0x01:
        case 0x30:
        case 0x31:
            emitter_dp.MOV32_TO_MEM(R14, RCX);
            break;
        case 0x02:
        case 0x0A:
        case 0x32:
        case 0x3A:
            //Alpha
            emitter_dp.MOV32_REG(R14, RDX);
            emitter_dp.AND32_REG_IMM(0x80000000, RDX);
            emitter_dp.SHR32_REG_IMM(16, RDX);

            //B
            emitter_dp.MOV32_REG(R14, RAX);
            emitter_dp.AND32_EAX(0x00F80000);
            emitter_dp.SHR32_REG_IMM(9, RAX);
            emitter_dp.OR32_REG(RAX, RDX);

            //G
            emitter_dp.MOV32_REG(R14, RAX);
            emitter_dp.AND32_EAX(0x0000F800);
            emitter_dp.SHR32_REG_IMM(6, RAX);
            emitter_dp.OR32_REG(RAX, RDX);

            //R
            emitter_dp.MOV32_REG(R14, RAX);
            emitter_dp.AND32_EAX(0x000000F8);
            emitter_dp.SHR32_REG_IMM(3, RAX);
            emitter_dp.OR32_REG(RAX, RDX);
            emitter_dp.MOV16_TO_MEM(RDX, RCX);
            break;
        default:
            Errors::die("[GS_t] Unrecognized frame format $%02X in recompile_draw_pixel", current_ctx->frame.format);
    }

    emitter_dp.set_jump_dest(do_not_update_rgba);
    jit_epilogue_draw_pixel();
    jit_draw_pixel_block.print_block();
    jit_draw_pixel_block.print_literal_pool();
    //jit_draw_pixel_block.set_current_block_rx();
    return jit_draw_pixel_heap.insert_block(state, &jit_draw_pixel_block);
}

void GraphicsSynthesizerThread::recompile_alpha_test()
{
    //If the condition is NEVER, do not compare and just proceed with the failure condition
    if (current_ctx->test.alpha_method != 0)
    {
        //Load alpha from color
        emitter_dp.MOV64_MR(R15, RAX);
        emitter_dp.SAR64_REG_IMM(48, RAX);

        //Compare alpha with REF
        emitter_dp.CMP32_EAX(current_ctx->test.alpha_ref);
        uint8_t* alpha_test_success = nullptr;
        switch (current_ctx->test.alpha_method)
        {
            case 2: //LESS
                alpha_test_success = emitter_dp.JCC_NEAR_DEFERRED(ConditionCode::L);
                break;
            case 3: //LEQUAL
                alpha_test_success = emitter_dp.JCC_NEAR_DEFERRED(ConditionCode::LE);
                break;
            case 4: //EQUAL
                alpha_test_success = emitter_dp.JCC_NEAR_DEFERRED(ConditionCode::E);
                break;
            case 5: //GEQUAL
                alpha_test_success = emitter_dp.JCC_NEAR_DEFERRED(ConditionCode::GE);
                break;
            case 6: //GREATER
                alpha_test_success = emitter_dp.JCC_NEAR_DEFERRED(ConditionCode::G);
                break;
            case 7: //NOTEQUAL
                alpha_test_success = emitter_dp.JCC_NEAR_DEFERRED(ConditionCode::NE);
                break;
        }

        //Test for failure
        switch (current_ctx->test.alpha_fail_method)
        {
            case 0: //KEEP - Update nothing
                jit_epilogue_draw_pixel();
                break;
            case 1: //FB_ONLY - Only update framebuffer
                emitter_dp.OR16_REG_IMM(0x2, RBX);
                break;
            case 2: //ZB_ONLY - Only update z-buffer
                emitter_dp.OR16_REG_IMM(0x5, RBX);
                break;
            case 3: //RGB_ONLY - Same as FB_ONLY, but ignore alpha
                emitter_dp.OR16_REG_IMM(0x6, RBX);
                break;
        }

        emitter_dp.set_jump_dest(alpha_test_success);
    }
    else
    {
        //Test for failure
        switch (current_ctx->test.alpha_fail_method)
        {
            case 0: //KEEP - Update nothing
                jit_epilogue_draw_pixel();
                break;
            case 1: //FB_ONLY - Only update framebuffer
                emitter_dp.OR16_REG_IMM(0x2, RBX);
                break;
            case 2: //ZB_ONLY - Only update z-buffer
                emitter_dp.OR16_REG_IMM(0x5, RBX);
                break;
            case 3: //RGB_ONLY - Same as FB_ONLY, but ignore alpha
                emitter_dp.OR16_REG_IMM(0x6, RBX);
                break;
        }
    }
}

void GraphicsSynthesizerThread::recompile_depth_test()
{
    if (current_ctx->test.depth_method == 0)
    {
        jit_epilogue_draw_pixel();
        return;
    }

    //Load address to zbuffer
    emitter_dp.load_addr((uint64_t)&current_ctx->zbuf.base_pointer, abi_args[0]);
    emitter_dp.load_addr((uint64_t)&current_ctx->frame.width, abi_args[1]);
    emitter_dp.MOV32_FROM_MEM(abi_args[0], abi_args[0]);
    emitter_dp.MOV32_FROM_MEM(abi_args[1], abi_args[1]);
    emitter_dp.SAR32_REG_IMM(8, abi_args[0]);
    emitter_dp.SAR32_REG_IMM(6, abi_args[1]);
    emitter_dp.MOV64_MR(R12, abi_args[2]);
    emitter_dp.MOV64_MR(R13, abi_args[3]);

    switch (current_ctx->zbuf.format)
    {
        case 0x00:
            //PSMCT32Z
            jit_call_func(emitter_dp, (uint64_t)&addr_PSMCT32Z);
            break;
        case 0x01:
            //PSMCT24Z
            jit_call_func(emitter_dp, (uint64_t)&addr_PSMCT32Z);
            break;
        case 0x02:
            //PSMCT16Z
            jit_call_func(emitter_dp, (uint64_t)&addr_PSMCT16Z);
            break;
        case 0x0A:
            //PSMCT16SZ
            jit_call_func(emitter_dp, (uint64_t)&addr_PSMCT16SZ);
            break;
        default:
            Errors::die("[GS_t] Unrecognized zbuf format $%02X\n", current_ctx->zbuf.format);
    }

    //RCX = zbuffer address
    emitter_dp.load_addr((uint64_t)local_mem, RCX);
    emitter_dp.ADD64_REG(RAX, RCX);

    if (current_ctx->test.depth_method != 1)
    {
        ConditionCode depth_comparison;
        if (current_ctx->test.depth_method == 2)
            depth_comparison = ConditionCode::GE;
        else
            depth_comparison = ConditionCode::G;

        emitter_dp.MOV32_FROM_MEM(RCX, RAX);

        if (current_ctx->zbuf.format & 0x2)
            emitter_dp.AND32_EAX(0xFFFF);
        else if (current_ctx->zbuf.format & 0x1)
            emitter_dp.AND32_EAX(0xFFFFFF);

        //64-bit compare to avoid signed bullshit
        emitter_dp.CMP64_REG(RAX, R14);
        uint8_t* depth_passed = emitter_dp.JCC_NEAR_DEFERRED(depth_comparison);

        //Depth test failed, do not go any further
        jit_epilogue_draw_pixel();

        emitter_dp.set_jump_dest(depth_passed);
    }

    //Update zbuffer
    if (!current_ctx->zbuf.no_update)
    {
        emitter_dp.TEST32_REG_IMM(0x2, RBX);
        uint8_t* do_not_update_z = emitter_dp.JCC_NEAR_DEFERRED(ConditionCode::NE);

        switch (current_ctx->zbuf.format)
        {
            case 0x00:
                emitter_dp.MOV32_TO_MEM(R14, RCX);
                break;
            case 0x01:
                emitter_dp.MOV32_FROM_MEM(RCX, RAX);
                emitter_dp.AND32_EAX(0xFF000000);
                emitter_dp.AND32_REG_IMM(0xFFFFFF, R14);
                emitter_dp.OR32_REG(RAX, R14);
                emitter_dp.MOV32_TO_MEM(R14, RCX);
                break;
            default:
                emitter_dp.MOV16_TO_MEM(R14, RCX);
        }

        emitter_dp.set_jump_dest(do_not_update_z);
    }
}

void GraphicsSynthesizerThread::recompile_alpha_blend()
{
    printf("Alpha blend: %d %d %d %d\n", current_ctx->alpha.spec_A, current_ctx->alpha.spec_B,
           current_ctx->alpha.spec_C, current_ctx->alpha.spec_D);

    if (PABE)
    {
        //If alpha < 0x80, do not perform alpha blending
        emitter_dp.MOV64_MR(R15, RAX);
        emitter_dp.SHR64_REG_IMM(48, RAX);
        emitter_dp.TEST32_EAX(0x80);

        uint8_t* pabe_success = emitter_dp.JCC_NEAR_DEFERRED(ConditionCode::NE);

        jit_epilogue_draw_pixel();

        emitter_dp.set_jump_dest(pabe_success);
    }

    //Local stack variables - vertex/texture color is stored in R15
    //Note that colors are 16-bit format so that we can handle overflows/underflows
    uint32_t fb_pixel_addr = 0xD8;

    emitter_dp.MOV32_FROM_MEM(RBP, RAX, fb_pixel_addr);

    //Convert 32-bit framebuffer color to 64-bit
    emitter_dp.MOVD_TO_XMM(RAX, XMM4);
    emitter_dp.PMOVZX8_TO_16(XMM4, XMM4);

    switch (current_ctx->alpha.spec_A)
    {
        case 0:
            emitter_dp.MOVQ_TO_XMM(R15, XMM0);
            break;
        case 1:
            emitter_dp.MOVAPS_REG(XMM4, XMM0);
            break;
        case 2:
        case 3:
            emitter_dp.XORPS(XMM0, XMM0);
            break;
    }

    switch (current_ctx->alpha.spec_B)
    {
        case 0:
            emitter_dp.MOVQ_TO_XMM(R15, XMM1);
            break;
        case 1:
            emitter_dp.MOVAPS_REG(XMM4, XMM1);
            break;
        case 2:
        case 3:
            //Do nothing. We can optimize out the subtraction in the alpha blending operation
            break;
    }

    switch (current_ctx->alpha.spec_C)
    {
        case 0:
            emitter_dp.MOV64_MR(R15, RAX);
            emitter_dp.SAR64_REG_IMM(48, RAX);
            break;
        case 1:
            if (!(current_ctx->frame.format & 0x1))
                emitter_dp.SAR64_REG_IMM(48, RAX);
            else
                emitter_dp.MOV32_REG_IMM(0x80, RAX);
            break;
        case 2:
        case 3:
            emitter_dp.MOV32_REG_IMM(current_ctx->alpha.fixed_alpha, RAX);
            break;
    }

    //Duplicate alpha into 4 16-bit words
    emitter_dp.MOVD_TO_XMM(RAX, XMM2);
    emitter_dp.PSHUFLW(0, XMM2, XMM2);

    switch (current_ctx->alpha.spec_D)
    {
        case 0:
            emitter_dp.MOVQ_TO_XMM(R15, XMM3);
            break;
        case 1:
            emitter_dp.MOVAPS_REG(XMM4, XMM3);
            break;
        case 2:
        case 3:
            break;
    }

    alignas(16) const static uint16_t and_const[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

    //color component = (((A - B) * C) >> 7) + D
    if (current_ctx->alpha.spec_B < 2)
    {
        //TODO: fix clamping in cases where (A - B) * C >= 0x8000 (with normal clamping, this goes to 0)
        emitter_dp.PSUBW(XMM1, XMM0);
        emitter_dp.PMULLW(XMM2, XMM0);
        emitter_dp.PSRAW(7, XMM0);
    }
    else
    {
        emitter_dp.PMULLW(XMM2, XMM0);
        emitter_dp.PSRLW(7, XMM0);
    }

    if (current_ctx->alpha.spec_D < 2)
        emitter_dp.PADDW(XMM3, XMM0);

    //Clamp color
    if (!COLCLAMP)
    {
        //Extract the lower 8 bits before packing
        emitter_dp.load_addr((uint64_t)&and_const, RAX);
        emitter_dp.PAND_XMM_FROM_MEM(RAX, XMM0);
    }

    emitter_dp.PACKUSWB(XMM0, XMM0);

    //Return color in R14
    emitter_dp.MOVD_FROM_XMM(XMM0, R14);

    //Append alpha to color
    emitter_dp.AND32_REG_IMM(0xFFFFFF, R14);
    emitter_dp.SHL32_REG_IMM(24, RAX);
    emitter_dp.OR32_REG(RAX, R14);
}

void GraphicsSynthesizerThread::jit_call_func(Emitter64& emitter, uint64_t addr)
{
#ifdef _MSC_VER
    emitter.SUB64_REG_IMM(0x20, RSP);
    emitter.MOV64_OI(addr, RAX);
    emitter.CALL_INDIR(RAX);
    emitter.ADD64_REG_IMM(0x20, RSP);
#else
    emitter.MOV64_OI(addr, RAX);
    emitter.CALL_INDIR(RAX);
#endif
}

void GraphicsSynthesizerThread::jit_epilogue_draw_pixel()
{
    emitter_dp.MOVAPS_FROM_MEM(RBP, XMM0, 0);
    emitter_dp.MOVAPS_FROM_MEM(RBP, XMM1, 0x10);
    emitter_dp.MOVAPS_FROM_MEM(RBP, XMM2, 0x20);
    emitter_dp.MOVAPS_FROM_MEM(RBP, XMM3, 0x30);
    emitter_dp.MOVAPS_FROM_MEM(RBP, XMM4, 0x40);

    emitter_dp.MOV64_FROM_MEM(RBP, RBX, 0x50);
    emitter_dp.MOV64_FROM_MEM(RBP, RDI, 0x58);
    emitter_dp.MOV64_FROM_MEM(RBP, RSI, 0x60);
    emitter_dp.ADD64_REG_IMM(0xF0, RSP);
    emitter_dp.POP(RBP);
    emitter_dp.RET();
}

GSTextureJitBlockRecord* GraphicsSynthesizerThread::recompile_tex_lookup(uint64_t state)
{
    jit_tex_lookup_block.clear();

    emitter_tex.PUSH(RBP);
    emitter_tex.SUB64_REG_IMM(0x100, RSP);
    emitter_tex.MOV64_MR(RSP, RBP);

    //Preserve used XMM registers on the stack
    emitter_tex.MOVAPS_TO_MEM(XMM0, RBP, 0);
    emitter_tex.MOVAPS_TO_MEM(XMM1, RBP, 0x10);

    //And preserve integer registers
    emitter_tex.MOV64_TO_MEM(RBX, RBP, 0x20);
    emitter_tex.MOV64_TO_MEM(RDI, RBP, 0x28);
    emitter_tex.MOV64_TO_MEM(RSI, RBP, 0x30);
    emitter_tex.MOV64_TO_MEM(R10, RBP, 0x38);
    emitter_tex.MOV64_TO_MEM(RDX, RBP, 0x40);

    //R12 = signed 16-bit u  R13 = signed 16-bit v  R14 = pointer to TexLookupInfo
    emitter_tex.MOVSX16_TO_32(R12, R12);
    emitter_tex.MOVSX16_TO_32(R13, R13);
    emitter_tex.SAR32_REG_IMM(4, R12);
    emitter_tex.SAR32_REG_IMM(4, R13);

    //Load tex width and height
    //RBX = width - 1, R15 = height - 1
    emitter_tex.MOV32_FROM_MEM(R14, RBX, (sizeof(RGBAQ_REG) * 3) + (4 * 4));
    emitter_tex.MOV32_REG(RBX, R15);
    emitter_tex.AND32_REG_IMM(0xFFFF, RBX);
    emitter_tex.DEC32(RBX);
    emitter_tex.SHR32_REG_IMM(16, R15);
    emitter_tex.DEC32(R15);

    //Min s, min t
    emitter_tex.XOR32_REG(RDX, RDX);
    emitter_tex.XOR32_REG(R8, R8);

    if (current_ctx->clamp.wrap_s >= 0x2)
    {
        emitter_tex.MOV32_FROM_MEM(R14, RCX, sizeof(RGBAQ_REG) * 3 + sizeof(float));
        emitter_tex.load_addr((uint64_t)&current_ctx->clamp.min_u, RDX);
        emitter_tex.MOV32_FROM_MEM(RDX, RDX);
        emitter_tex.AND32_REG_IMM(0xFFFF, RDX);

        if (current_ctx->clamp.wrap_s == 0x2)
            emitter_tex.SHR32_CL(RDX);

        emitter_tex.XOR32_REG(RBX, RBX);
        emitter_tex.load_addr((uint64_t)&current_ctx->clamp.max_u, RBX);
        emitter_tex.MOV32_FROM_MEM(RBX, RBX);
        emitter_tex.AND32_REG_IMM(0xFFFF, RBX);

        if (current_ctx->clamp.wrap_s == 0x2)
            emitter_tex.SHR32_CL(RBX);
    }

    if (current_ctx->clamp.wrap_t >= 0x2)
    {
        emitter_tex.MOV32_FROM_MEM(R14, RCX, sizeof(RGBAQ_REG) * 3 + sizeof(float));
        emitter_tex.load_addr((uint64_t)&current_ctx->clamp.min_v, R8);
        emitter_tex.MOV32_FROM_MEM(R8, R8);
        emitter_tex.AND32_REG_IMM(0xFFFF, R8);
        if (current_ctx->clamp.wrap_t == 0x2)
            emitter_tex.SHR32_CL(R8);

        emitter_tex.XOR32_REG(R15, R15);
        emitter_tex.load_addr((uint64_t)&current_ctx->clamp.max_v, R15);
        emitter_tex.MOV32_FROM_MEM(R15, R15);
        emitter_tex.AND32_REG_IMM(0xFFFF, R15);

        if (current_ctx->clamp.wrap_t == 0x2)
            emitter_tex.SHR32_CL(R15);
    }

    //Clamp u/v (s/t) appropriately
    switch (current_ctx->clamp.wrap_s)
    {
        case 0x0:
            //Repeat
            emitter_tex.AND32_REG(RBX, R12);
            break;
        case 0x1:
        case 0x2:
            //Clamp
        {
            //Is u > max_width?
            emitter_tex.CMP32_REG(RBX, R12);
            uint8_t* no_clamp_hi = emitter_tex.JCC_NEAR_DEFERRED(ConditionCode::LE);

            //u > max_width
            emitter_tex.MOV32_REG(RBX, R12);
            uint8_t* clamp_hi_end = emitter_tex.JMP_NEAR_DEFERRED();

            //Is u < min_width?
            emitter_tex.set_jump_dest(no_clamp_hi);
            emitter_tex.CMP32_REG(RDX, R12);
            uint8_t* no_clamp_lo = emitter_tex.JCC_NEAR_DEFERRED(ConditionCode::GE);

            //u < min_width
            emitter_tex.MOV32_REG(RDX, R12);

            //End
            emitter_tex.set_jump_dest(no_clamp_lo);
            emitter_tex.set_jump_dest(clamp_hi_end);
        }
            break;
        case 0x3:
            //u = (u & (min_u | 0xF)) | max_u
            emitter_tex.OR32_REG_IMM(0xF, RDX);
            emitter_tex.AND32_REG(RDX, R12);
            emitter_tex.OR32_REG(RBX, R12);
            break;
        default:
            Errors::die("[GS JIT] Unrecognized wrap s mode $%02X", current_ctx->clamp.wrap_s);
    }

    switch (current_ctx->clamp.wrap_t)
    {
        case 0x0:
            //Repeat
            emitter_tex.AND32_REG(R15, R13);
            break;
        case 0x1:
        case 0x2:
            //Clamp
        {
            //Is v > max_height?
            emitter_tex.CMP32_REG(R15, R13);
            uint8_t* no_clamp_hi = emitter_tex.JCC_NEAR_DEFERRED(ConditionCode::LE);

            //v > max_height
            emitter_tex.MOV32_REG(R15, R13);
            uint8_t* clamp_hi_end = emitter_tex.JMP_NEAR_DEFERRED();

            //Is v < min_height?
            emitter_tex.set_jump_dest(no_clamp_hi);
            emitter_tex.CMP32_REG(R8, R13);
            uint8_t* no_clamp_lo = emitter_tex.JCC_NEAR_DEFERRED(ConditionCode::GE);

            //v < min_height
            emitter_tex.MOV32_REG(R8, R13);

            //End
            emitter_tex.set_jump_dest(no_clamp_lo);
            emitter_tex.set_jump_dest(clamp_hi_end);
        }
            break;
        case 0x3:
            //v = (v & (min_v | 0xF)) | max_v
            emitter_tex.OR32_REG_IMM(0xF, R8);
            emitter_tex.AND32_REG(R8, R13);
            emitter_tex.OR32_REG(R15, R13);
            break;
        default:
            Errors::die("[GS JIT] Unrecognized wrap t mode $%02X", current_ctx->clamp.wrap_t);
    }

    //Load the texture pixel
    //TODO: bilinear filtering
    emitter_tex.MOV32_FROM_MEM(R14, abi_args[0], (sizeof(RGBAQ_REG) * 3) + (4 * 2));
    emitter_tex.SHR32_REG_IMM(8, abi_args[0]);
    emitter_tex.MOV32_FROM_MEM(R14, abi_args[1], (sizeof(RGBAQ_REG) * 3) + (4 * 3));
    emitter_tex.SHR32_REG_IMM(6, abi_args[1]);
    emitter_tex.MOV32_REG(R12, abi_args[2]);
    emitter_tex.MOV32_REG(R13, abi_args[3]);

    switch (current_ctx->tex0.format)
    {
        case 0x00:
        case 0x30:
            if (current_ctx->tex0.format & 0x30)
                jit_call_func(emitter_tex, (uint64_t)&addr_PSMCT32Z);
            else
                jit_call_func(emitter_tex, (uint64_t)&addr_PSMCT32);
            emitter_tex.load_addr((uint64_t)local_mem, RCX);
            emitter_tex.ADD64_REG(RCX, RAX);
            emitter_tex.MOV32_FROM_MEM(RAX, RAX);
            break;
        case 0x01:
        case 0x31:
            if (current_ctx->tex0.format & 0x30)
                jit_call_func(emitter_tex, (uint64_t)&addr_PSMCT32Z);
            else
                jit_call_func(emitter_tex, (uint64_t)&addr_PSMCT32);
            emitter_tex.load_addr((uint64_t)local_mem, RCX);
            emitter_tex.ADD64_REG(RCX, RAX);
            emitter_tex.MOV32_FROM_MEM(RAX, RAX);
            emitter_tex.AND32_EAX(0xFFFFFF);
            emitter_tex.MOV32_REG_IMM(TEXA.alpha0 << 24, RDI);
            if (TEXA.trans_black)
            {
                emitter_tex.OR32_REG(RAX, RDI);
                emitter_tex.TEST32_EAX(0xFFFFFF);
                emitter_tex.CMOVCC32_REG(ConditionCode::NE, RDI, RAX);
            }
            else
                emitter_tex.OR32_REG(RDI, RAX);
            break;
        case 0x02:
        case 0x0A:
        case 0x32:
        case 0x3A:
            if (current_ctx->tex0.format & 0x30)
            {
                if (current_ctx->tex0.format & 0x8)
                    jit_call_func(emitter_tex, (uint64_t)&addr_PSMCT16SZ);
                else
                    jit_call_func(emitter_tex, (uint64_t)&addr_PSMCT16Z);
            }
            else
            {
                if (current_ctx->tex0.format & 0x8)
                    jit_call_func(emitter_tex, (uint64_t)&addr_PSMCT16S);
                else
                    jit_call_func(emitter_tex, (uint64_t)&addr_PSMCT16);
            }
            emitter_tex.load_addr((uint64_t)local_mem, RCX);
            emitter_tex.ADD64_REG(RCX, RAX);
            emitter_tex.MOV32_FROM_MEM(RAX, RAX);
            emitter_tex.AND32_EAX(0xFFFF);
            recompile_convert_16bit_tex(RAX, RCX, RSI);
            break;
        case 0x09:
            //Invalid texture format used by FFX
            emitter_tex.MOV32_REG_IMM(0, RAX);
            break;
        case 0x13:
            jit_call_func(emitter_tex, (uint64_t)&addr_PSMCT8);
            emitter_tex.load_addr((uint64_t)local_mem, RCX);
            emitter_tex.ADD64_REG(RCX, RAX);
            emitter_tex.MOV32_FROM_MEM(RAX, RDI);
            emitter_tex.AND32_REG_IMM(0xFF, RDI);

            if (current_ctx->tex0.use_CSM2)
                recompile_csm2_lookup();
            else
                recompile_clut_lookup();
            break;
        case 0x14:
            jit_call_func(emitter_tex, (uint64_t)&addr_PSMCT4);
            emitter_tex.MOV32_REG(RAX, RCX);
            emitter_tex.SHR32_REG_IMM(1, RAX);
            emitter_tex.load_addr((uint64_t)local_mem, RDI);
            emitter_tex.ADD64_REG(RDI, RAX);

            //index = (local_mem[addr >> 1] >> ((addr & 0x1) << 2)) & 0xF
            //We do a 32-bit move as an 8-bit move converts EDI to BH. Not what we want
            emitter_tex.MOV32_FROM_MEM(RAX, RDI);
            emitter_tex.AND32_REG_IMM(0x1, RCX);
            emitter_tex.SHL32_REG_IMM(2, RCX);
            emitter_tex.SHR32_CL(RDI);
            emitter_tex.AND32_REG_IMM(0xF, RDI);

            if (current_ctx->tex0.use_CSM2)
                recompile_csm2_lookup();
            else
                recompile_clut_lookup();
            break;
        case 0x1B:
            jit_call_func(emitter_tex, (uint64_t)&addr_PSMCT32);
            emitter_tex.load_addr((uint64_t)local_mem, RCX);
            emitter_tex.ADD64_REG(RCX, RAX);
            emitter_tex.MOV32_FROM_MEM(RAX, RDI);
            emitter_tex.SHR32_REG_IMM(24, RDI);

            if (current_ctx->tex0.use_CSM2)
                recompile_csm2_lookup();
            else
                recompile_clut_lookup();
            break;
        case 0x24:
            jit_call_func(emitter_tex, (uint64_t)&addr_PSMCT32);
            emitter_tex.load_addr((uint64_t)local_mem, RCX);
            emitter_tex.ADD64_REG(RCX, RAX);
            emitter_tex.MOV32_FROM_MEM(RAX, RDI);
            emitter_tex.SHR32_REG_IMM(24, RDI);
            emitter_tex.AND32_REG_IMM(0xF, RDI);

            if (current_ctx->tex0.use_CSM2)
                recompile_csm2_lookup();
            else
                recompile_clut_lookup();
            break;
        case 0x2C:
            jit_call_func(emitter_tex, (uint64_t)&addr_PSMCT32);
            emitter_tex.load_addr((uint64_t)local_mem, RCX);
            emitter_tex.ADD64_REG(RCX, RAX);
            emitter_tex.MOV32_FROM_MEM(RAX, RDI);
            emitter_tex.SHR32_REG_IMM(28, RDI);

            if (current_ctx->tex0.use_CSM2)
                recompile_csm2_lookup();
            else
                recompile_clut_lookup();
            break;
        default:
            Errors::die("[GS JIT] Unrecognized texture format $%02X", current_ctx->tex0.format);
    }

    //Expand the texture color to 64-bit (16 bits for each color)
    emitter_tex.MOVD_TO_XMM(RAX, XMM0);
    emitter_tex.PMOVZX8_TO_16(XMM0, XMM0);
    emitter_tex.MOVQ_FROM_XMM(XMM0, RSI);

    //Read the vertex color
    //Since the vertex color is first, and each component is 16-bit, we can simply do a 64-bit move
    emitter_tex.MOV64_FROM_MEM(R14, RCX, 0);
    emitter_tex.MOVQ_TO_XMM(RCX, XMM1);

    switch (current_ctx->tex0.color_function)
    {
        case 0: //Modulate
            //tex_color = (tex_color * vtx_color) >> 7
            emitter_tex.PMULLW(XMM1, XMM0);
            emitter_tex.PSRLW(7, XMM0);

            //Clamp colors and re-convert back to 16-bit
            emitter_tex.PACKUSWB(XMM0, XMM0);
            emitter_tex.PMOVZX8_TO_16(XMM0, XMM0);
            emitter_tex.MOVQ_FROM_XMM(XMM0, RAX);
            break;
        case 1: //Decal
            emitter_tex.MOVQ_FROM_XMM(XMM0, RAX);
            break;
        case 2: //Highlight
        case 3: //Highlight2
            //tex_color = ((tex_color * vtx_color) >> 7) + vtx_alpha
            emitter_tex.PMULLW(XMM1, XMM0);
            emitter_tex.PSRLW(7, XMM0);

            emitter_tex.MOV64_MR(RCX, RDX);
            emitter_tex.SHR64_REG_IMM(48, RDX);
            emitter_tex.MOVQ_TO_XMM(RDX, XMM1);
            emitter_tex.PSHUFLW(0, XMM1, XMM1);
            emitter_tex.PADDW(XMM1, XMM0);

            emitter_tex.PACKUSWB(XMM0, XMM0);
            emitter_tex.PMOVZX8_TO_16(XMM0, XMM0);
            emitter_tex.MOVQ_FROM_XMM(XMM0, RAX);
            break;
        default:
            Errors::die("[GS JIT] Unrecognized color function $%02X", current_ctx->tex0.color_function);
    }

    //Store tex_color in the TexLookupInfo struct
    emitter_tex.MOV64_TO_MEM(RAX, R14, sizeof(RGBAQ_REG));

    if (!current_ctx->tex0.use_alpha)
    {
        //tex_color.a = vtx_color.a
        emitter_tex.SHR64_REG_IMM(48, RCX);
        emitter_tex.MOV16_TO_MEM(RCX, R14, sizeof(RGBAQ_REG) + (sizeof(uint16_t) * 3));
    }
    else if (current_ctx->tex0.color_function == 2)
    {
        //tex_color.a += vtx_color.a
        //TODO: clamp
        emitter_tex.ADD64_REG(RSI, RCX);
        emitter_tex.SHR64_REG_IMM(48, RCX);
        emitter_tex.MOV16_TO_MEM(RCX, R14, sizeof(RGBAQ_REG) + (sizeof(uint16_t) * 3));
    }
    else if (current_ctx->tex0.color_function == 3)
    {
        //Keep tex_color.a unmodified (modulation equation affected alpha)
        emitter_tex.SHR64_REG_IMM(48, RSI);
        emitter_tex.MOV16_TO_MEM(RSI, R14, sizeof(RGBAQ_REG) + (sizeof(uint16_t) * 3));
    }

    emitter_tex.MOVAPS_FROM_MEM(RBP, XMM0, 0);
    emitter_tex.MOVAPS_FROM_MEM(RBP, XMM1, 0x10);
    emitter_tex.MOV64_FROM_MEM(RBP, RBX, 0x20);
    emitter_tex.MOV64_FROM_MEM(RBP, RDI, 0x28);
    emitter_tex.MOV64_FROM_MEM(RBP, RSI, 0x30);
    emitter_tex.MOV64_FROM_MEM(RBP, R10, 0x38);
    emitter_tex.MOV64_FROM_MEM(RBP, RDX, 0x40);

    emitter_tex.ADD64_REG_IMM(0x100, RSP);

    emitter_tex.POP(RBP);
    emitter_tex.RET();

    jit_tex_lookup_block.print_block();
    jit_tex_lookup_block.print_literal_pool();
    //jit_tex_lookup_block.set_current_block_rx();
    return jit_tex_lookup_heap.insert_block(state, &jit_tex_lookup_block);
}

void GraphicsSynthesizerThread::recompile_clut_lookup()
{
    //Input: RDI (index)
    //Output: RAX (color in 32-bit format)

    //RCX = current_ctx->tex0.CLUT_offset << 1
    emitter_tex.load_addr((uint64_t)&current_ctx->tex0.CLUT_offset, RCX);
    emitter_tex.MOV32_FROM_MEM(RCX, RCX);
    emitter_tex.SHL32_REG_IMM(1, RCX);
    emitter_tex.load_addr((uint64_t)&clut_cache, RAX);

    switch (current_ctx->tex0.CLUT_format)
    {
        case 0x00:
        case 0x01:
            emitter_tex.SHL32_REG_IMM(2, RDI);
            emitter_tex.ADD32_REG(RDI, RCX);
            emitter_tex.AND32_REG_IMM(0x7FF, RCX);
            emitter_tex.ADD64_REG(RCX, RAX);
            emitter_tex.MOV32_FROM_MEM(RAX, RAX);
            break;
        case 0x02:
        case 0x0A:
            emitter_tex.SHL32_REG_IMM(1, RDI);
            emitter_tex.ADD32_REG(RDI, RCX);
            emitter_tex.AND32_REG_IMM(0x3FF, RCX);
            emitter_tex.ADD64_REG(RCX, RAX);
            emitter_tex.MOV32_FROM_MEM(RAX, RAX);
            emitter_tex.AND32_EAX(0xFFFF);

            recompile_convert_16bit_tex(RAX, RCX, RDI);
            break;
        default:
            Errors::die("[GS JIT] Unrecognized CLUT format $%02X", current_ctx->tex0.CLUT_format);
    }
}

void GraphicsSynthesizerThread::recompile_csm2_lookup()
{
    //color = *(uint16_t*)&clut_cache[index << 1]
    emitter_tex.load_addr((uint64_t)&clut_cache, RAX);
    emitter_tex.SHL32_REG_IMM(1, RDI);
    emitter_tex.ADD64_REG(RDI, RAX);
    emitter_tex.MOV32_FROM_MEM(RAX, RAX);
    emitter_tex.AND32_EAX(0xFFFF);

    recompile_convert_16bit_tex(RAX, RDI, RSI);
}

void GraphicsSynthesizerThread::recompile_convert_16bit_tex(REG_64 color, REG_64 temp, REG_64 temp2)
{
    //R
    emitter_tex.MOV32_REG(color, temp);
    emitter_tex.AND32_REG_IMM(0x1F, temp);
    emitter_tex.SHL32_REG_IMM(3, temp);
    emitter_tex.MOV32_REG(temp, temp2);

    //G
    emitter_tex.MOV32_REG(color, temp);
    emitter_tex.AND32_REG_IMM(0x1F << 5, temp);
    emitter_tex.SHL32_REG_IMM(6, temp);
    emitter_tex.OR32_REG(temp, temp2);

    //B
    emitter_tex.MOV32_REG(color, temp);
    emitter_tex.AND32_REG_IMM(0x1F << 10, temp);
    emitter_tex.SHL32_REG_IMM(9, temp);
    emitter_tex.OR32_REG(temp, temp2);

    //A
    emitter_tex.TEST32_REG_IMM(1 << 15, color);

    uint8_t* bit_set_dest = emitter_tex.JCC_NEAR_DEFERRED(ConditionCode::NE);

    //Bit not set
    if (TEXA.trans_black)
    {
        emitter_tex.TEST32_REG_IMM(0xFFFF, color);
        uint8_t* trans_dest = emitter_tex.JCC_NEAR_DEFERRED(ConditionCode::E);

        emitter_tex.OR32_REG_IMM(TEXA.alpha0 << 24, temp2);

        emitter_tex.set_jump_dest(trans_dest);
    }
    else
        emitter_tex.OR32_REG_IMM(TEXA.alpha0 << 24, temp2);

    uint8_t* bit_not_set_end = emitter_tex.JMP_NEAR_DEFERRED();

    emitter_tex.set_jump_dest(bit_set_dest);
    emitter_tex.OR32_REG_IMM(TEXA.alpha1 << 24, temp2);

    emitter_tex.set_jump_dest(bit_not_set_end);
    emitter_tex.MOV32_REG(temp2, color);
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
    state->read((char*)&PRMODE, sizeof(PRMODE));
    int current_PRMODE_id = 1;
    state->read((char*)&current_PRMODE_id, sizeof(current_PRMODE_id));
    if (current_PRMODE_id == 1)
        current_PRMODE = &PRIM;
    else
        current_PRMODE = &PRMODE;
    state->read((char*)&RGBAQ, sizeof(RGBAQ));
    state->read((char*)&UV, sizeof(UV));
    state->read((char*)&ST, sizeof(ST));
    state->read((char*)&TEXA, sizeof(TEXA));
    state->read((char*)&TEXCLUT, sizeof(TEXCLUT));
    state->read((char*)&DTHE, sizeof(DTHE));
    state->read((char*)&COLCLAMP, sizeof(COLCLAMP));
    state->read((char*)&FOG, sizeof(FOG));
    state->read((char*)&FOGCOL, sizeof(FOGCOL));
    state->read((char*)&PABE, sizeof(PABE));
    state->read((char*)&SCANMSK, sizeof(SCANMSK));

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
    state->write((char*)&PRMODE, sizeof(PRMODE));

    int current_PRMODE_id = 1;
    if (current_PRMODE == &PRMODE)
        current_PRMODE_id = 2;
    state->write((char*)&current_PRMODE_id, sizeof(current_PRMODE_id));

    state->write((char*)&RGBAQ, sizeof(RGBAQ));
    state->write((char*)&UV, sizeof(UV));
    state->write((char*)&ST, sizeof(ST));
    state->write((char*)&TEXA, sizeof(TEXA));
    state->write((char*)&TEXCLUT, sizeof(TEXCLUT));
    state->write((char*)&DTHE, sizeof(DTHE));
    state->write((char*)&COLCLAMP, sizeof(COLCLAMP));
    state->write((char*)&FOG, sizeof(FOG));
    state->write((char*)&FOGCOL, sizeof(FOGCOL));
    state->write((char*)&PABE, sizeof(PABE));
    state->write((char*)&SCANMSK, sizeof(SCANMSK));

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
