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

    uint current_button = (uint)pad.getEvent(selected_device);

    std::cout << "KeyCode: " << unsigned(current_button) << std::endl;

    button_map.emplace((BUTTONS)button, current_button);

}


PadConfigure::~PadConfigure()
{
    delete ui;
}
