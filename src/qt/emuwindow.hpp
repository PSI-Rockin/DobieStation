#ifndef EMUWINDOW_HPP
#define EMUWINDOW_HPP

#include <chrono>
#include <QMainWindow>
#include <QStackedWidget>

#include "emuthread.hpp"

#include "../qt/settings.hpp"
#include "../core/emulator.hpp"

class SettingsWindow;
class RenderWidget;

class EmuWindow : public QMainWindow
{
    Q_OBJECT
    private:
        EmuThread emu_thread;
        QString ee_mode;
        QString vu1_mode;
        std::chrono::system_clock::time_point old_frametime;
        double framerate_avg, frametime_avg;
        double frametime_list[60];
        int frametime_list_index = 0;

        QFileInfo current_ROM;
        QMenu* file_menu;
        QMenu* options_menu;
        QMenu* emulation_menu;
        QMenu* window_menu;
        QAction* load_rom_action;
        QAction* load_bios_action;
        QAction* load_state_action;
        QAction* save_state_action;
        QAction* exit_action;
        QStackedWidget* stack_widget;
        RenderWidget* render_widget;

        SettingsWindow* settings_window = nullptr;

        void set_vu1_mode();
        void set_ee_mode();
        void show_render_view();
        void show_default_view();
    public:
        explicit EmuWindow(QWidget *parent = nullptr);
        ~EmuWindow() final;

        int init(int argc, char** argv);
        int load_exec(const char* file_name, bool skip_BIOS);

        void create_menu();

        bool load_bios();
        void open_settings_window();
        void closeEvent(QCloseEvent *event) override;
        void keyPressEvent(QKeyEvent *event) override;
        void keyReleaseEvent(QKeyEvent *event) override;

    protected:
    #ifndef QT_NO_CONTEXTMENU
        void contextMenuEvent(QContextMenuEvent* event) override;
    #endif
    
    signals:
        void shutdown();
        void press_key(PAD_BUTTON button);
        void release_key(PAD_BUTTON button);
        void update_joystick(JOYSTICK joystick, JOYSTICK_AXIS axis, uint8_t val);
    public slots:
        void update_FPS(double FPS);
        void open_file_no_skip();
        void open_file_skip();
        void load_state();
        void save_state();
        void bios_error(QString err);
        void emu_error(QString err);
        void emu_non_fatal_error(QString err);
};

#endif // EMUWINDOW_HPP
