#ifndef PAD_CONFIGURE_H
#define PAD_CONFIGURE_H

#include <QDialog>
#include <QComboBox>
<<<<<<< HEAD
#include <QTimer>
#include <QButtonGroup>
#include "settings.hpp"
#include <map>
=======
>>>>>>> a77f949... Work on configureator, not sure how to extend that to windows stuff
#include "../input_common/input_api.hpp"


namespace Ui {
class pad_configure;
}

class PadConfigure : public QDialog
{
    Q_OBJECT

public:
<<<<<<< HEAD
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
#elif WIN32
    std::vector<xinput_controller> XinputDevices;
#endif
     QButtonGroup *button_widgets;

    std::map<BUTTONS, int> button_map; 
=======
    explicit pad_configure(QWidget *parent = nullptr);
    void Initalize();
    ~pad_configure();

private:
    Ui::pad_configure *ui;
    QComboBox *Interesting;
    LinuxInput Pad;

    std::vector<evdev_controller *> CurrentDevices;

    BUTTONS Current_To_Config;
>>>>>>> a77f949... Work on configureator, not sure how to extend that to windows stuff

};

#endif // PAD_CONFIGURE_H
