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

    vu1_jit = Settings::get_vu1_jit_enabled();
    jit_checkbox->setChecked(vu1_jit);
    interpreter_checkbox->setChecked(!vu1_jit);

    connect(jit_checkbox, &QRadioButton::clicked, this, [=] (){
        vu1_jit = true;
    });

    connect(interpreter_checkbox, &QRadioButton::clicked, this, [=] (){
        vu1_jit = false;
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

    QVBoxLayout* edit_layout = new QVBoxLayout;
    edit_layout->addWidget(path_list);

    QPushButton* add_directory = new QPushButton(tr("Add"));
    QPushButton* remove_directory = new QPushButton(tr("Remove"));

    connect(remove_directory, &QAbstractButton::clicked, this, [=]() {
        int row = path_list->currentRow();

        path_list->takeItem(row);
    });

    connect(add_directory, &QAbstractButton::clicked, this, [=]() {
        QString path = QFileDialog::getExistingDirectory(
            this, tr("Open Rom Directory"), Settings::get_last_used_path()
        );

        if (!path.isEmpty())
            path_list->insertItems(path_list->count(), { path });
    });

    QHBoxLayout* button_layout = new QHBoxLayout;
    button_layout->addStretch();
    button_layout->addWidget(add_directory);
    button_layout->addWidget(remove_directory);

    edit_layout->addLayout(button_layout);

    QPushButton* browse_button = new QPushButton(tr("Browse"));
    connect(browse_button, &QAbstractButton::clicked,
        this, &PathTab::browse_for_bios);

    bios_reader = BiosReader(Settings::get_bios_path());
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

void PathTab::browse_for_bios()
{
    QString path = QFileDialog::getOpenFileName(
        this, tr("Open Bios"), Settings::get_last_used_path(),
        tr("Bios File (*.bin)")
    );

    Settings::set_last_used_path(path);

    if (!path.isEmpty())
    {
        bios_reader = BiosReader(path);

        bios_info->setText(bios_reader.to_string());
    }
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

    QDialogButtonBox* close_button =
        new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(close_button, &QDialogButtonBox::rejected, this,
        &SettingsWindow::save_and_reject);

    QVBoxLayout* layout = new QVBoxLayout;
    layout->addWidget(tab_widget);
    layout->addWidget(close_button);

    setLayout(layout);
}

void SettingsWindow::save_and_reject()
{
    Settings::set_bios_path(path_tab->bios_reader.path());
    Settings::set_vu1_jit_enabled(general_tab->vu1_jit);
    Settings::save();

    reject();
}