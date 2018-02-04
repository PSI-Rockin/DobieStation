#include <fstream>
#include <iostream>
#include <tgmath.h>

#include <QPainter>
#include <QString>
#include "emuwindow.hpp"

using namespace std;

ifstream::pos_type filesize(const char* filename)
{
    ifstream in(filename, ifstream::ate | ifstream::binary);
    return in.tellg();
}

EmuWindow::EmuWindow(QWidget *parent) : QMainWindow(parent)
{
    old_frametime = chrono::system_clock::now();
    old_update_time = chrono::system_clock::now();
    framerate_avg = 0.0;
}

int EmuWindow::init(const char* bios_name, const char* file_name)
{
    //Initialize emulator
    e.reset();
    ifstream BIOS_file(bios_name, ios::binary | ios::in);
    if (!BIOS_file.is_open())
    {
        printf("Failed to load PS2 BIOS from %s\n", bios_name);
        return 1;
    }
    printf("Loaded PS2 BIOS.\n");
    uint8_t* BIOS = new uint8_t[1024 * 1024 * 4];
    BIOS_file.read((char*)BIOS, 1024 * 1024 * 4);
    BIOS_file.close();
    e.load_BIOS(BIOS);
    delete[] BIOS;
    BIOS = nullptr;

    ifstream ELF_file(file_name, ios::binary | ios::in);
    if (!ELF_file.is_open())
    {
        printf("Failed to load %s\n", file_name);
        return 1;
    }
    long long ELF_size = filesize(file_name);
    uint8_t* ELF = new uint8_t[ELF_size];
    ELF_file.read((char*)ELF, ELF_size);
    ELF_file.close();

    printf("Loaded %s\n", file_name);
    printf("Size: %lld\n", ELF_size);
    e.load_ELF(ELF);
    delete[] ELF;
    ELF = nullptr;

    //Initialize window
    is_running = true;
    setWindowTitle("DobieStation");
    resize(640, 448);
    show();
    return 0;
}

bool EmuWindow::running()
{
    return is_running;
}

void EmuWindow::emulate()
{
    e.run();
    update();
}

void EmuWindow::paintEvent(QPaintEvent *event)
{
    QPainter painter(this);
    uint32_t* buffer = e.get_framebuffer();
    painter.fillRect(rect(), Qt::black);
    if (!buffer)
        return;

    QImage image((uint8_t*)buffer, 640, 256, QImage::Format_RGBA8888);

    int new_w, new_h;
    e.get_resolution(new_w, new_h);
    image = image.scaled(new_w, new_h);
    resize(new_w, new_h);

    painter.drawPixmap(0, 0, QPixmap::fromImage(image));
}

void EmuWindow::closeEvent(QCloseEvent *event)
{
    event->accept();
    is_running = false;
}

double EmuWindow::getFrameRate()
{
    //Returns the framerate since old_frametime was set to the current time
    chrono::system_clock::time_point now = chrono::system_clock::now();
    chrono::duration<double> elapsed_seconds = now - old_frametime;
    return (1 / elapsed_seconds.count());
}

void EmuWindow::updateWindowTitle()
{
    /*
    Updates window title every second
    Framerate displayed is the average framerate over 1 second
    */
    chrono::system_clock::time_point now = chrono::system_clock::now();
    chrono::duration<double> elapsed_update_seconds = now - old_update_time;

    double framerate = getFrameRate();
    framerate_avg = (framerate_avg + framerate) / 2;
    if (elapsed_update_seconds.count() >= 1.0)
    {
        int rf = (int) round(framerate_avg); // rounded framerate
        string new_title = "DobieStation - FPS: " + to_string(rf);
        setWindowTitle(QString::fromStdString(new_title));
        old_update_time = chrono::system_clock::now();
        framerate_avg = framerate;
    }
}

void EmuWindow::limitFrameRate()
{
    while (getFrameRate() > 60.0)
    {
    }
}

void EmuWindow::resetFrameTime()
{
    old_frametime = chrono::system_clock::now();
}
