#include <QSettings>
#include <QString>

#include "settings.hpp"

extern char* bios_name;

// https://wiki.qt.io/Technical_FAQ#How_can_I_convert_a_QString_to_char.2A_and_vice_versa.3F
bool load_settings()
{
    QSettings settings("PSI", "Dobiestation");
    QString q_bios = settings.value("bios", "").toString();
    QByteArray ba = q_bios.toLocal8Bit();

    bios_name = ba.data();

    printf("Loaded config! Bios: %s\n", bios_name);
}

bool save_settings()
{
    // Note: bizarrely, bios_name seems to get totally erased when initializing settings. 
    // Grab a copy first thing.
    QString q_bios = QString::fromLocal8Bit(bios_name);
    QSettings settings("PSI", "Dobiestation");

    settings.setValue("bios", q_bios);
    settings.sync();

    printf("Saved config! Bios: %s\n", qPrintable(q_bios));
}