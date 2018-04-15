#include <QApplication>
#include "emuwindow.hpp"

using namespace std;

int main(int argc, char** argv)
{
    QApplication a(argc, argv);
    EmuWindow* window = new EmuWindow();
    if (window->init(argc, argv))
        return 1;
    bool enable_frame_rate_limiting = true;
    while (window->running())
    {
        a.processEvents();
		if(window->exec_loaded())
		{
			window->emulate();
			if (enable_frame_rate_limiting)
				window->limit_frame_rate();
			window->update_window_title();
			window->reset_frame_time();
		}
    }

    return 0;
}
