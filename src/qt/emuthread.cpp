#include <cmath>
#include <chrono>
#include <fstream>

#include "emuthread.hpp"

using namespace std;

template <typename Func>
void EmuThread::wait_for_lock(Func f)
{
    block_run_loop = true;
    QMutexLocker locker(&emu_mutex);
    f();
    block_run_loop = false;
}

EmuThread::EmuThread()
{
    abort = false;
    pause_status = 0x0;
    gsdump_reading = false;
    frame_advance = false;
    block_run_loop = false;
    gsdump_read_buffer = new GSMessage[GSDUMP_BUFFERED_MESSAGES];
}

EmuThread::~EmuThread()
{
    delete[] gsdump_read_buffer;
}

void EmuThread::reset()
{
    e.reset();
    buffered_gs_messages = 0;
    current_gs_message = 0;
}

void EmuThread::set_skip_BIOS_hack(SKIP_HACK skip)
{
    wait_for_lock([=]() { e.set_skip_BIOS_hack(skip); } );
}

void EmuThread::set_ee_mode(CPU_MODE mode)
{
    wait_for_lock([=]() {  e.set_ee_mode(mode); } );
}

void EmuThread::set_vu1_mode(CPU_MODE mode)
{
    wait_for_lock([=]() { e.set_vu1_mode(mode); } );
}

void EmuThread::load_BIOS(const uint8_t *BIOS)
{
    wait_for_lock([=]() { e.load_BIOS(BIOS); } );
}

void EmuThread::load_ELF(const uint8_t *ELF, uint64_t ELF_size)
{
    wait_for_lock([=]() 
    {
        e.reset();
        e.load_ELF(ELF, ELF_size);
    } );
}

void EmuThread::load_CDVD(const char* name, CDVD_CONTAINER type)
{
    wait_for_lock([=]() 
    { 
        e.reset();
        e.load_CDVD(name, type);
    } );
}

bool EmuThread::load_state(const char *name)
{
    bool fail = false;
    wait_for_lock([=, &fail]() { if (!e.request_load_state(name)) fail = true; } );

    return fail;
}

bool EmuThread::save_state(const char *name)
{
    bool fail = false;

    wait_for_lock([=, &fail]() { if (!e.request_save_state(name)) fail = true; } );

    return fail;
}

bool EmuThread::gsdump_read(const char *name)
{
    wait_for_lock([=]() 
    { 
        gsdump.open(name,ios::binary);
        if (!gsdump.is_open())
            return 1;
        e.get_gs().reset();
        e.get_gs().load_state(gsdump);

        printf("loaded gsdump\n");
        gsdump_reading = true;
    });
    return 0;
}

void EmuThread::gsdump_write_toggle()
{
    wait_for_lock([=]() { e.request_gsdump_toggle(); } );
}

void EmuThread::gsdump_single_frame()
{
    wait_for_lock([=]() { e.request_gsdump_single_frame(); } );
}

GSMessage& EmuThread::get_next_gsdump_message()
{
    if(!buffered_gs_messages) {
        gsdump.read((char*)gsdump_read_buffer, sizeof(GSMessage) * GSDUMP_BUFFERED_MESSAGES);
        current_gs_message = 0;
        buffered_gs_messages = gsdump.gcount() / sizeof(GSMessage);
    }
    GSMessage& data = gsdump_read_buffer[current_gs_message];
    current_gs_message++;
    buffered_gs_messages--;
    return data;
}

bool EmuThread::gsdump_eof()
{
    return gsdump.eof() && !buffered_gs_messages;
}

void EmuThread::gsdump_run()
{
    printf("gsdump frame\n");
    try
    {
        int draws_sent = 10;
        while (true)
        {
            GSMessage& data = get_next_gsdump_message();

            switch (data.type)
            {
                case set_xyz_t:
                    e.get_gs().send_message(data);
                    e.get_gs().wake_gs_thread();
                    if (frame_advance && data.payload.xyz_payload.drawing_kick && --draws_sent <= 0)
                    {
                        uint16_t w, h;
                        uint32_t* frame = e.get_gs().render_partial_frame(w, h);
                        emit completed_frame(frame, w, h, w, h);
                        pause(PAUSE_EVENT::FRAME_ADVANCE);
                        return;
                    }
                    break;
                case render_crt_t:
                {
                    printf("gsdump frame render\n");
                    e.get_gs().render_CRT();
                    int w, h, new_w, new_h;
                    e.get_inner_resolution(w, h);
                    e.get_resolution(new_w, new_h);

                    emit completed_frame(e.get_gs().get_framebuffer(), w, h, new_w, new_h);
                    printf("gsdump frame render complete\n");
                    pause(PAUSE_EVENT::FRAME_ADVANCE);
                    return;
                }
                case gsdump_t:
                    pause(PAUSE_EVENT::GAME_NOT_LOADED);
                    if (gsdump.peek() != EOF)
                        Errors::die("gsdump ended before end of file!");
                    gsdump.close();
                    Errors::die("gsdump ended successfully\n");
                case save_state_t:
                case load_state_t:
                    Errors::die("save_state save/load during gsdump not supported!");
                default:
                    e.get_gs().send_message(data);
                    //e.get_gs().wake_gs_thread();
            }
            if (gsdump_eof())
                Errors::die("gs dump unexpectedly ended");
        }
    }
    catch (Emulation_error &e)
    {
        printf("Fatal emulation error occurred running gsdump, stopping execution\n%s\n", e.what());
        emit emu_error(QString(e.what()));
        pause(PAUSE_EVENT::GAME_NOT_LOADED);
    }
    catch (non_fatal_error &e)
    {
        printf("non_fatal emulation occurred running gsdump\n%s\n", e.what());
        emit emu_non_fatal_error(QString(e.what()));
        pause(PAUSE_EVENT::MESSAGE_BOX);
    }
}

void EmuThread::run()
{
    forever
    {
        if (abort)
            return;
        else if (pause_status || block_run_loop)
        {
            usleep(10000);
        }
        else if (gsdump_reading)
            gsdump_run();
        else
        {
            if (frame_advance)
                pause(PAUSE_EVENT::FRAME_ADVANCE);
            try
            {
                QMutexLocker locker(&emu_mutex);
                e.run();
                int w, h, new_w, new_h;
                e.get_inner_resolution(w, h);
                e.get_resolution(new_w, new_h);
                emit completed_frame(e.get_framebuffer(), w, h, new_w, new_h);

                //Update FPS
                double FPS;
                do
                {
                    chrono::system_clock::time_point now = chrono::system_clock::now();
                    chrono::duration<double> elapsed_seconds = now - old_frametime;
                    FPS = 1.0 / elapsed_seconds.count();
                } while (FPS > 60.0);
                old_frametime = chrono::system_clock::now();
                emit update_FPS(FPS);
            }
            catch (non_fatal_error &error)
            {
                printf("non_fatal emulation error occurred\n%s\n", error.what());
                emit emu_non_fatal_error(QString(error.what()));
                pause(PAUSE_EVENT::MESSAGE_BOX);
            }
            catch (Emulation_error &error)
            {
                e.print_state();
                printf("Fatal emulation error occurred, stopping execution\n%s\n", error.what());
                fflush(stdout);
                emit emu_error(QString(error.what()));
                pause(PAUSE_EVENT::GAME_NOT_LOADED);
            }
        }
    }
}

void EmuThread::shutdown()
{
    abort = true;
}

void EmuThread::press_key(PAD_BUTTON button)
{
    wait_for_lock([=]() { e.press_button(button); });
}

void EmuThread::release_key(PAD_BUTTON button)
{
    wait_for_lock([=]() { e.release_button(button); });
}

void EmuThread::update_joystick(JOYSTICK joystick, JOYSTICK_AXIS axis, uint8_t val)
{
    wait_for_lock([=]() { e.update_joystick(joystick, axis, val); });
}

void EmuThread::pause(PAUSE_EVENT event)
{
    pause_status |= 1 << event;
}

void EmuThread::unpause(PAUSE_EVENT event)
{
    pause_status &= ~(1 << event);
}
