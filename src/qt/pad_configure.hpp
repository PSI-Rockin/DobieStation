#ifndef PAD_CONFIGURE_H
#define PAD_CONFIGURE_H

#include <QDialog>
#include <QComboBox>
#include <QTimer>
#include <QButtonGroup>
#include "settings.hpp"
#include <map>
#include "../input_common/input_api.hpp"


namespace Ui {
class pad_configure;
}

class PadConfigure : public QDialog
{
    Q_OBJECT

public:
    explicit PadConfigure(QWidget *parent = nullptr);
    void get_input(int button);
    void save();
    ~PadConfigure();

private:
    Ui::pad_configure *ui;
    QComboBox *Interesting;  
    QTimer* CurrentTimer;  
    int selected_device;

    InputManager pad;


#ifdef __linux__
    std::vector<evdev_controller *> EvdevDevices;
#elif _WIN32
    std::vector<XINPUT_STATE> XinputDevices;
#endif
     QButtonGroup *button_widgets;

    std::map<BUTTONS, uint> button_map; 

};

#endif // PAD_CONFIGURE_H
