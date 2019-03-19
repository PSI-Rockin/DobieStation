#ifndef SETTINGS_H
#define SETTINGS_H

#include <QObject>
#include <QSettings>
#include <QString>
#include <QDir>

class Settings final : public QObject
{
    Q_OBJECT
public:
    static Settings& instance();
    static QSettings& qsettings();

    // you won't be needing these
    // use instance() instead
    Settings(const Settings&) = delete;
    Settings& operator=(const Settings&) = delete;
    Settings(Settings&&) = delete;
    Settings& operator=(Settings&&) = delete;

    QString bios_path;
    QString last_used_directory;
    QStringList rom_directories;
    QStringList recent_roms;
    bool vu1_jit_enabled;

    void save();
    void reset();
    void update_last_used_directory(QString path);
    void add_rom_directory(QString directory);
    void add_rom_path(QString path);
    void set_bios_path(QString path);

    void remove_rom_directory(QString directory);
    void clear_rom_paths();
private:
    Settings();
signals:
    void rom_path_added(QString path);
    void rom_directory_added(QString directory);
    void rom_directory_removed(QString directory);
    void bios_changed(QString path);
    void reload();
};
#endif
