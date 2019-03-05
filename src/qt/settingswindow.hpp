#ifndef SETTINGSWINDOW_HPP
#define SETTINGSWINDOW_HPP

#include <QDialog>
#include <QLabel>

#include "bios.hpp"

class GeneralTab : public QWidget
{
    Q_OBJECT
private:
    QLabel* bios_info = nullptr;
public:
    BiosReader bios_reader;
    bool vu1_jit = false;

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