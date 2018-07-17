#ifndef EMUWINDOW_HPP
#define EMUWINDOW_HPP

#include <chrono>
#include <QMainWindow>
#include <QPaintEvent>

#include "emuthread.hpp"

#include "../core/emulator.hpp"

class EmuWindow : public QMainWindow
{
    Q_OBJECT
    private:
        EmuThread emuthread;
        std::string title;
        QImage final_image;
        std::chrono::system_clock::time_point old_frametime;
        std::chrono::system_clock::time_point old_update_time;
        double framerate_avg;

        QMenu* file_menu;
        QMenu* options_menu;
        QAction* load_rom_action;
        QAction* load_bios_action;
        QAction* exit_action;
        QAction* logger_options_action;
        LoggerWindow* log_options_window;

    public:
        explicit EmuWindow(QWidget *parent = nullptr);
        int init(int argc, char** argv);
        int load_exec(const char* file_name, bool skip_BIOS);

        void create_menu();

        void paintEvent(QPaintEvent *event);
        void closeEvent(QCloseEvent *event);
        void keyPressEvent(QKeyEvent *event);
        void keyReleaseEvent(QKeyEvent *event);

    protected:
    #ifndef QT_NO_CONTEXTMENU
        void contextMenuEvent(QContextMenuEvent* event) override;
    #endif
    
    signals:
        void shutdown();
        void press_key(PAD_BUTTON button);
        void release_key(PAD_BUTTON button);
    public slots:
        void update_FPS(int FPS);
        void draw_frame(uint32_t* buffer, int inner_w, int inner_h, int final_w, int final_h);
        void open_file_no_skip();
        void open_file_skip();
};

#endif // EMUWINDOW_HPP
