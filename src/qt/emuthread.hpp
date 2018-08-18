#ifndef EMUTHREAD_HPP
#define EMUTHREAD_HPP

#include <chrono>

#include <QMutex>
#include <QThread>
#include <string>

#include "../core/emulator.hpp"
#include "../core/errors.hpp"

enum PAUSE_EVENT
{
    GAME_NOT_LOADED,
    FILE_DIALOG,
    FRAME_ADVANCE,
};

class EmuThread : public QThread
{
    Q_OBJECT
    private:
        bool abort;
        uint32_t pause_status;
        QMutex emu_mutex, load_mutex, pause_mutex;
        Emulator e;

        std::chrono::system_clock::time_point old_frametime;
        std::ifstream gsdump;
        std::atomic_bool gsdump_reading;
        void gsdump_run();
    public:
        EmuThread();

        void reset();

        void set_skip_BIOS_hack(SKIP_HACK skip);
        void load_BIOS(uint8_t* BIOS);
        void load_ELF(uint8_t* ELF, uint64_t ELF_size);
        void load_CDVD(const char* name);

        bool load_state(const char* name);
        bool save_state(const char* name);
        bool gsdump_read(const char* name);
        void gsdump_write_toggle();
        void gsdump_single_frame();
        bool frame_advance;
    protected:
        void run() override;
    signals:
        void completed_frame(uint32_t* buffer, int inner_w, int inner_h, int final_w, int final_h);
        void update_FPS(int FPS);
        void emu_error(QString err);
    public slots:
        void shutdown();
        void press_key(PAD_BUTTON button);
        void release_key(PAD_BUTTON button);
        void pause(PAUSE_EVENT event);
        void unpause(PAUSE_EVENT event);
};

#endif // EMUTHREAD_HPP
