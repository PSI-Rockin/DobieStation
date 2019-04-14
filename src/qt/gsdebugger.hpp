#ifndef GSDEBUGGER_HPP
#define GSDEBUGGER_HPP
#include <QDockWidget>

class GSControlDock : public QDockWidget
{
    Q_OBJECT
public:
     explicit GSControlDock(QWidget *parent = nullptr);
signals:
    void request_single_draw();
    void request_ten_draws();
    void request_next_frame();
    void request_draw_frame();
    void request_goto_draw(int location);
};

class ImageDock : public QDockWidget
{
    Q_OBJECT
public:
    enum Channel
    {
        ALL,
        RED,
        GREEN,
        BLUE,
    };
    explicit ImageDock(QWidget *parent = nullptr);
signals:
    void request_channel(int channel);
};
#endif