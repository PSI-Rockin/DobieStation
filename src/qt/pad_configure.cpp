#include "pad_configure.hpp"
#include "ui_pad_configure.h"

PadConfigure::PadConfigure(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::pad_configure)
{
    ui->setupUi(this);
    Interesting = ui->Interesting_Devices;
    button_widgets = new QButtonGroup(this);
    button_widgets->addButton(ui->Circle);
    button_widgets->addButton(ui->Cross);
    button_widgets->addButton(ui->Triangle);
    button_widgets->addButton(ui->Square);
    button_widgets->addButton(ui->Dpad_Down);
    button_widgets->addButton(ui->Dpad_Up); 
    button_widgets->addButton(ui->Dpad_Left);    
    button_widgets->addButton(ui->Dpad_Right);
    button_widgets->addButton(ui->Start);
    button_widgets->addButton(ui->Select);

    initalize(); // This should go after all butions are found
}

void PadConfigure::initalize()
{

    pad.reset();

    EvdevDevices = pad.get_interesting_devices();

    for(int i = 0; i < pad.get_interesting_devices().size(); i++)
    {
        Interesting->addItem( QString::fromStdString(EvdevDevices[i]->controller_name));
    }
}


void PadConfigure::update()
{

    selected_device = Interesting->currentIndex();

}


PadConfigure::~PadConfigure()
{
    delete ui;
}
