#ifndef EMUWINDOW_HPP
#define EMUWINDOW_HPP

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
        int init(const char* bios_name, const char* ELF_name);
        bool running();
        void emulate();

        void paintEvent(QPaintEvent *event);
        void closeEvent(QCloseEvent *event);
        double getFrameRate();
        void updateWindowTitle();
        void limitFrameRate();
        void resetFrameTime();
    signals:

    public slots:
};

#endif // EMUWINDOW_HPP
