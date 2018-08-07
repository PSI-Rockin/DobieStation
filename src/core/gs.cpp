#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include "ee/intc.hpp"
#include "gs.hpp"
#include "gsthread.hpp"
#include "errors.hpp"
using namespace std;

/**
This is a manager for the gsthread. It deals with communication
in/out of the GS, mostly with the GIF but also with the privileged
registers.
**/

GraphicsSynthesizer::GraphicsSynthesizer(INTC* intc) : intc(intc)
{
    frame_complete = false;
    output_buffer1 = nullptr;
    output_buffer2 = nullptr;
    message_queue = nullptr;
    return_queue = nullptr;
    gsthread_id = std::thread();//no thread/default constructor
}

GraphicsSynthesizer::~GraphicsSynthesizer()
{
    if (gsthread_id.joinable())
    {
        GS_message_payload payload;
        payload.no_payload = {0};
        send_message({ GS_command::die_t,payload });
        gsthread_id.join();
    }
    if (output_buffer1)
        delete[] output_buffer1;
    if (output_buffer2)
        delete[] output_buffer2;
    if (message_queue)
        delete message_queue;
    if (return_queue)
        delete return_queue;
}

void GraphicsSynthesizer::reset()
{
    if (!output_buffer1)
        output_buffer1 = new uint32_t[1920 * 1280];
    if (!output_buffer2)
        output_buffer2 = new uint32_t[1920 * 1280];
    if (!message_queue)
        message_queue = new gs_fifo();
    if (!return_queue)
        return_queue = new gs_return_fifo();
    current_lock = std::unique_lock<std::mutex>();
    using_first_buffer = true;
    frame_count = 0;
    set_CRT(false, 0x2, false);
    reg.reset();
    if (gsthread_id.joinable())
    {
        GS_message_payload payload;
        payload.no_payload = {0};
        send_message({ GS_command::die_t,payload });
        gsthread_id.join();
    }
    {
        GS_return_message data;
        while (return_queue->pop(data));

        GS_message data2;
        while (message_queue->pop(data2));
    }
    gsthread_id = std::thread(&GraphicsSynthesizerThread::event_loop, message_queue, return_queue);//pass references to the fifos

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
    reg.set_CRT(interlaced, mode, frame_mode);

    GS_message_payload payload;
    payload.crt_payload = { interlaced, mode, frame_mode };
    send_message({ GS_command::set_crt_t,payload });
}

void wait_for_return(gs_return_fifo *return_queue, GS_return type, GS_return_message &data)
{
    printf("wait for return\n");
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
                Errors::die("[GS] return message expected %d but was %d!\n", type, data.type);
        }
        else
            std::this_thread::yield();
    }
}

uint32_t* GraphicsSynthesizer::get_framebuffer()
{
    uint32_t* out;
    GS_return_message data;
    wait_for_return(return_queue, GS_return::render_complete_t, data);
    if (using_first_buffer)
    {
        while (!output_buffer1_mutex.try_lock())
        {
            printf("[GS] buffer 1 lock failed!\n");
            std::this_thread::yield();
        }
        current_lock = std::unique_lock<std::mutex>(output_buffer1_mutex, std::adopt_lock);
        out = output_buffer1;
    }
    else
    {
        while (!output_buffer2_mutex.try_lock())
        {
            printf("[GS] buffer 2 lock failed!\n");
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
    GS_message_payload payload;
    payload.vblank_payload = { is_VBLANK };
    send_message({ GS_command::set_vblank_t,payload });

    reg.set_VBLANK(is_VBLANK);

    if (is_VBLANK)
    {
        printf("[GS] VBLANK start\n");
        if (!reg.IMR.vsync)
            intc->assert_IRQ((int)Interrupt::GS);
        intc->assert_IRQ((int)Interrupt::VBLANK_START);
    }
    else
    {
        printf("[GS] VBLANK end\n");
        intc->assert_IRQ((int)Interrupt::VBLANK_END);
        frame_count++;
    }
}

void GraphicsSynthesizer::assert_VSYNC()
{
    GS_message_payload payload;
    payload.no_payload = {};
    message_queue->push({ GS_command::assert_vsync_t,payload });

    if (reg.assert_VSYNC())
        intc->assert_IRQ((int)Interrupt::GS);
}

void GraphicsSynthesizer::assert_FINISH()
{
    GS_message_payload payload;
    payload.no_payload = { };
    send_message({ GS_command::assert_finish_t,payload });

    if (reg.assert_FINISH())
        intc->assert_IRQ((int)Interrupt::GS);
}

void GraphicsSynthesizer::render_CRT()
{
    GS_message_payload payload;
    if (using_first_buffer)
        payload.render_payload = { output_buffer1, &output_buffer1_mutex };
    else
        payload.render_payload = { output_buffer2, &output_buffer2_mutex }; ;
    send_message({ GS_command::render_crt_t,payload });
}

uint32_t* GraphicsSynthesizer::render_partial_frame(uint16_t& width, uint16_t& height)
{
    GS_message_payload payload;
    if (using_first_buffer)
        payload.render_payload = { output_buffer1, &output_buffer1_mutex };
    else
        payload.render_payload = { output_buffer2, &output_buffer2_mutex }; ;
    send_message({ GS_command::memdump_t,payload });

    GS_return_message data;
    wait_for_return(return_queue, GS_return::gsdump_render_partial_done_t, data);
    width = data.payload.xy_payload.x;
    height = data.payload.xy_payload.y;


    if (using_first_buffer)
    {
        using_first_buffer = false;
        current_lock = std::unique_lock<std::mutex>(output_buffer1_mutex);
        return output_buffer1;
    }
    else
    {
        using_first_buffer = true;
        current_lock = std::unique_lock<std::mutex>(output_buffer2_mutex);
        return output_buffer2;
    }
}

void GraphicsSynthesizer::get_resolution(int &w, int &h)
{
    reg.get_resolution(w, h);
}

void GraphicsSynthesizer::get_inner_resolution(int &w, int &h)
{
    reg.get_inner_resolution(w, h);
}

void GraphicsSynthesizer::write64(uint32_t addr, uint64_t value)
{
    GS_message_payload payload;
    payload.write64_payload = { addr, value };
    send_message({ GS_command::write64_t,payload });

    //also check for interrupt pre-processing
    reg.write64(addr, value);
}

void GraphicsSynthesizer::write64_privileged(uint32_t addr, uint64_t value)
{
    GS_message_payload payload;
    payload.write64_payload = { addr, value };
    send_message({ GS_command::write64_privileged_t,payload });

    reg.write64_privileged(addr, value);
}

void GraphicsSynthesizer::write32_privileged(uint32_t addr, uint32_t value)
{
    GS_message_payload payload;
    payload.write32_payload = { addr, value };
    send_message({ GS_command::write32_privileged_t,payload });

    reg.write32_privileged(addr, value);
}

uint32_t GraphicsSynthesizer::read32_privileged(uint32_t addr)
{
    return reg.read32_privileged(addr);
}

uint64_t GraphicsSynthesizer::read64_privileged(uint32_t addr)
{
    return reg.read64_privileged(addr);
}

void GraphicsSynthesizer::set_RGBA(uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    GS_message_payload payload;
    payload.rgba_payload = { r, g, b, a };
    send_message({ GS_command::set_rgba_t,payload });
}

void GraphicsSynthesizer::set_STQ(uint32_t s, uint32_t t, uint32_t q)
{
    GS_message_payload payload;
    payload.stq_payload = { s, t, q };
    send_message({ GS_command::set_stq_t,payload });
}

void GraphicsSynthesizer::set_UV(uint16_t u, uint16_t v)
{
    GS_message_payload payload;
    payload.uv_payload = { u, v };
    send_message({ GS_command::set_uv_t,payload });
}

void GraphicsSynthesizer::set_Q(float q)
{
    GS_message_payload payload;
    payload.q_payload = { q };
    send_message({ GS_command::set_q_t,payload });
}

void GraphicsSynthesizer::set_XYZ(uint32_t x, uint32_t y, uint32_t z, bool drawing_kick)
{
    GS_message_payload payload;
    payload.xyz_payload = { x, y, z, drawing_kick };
    send_message({ GS_command::set_xyz_t,payload });
}

void GraphicsSynthesizer::load_state(ifstream &state)
{
    GS_message_payload payload;
    payload.loadstate_payload = {&state};
    send_message({ GS_command::loadstate_t, payload});
    GS_return_message data;
    wait_for_return(return_queue, GS_return::loadstate_done_t, data);
    state.read((char*)&reg, sizeof(reg));
}

void GraphicsSynthesizer::save_state(ofstream &state)
{
    GS_message_payload payload;
    payload.savestate_payload = {&state};
    send_message({ GS_command::savestate_t,payload });
    GS_return_message data;
    wait_for_return(return_queue, GS_return::savestate_done_t, data);
    state.write((char*)&reg, sizeof(reg));
}
void GraphicsSynthesizer::send_message(GS_message message)
{
    message_queue->push(message);
}
void GraphicsSynthesizer::send_dump_request()
{
    GS_message_payload p;
    p.no_payload = { 0 };
    send_message({ gsdump_t, p });
}