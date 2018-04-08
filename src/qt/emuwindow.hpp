#ifndef EMUWINDOW_HPP
#define EMUWINDOW_HPP

#include <chrono>

#include <QCloseEvent>
#include <QMainWindow>
#include <QPaintEvent>
#include "../core/emulator.hpp"

class EmuWindow : public QMainWindow
{
    Q_OBJECT
    private:
        Emulator e;
        bool is_running;
        std::chrono::system_clock::time_point old_frametime;
        std::chrono::system_clock::time_point old_update_time;
        double framerate_avg;
    public:
        explicit EmuWindow(QWidget *parent = nullptr);
        int init(int argc, char** argv);
        bool running();
        void emulate();

        void paintEvent(QPaintEvent *event);
        void closeEvent(QCloseEvent *event);

        double get_frame_rate();
        void update_window_title();
        void limit_frame_rate();
        void reset_frame_time();
    signals:

    public slots:
};

#endif // EMUWINDOW_HPP
