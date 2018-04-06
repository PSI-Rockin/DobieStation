#include <fstream>
#include <iostream>
#include <cmath>

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

int EmuWindow::init(int argc, char** argv)
{
    if (argc < 3)
    {
        printf("Args: [BIOS] [ELF/ISO]\n");
        return 1;
    }

    //Initialize emulator
    e.reset();
    char* bios_name = argv[1];
    char* file_name = argv[2];

    bool skip_BIOS = false;
    //Flag parsing - to be reworked
    if (argc >= 4)
    {
        if (strcmp(argv[3], "-skip") == 0)
            skip_BIOS = true;
    }

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

    ifstream exec_file(file_name, ios::binary | ios::in);
    if (!exec_file.is_open())
    {
        printf("Failed to load %s\n", file_name);
        return 1;
    }

    //Basic file format detection
    string file_string = file_name;
    string format = file_string.substr(file_string.length() - 4);
    printf("%s\n", format.c_str());

    if (format == ".elf")
    {
        long long ELF_size = filesize(file_name);
        uint8_t* ELF = new uint8_t[ELF_size];
        exec_file.read((char*)ELF, ELF_size);
        exec_file.close();

        printf("Loaded %s\n", file_name);
        printf("Size: %lld\n", ELF_size);
        e.load_ELF(ELF, ELF_size);
        delete[] ELF;
        ELF = nullptr;
        if (skip_BIOS)
            e.set_skip_BIOS_hack(SKIP_HACK::LOAD_ELF);
    }
    else if (format == ".iso")
    {
        exec_file.close();
        e.load_CDVD(file_name);
        if (skip_BIOS)
            e.set_skip_BIOS_hack(SKIP_HACK::LOAD_DISC);
    }
    else
    {
        printf("Unrecognized file format %s\n", format.c_str());
        return 1;
    }

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

    //Get the resolution of the GS image
    int inner_w, inner_h;
    e.get_inner_resolution(inner_w, inner_h);

    if (!inner_w || !inner_h)
        return;

    //Scale it to the TV screen
    QImage image((uint8_t*)buffer, inner_w, inner_h, QImage::Format_RGBA8888);

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

double EmuWindow::get_frame_rate()
{
    //Returns the framerate since old_frametime was set to the current time
    chrono::system_clock::time_point now = chrono::system_clock::now();
    chrono::duration<double> elapsed_seconds = now - old_frametime;
    return (1 / elapsed_seconds.count());
}

void EmuWindow::update_window_title()
{
    /*
    Updates window title every second
    Framerate displayed is the average framerate over 1 second
    */
    chrono::system_clock::time_point now = chrono::system_clock::now();
    chrono::duration<double> elapsed_update_seconds = now - old_update_time;

    double framerate = get_frame_rate();
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

void EmuWindow::limit_frame_rate()
{
    while (get_frame_rate() > 60.0);
}

void EmuWindow::reset_frame_time()
{
    old_frametime = chrono::system_clock::now();
}
