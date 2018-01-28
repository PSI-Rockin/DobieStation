#include <QApplication>
#include "emuwindow.hpp"

using namespace std;

int main(int argc, char** argv)
{
    QApplication a(argc, argv);
    EmuWindow* window = new EmuWindow();
    if (window->init())
        return 1;
    while (window->running())
    {
        a.processEvents();
        window->emulate();
    }

    return 0;
}
