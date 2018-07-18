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
        message_queue->push({ GS_command::die_t,payload });
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
        message_queue->push({ GS_command::die_t,payload });
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

void GraphicsSynthesizer::memdump()
{
    GS_message_payload payload;
    payload.no_payload = { };
    message_queue->push({ GS_command::memdump_t,payload });
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
    message_queue->push({ GS_command::set_crt_t,payload });
}

void wait_for_return(gs_return_fifo *return_queue)
{
    GS_return_message data;
    while (true)
    {
        if (return_queue->pop(data))
        {
            switch (data.type)
            {
                case render_complete_t:
                    return;
                default:
                    printf("[GS] Unhandled return message!\n");
                    exit(1);
            }
        }
        else
        {
            //printf("[GS] GS thread has not finished rendering!\n");
            std::this_thread::yield();
        }
    }

}
uint32_t* GraphicsSynthesizer::get_framebuffer()
{
    uint32_t* out;
    wait_for_return(return_queue);
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
    message_queue->push({ GS_command::set_vblank_t,payload });

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

void GraphicsSynthesizer::assert_FINISH()
{
    GS_message_payload payload;
    payload.no_payload = { };
    message_queue->push({ GS_command::assert_finish_t,payload });

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
    message_queue->push({ GS_command::render_crt_t,payload });
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
    message_queue->push({ GS_command::write64_t,payload });

    //also check for interrupt pre-processing
    reg.write64(addr, value);
}
void GraphicsSynthesizer::write64_privileged(uint32_t addr, uint64_t value)
{
    GS_message_payload payload;
    payload.write64_payload = { addr, value };
    message_queue->push({ GS_command::write64_privileged_t,payload });

    reg.write64_privileged(addr, value);
}
void GraphicsSynthesizer::write32_privileged(uint32_t addr, uint32_t value)
{
    GS_message_payload payload;
    payload.write32_payload = { addr, value };
    message_queue->push({ GS_command::write32_privileged_t,payload });

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
    message_queue->push({ GS_command::set_rgba_t,payload });
}
void GraphicsSynthesizer::set_STQ(uint32_t s, uint32_t t, uint32_t q)
{
    GS_message_payload payload;
    payload.stq_payload = { s, t, q };
    message_queue->push({ GS_command::set_stq_t,payload });
}
void GraphicsSynthesizer::set_UV(uint16_t u, uint16_t v)
{
    GS_message_payload payload;
    payload.uv_payload = { u, v };
    message_queue->push({ GS_command::set_uv_t,payload });
}
void GraphicsSynthesizer::set_Q(float q)
{
    GS_message_payload payload;
    payload.q_payload = { 1 };
    message_queue->push({ GS_command::set_q_t,payload });
}
void GraphicsSynthesizer::set_XYZ(uint32_t x, uint32_t y, uint32_t z, bool drawing_kick)
{
    GS_message_payload payload;
    payload.xyz_payload = { x, y, z, drawing_kick };
    message_queue->push({ GS_command::set_xyz_t,payload });
}
