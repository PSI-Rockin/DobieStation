#ifndef CSO_READER_H
#define CSO_READER_H

#include <fstream>
#include <cstdint>
#include "cdvd_container.hpp"

class CSO_Reader : public CDVD_Container
{
    protected:
        std::ifstream m_file;
        size_t m_size;
        uint32_t m_shift;
        uint32_t m_blocksize;
        uint8_t m_version;
        uint64_t m_virtptr;

        uint32_t* m_indices;

        uint32_t m_framesize;
        uint32_t m_curframe;
        uint8_t* m_frame;
        uint8_t* m_readbuf;

        struct libdeflate_decompressor* m_inflate;

        bool read_block_internal(uint32_t block);
    public:
        CSO_Reader();
        ~CSO_Reader();

        bool open(std::string name);
        void close();
        size_t read(uint8_t* dst, size_t size);
        void seek(size_t ofs, std::ios::seekdir whence);

        bool is_open();
        size_t get_size();

        uint8_t get_version();
        uint32_t get_blocksize();
        uint32_t get_numblocks();
        uint64_t tell();
};

#endif//CSO_READER_H
