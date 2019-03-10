#ifndef SETTINGSWINDOW_HPP
#define SETTINGSWINDOW_HPP

#include <QDialog>
#include <QLabel>
#include <QListWidget>

#include "bios.hpp"

class GeneralTab : public QWidget
{
    Q_OBJECT
public:
    bool vu1_jit = false;

    explicit GeneralTab(QWidget* parent = nullptr);
};

class PathTab : public QWidget
{
    Q_OBJECT
public:
    BiosReader bios_reader;
    QListWidget* path_list = nullptr;

    explicit PathTab(QWidget* parent = nullptr);
private:
    QLabel* bios_info = nullptr;
    void browse_for_bios();
};

class SettingsWindow : public QDialog
{
    Q_OBJECT
private:
    GeneralTab* general_tab = nullptr;
    PathTab* path_tab = nullptr;
public:
    explicit SettingsWindow(QWidget *parent = nullptr);
private slots:
    void save_and_reject();
};
#endif