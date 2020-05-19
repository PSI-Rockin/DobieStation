#ifndef PAD_CONFIGURE_H
#define PAD_CONFIGURE_H

#include <QDialog>
#include <QComboBox>
#include <QButtonGroup>
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
    void initalize();
    void update();
    void get_input();
    ~PadConfigure();

private:
    Ui::pad_configure *ui;
    QComboBox *Interesting;    
    int selected_device;

    InputManager pad;


#ifdef __linux__
    std::vector<evdev_controller *> EvdevDevices;
#elif _WIN32
    std::vector<XINPUT_STATE> XinputDevices;
#endif
     QButtonGroup *button_widgets;

    std::map<BUTTONS, uint32_t> button_set; 

};

#endif // PAD_CONFIGURE_H
