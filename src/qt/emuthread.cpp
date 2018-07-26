#include <cmath>
#include <chrono>
#include <fstream>

#include "emuthread.hpp"

using namespace std;

EmuThread::EmuThread()
{
    abort = false;
    pause_status = 0x0;
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

void EmuThread::run()
{
    forever
    {
        emu_mutex.lock();
        if (abort)
        {
            emu_mutex.unlock();
            return;
        }
        else if (pause_status)
            usleep(10000);
        else
        {
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
                emit emu_error(std::string(e.what()));
            }
        }
        emu_mutex.unlock();
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
