#ifndef SETTINGS_H
#define SETTINGS_H

#include <QString>

namespace Settings
{
    extern QString bios_path;

    extern char* as_char(QString q_str);
    extern bool load();
    extern bool save();
};

#endif
