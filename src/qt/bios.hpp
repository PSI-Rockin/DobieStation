#ifndef BIOS_H
#define BIOS_H
#include <QByteArray>
#include <QString>
#include <QFile>
#include <QDate>

class BiosReader
{
    private:
        const int BIOS_SIZE = 1024 * 1024 * 4;
        const int ROMDIR_ENTRY_SIZE = 16;

        QByteArray bios_data;
        QByteArray romdir;
        QString bios_path;

        QString bios_date;
        QString bios_country;
        QString bios_ver;

        bool valid;
    public:
        BiosReader() = delete;
        BiosReader(QString path);

        QString to_string() const;
        QString path() const;
        QByteArray get_module(QString name) const;

        bool is_valid() const;
        const char* const_data() const;
        uint8_t* data();
};
#endif