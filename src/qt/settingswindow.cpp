#include <QVBoxLayout>
#include <QGridLayout>
#include <QTabWidget>
#include <QLabel>
#include <QPushButton>
#include <QDialogButtonBox>
#include <QComboBox>
#include <QDir>
#include <QFileDialog>
#include <QDialog>
#include <QWidget>
#include <QGroupBox>
#include <QRadioButton>

#include "settingswindow.hpp"
#include "settings.hpp"

GeneralTab::GeneralTab(QWidget* parent)
    : QWidget(parent)
{
    QRadioButton* jit_checkbox = new QRadioButton(tr("JIT"));
    QRadioButton* interpreter_checkbox = new QRadioButton(tr("Interpreter"));

    bool vu1_jit = Settings::instance().vu1_jit_enabled;
    jit_checkbox->setChecked(vu1_jit);
    interpreter_checkbox->setChecked(!vu1_jit);

    connect(jit_checkbox, &QRadioButton::clicked, this, [=] (){
        Settings::instance().vu1_jit_enabled = true;
    });

    connect(interpreter_checkbox, &QRadioButton::clicked, this, [=] (){
        Settings::instance().vu1_jit_enabled = false;
    });

    connect(&Settings::instance(), &Settings::reload, this, [=]() {
        bool vu1_jit_enabled = Settings::instance().vu1_jit_enabled;
        jit_checkbox->setChecked(vu1_jit_enabled);
        interpreter_checkbox->setChecked(!vu1_jit_enabled);
    });

    QVBoxLayout* vu1_layout = new QVBoxLayout;
    vu1_layout->addWidget(jit_checkbox);
    vu1_layout->addWidget(interpreter_checkbox);

    QGroupBox* vu1_groupbox = new QGroupBox(tr("VU1"));
    vu1_groupbox->setLayout(vu1_layout);

    QVBoxLayout* layout = new QVBoxLayout;
    layout->addWidget(vu1_groupbox);
    layout->addStretch(1);

    setLayout(layout);

    setMinimumWidth(400);
}

PathTab::PathTab(QWidget* parent)
    : QWidget(parent)
{
    path_list = new QListWidget;
    path_list->setSpacing(1);
    path_list->insertItems(0, Settings::instance().rom_directories);

    QVBoxLayout* edit_layout = new QVBoxLayout;
    edit_layout->addWidget(path_list);

    QPushButton* add_directory = new QPushButton(tr("Add"));
    QPushButton* remove_directory = new QPushButton(tr("Remove"));

    connect(remove_directory, &QAbstractButton::clicked, this, [=]() {
        int row = path_list->currentRow();

        auto widget = path_list->takeItem(row);
        if (!widget)
            return;

        Settings::instance().remove_rom_directory(widget->text());
    });

    connect(add_directory, &QAbstractButton::clicked, this, [=]() {
        QString path = QFileDialog::getExistingDirectory(
            this, tr("Open Rom Directory"),
            Settings::instance().last_used_path
        );

        Settings::instance().add_rom_directory(path);
    });

    connect(&Settings::instance(), &Settings::rom_directory_added, this, [=](QString path) {
        path_list->insertItems(path_list->count(), { path });
    });

    connect(&Settings::instance(), &Settings::bios_changed, this, [=](QString path) {
        BiosReader bios_reader(path);
        bios_info->setText(bios_reader.to_string());
    });

    connect(&Settings::instance(), &Settings::reload, this, [=]() {
        BiosReader bios_reader(Settings::instance().bios_path);
        bios_info->setText(bios_reader.to_string());

        path_list->clear();
        path_list->insertItems(0, Settings::instance().rom_directories);
    });

    QHBoxLayout* button_layout = new QHBoxLayout;
    button_layout->addStretch();
    button_layout->addWidget(add_directory);
    button_layout->addWidget(remove_directory);

    edit_layout->addLayout(button_layout);

    QPushButton* browse_button = new QPushButton(tr("Browse"));
    connect(browse_button, &QAbstractButton::clicked, this, [=]() {
        QString path = QFileDialog::getOpenFileName(
            this, tr("Open Bios"), Settings::instance().last_used_path,
            tr("Bios File (*.bin)")
        );

        Settings::instance().set_bios_path(path);
    });

    BiosReader bios_reader(Settings::instance().bios_path);
    bios_info = new QLabel(bios_reader.to_string());

    QGridLayout* bios_layout = new QGridLayout;
    bios_layout->addWidget(bios_info, 0, 0);
    bios_layout->addWidget(browse_button, 0, 3);

    bios_layout->setColumnStretch(1, 1);
    bios_layout->setAlignment(Qt::AlignTop);

    QGroupBox* path_groupbox = new QGroupBox(tr("Rom Directories"));
    path_groupbox->setLayout(edit_layout);

    QGroupBox* bios_groupbox = new QGroupBox(tr("Bios"));
    bios_groupbox->setLayout(bios_layout);

    QVBoxLayout* layout = new QVBoxLayout;
    layout->addWidget(path_groupbox);
    layout->addWidget(bios_groupbox);

    setLayout(layout);
}

SettingsWindow::SettingsWindow(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Settings"));
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

    general_tab = new GeneralTab(this);
    path_tab = new PathTab(this);

    QTabWidget* tab_widget = new QTabWidget(this);
    tab_widget->addTab(general_tab, tr("General"));
    tab_widget->addTab(path_tab, tr("Paths"));

    QDialogButtonBox* close_buttons = new QDialogButtonBox(
        QDialogButtonBox::Close | QDialogButtonBox::Ok, this
    );
    connect(close_buttons, &QDialogButtonBox::accepted, this, [=]() {
        Settings::instance().save();
        accept();
    });
    connect(close_buttons, &QDialogButtonBox::rejected, this, [=]() {
        Settings::instance().reset();
        reject();
    });

    QVBoxLayout* layout = new QVBoxLayout;
    layout->addWidget(tab_widget);
    layout->addWidget(close_buttons);

    setLayout(layout);
}