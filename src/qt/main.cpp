#include <QApplication>
#include "emuwindow.hpp"

using namespace std;

int main(int argc, char** argv)
{
    QApplication a(argc, argv);
    EmuWindow* window = new EmuWindow();
    if (argc >= 3)
    {
        const char* bios_name = argv[1];
        const char* ELF_name = argv[2];
        if (window->init(bios_name, ELF_name))
            return 1;
    }
    else
    {
        printf("Arguments: bios_file ELF_file\n");
        return 0;
    }
    bool enable_frame_rate_limiting = true;
    while (window->running())
    {
        a.processEvents();
        window->emulate();
        if (enable_frame_rate_limiting)
            window->limit_frame_rate();
        window->update_window_title();
        window->reset_frame_time();
    }

    return 0;
}
