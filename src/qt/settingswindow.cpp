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

#include "settingswindow.hpp"
#include "settings.hpp"

GeneralTab::GeneralTab(QWidget* parent)
    : QWidget(parent)
{
    QPushButton* browse_button = new QPushButton(tr("Browse"), this);
    connect(browse_button, &QAbstractButton::clicked, this,
        &GeneralTab::browse_for_bios);

    QLabel* bios_info = new QLabel("Not a valid bios file", this);

    QGridLayout* layout = new QGridLayout;
    layout->addWidget(new QLabel(tr("Bios:"), this), 0, 0);
    layout->addWidget(bios_info, 1, 0);
    layout->addWidget(browse_button, 1, 3);

    layout->setColumnStretch(1, 1);
    layout->setAlignment(Qt::AlignTop);

    setLayout(layout);
}

void GeneralTab::browse_for_bios()
{
    QString path = QFileDialog::getOpenFileName(
        this, tr("Open Bios"), "", tr("Bios File (*.bin)")
    );

    if(!path.isEmpty())
        bios_path = path;
}

SettingsWindow::SettingsWindow(QWidget *parent)
    : QDialog(parent)
{
    Settings::load();

    setWindowTitle(tr("Settings"));
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

    general_tab = new GeneralTab(this);
    general_tab->bios_path = Settings::bios_path;

    QTabWidget* tab_widget = new QTabWidget(this);
    tab_widget->addTab(general_tab, tr("General"));

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
    Settings::bios_path = general_tab->bios_path;

    Settings::save();

    reject();
}