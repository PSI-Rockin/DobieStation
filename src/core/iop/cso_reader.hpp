#ifndef CSO_READER_H
#define CSO_READER_H

#include <fstream>
#include <cstdint>

class CSO_Reader
{
protected:
    std::ifstream m_file;
    uint64_t m_size;
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
    
    uint8_t get_version();
    uint64_t get_size();
    uint32_t get_blocksize();
    uint32_t get_numblocks();
    
    bool isopen();
    void seek(int64_t ofs, std::ios::seekdir whence);
    uint64_t tell();
    uint64_t read(uint8_t* dst, uint64_t size);
    
    bool open(const char* path);
    void close();
};

#endif//CSO_READER_H
