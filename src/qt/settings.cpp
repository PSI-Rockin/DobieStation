#include "settings.hpp"

Settings::Settings()
{
    connect(this, &Settings::rom_path_added, [=](const QString& path) {
        QFileInfo file_info(path);
        update_last_used_directory(file_info.absoluteDir().path());
    });

    connect(this, &Settings::bios_changed, [=](const QString& path) {
        QFileInfo file_info(path);
        update_last_used_directory(file_info.absoluteDir().path());
    });

    connect(this, &Settings::rom_directory_added, [=](const QString& path) {
        rom_directories_to_add.append(path);
        update_last_used_directory(path);
    });

    connect(this, &Settings::rom_directory_removed, [=](const QString& path) {
        rom_directories_to_remove.append(path);
    });

    reset();
}

void Settings::reset()
{
    bios_path = qsettings().value("bios_path", "").toString();
    rom_directories = qsettings().value("rom_directories", {}).toStringList();
    recent_roms = qsettings().value("recent_roms", {}).toStringList();
    ee_jit_enabled = qsettings().value("ee_jit_enabled", true).toBool();
    vu1_jit_enabled = qsettings().value("vu1_jit_enabled", true).toBool();
    last_used_directory = qsettings().value("last_used_dir", QDir::homePath()).toString();
    screenshot_directory = qsettings().value("screenshot_directory", QDir::homePath()).toString();

    rom_directories_to_add = QStringList();
    rom_directories_to_remove = QStringList();

    emit reload();
}

void Settings::save()
{
    // NOTE: order matters!
    // If the user for example removes a directory
    // and adds the parent directory before a save then
    // doing the removal after the add will cause
    // the sub directory not to be added properly
    for (const auto& directory : rom_directories_to_remove)
    {
        rom_directories.removeAll(directory);
        emit rom_directory_uncommitted(directory);
    }

    for (const auto& directory : rom_directories_to_add)
    {
        rom_directories.append(directory);
        emit rom_directory_committed(directory);
    }

    rom_directories.sort(Qt::CaseInsensitive);
    rom_directories.removeDuplicates();

    qsettings().setValue("rom_directories", rom_directories);
    qsettings().setValue("bios_path", bios_path);
    qsettings().setValue("ee_jit_enabled", ee_jit_enabled);
    qsettings().setValue("vu1_jit_enabled", vu1_jit_enabled);
    qsettings().setValue("screenshot_directory", screenshot_directory);
    qsettings().sync();
    reset();
}

Settings& Settings::instance()
{
    static Settings settings;
    return settings;
}

QSettings& Settings::qsettings()
{
    static QSettings settings;
    return settings;
}

void Settings::update_last_used_directory(const QString& path)
{
    last_used_directory = path;
    qsettings().setValue("last_used_dir", last_used_directory);
}

void Settings::add_rom_directory(const QString& path)
{
    if (path.isEmpty())
        return;

    if (!rom_directories.contains(path, Qt::CaseInsensitive))
    {
        emit rom_directory_added(path);
    }
}

void Settings::remove_rom_directory(const QString& path)
{
    if (path.isEmpty())
        return;

    if (rom_directories.contains(path, Qt::CaseInsensitive))
    {
        emit rom_directory_removed(path);
    }
}

void Settings::add_rom_path(const QString& path)
{
    if (path.isEmpty())
        return;

    if (recent_roms.contains(path))
        recent_roms.removeAt(recent_roms.indexOf(path));

    recent_roms.prepend(path);

    if (recent_roms.size() > 10)
        recent_roms = recent_roms.mid(0, 10);

    qsettings().setValue("recent_roms", recent_roms);
    emit rom_path_added(path);
}

void Settings::clear_rom_paths()
{
    recent_roms = QStringList();
    qsettings().setValue("recent_roms", {});
}

void Settings::set_bios_path(const QString& path)
{
    if (path.isEmpty())
        return;

    bios_path = path;
    emit bios_changed(path);
}

void Settings::set_screenshot_directory(const QString& directory)
{
    if (directory.isEmpty())
        return;

    screenshot_directory = directory;
    emit screenshot_directory_changed(directory);
}
