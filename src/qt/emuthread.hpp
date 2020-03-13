#ifndef EMUTHREAD_HPP
#define EMUTHREAD_HPP

#include <chrono>

#include <QMutex>
#include <QThread>
#include <string>

#include "../core/emulator.hpp"
#include "../core/errors.hpp"

#define GSDUMP_BUFFERED_MESSAGES 100000

enum PAUSE_EVENT
{
    GAME_NOT_LOADED,
    FILE_DIALOG,
    MESSAGE_BOX,
    FRAME_ADVANCE,
    USER_REQUESTED
};

class EmuThread : public QThread
{
    Q_OBJECT
    private:
        std::atomic_bool abort;
        std::atomic<uint32_t> pause_status;
        QMutex emu_mutex;
        Emulator e;

        std::chrono::system_clock::time_point old_frametime;
        std::ifstream gsdump;
        std::atomic_bool gsdump_reading;
        std::atomic_bool block_run_loop;
        GSMessage* gsdump_read_buffer;
        int buffered_gs_messages;
        int current_gs_message;

        void gsdump_run();
        template <typename Func> void wait_for_lock(Func f);
    public:
        EmuThread();
        ~EmuThread();
        void reset();

        void set_skip_BIOS_hack(SKIP_HACK skip);
        void set_ee_mode(CPU_MODE mode);
        void set_vu1_mode(CPU_MODE mode);
        void load_BIOS(const uint8_t* BIOS);
        void load_ELF(const uint8_t* ELF, uint64_t ELF_size);
        void load_CDVD(const char* name, CDVD_CONTAINER type);

        bool load_state(const char* name);
        bool save_state(const char* name);
        bool gsdump_read(const char* name);
        void gsdump_write_toggle();
        void gsdump_single_frame();
        GSMessage& get_next_gsdump_message();
        bool gsdump_eof();
        std::atomic_bool frame_advance;
    protected:
        void run() override;
    signals:
        void completed_frame(uint32_t* buffer, int inner_w, int inner_h, int final_w, int final_h);
        void update_FPS(double FPS);
        void emu_error(QString err);
        void emu_non_fatal_error(QString err);
    public slots:
        void shutdown();
        void press_key(PAD_BUTTON button);
        void release_key(PAD_BUTTON button);
        void update_joystick(JOYSTICK joystick, JOYSTICK_AXIS axis, uint8_t val);
        void pause(PAUSE_EVENT event);
        void unpause(PAUSE_EVENT event);
};

#endif // EMUTHREAD_HPP
