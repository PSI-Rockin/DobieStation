#include <QApplication>
#include "emuwindow.hpp"

using namespace std;

int main(int argc, char** argv)
{
    QApplication a(argc, argv);
    EmuWindow* window = new EmuWindow();
    if (window->init(argc, argv))
        return 1;
    a.exec();
    return 0;
}
