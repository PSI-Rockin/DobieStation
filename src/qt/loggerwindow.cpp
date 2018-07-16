#include "loggerwindow.hpp"
#include "../core/logger.hpp"

#include "qcheckbox.h"
#include "qlayout.h"

LoggerWindow::LoggerWindow(QWidget *parent) : QDialog(parent)
{
    auto layout = new QVBoxLayout(this);
    QCheckBox* list[Logger::OriginsCount];
    for(int i=0;i<Logger::OriginsCount;i++){
        list[i] = new QCheckBox(Logger::LogOriginNames[i], this);
        layout->addWidget(list[i], i, 0);
        connect(list[i], &QCheckBox::toggled,
            [=](bool checked) { Logger::toggle((Logger::LogOrigin)i, checked);});

    }
}
