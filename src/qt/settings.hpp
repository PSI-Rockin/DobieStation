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
        QString screenshot_directory;
        QStringList rom_directories;
        QStringList rom_directories_to_add;
        QStringList rom_directories_to_remove;
        QStringList recent_roms;

        int scaling_factor;

        bool vu0_jit_enabled;
        bool vu1_jit_enabled;
        bool ee_jit_enabled;
        bool d_theme;
        bool l_theme;

        QString memcard_path;

        void save();
        void reset();
        void update_last_used_directory(const QString& path);
        void add_rom_directory(const QString& directory);
        void add_rom_path(const QString& path);
        void set_bios_path(const QString& path);
        void set_screenshot_directory(const QString& directory);
        void set_memcard_path(const QString& path);
        void remove_rom_directory(const QString& directory);
        void clear_rom_paths();
    private:
        Settings();
    signals:
        void rom_path_added(const QString& path);
        void rom_directory_added(const QString& directory);
        void rom_directory_removed(const QString& directory);
        void rom_directory_committed(const QString& directory);
        void rom_directory_uncommitted(const QString& directory);
        void bios_changed(const QString& path);
        void screenshot_directory_changed(const QString& directory);
        void memcard_changed(const QString& path);
        void reload();
};
#endif
