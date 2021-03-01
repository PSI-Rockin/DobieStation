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
    QRadioButton* ee_jit_checkbox = new QRadioButton(tr("JIT"));
    QRadioButton* vu0_jit_checkbox = new QRadioButton(tr("JIT - Experimental"));
    QRadioButton* vu1_jit_checkbox = new QRadioButton(tr("JIT"));
    QRadioButton* ee_interpreter_checkbox = new QRadioButton(tr("Interpreter"));
    QRadioButton* vu0_interpreter_checkbox = new QRadioButton(tr("Interpreter"));
    QRadioButton* vu1_interpreter_checkbox = new QRadioButton(tr("Interpreter"));
    QRadioButton* light_theme_checkbox = new QRadioButton(tr("Light Theme"));
    QRadioButton*  darktheme_checkbox = new QRadioButton(tr("Dark Theme"));


    bool ee_jit = Settings::instance().ee_jit_enabled;
    bool vu0_jit = Settings::instance().vu0_jit_enabled;
    bool vu1_jit = Settings::instance().vu1_jit_enabled;
    bool l_theme = Settings::instance().l_theme;
    bool d_theme = Settings::instance().d_theme;

    light_theme_checkbox->setChecked(l_theme);
    darktheme_checkbox->setChecked(d_theme);
    ee_jit_checkbox->setChecked(ee_jit);
    ee_interpreter_checkbox->setChecked(!ee_jit);
    vu0_jit_checkbox->setChecked(vu0_jit);
    vu0_interpreter_checkbox->setChecked(!vu0_jit);
    vu1_jit_checkbox->setChecked(vu1_jit);
    vu1_interpreter_checkbox->setChecked(!vu1_jit);

    connect(ee_jit_checkbox, &QRadioButton::clicked, this, [=]() {
        Settings::instance().ee_jit_enabled = true;
    });

    connect(ee_interpreter_checkbox, &QRadioButton::clicked, this, [=]() {
        Settings::instance().ee_jit_enabled = false;
    });

    connect(vu0_jit_checkbox, &QRadioButton::clicked, this, [=]() {
        Settings::instance().vu0_jit_enabled = true;
    });

    connect(vu0_interpreter_checkbox, &QRadioButton::clicked, this, [=]() {
        Settings::instance().vu0_jit_enabled = false;
    });

    connect(vu1_jit_checkbox, &QRadioButton::clicked, this, [=]() {
        Settings::instance().vu1_jit_enabled = true;
    });

    connect(vu1_interpreter_checkbox, &QRadioButton::clicked, this, [=]() {
        Settings::instance().vu1_jit_enabled = false;
    });
    connect(light_theme_checkbox, &QRadioButton::clicked, this, [=]() {
        Settings::instance().l_theme = true;
        Settings::instance().d_theme = false;
    });
    
    connect(darktheme_checkbox, &QRadioButton::clicked, this, [=]() {
        Settings::instance().d_theme = true;
        Settings::instance().l_theme = false;
    });

    connect(&Settings::instance(), &Settings::reload, this, [=]() {
        bool ee_jit_enabled = Settings::instance().ee_jit_enabled;
        bool vu0_jit_enabled = Settings::instance().vu0_jit_enabled;
        bool vu1_jit_enabled = Settings::instance().vu1_jit_enabled;
        bool  l_theme =Settings::instance().l_theme;
        bool  d_theme =Settings::instance().d_theme;
        ee_jit_checkbox->setChecked(ee_jit_enabled);
        ee_interpreter_checkbox->setChecked(!ee_jit_enabled);
        vu0_jit_checkbox->setChecked(vu0_jit_enabled);
        vu0_interpreter_checkbox->setChecked(!vu0_jit_enabled);
        vu1_jit_checkbox->setChecked(vu1_jit_enabled);
        vu1_interpreter_checkbox->setChecked(!vu1_jit_enabled);
        light_theme_checkbox->setChecked(l_theme);
        darktheme_checkbox->setChecked(d_theme);
    });


    QVBoxLayout* vu0_layout = new QVBoxLayout;
    vu0_layout->addWidget(vu0_jit_checkbox);
    vu0_layout->addWidget(vu0_interpreter_checkbox);

    QVBoxLayout* theme_layout = new QVBoxLayout;
    theme_layout->addWidget(light_theme_checkbox);
    theme_layout->addWidget(darktheme_checkbox);
     
    QGroupBox* theme_group = new QGroupBox(tr("Theming")); 
    theme_group->setLayout(theme_layout);

    QGroupBox* vu0_groupbox = new QGroupBox(tr("VU0"));
    vu0_groupbox->setLayout(vu0_layout);

    QVBoxLayout* vu1_layout = new QVBoxLayout;
    vu1_layout->addWidget(vu1_jit_checkbox);
    vu1_layout->addWidget(vu1_interpreter_checkbox);

    QGroupBox* vu1_groupbox = new QGroupBox(tr("VU1"));
    vu1_groupbox->setLayout(vu1_layout);

    QVBoxLayout* ee_layout = new QVBoxLayout;
    ee_layout->addWidget(ee_jit_checkbox);
    ee_layout->addWidget(ee_interpreter_checkbox);

    QGroupBox* ee_groupbox = new QGroupBox(tr("EE"));
    ee_groupbox->setLayout(ee_layout);    

    QVBoxLayout* layout = new QVBoxLayout;
    layout->addWidget(ee_groupbox);
    layout->addWidget(vu0_groupbox);
    layout->addWidget(vu1_groupbox);
    layout->addWidget(theme_group);
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
            Settings::instance().last_used_directory
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
    connect(browse_button, &QAbstractButton::clicked, [=]() {
        QString path = QFileDialog::getOpenFileName(
            this, tr("Open Bios"), Settings::instance().last_used_directory,
            tr("Bios File (*.bin)")
        );

        Settings::instance().set_bios_path(path);
    });

    BiosReader bios_reader(Settings::instance().bios_path);
    bios_info = new QLabel(bios_reader.to_string());

    QPushButton* screenshot_button = new QPushButton(tr("Browse"));
    connect(screenshot_button, &QAbstractButton::clicked, [=]() {
        QString directory = QFileDialog::getExistingDirectory(
            this, tr("Choose Screenshot Directory"),
            Settings::instance().last_used_directory
        );

        Settings::instance().set_screenshot_directory(directory);
    });

    auto screenshot_label =
        new QLabel(Settings::instance().screenshot_directory);

    connect(&Settings::instance(), &Settings::screenshot_directory_changed, [=](QString directory) {
        screenshot_label->setText(directory);
    });

    QGridLayout* other_layout = new QGridLayout;
    other_layout->addWidget(new QLabel(tr("Bios:")), 0, 0);
    other_layout->addWidget(bios_info, 0, 2);
    other_layout->addWidget(browse_button, 0, 3);
    other_layout->addWidget(new QLabel(tr("Screenshots:")), 1, 0);
    other_layout->addWidget(screenshot_label, 1, 2);
    other_layout->addWidget(screenshot_button, 1, 3);

    other_layout->setColumnStretch(1, 1);
    other_layout->setAlignment(Qt::AlignTop);

    QGroupBox* path_groupbox = new QGroupBox(tr("Rom Directories"));
    path_groupbox->setLayout(edit_layout);

    QGroupBox* other_groupbox = new QGroupBox(tr("Other"));
    other_groupbox->setLayout(other_layout);

    QVBoxLayout* layout = new QVBoxLayout;
    layout->addWidget(path_groupbox);
    layout->addWidget(other_groupbox);

    setLayout(layout);
}

SettingsWindow::SettingsWindow(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Settings"));
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

    general_tab = new GeneralTab(this);
    path_tab = new PathTab(this);

    tab_widget = new QTabWidget(this);
    tab_widget->addTab(general_tab, tr("General"));
    tab_widget->addTab(path_tab, tr("Paths"));
    tab_widget->setCurrentIndex(TAB::GENERAL);

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

void SettingsWindow::show_path_tab()
{
    tab_widget->setCurrentIndex(TAB::PATH);
}