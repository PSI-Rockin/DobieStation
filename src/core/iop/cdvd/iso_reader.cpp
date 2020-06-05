#include "iso_reader.hpp"

ISO_Reader::ISO_Reader()
{

}

bool ISO_Reader::open(std::string name)
{
    if (file.is_open())
        close();

    file.open(name, std::ios::binary);
    return file.is_open();
}

void ISO_Reader::close()
{
    file.close();
}

size_t ISO_Reader::read(uint8_t* buff, size_t bytes)
{
    file.read((char*)buff, bytes);
    return file.gcount();
}

void ISO_Reader::seek(size_t pos, std::ios::seekdir whence)
{
    file.seekg(pos * 2048);
}

bool ISO_Reader::is_open()
{
    return file.is_open();
}

size_t ISO_Reader::get_size()
{
    file.seekg(0, std::ios::end);
    return file.tellg();
}
