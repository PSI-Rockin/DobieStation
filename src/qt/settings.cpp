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

    void set_vu1_jit_enabled(bool enabled)
    {
        settings.setValue("vu1_jit", enabled);
    }

    QString get_bios_path()
    {
        return settings.value("bios", "").toString();
    }

    bool get_vu1_jit_enabled()
    {
        return settings.value("vu1_jit", "").toBool();
    }

    void save()
    {
        settings.sync();
        printf("saved!\n");
    }
};
