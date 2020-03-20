#include <cstring>
#include "bincuereader.hpp"

/**
 * Currently the CUE file is not parsed or even opened at all. I don't think this matters a whole lot for
 * PS2 games, but it will cause quite a few problems with PSX games.
 * Since there's no good way to test CUE parsing, it will have to wait for later.
 */

BinCueReader::BinCueReader()
{

}

bool BinCueReader::open(std::string name)
{
    if (bin_file.is_open())
        close();

    bin_file.open(name, std::ios::binary);
    return bin_file.is_open();
}

void BinCueReader::close()
{
    bin_file.close();
}

size_t BinCueReader::read(uint8_t* buff, size_t bytes)
{
    uint8_t temp[0x930];
    bin_file.read((char*)temp, 0x930);
    memcpy(buff, temp + 0x18, bytes);
    return bin_file.gcount();
}

void BinCueReader::seek(size_t pos, std::ios::seekdir whence)
{
    bin_file.seekg(pos * 0x930);
}

bool BinCueReader::is_open()
{
    return bin_file.is_open();
}

size_t BinCueReader::get_size()
{
    bin_file.seekg(0, std::ios::end);
    return bin_file.tellg();
}
