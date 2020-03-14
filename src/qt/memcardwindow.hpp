#ifndef MEMCARDWINDOW_HPP
#define MEMCARDWINDOW_HPP

#include <QDialog>

namespace Ui {
class MemcardWindow;
}

class MemcardWindow : public QDialog
{
    Q_OBJECT

public:
    explicit MemcardWindow(QWidget *parent = 0);
    ~MemcardWindow();

private:
    Ui::MemcardWindow *ui;
};

#endif // MEMCARDWINDOW_HPP
