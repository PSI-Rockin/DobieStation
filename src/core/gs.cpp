#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include "ee/intc.hpp"
#include "gsthread.hpp"

#include "gs.hpp"

using namespace std;

/**
This is a manager for the gsthread. It deals with communication
in/out of the GS, mostly with the GIF but also with the privilaged
registers.
**/

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
        output_buffer = new uint32_t[1920 * 1280];
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
    PSMCT24_color = 0;
    PSMCT24_unpacked_count = 0;
    set_CRT(false, 0x2, false);
}

void GraphicsSynthesizer::memdump()
{
    ofstream file("memdump.bin", std::ios::binary);
    file.write((char*)local_mem, 1024 * 1024 * 4);
    file.close();
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

}

void GraphicsSynthesizer::assert_FINISH()
{

}

void GraphicsSynthesizer::render_CRT()
{

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

void GraphicsSynthesizer::write64(uint32_t addr, uint64_t value) {
    GS_message_payload payload;
    payload.write64_payload = { addr, value };
    MessageQueue.push({ GS_command::write64,payload });
}


void GraphicsSynthesizer::write64_privileged(uint32_t addr, uint64_t value) {
    GS_message_payload payload;
    payload.write64_payload = { addr, value };
    MessageQueue.push({ GS_command::write64_privileged,payload });
    //also update local copies of privileged registers
}
void GraphicsSynthesizer::write32_privileged(uint32_t addr, uint32_t value) {
    GS_message_payload payload;
    payload.write32_payload = { addr, value };
    MessageQueue.push({ GS_command::write32_privileged,payload });
    //also update local copies of privileged registers
}

//reads from local copies only
uint32_t GraphicsSynthesizer::read32_privileged(uint32_t addr)
{
    printf("[GS] read32_privileged!\n");
    addr &= 0xFFFF;
    switch (addr)
    {
    case 0x1000:
    {
        uint32_t reg = 0;
        /*reg |= FINISH_generated << 1;
        reg |= VBLANK_generated << 3;
        reg |= is_odd_frame << 13;*/
        return reg;
    }
    default:
        printf("[GS] Unrecognized privileged read32 from $%04X\n", addr);
        return 0;
    }
}

uint64_t GraphicsSynthesizer::read64_privileged(uint32_t addr)
{
    printf("[GS] read64_privileged!\n");
    addr &= 0xFFFF;
    switch (addr)
    {
    case 0x1000:
    {
        uint64_t reg = 0;
        /*reg |= FINISH_generated << 1;
        reg |= VBLANK_generated << 3;
        reg |= is_odd_frame << 13;*/
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
    MessageQueue.push({ GS_command::set_rgba,payload });
}
void GraphicsSynthesizer::set_STQ(uint32_t s, uint32_t t, uint32_t q) {
    GS_message_payload payload;
    payload.stq_payload = { s, t, q };
    MessageQueue.push({ GS_command::set_stq,payload });
}
void GraphicsSynthesizer::set_UV(uint16_t u, uint16_t v) {
    GS_message_payload payload;
    payload.uv_payload = { u, v };
    MessageQueue.push({ GS_command::set_uv,payload });
}
void GraphicsSynthesizer::set_Q(float q) {
    GS_message_payload payload;
    payload.q_payload = { 1 };
    MessageQueue.push({ GS_command::set_q,payload });
}
void GraphicsSynthesizer::set_XYZ(uint32_t x, uint32_t y, uint32_t z, bool drawing_kick) {
    GS_message_payload payload;
    payload.xyz_payload = { x, y, z, drawing_kick };
    MessageQueue.push({ GS_command::set_xyz,payload });
}