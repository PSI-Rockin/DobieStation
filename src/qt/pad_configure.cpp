#include "pad_configure.hpp"
#include "ui_pad_configure.h"

pad_configure::pad_configure(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::pad_configure)
{
    ui->setupUi(this);
    Interesting = findChild<QComboBox*>("Interesting_Devices");
    Pad.reset();
}

void pad_configure::Initalize()
{

    CurrentDevices = Pad.get_interesting_devices();

    for(int i = 0; i < Pad.get_interesting_devices().size(); i++)
    {
        Interesting->addItem( QString::fromStdString(CurrentDevices[i]->controller_name));
    }
}


pad_configure::~pad_configure()
{
    delete ui;
}
