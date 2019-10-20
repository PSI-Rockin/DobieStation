#include <QApplication>
#include "emuwindow.hpp"

using namespace std;

int main(int argc, char** argv)
{
    QApplication::setOrganizationName("PSI");
    QApplication::setApplicationName("DobieStation");
    QApplication::setOrganizationDomain("https://github.com/PSI-Rockin/DobieStation");

    QApplication a(argc, argv);

    ds_log = new ds_logger;

    EmuWindow* window = new EmuWindow();

    if (window->init(argc, argv))
        return 1;

    a.exec();
    return 0;
}
