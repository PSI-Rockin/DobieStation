#include <QDebug>
#include <QtEndian>

#include "bios.hpp"

BiosReader::BiosReader(QString path)
    : bios_path(path), valid(false)
{
    QFile file(bios_path);
    if (!file.open(QIODevice::ReadOnly))
        return;

    bios_data = file.read(BIOS_SIZE);

    // exit early on game.bin
    if (!bios_data.contains("KERNEL"))
        return;

    auto romdir_tag = bios_data.mid(
        bios_data.indexOf("ROMDIR"), ROMDIR_ENTRY_SIZE
    );

    if (romdir_tag.isEmpty())
        return;

    quint16 romdir_size = qFromLittleEndian<quint16>(
        romdir_tag.mid(12, sizeof(quint16))
    );

    int romdir_index = bios_data.indexOf("RESET");

    romdir = bios_data.mid(romdir_index, romdir_size);

    // ASCII data
    // version major: 2 bytes
    // version minor: 2 bytes
    // country code: 1 byte
    // console code: 1 byte
    // year: 4 bytes
    // month: 2 bytes
    // day: 2 bytes
    auto romver = get_module("ROMVER");

    if (romver.isEmpty())
        return;

    // 0000 -> 00.00
    bios_ver =  romver.mid(0, 4).insert(2, ".");

    auto country_code = romver.mid(4, 1);

    if (country_code == "A")
        bios_country = "USA";
    else if (country_code == "J")
        bios_country = "Japan";
    else if (country_code == "E")
        bios_country = "Europe";
    else
        bios_country = "Unknown";

    int year = QString(romver.mid(6, 4)).toInt();
    int month = QString(romver.mid(10, 2)).toInt();
    int day = QString(romver.mid(12, 2)).toInt();

    bios_date = QDate(year, month, day).toString(
        Qt::DateFormat::LocalDate
    );

    valid = true;
}

QByteArray BiosReader::get_module(QString name) const
{
    int index = romdir.indexOf(name);
    auto tag = romdir.mid(index, ROMDIR_ENTRY_SIZE);

    if (tag.isEmpty())
        return QByteArray();

    // name: 10 bytes
    // ext: 2 bytes
    // size 4 bytes
    const int SIZE_OFFSET = 12;

    quint32 module_size = qFromLittleEndian<quint32>(
        tag.mid(SIZE_OFFSET, sizeof(quint32))
    );

    int modules_before = index / ROMDIR_ENTRY_SIZE;

    quint32 pos = 0;
    for (int i = 0; i < modules_before; ++i)
    {
        int offset = i * ROMDIR_ENTRY_SIZE;

        quint32 size = qFromLittleEndian<quint32>(
            romdir.mid(offset + SIZE_OFFSET, sizeof(quint32))
        );

        if ((size % 0x10) == 0)
            pos += size;
        else
            pos += (size + 0x10) & 0xfffffff0;
    }

    return bios_data.mid(pos, module_size);
}

QString BiosReader::to_string() const
{
    if (!is_valid())
        return QString("Not a valid bios file.");

    return QString("%1 v%2 (%3)")
        .arg(bios_country)
        .arg(bios_ver)
        .arg(bios_date);
}

QString BiosReader::path() const
{
    return bios_path;
}

bool BiosReader::is_valid() const
{
    return valid;
}

const char* BiosReader::const_data() const
{
    return bios_data.constData();
}

uint8_t* BiosReader::data()
{
    return reinterpret_cast<uint8_t*>(bios_data.data());

}