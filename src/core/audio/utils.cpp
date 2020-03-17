#include "utils.hpp"
#include <cstdint>
#include <iostream>
#include <fstream>

void writewav(std::vector<int16_t> pcm, std::string filename)
{
    std::ofstream out(filename, std::fstream::out | std::fstream::binary);

    // HEADER
    out.write("RIFF", 4);
    uint32_t chunksize = 36 + (uint32_t)(pcm.size() * 2);
    out.write((char*)&chunksize, 4);

    out.write("WAVE", 4);

    // fmt
    out.write("fmt ", 4);
    uint32_t subchunk1s = 16;
    out.write((char*)&subchunk1s, 4);
    uint16_t format = 1;
    out.write((char*)&format, 2);
    uint16_t channels = 1;
    out.write((char*)&channels, 2);
    uint32_t samplerate = 44100;
    out.write((char*)&samplerate, 4);
    uint32_t byterate = 44100 * 1 * 16/8;
    out.write((char*)&byterate, 4);
    uint16_t blockalign = 1 * 16/8;
    out.write((char*)&blockalign, 2);
    uint16_t bits = 16;
    out.write((char*)&bits, 2);


    // data
    out.write("data", 4);
    uint32_t subchunk2s = (uint32_t)pcm.size() * 2;
    out.write((char*)&subchunk2s, 4);

    for (auto s : pcm)
    {
        out.write((char*)&s, 2);

    }
    out.close();

}

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
