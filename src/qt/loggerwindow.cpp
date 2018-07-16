#include "loggerwindow.hpp"
#include "../core/logger.hpp"

#include "qcheckbox.h"
#include "qlayout.h"

LoggerWindow::LoggerWindow(QWidget *parent) : QDialog(parent)
{
    auto layout = new QVBoxLayout(this);
    QCheckBox* list[LogOriginsCount];
    for(int i=0;i<LogOriginsCount;i++){
        list[i] = new QCheckBox(LogOriginNames[i], this);
        layout->addWidget(list[i], i, 0);
        connect(list[i], &QCheckBox::toggled,
            [=](bool checked) { Logger::toggle((LogOrigin)i, checked);});

    }
}
