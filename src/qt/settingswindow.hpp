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
    explicit GeneralTab(QWidget* parent = nullptr);
};

class PathTab : public QWidget
{
    Q_OBJECT
public:
    QListWidget* path_list = nullptr;

    explicit PathTab(QWidget* parent = nullptr);
private:
    QLabel* bios_info = nullptr;
};

class SettingsWindow : public QDialog
{
    Q_OBJECT
private:
    GeneralTab* general_tab = nullptr;
    PathTab* path_tab = nullptr;
public:
    explicit SettingsWindow(QWidget *parent = nullptr);
};
#endif