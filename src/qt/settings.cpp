#include <QDir>

#include "settings.hpp"

namespace Settings
{
    static QSettings settings("PSI", "Dobiestation");

    QSettings& get_instance()
    {
        return settings;
    }

    void set_bios_path(QString bios)
    {
        settings.setValue("bios", bios);
    }

    void set_last_used_path(QString path)
    {
        QFileInfo info(path);

        if(!info.path().isEmpty())
            settings.setValue("last_directory", info.path());
    }

    void set_vu1_jit_enabled(bool enabled)
    {
        settings.setValue("vu1_jit", enabled);
    }

    QString get_bios_path()
    {
        return settings.value("bios", "").toString();
    }

    QString get_last_used_path()
    {
        QString path = settings.value(
            "last_directory", QDir::homePath()
        ).toString();

        return path;
    }

    bool get_vu1_jit_enabled()
    {
        return settings.value("vu1_jit", true).toBool();
    }

    void save()
    {
        settings.sync();
        printf("saved!\n");
    }
};
