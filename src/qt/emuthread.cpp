#include <cmath>
#include <chrono>
#include <fstream>

#include "emuthread.hpp"

EmuThread::EmuThread()
    : abort(false),
    pause_status(0x0),
    gsdump_reading(false),
    frame_advance(false),
    gs_breakpoint(GSDump::Replayer::FRAME)
{

}

void EmuThread::reset()
{
    e.reset();
}

void EmuThread::set_skip_BIOS_hack(SKIP_HACK skip)
{
    QMutexLocker locker(&load_mutex);

    e.set_skip_BIOS_hack(skip);
}

void EmuThread::set_vu1_mode(VU_MODE mode)
{
    QMutexLocker locker(&load_mutex);

    e.set_vu1_mode(mode);
}

void EmuThread::load_BIOS(const uint8_t *BIOS)
{
    QMutexLocker locker(&load_mutex);

    e.load_BIOS(BIOS);
}

void EmuThread::load_ELF(const uint8_t *ELF, uint64_t ELF_size)
{
    QMutexLocker locker(&load_mutex);

    e.reset();
    e.load_ELF(ELF, ELF_size);
}

void EmuThread::load_CDVD(const char* name, CDVD_CONTAINER type)
{
    QMutexLocker locker(&load_mutex);

    e.reset();
    e.load_CDVD(name, type);
}

bool EmuThread::load_state(const char *name)
{
    QMutexLocker locker(&load_mutex);

    bool fail = false;
    if (!e.request_load_state(name))
        fail = true;

    return fail;
}

bool EmuThread::save_state(const char *name)
{
    QMutexLocker locker(&load_mutex);

    bool fail = false;
    if (!e.request_save_state(name))
        fail = true;

    return fail;
}

bool EmuThread::gsdump_read(const char *name)
{
    QMutexLocker locker(&load_mutex);

    gsdump.open(name);

    if (!gsdump.is_open())
        return 1;
    
    e.reset();
    e.get_gs().load_state(gsdump.stream());
    
    gsdump_reading = true;

    return 0;
}

void EmuThread::gsdump_end()
{
    gsdump_reading = false;
}

void EmuThread::gsdump_write_toggle()
{
    QMutexLocker locker(&load_mutex);

    e.request_gsdump_toggle();
}

void EmuThread::gsdump_single_frame()
{
    QMutexLocker locker(&load_mutex);

    e.request_gsdump_single_frame();
}

void EmuThread::gsdump_run()
{
    try
    {
        if (!gsdump.is_open())
            Errors::die("Couldn't load gsdump");

        do
        {
            gsdump.next();
            GSMessage data = gsdump.message();

            switch (data.type)
            {
            case set_xyz_t:
            case set_xyzf_t:
            {
                uint16_t w, h;

                e.get_gs().send_message(data);
                if (gsdump.draw())
                {
                    printf("[GSDUMP] partial frame\n");
                    emit completed_frame(
                        e.get_gs().render_partial_frame(w, h),
                        w, h, w, h
                    );
                }

                emit update_counters(
                    gsdump.current_frame(), gsdump.current_draw()
                );
                break;
            }
            case render_crt_t:
            {
                int w, h, new_w, new_h;
                e.get_gs().render_CRT();

                e.get_inner_resolution(w, h);
                e.get_resolution(new_w, new_h);

                printf("[GSDUMP] render CRT\n");
                emit completed_frame(
                    e.get_gs().get_framebuffer(),
                    w, h, new_w, new_h
                );
                emit update_counters(
                    gsdump.current_frame(), gsdump.current_draw()
                );
                break;
            }
            case load_state_t:
            case save_state_t:
                pause(PAUSE_EVENT::GAME_NOT_LOADED);
                Errors::die("[GSDUMP] Cannot save or load state in a GSDump\n");
            case gsdump_t:
                pause(PAUSE_EVENT::GAME_NOT_LOADED);
                Errors::die("[GSDUMP] End of dump");
            default:
                e.get_gs().send_message(data);
            }

        } while (!gsdump.breakpoint_hit());

        pause(PAUSE_EVENT::FRAME_ADVANCE);
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
                using namespace std;
                do
                {
                    chrono::system_clock::time_point now = chrono::system_clock::now();
                    chrono::duration<double> elapsed_seconds = now - old_frametime;
                    FPS = 1 / elapsed_seconds.count();
                } while (FPS > 60.0);
                old_frametime = chrono::system_clock::now();
                emit update_FPS((int)round(FPS));
            }
            catch (non_fatal_error &e)
            {
                printf("non_fatal emulation error occurred\n%s\n", e.what());
                emit emu_non_fatal_error(QString(e.what()));
                pause(PAUSE_EVENT::MESSAGE_BOX);
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
    QMutexLocker locker(&pause_mutex);

    abort = true;
}

void EmuThread::press_key(PAD_BUTTON button)
{
    QMutexLocker locker(&pause_mutex);

    e.press_button(button);
}

void EmuThread::release_key(PAD_BUTTON button)
{
    QMutexLocker locker(&pause_mutex);

    e.release_button(button);
}

void EmuThread::update_joystick(JOYSTICK joystick, JOYSTICK_AXIS axis, uint8_t val)
{
    QMutexLocker locker(&pause_mutex);

    e.update_joystick(joystick, axis, val);
}

void EmuThread::pause(PAUSE_EVENT event)
{
    pause_status |= 1 << event;
}

void EmuThread::unpause(PAUSE_EVENT event)
{
    pause_status &= ~(1 << event);
}

void EmuThread::set_gs_breakpoint(GSDump::Replayer::ADVANCE advance, int inc, bool is_goto)
{
    gsdump.set_breakpoint(advance, inc, is_goto);
}
