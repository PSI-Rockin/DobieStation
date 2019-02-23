#ifndef SETTINGSWINDOW_HPP
#define SETTINGSWINDOW_HPP

#include <QDialog>

class GeneralTab : public QWidget
{
    Q_OBJECT
public:
    QString bios_path = nullptr;

    explicit GeneralTab(QWidget* parent = nullptr);
private slots:
    void browse_for_bios();
};

class SettingsWindow : public QDialog
{
    Q_OBJECT
private:
    GeneralTab* general_tab = nullptr;
public:
    explicit SettingsWindow(QWidget *parent = nullptr);
private slots:
    void save_and_reject();
};
#endif