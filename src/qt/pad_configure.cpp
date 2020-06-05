#include "pad_configure.hpp"
#include "ui_pad_configure.h"

PadConfigure::PadConfigure(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::pad_configure)
{
    ui->setupUi(this);
    Interesting = ui->Interesting_Devices;


    pad.reset();

    button_widgets = new QButtonGroup(this);
    button_widgets->addButton(ui->Select);
    
    
    button_widgets->addButton(ui->Start);
    button_widgets->addButton(ui->Dpad_Up);
    button_widgets->addButton(ui->Dpad_Down);
    button_widgets->addButton(ui->Dpad_Left);    
    button_widgets->addButton(ui->Dpad_Right);
    
    button_widgets->addButton(ui->Triangle);
    button_widgets->addButton(ui->Circle);
    button_widgets->addButton(ui->Cross);
    button_widgets->addButton(ui->Square);
    
    EvdevDevices = pad.get_interesting_devices();

    for(int i = 0; i < pad.get_interesting_devices().size(); i++)
    {
        Interesting->addItem( QString::fromStdString(EvdevDevices[i]->controller_name));
    }
            
    selected_device = Interesting->currentIndex();

    connect(button_widgets, static_cast<void(QButtonGroup::*)(int)>(&QButtonGroup::buttonClicked), this, &PadConfigure::get_input);
    
} 

void PadConfigure::get_input(int button)
{

    int current_device = selected_device;
    CurrentTimer = new QTimer(this);
    connect(CurrentTimer, &QTimer::timeout, [&, current_device]() {pad.getEvent(selected_device);});
    CurrentTimer->start(1000);

    if (CurrentTimer->isActive())
    {
        int current_button = pad.getEvent(selected_device);

        std::cout << "Key Code: " << unsigned(current_button) << std::endl;

        if (current_button > 0)
        {
            button_map.emplace((BUTTONS)button, current_button);
            CurrentTimer->stop();
        }
    }

    else
    {
        return;
    }
}


PadConfigure::~PadConfigure()
{
    delete ui;
}
