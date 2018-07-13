#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include "ee/intc.hpp"
#include "gs.hpp"
#include "gsthread.hpp"

using namespace std;

/**
This is a manager for the gsthread. It deals with communication
in/out of the GS, mostly with the GIF but also with the privilaged
registers.
**/

GraphicsSynthesizer::GraphicsSynthesizer(INTC* intc) : intc(intc)
{
    frame_complete = false;
    output_buffer1 = nullptr;
    output_buffer2 = nullptr;
    MessageQueue = nullptr;
    gsthread_id = std::thread();//no thread/default constructor
}

GraphicsSynthesizer::~GraphicsSynthesizer()
{
    if (output_buffer1)
        delete[] output_buffer1;
    if (output_buffer2)
        delete[] output_buffer2;
    if (MessageQueue)
        delete MessageQueue;
    if (gsthread_id.joinable())
    {
        GS_message_payload payload;
        payload.no_payload = {};
        MessageQueue->push({ GS_command::die_t,payload });
        gsthread_id.join();
    }
}

void GraphicsSynthesizer::reset()
{
    if (!output_buffer1)
        output_buffer1 = new uint32_t[1920 * 1280];
    if (!output_buffer2)
        output_buffer2 = new uint32_t[1920 * 1280];
    if (!MessageQueue)
        MessageQueue = new gs_fifo();
    current_lock = std::unique_lock<std::mutex>();
    using_first_buffer = true;
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
    DISPFB2.y = 0;
    DISPFB2.x = 0;
    IMR.signal = true;
    IMR.finish = true;
    IMR.hsync = true;
    IMR.vsync = true;
    IMR.rawt = true;
    set_CRT(false, 0x2, false);
    if (gsthread_id.joinable())
    {
        GS_message_payload payload;
        payload.no_payload = {};
        MessageQueue->push({ GS_command::die_t,payload });
        gsthread_id.join();
    }
    gsthread_id = std::thread(&GraphicsSynthesizerThread::event_loop, MessageQueue);//pass a reference to the fifo

    is_odd_frame = false;
    VBLANK_enabled = false;
    VBLANK_generated = false;
    FINISH_enabled = false;
    FINISH_generated = false;
    FINISH_requested = false;
}

void GraphicsSynthesizer::memdump()
{
    GS_message_payload payload;
    payload.no_payload = { };
    MessageQueue->push({ GS_command::memdump_t,payload });
}

void GraphicsSynthesizer::start_frame()
{
    frame_complete = false;
    current_lock = std::unique_lock<std::mutex>();
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

    GS_message_payload payload;
    payload.crt_payload = { interlaced, mode, frame_mode };
    MessageQueue->push({ GS_command::set_crt_t,payload });
}

uint32_t* GraphicsSynthesizer::get_framebuffer()
{
    uint32_t* out;
    if (using_first_buffer) {
        while (!output_buffer1_mutex.try_lock()) {
            printf("[GS] buffer 1 lock failed!");
            std::this_thread::yield();
        }
        current_lock = std::unique_lock<std::mutex>(output_buffer1_mutex, std::adopt_lock);
        out = output_buffer1;
    }
    else {
        while (!output_buffer2_mutex.try_lock()) {
            printf("[GS] buffer 2 lock failed!");
            std::this_thread::yield();
        }
        current_lock = std::unique_lock<std::mutex>(output_buffer2_mutex, std::adopt_lock);
        out = output_buffer2;
    }
    using_first_buffer = !using_first_buffer;
    return out;
}

void GraphicsSynthesizer::set_VBLANK(bool is_VBLANK)
{
    if (is_VBLANK)
    {
        printf("[GS] VBLANK start\n");
        if (!IMR.vsync)
            intc->assert_IRQ((int)Interrupt::GS);
        intc->assert_IRQ((int)Interrupt::VBLANK_START);
    }
    else
    {
        printf("[GS] VBLANK end\n");
        intc->assert_IRQ((int)Interrupt::VBLANK_END);
        frame_count++;
        is_odd_frame = !is_odd_frame;
    }
    VBLANK_generated = is_VBLANK;
}

void GraphicsSynthesizer::assert_FINISH()
{
    if (FINISH_requested && FINISH_enabled)
    {
        FINISH_generated = true;
        FINISH_enabled = false;
        FINISH_requested = false;
        if (!IMR.finish)
            intc->assert_IRQ((int)Interrupt::GS);
    }
}

void GraphicsSynthesizer::render_CRT()
{
    GS_message_payload payload;
    if (using_first_buffer)
        payload.render_payload = { output_buffer1, &output_buffer1_mutex };
    else
        payload.render_payload = { output_buffer2, &output_buffer2_mutex }; ;
    MessageQueue->push({ GS_command::render_crt_t,payload });
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

void GraphicsSynthesizer::write64(uint32_t addr, uint64_t value) {
    GS_message_payload payload;
    payload.write64_payload = { addr, value };
    MessageQueue->push({ GS_command::write64_t,payload });
    //also check for interrupt pre-processing

    addr &= 0xFFFF;
    switch (addr)
    {
    case 0x0061:
        printf("[GS] FINISH Write\n");
        FINISH_requested = true;
        break;
    } //Should probably have SINGAL and LABEL?
}


void GraphicsSynthesizer::write64_privileged(uint32_t addr, uint64_t value) {
    GS_message_payload payload;
    payload.write64_payload = { addr, value };
    MessageQueue->push({ GS_command::write64_privileged_t,payload });
    //also update local copies of privileged registers
    addr &= 0xFFFF;
    switch (addr)
    {
    case 0x0000:
        printf("[GS] Write PMODE: $%08X_%08X\n", value >> 32, value);
        PMODE.circuit1 = value & 0x1;
        PMODE.circuit2 = value & 0x2;
        PMODE.output_switching = (value >> 2) & 0x7;
        PMODE.use_ALP = value & (1 << 5);
        PMODE.out1_circuit2 = value & (1 << 6);
        PMODE.blend_with_bg = value & (1 << 7);
        PMODE.ALP = (value >> 8) & 0xFF;
        break;
    case 0x0020:
        printf("[GS] Write SMODE2: $%08X_%08X\n", value >> 32, value);
        SMODE2.interlaced = value & 0x1;
        SMODE2.frame_mode = value & 0x2;
        SMODE2.power_mode = (value >> 2) & 0x3;
        break;
    case 0x0070:
        printf("[GS] Write DISPFB1: $%08X_%08X\n", value >> 32, value);
        DISPFB1.frame_base = (value & 0x3FF) * 2048;
        DISPFB1.width = ((value >> 9) & 0x3F) * 64;
        DISPFB1.format = (value >> 14) & 0x1F;
        DISPFB1.x = (value >> 32) & 0x7FF;
        DISPFB1.y = (value >> 43) & 0x7FF;
        break;
    case 0x0080:
        printf("[GS] Write DISPLAY1: $%08X_%08X\n", value >> 32, value);
        DISPLAY1.x = value & 0xFFF;
        DISPLAY1.y = (value >> 12) & 0x7FF;
        DISPLAY1.magnify_x = ((value >> 23) & 0xF) + 1;
        DISPLAY1.magnify_y = ((value >> 27) & 0x3) + 1;
        DISPLAY1.width = ((value >> 32) & 0xFFF) + 1;
        DISPLAY1.height = ((value >> 44) & 0x7FF) + 1;
        break;
    case 0x0090:
        printf("[GS] Write DISPFB2: $%08X_%08X\n", value >> 32, value);
        DISPFB2.frame_base = (value & 0x3FF) * 2048;
        DISPFB2.width = ((value >> 9) & 0x3F) * 64;
        DISPFB2.format = (value >> 14) & 0x1F;
        DISPFB2.x = (value >> 32) & 0x7FF;
        DISPFB2.y = (value >> 43) & 0x7FF;
        break;
    case 0x00A0:
        printf("[GS] Write DISPLAY2: $%08X_%08X\n", value >> 32, value);
        DISPLAY2.x = value & 0xFFF;
        DISPLAY2.y = (value >> 12) & 0x7FF;
        DISPLAY2.magnify_x = ((value >> 23) & 0xF) + 1;
        DISPLAY2.magnify_y = ((value >> 27) & 0x3) + 1;
        DISPLAY2.width = ((value >> 32) & 0xFFF) + 1;
        DISPLAY2.height = ((value >> 44) & 0x7FF) + 1;
        printf("MAGH: %d\n", DISPLAY2.magnify_x);
        printf("MAGV: %d\n", DISPLAY2.magnify_y);
        break;
    case 0x1000:
        if (value & 0x2)
        {
            FINISH_enabled = true;
            FINISH_generated = false;
        }
        if (value & 0x8)
        {
            VBLANK_enabled = true;
            VBLANK_generated = false;
        }
        break;
    case 0x1010:
        printf("[GS] Write64 GS_IMR: $%08X_%08X\n", value >> 32, value);
        IMR.signal = value & (1 << 8);
        IMR.finish = value & (1 << 9);
        IMR.hsync = value & (1 << 10);
        IMR.vsync = value & (1 << 11);
        IMR.rawt = value & (1 << 12);
        break;
    case 0x1040:
        printf("[GS] Write64 to GS_BUSDIR: $%08X_%08X\n", value >> 32, value);
        BUSDIR = (uint8_t)value;
        break;
    default:
        printf("[GS] Unrecognized privileged write64 to reg $%04X: $%08X_%08X\n", addr, value >> 32, value);
    }
}
void GraphicsSynthesizer::write32_privileged(uint32_t addr, uint32_t value) {
    GS_message_payload payload;
    payload.write32_payload = { addr, value };
    MessageQueue->push({ GS_command::write32_privileged_t,payload });
    //also update local copies of privileged registers
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
        if (value & 0x2)
        {
            FINISH_enabled = true;
            FINISH_generated = false;
        }
        if (value & 0x8)
        {
            VBLANK_enabled = true;
            VBLANK_generated = false;
        }
        break;
    case 0x1010:
        printf("[GS] Write32 GS_IMR: $%08X\n", value);
        IMR.signal = value & (1 << 8);
        IMR.finish = value & (1 << 9);
        IMR.hsync = value & (1 << 10);
        IMR.vsync = value & (1 << 11);
        IMR.rawt = value & (1 << 12);
        break;
    default:
        printf("\n[GS] Unrecognized privileged write32 to reg $%04X: $%08X", addr, value);
    }
}

//reads from local copies only
uint32_t GraphicsSynthesizer::read32_privileged(uint32_t addr)
{
    addr &= 0xFFFF;
    switch (addr)
    {
    case 0x1000:
    {
        uint32_t reg = 0;
        reg |= FINISH_generated << 1;
        reg |= VBLANK_generated << 3;
        reg |= is_odd_frame << 13;
        printf("[GS] read32_privileged!: CSR = %04X\n", reg);
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
        reg |= FINISH_generated << 1;
        reg |= VBLANK_generated << 3;
        reg |= is_odd_frame << 13;
        printf("[GS] read64_privileged!: CSR = %08X\n", reg);
        return reg;
    }
    default:
        printf("[GS] Unrecognized privileged read64 from $%04X\n", addr);
        return 0;
    }
}

void GraphicsSynthesizer::set_RGBA(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    GS_message_payload payload;
    payload.rgba_payload = { r, g, b, a };
    MessageQueue->push({ GS_command::set_rgba_t,payload });
}
void GraphicsSynthesizer::set_STQ(uint32_t s, uint32_t t, uint32_t q) {
    GS_message_payload payload;
    payload.stq_payload = { s, t, q };
    MessageQueue->push({ GS_command::set_stq_t,payload });
}
void GraphicsSynthesizer::set_UV(uint16_t u, uint16_t v) {
    GS_message_payload payload;
    payload.uv_payload = { u, v };
    MessageQueue->push({ GS_command::set_uv_t,payload });
}
void GraphicsSynthesizer::set_Q(float q) {
    GS_message_payload payload;
    payload.q_payload = { 1 };
    MessageQueue->push({ GS_command::set_q_t,payload });
}
void GraphicsSynthesizer::set_XYZ(uint32_t x, uint32_t y, uint32_t z, bool drawing_kick) {
    GS_message_payload payload;
    payload.xyz_payload = { x, y, z, drawing_kick };
    MessageQueue->push({ GS_command::set_xyz_t,payload });
}