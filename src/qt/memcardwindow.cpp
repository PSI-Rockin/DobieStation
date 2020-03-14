#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFileInfo>

#include "memcardwindow.hpp"
#include "settings.hpp"
#include "ui_memcardwindow.h"

MemcardWindow::MemcardWindow(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::MemcardWindow)
{
    ui->setupUi(this);
    if (Settings::instance().memcard_path.isEmpty())
        ui->memcard_info->setText("No memcard selected.");
    else
        ui->memcard_info->setText(QFileInfo(Settings::instance().memcard_path).fileName());

    connect(ui->browse_button, &QAbstractButton::clicked, this, [=]() {
        QString path = QFileDialog::getOpenFileName(
            this, tr("Open Memcard"), Settings::instance().last_used_directory,
            tr("PS2 memcard image (*.ps2)")
        );

        Settings::instance().set_memcard_path(path);
        Settings::instance().save();
    });

    connect(&Settings::instance(), &Settings::memcard_changed, this, [=](QString path) {
        QFileInfo f(path);
        ui->memcard_info->setText(f.fileName());
    });
}

MemcardWindow::~MemcardWindow()
{
    delete ui;
}
