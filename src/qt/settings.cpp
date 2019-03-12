#include "settings.hpp"
#include <QDebug>

static QSettings qsettings("PSI", "Dobiestation");

Settings::Settings()
{
    connect(this, &Settings::rom_directory_added, this, [=](QString directory) {
        last_used_path = directory;
    });

    connect(this, &Settings::rom_path_added, this, [=](QString path) {
        QFileInfo file_info(path);

        last_used_path = file_info.absoluteDir().path();
    });

    connect(this, &Settings::bios_changed, this, [=](QString path) {
        QFileInfo file_info(path);

        last_used_path = file_info.absoluteDir().path();
    });

    reset();
}

void Settings::reset()
{
    bios_path = qsettings.value("bios_path", "").toString();
    rom_directories = qsettings.value("rom_directories", {}).toStringList();
    recent_roms = qsettings.value("recent_roms", {}).toStringList();
    vu1_jit_enabled = qsettings.value("vu1_jit_enabled", true).toBool();
    last_used_path = qsettings.value("last_used_dir", QDir::homePath()).toString();

    emit reload();
}

void Settings::save()
{
    qsettings.setValue("rom_directories", rom_directories);
    qsettings.setValue("bios_path", bios_path);
    qsettings.setValue("vu1_jit_enabled", vu1_jit_enabled);
    qsettings.sync();
}

Settings& Settings::instance()
{
    static Settings settings;
    return settings;
}

void Settings::add_rom_directory(QString path)
{
    if (path.isEmpty())
        return;

    if (!rom_directories.contains(path, Qt::CaseInsensitive))
    {
        rom_directories.append(path);
        emit rom_directory_added(path);
    }
}

void Settings::remove_rom_directory(QString path)
{
    if (path.isEmpty())
        return;

    if (rom_directories.contains(path, Qt::CaseInsensitive))
    {
        rom_directories.removeAll(path);
        emit rom_directory_removed(path);
    }
}

void Settings::add_rom_path(QString path)
{
    if (path.isEmpty())
        return;

    if (!recent_roms.contains(path, Qt::CaseInsensitive))
    {
        recent_roms.prepend(path);
        qsettings.setValue("recent_roms", recent_roms);
        emit rom_path_added(path);
    }
}

void Settings::clear_rom_paths()
{
    recent_roms = QStringList();
    qsettings.setValue("recent_roms", {});
}

void Settings::set_bios_path(QString path)
{
    if (path.isEmpty())
        return;

    bios_path = path;
    emit bios_changed(path);
}
