#include <QSettings>
#include <QString>

#include "settings.hpp"

namespace Settings
{
    QString bios_path;

    // If we had a files for QT helper functions, this could go there, but here works for now.
    // https://wiki.qt.io/Technical_FAQ#How_can_I_convert_a_QString_to_char.2A_and_vice_versa.3F
    char* as_char(QString q_str)
    {
        QByteArray ba = q_str.toLocal8Bit();
        return ba.data();
    }

    bool load()
    {
        QSettings conf("PSI", "Dobiestation");

        bios_path = conf.value("bios", "").toString();
        printf("Loaded config! Bios: %s\n", qPrintable(bios_path));
    }

    bool save()
    {
        QSettings conf("PSI", "Dobiestation");

        conf.setValue("bios", bios_path);
        conf.sync();

        printf("Saved config! Bios: %s\n", qPrintable(bios_path));
    }
};
