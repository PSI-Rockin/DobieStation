/*
CISO (v0, v1) decoder implementation
 copyleft 2019 a dinosaur

Based off reference by unknownbrackets:
 - https://github.com/unknownbrackets/maxcso/blob/master/README_CSO.md
*/

#include "cso_reader.hpp"
#include <libdeflate.h>
#include <cstring>
#include <cassert>

constexpr uint32_t FOURCC(const char chars[4])
{
    return
        ((uint32_t)(unsigned char)chars[0] << 0) |
        ((uint32_t)(unsigned char)chars[1] << 8) |
        ((uint32_t)(unsigned char)chars[2] << 16) |
        ((uint32_t)(unsigned char)chars[3] << 24);
}

struct CSO_Header
{
    // fourcc "CISO"
    uint32_t magic;
    
    // v0, v1: not reliable
    // v2: must be 0x18
    uint32_t header_len;
    
    // uncompressed size of original ISO
    uint64_t raw_len;
    
    // usually 2048
    uint32_t block_len;
    
    uint8_t version;
    uint8_t index_shift;
    uint8_t reserved[2];
};

#define IDX_COMPRESS_BIT (0x80000000)


CSO_Reader::CSO_Reader() :
    m_size(0), m_shift(0), m_blocksize(0), m_version(0), m_virtptr(0),
    m_indices(nullptr),
    m_framesize(0), m_curframe(0xFFFFFFFF),
    m_frame(nullptr), m_readbuf(nullptr),
    m_inflate(nullptr) {}

CSO_Reader::~CSO_Reader()
{
    close();
}


uint8_t CSO_Reader::get_version()
{
    return m_version;
}

uint64_t CSO_Reader::get_size()
{
    return m_size;
}

uint32_t CSO_Reader::get_blocksize()
{
    return m_blocksize;
}

uint32_t CSO_Reader::get_numblocks()
{
    return (uint32_t)(m_size / m_blocksize);
}


bool CSO_Reader::isopen()
{
    return m_file.is_open();
}

void CSO_Reader::seek(int64_t ofs, std::ios::seekdir whence)
{
    if (whence == std::ios::beg)
    {
        if ((uint64_t)ofs < m_size)
            m_virtptr = (uint64_t)ofs;
    }
    else
    if (whence == std::ios::cur)
    {
        if (m_virtptr + ofs < m_size)
            m_virtptr = m_virtptr + ofs;
    }
    else
    if (whence == std::ios::end)
    {
        if (m_size - ofs < m_size)
            m_virtptr = m_size - ofs;
    }
}

uint64_t CSO_Reader::tell()
{
    return m_virtptr;
}

bool CSO_Reader::read_block_internal(uint32_t block)
{
    // if this block was decoded last time we don't need to do it again
    if (block == m_curframe)
        return true;
    
    uint32_t index = m_indices[block];
    uint64_t ofs = (uint64_t)(index & ~IDX_COMPRESS_BIT) << m_shift;
    uint64_t len = ((uint64_t)(m_indices[block + 1] & ~IDX_COMPRESS_BIT) << m_shift) - ofs;
    
    if (index & IDX_COMPRESS_BIT) // if uncompressed
    {
        m_file.seekg(ofs, std::ios::beg);
        m_file.read((char*)m_frame, len);
        if ((uint64_t)m_file.gcount() != len)
        {
            fprintf(stderr, "read error reading (uncompressed) block %d\n", block);
            m_curframe = 0xFFFFFFFF;
            return false;
        }
    }
    else // compressed
    {
        m_file.seekg(ofs, std::ios::beg);
        m_file.read((char*)m_readbuf, len);
        if ((uint64_t)m_file.gcount() != len)
        {
            fprintf(stderr, "read error reading (compressed) block %d\n", block);
            return false;
        }
        
        size_t read;
        auto res = libdeflate_deflate_decompress(m_inflate, m_readbuf, len, m_frame, m_framesize, &read);
        if (res != LIBDEFLATE_SUCCESS)
        {
            fprintf(stderr, "libdeflate error on block %d: %d\n", block, res);
            m_curframe = 0xFFFFFFFF;
            return false;
        }
        
        if (read < m_blocksize)
        {
            fprintf(stderr, "compressed sector %d decoded to less than the blocksize\n", block);
            m_curframe = 0xFFFFFFFF;
            return false;
        }
    }
    
    m_curframe = block;
    return true;
}

uint64_t CSO_Reader::read(uint8_t* dst, uint64_t size)
{
    assert(size);
    assert(m_virtptr + size <= m_size);
    
    const uint64_t start = m_virtptr;
    const uint64_t end = start + size;
    const auto start_block = (uint32_t)(start / m_blocksize);
    const auto end_block = (uint32_t)(end / m_blocksize);
    
    uint64_t total_read = 0;
    for (uint32_t i = start_block; i <= end_block; ++i)
    {
        if (!read_block_internal(i))
            return total_read;
        
        const uint64_t local_ofs = start - start_block * m_blocksize;
        uint64_t readlen = m_blocksize;
        if (i == start_block)
            readlen -= local_ofs;
        if (i == end_block)
            readlen -= m_blocksize - (end - end_block * m_blocksize);
        
        memcpy(dst, m_frame + local_ofs, readlen);
        total_read += readlen;
        m_virtptr += readlen;
        dst += readlen;
    }
    
    return total_read;
}


bool CSO_Reader::open(const char* path)
{
    close();
    m_file = std::ifstream(path, std::ios::binary | std::ios::ate);
    if (!m_file.is_open())
    {
        fprintf(stderr, "failed to open file\n");
        return false;
    }
    auto file_len = m_file.tellg();
    m_file.seekg(0, std::ios::beg);
    
    CSO_Header header;
    m_file.read((char*)&header.magic, sizeof(uint32_t));
    m_file.read((char*)&header.header_len, sizeof(uint32_t));
    m_file.read((char*)&header.raw_len, sizeof(uint64_t));
    m_file.read((char*)&header.block_len, sizeof(uint32_t));
    m_file.read((char*)&header.version, sizeof(uint8_t));
    m_file.read((char*)&header.index_shift, sizeof(uint8_t));
    m_file.read((char*)header.reserved, 2 * sizeof(uint8_t));
    
    // validate header
    if (header.magic != FOURCC("CISO"))
    {
        fprintf(stderr, "file is not a CSO!\n");
        return false;
    }
    if (header.version > 1)
    {
        fprintf(stderr, "unsupported CSO version or corrupt file\n");
        return false;
    }
    
    // reinstate these if CSOv2 support is ever added
    /*
    if (header.version == 2 && header.header_len != 0x18)
        return false;
    if (header.version == 2 && (header.reserved[0] || header.reserved[1]))
        return false;
    */
    
    // read indices
    auto num_entries = (uint32_t)((header.raw_len + header.block_len - 1) / header.block_len) + 1;
    m_indices = new uint32_t[num_entries];
    m_file.read((char*)m_indices, num_entries * sizeof(uint32_t));
    if ((uint32_t)m_file.gcount() != num_entries * sizeof(uint32_t))
    {
        fprintf(stderr, "failed to read CSO indices\n");
        close();
        return false;
    }
    
    // sanity check indices
    uint32_t lastidx = m_indices[0];
    if ((lastidx & ~IDX_COMPRESS_BIT) << header.index_shift < 0x18)
    {
        fprintf(stderr, "CSO indices are corrupted (starts within header)\n");
        close();
        return false;
    }
    
    uint32_t framesize = header.block_len + (1 << m_shift);
    for (unsigned i = 1; i < num_entries; ++i)
    {
        uint32_t lastpos = (lastidx & ~IDX_COMPRESS_BIT) << header.index_shift;
        if (lastpos > file_len)
        {
            fprintf(stderr, "CSO indices are corrupted (outside file)\n");
            close();
            return false;
        }
        
        uint32_t idx = m_indices[i];
        uint32_t pos = (idx & ~IDX_COMPRESS_BIT) << header.index_shift;
        uint32_t len = pos - lastpos;
        if (len <= 0)
        {
            fprintf(stderr, "CSO indices are corrupted (out of order)\n");
            close();
            return false;
        }
        else if (len > framesize)
        {
            fprintf(stderr, "CSO indices are corrupted (index too large)\n");
            close();
            return false;
        }
        else if ((lastidx & IDX_COMPRESS_BIT) && len < header.block_len)
        {
            fprintf(stderr, "CSO indices are corrupted (uncompressed index smaller than block size)\n");
            close();
            return false;
        }
        lastidx = idx;
    }
    
    m_version = header.version;
    m_size = header.raw_len;
    m_shift = header.index_shift;
    m_blocksize = header.block_len;
    m_framesize = framesize;
    m_frame = new uint8_t[m_framesize];
    m_readbuf = new uint8_t[m_framesize];
    
    // setup libdeflate
    m_inflate = libdeflate_alloc_decompressor();
    if (!m_inflate)
    {
        fprintf(stderr, "failed to allocate decompressor\n");
        close();
        return false;
    }
    
    return true;
}

void CSO_Reader::close()
{
    if (m_inflate)
    {
        libdeflate_free_decompressor(m_inflate);
        m_inflate = nullptr;
    }
    
    if (m_readbuf)
    {
        delete[] m_readbuf;
        m_readbuf = nullptr;
    }
    
    if (m_frame)
    {
        delete[] m_frame;
        m_frame = nullptr;
    }
    
    if (m_indices)
    {
        delete[] m_indices;
        m_indices = nullptr;
    }
    
    if (m_file.is_open())
        m_file.close();
    
    m_version = 0;
    m_virtptr = 0;
    m_size = 0;
    m_shift = 0;
    m_blocksize = 0;
    m_framesize = 0;
    m_curframe = 0xFFFFFFFF;
}
