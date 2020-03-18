#include "utils.hpp"
#include <cstdint>
#include <iostream>
#include <fstream>

WAVWriter::WAVWriter(std::string filename, int channels) : file(filename, std::fstream::out | std::fstream::binary),
                                                           channels(channels)
{
    update_header();
}

WAVWriter::WAVWriter(std::string filename) : file(filename, std::fstream::out | std::fstream::binary)
{
    update_header();
}

void WAVWriter::update_header()
{
    file.seekp(std::ios::beg);
    //printf("test\n");

    // Header
    file.write(RIFF, 4);
    uint32_t size1 = header_size+data_size;
    file.write((char*)&size1, 4);
    file.write(WAVE, 4);

    // FMT
    file.write(fmt, 4);
    file.write((char*)&format_size, 4);
    file.write((char*)&format, 2);
    file.write((char*)&channels, 2);
    file.write((char*)&sample_rate, 4);
    uint32_t byte_rate = sample_rate*(uint16_t)channels*sample_size/8;
    file.write((char*)&byte_rate, 4);
    uint16_t block_alignment = (uint16_t)channels*sample_size/8;
    file.write((char*)&block_alignment, 2);
    file.write((char*)&sample_size, 2);

    file.write((char*)data, 4);
    uint32_t size = header_size + data_size;
    file.write((char*)&size, 4);
}

void WAVWriter::append_pcm_stereo(std::vector<int16_t> left, std::vector<int16_t> right)
{
    file.seekp(44+data_size);
    uint32_t samples = (uint32_t)left.size();

    for (uint32_t i = 0; i < samples; i++)
    {
        file.write((char*)&(left.at(i)), 2);
        file.write((char*)&(right.at(i)), 2);
    }

    data_size += samples * 4;

    update_header();
}

void WAVWriter::append_pcm(std::vector<int16_t> pcm)
{
    file.seekp(44+data_size);

    for (auto s : pcm)
    {
        file.write((char*)&s, 2);

    }

    data_size += (uint32_t)pcm.size() * 2;

    update_header();
}

WAVWriter::~WAVWriter()
{
    file.close();
}
