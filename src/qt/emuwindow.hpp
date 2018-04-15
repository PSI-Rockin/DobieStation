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
		bool is_exec_loaded;
        std::chrono::system_clock::time_point old_frametime;
        std::chrono::system_clock::time_point old_update_time;
        double framerate_avg;

		QMenu* file_menu;
		QAction* open_action;
		QAction* open_action_skip;
		QAction* exit_action;
    public:
        explicit EmuWindow(QWidget *parent = nullptr);
        int init(int argc, char** argv);
		int load_exec(const char* file_name, bool skip_BIOS);

		bool exec_loaded();
		bool running();

        void emulate();

		void create_menu();

        void paintEvent(QPaintEvent *event);
        void closeEvent(QCloseEvent *event);

        double get_frame_rate();
        void update_window_title();
        void limit_frame_rate();
        void reset_frame_time();

	protected:
	#ifndef QT_NO_CONTEXTMENU
		void contextMenuEvent(QContextMenuEvent* event) override;
	#endif
	
    signals:

    public slots:
		void open_file_no_skip();
		void open_file_skip();
};

#endif // EMUWINDOW_HPP
