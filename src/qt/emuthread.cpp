#include <cmath>
#include <chrono>
#include <fstream>

#include "emuthread.hpp"

using namespace std;

EmuThread::EmuThread()
{
    abort = false;
    pause_status = 0x0;
    gsdump_reading = false;
    frame_advance = false;
}

void EmuThread::reset()
{
    e.reset();
}

void EmuThread::set_skip_BIOS_hack(SKIP_HACK skip)
{
    load_mutex.lock();
    e.set_skip_BIOS_hack(skip);
    load_mutex.unlock();
}

void EmuThread::load_BIOS(uint8_t *BIOS)
{
    load_mutex.lock();
    e.load_BIOS(BIOS);
    load_mutex.unlock();
}

void EmuThread::load_ELF(uint8_t *ELF, uint64_t ELF_size)
{
    load_mutex.lock();
    e.reset();
    e.load_ELF(ELF, ELF_size);
    load_mutex.unlock();
}

void EmuThread::load_CDVD(const char* name)
{
    load_mutex.lock();
    e.reset();
    e.load_CDVD(name);
    load_mutex.unlock();
}

bool EmuThread::load_state(const char *name)
{
    load_mutex.lock();
    bool fail = false;
    if (!e.request_load_state(name))
        fail = true;
    load_mutex.unlock();
    return fail;
}

bool EmuThread::save_state(const char *name)
{
    load_mutex.lock();
    bool fail = false;
    if (!e.request_save_state(name))
        fail = true;
    load_mutex.unlock();
    return fail;
}

bool EmuThread::gsdump_read(const char *name)
{
    load_mutex.lock();
    gsdump.open(name,ios::binary);
    if (!gsdump.is_open())
        return 1;
    e.get_gs().reset();
    e.get_gs().load_state(gsdump);
    load_mutex.unlock();
    printf("loaded gsdump\n");
    gsdump_reading = true;
    return 0;
}

void EmuThread::gsdump_write_toggle()
{
    load_mutex.lock();
    e.request_gsdump_toggle();
    load_mutex.unlock();
}

void EmuThread::gsdump_single_frame()
{
    load_mutex.lock();
    e.request_gsdump_single_frame();
    load_mutex.unlock();
}

void EmuThread::gsdump_run()
{
    printf("gsdump frame\n");
    try
    {
        int draws_sent = 10;
        while (true)
        {
            GS_message data;
            gsdump.read((char*)&data, sizeof(data));
            switch (data.type)
            {
                case set_xyz_t:
                    e.get_gs().send_message(data);
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
                    return;
                case savestate_t:
                case loadstate_t:
                    Errors::die("savestate save/load during gsdump not supported!");
                default:
                    e.get_gs().send_message(data);
            }
            if (gsdump.eof())
                Errors::die("gs dump unexpectedly ended");
        }
    }
    catch (Emulation_error &e)
    {
        printf("Fatal emulation error occurred running gsdump, stopping execution\n%s\n", e.what());
        emit emu_error(QString(e.what()));
        pause(PAUSE_EVENT::GAME_NOT_LOADED);
    }
}
void EmuThread::run()
{
    forever
    {
        QMutexLocker locker(&emu_mutex);
        if (abort)
            return;
        else if (pause_status)
            usleep(10000);
        else if (gsdump_reading)
            gsdump_run();
        else
        {
            if (frame_advance)
                pause(PAUSE_EVENT::FRAME_ADVANCE);
            try
            {
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
                    FPS = 1 / elapsed_seconds.count();
                } while (FPS > 60.0);
                old_frametime = chrono::system_clock::now();
                emit update_FPS((int)round(FPS));
            }
            catch (Emulation_error &e)
            {
                printf("Fatal emulation error occurred, stopping execution\n%s\n", e.what());
                emit emu_error(QString(e.what()));
                pause(PAUSE_EVENT::GAME_NOT_LOADED);
            }
        }
    }
}

void EmuThread::shutdown()
{
    pause_mutex.lock();
    abort = true;
    pause_mutex.unlock();
}

void EmuThread::press_key(PAD_BUTTON button)
{
    pause_mutex.lock();
    e.press_button(button);
    pause_mutex.unlock();
}

void EmuThread::release_key(PAD_BUTTON button)
{
    pause_mutex.lock();
    e.release_button(button);
    pause_mutex.unlock();
}

void EmuThread::pause(PAUSE_EVENT event)
{
    pause_status |= 1 << event;
}

void EmuThread::unpause(PAUSE_EVENT event)
{
    pause_status &= ~(1 << event);
}
