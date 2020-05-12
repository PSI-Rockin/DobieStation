#ifndef PAD_CONFIGURE_H
#define PAD_CONFIGURE_H

#include <QDialog>
#include <QComboBox>
#include "../input_common/input_api.hpp"


namespace Ui {
class pad_configure;
}

class pad_configure : public QDialog
{
    Q_OBJECT

public:
    explicit pad_configure(QWidget *parent = nullptr);
    void Initalize();
    ~pad_configure();

private:
    Ui::pad_configure *ui;
    QComboBox *Interesting;
    LinuxInput Pad;

    std::vector<evdev_controller *> CurrentDevices;

    BUTTONS Current_To_Config;

};

#endif // PAD_CONFIGURE_H
