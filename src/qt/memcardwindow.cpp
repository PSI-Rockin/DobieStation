#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFileInfo>

#include <fstream>

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

    connect(ui->create_button, &QAbstractButton::clicked, this, [=]() {
        QString path = QFileDialog::getSaveFileName(
            this, tr("Create Memcard"), Settings::instance().last_used_directory,
            tr("PS2 memcard image (*.ps2)")
        );

        //Page size of 0x200 (+ ECC), 0x4000 pages, for a total of 0x840000 bytes, the size of an 8 MB card
        size_t memcard_size = (0x200 + 16) * 0x4000;

        //Clear the entire file to 0xFF, required for games to format the card properly
        uint8_t* fill_buff = new uint8_t[memcard_size];
        memset(fill_buff, 0xFF, memcard_size);
        std::ofstream file(path.toStdString());
        file.write((char*)fill_buff, memcard_size);
        file.close();
        delete[] fill_buff;

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
