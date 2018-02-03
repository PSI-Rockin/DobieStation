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
    public:
        explicit EmuWindow(QWidget *parent = nullptr);
        int init(const char* bios_name, const char* ELF_name);
        bool running();
        void emulate();

        void paintEvent(QPaintEvent *event);
        void closeEvent(QCloseEvent *event);
    signals:

    public slots:
};

#endif // EMUWINDOW_HPP
