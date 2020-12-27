#include "chd_reader.hpp"
#include <cstring>
#include <cassert>

bool CHD_Reader::open(std::string name)
{
    chd_error err = chd_open(name.c_str(), CHD_OPEN_READ, nullptr, &m_file);
    if (err != CHDERR_NONE)
    {
        fprintf(stderr, "chd: chd_open: %s\n", chd_error_string(err));
        return false;
    }

    m_header = chd_get_header(m_file);
    printf("chd: opened file %s\n", name.c_str());

    m_size = m_header->logicalbytes;
    printf("chd: size %zd\n", m_size);

    m_buf = std::make_unique<uint8_t[]>(m_header->hunkbytes);
    printf("chd: hunk size %d\n", m_header->hunkbytes);
    printf("chd: unitbytes %d\n", m_header->unitbytes);

    // read first hunk
    err = chd_read(m_file, 0, m_buf.get());
    if (err != CHDERR_NONE)
    {
        fprintf(stderr, "chd: chd_read: %s\n", chd_error_string(err));
        return false;
    }

    return true;
}

void CHD_Reader::close()
{
    printf("chd: close");
    chd_close(m_file);
    delete(m_header);
}

size_t CHD_Reader::read(uint8_t *buff, size_t bytes)
{
    assert(bytes);
    assert(m_virtptr + bytes <= m_size);

    printf("chd: reading %zd bytes from %lu\n", bytes, m_virtptr);

    const uint64_t start = m_virtptr;
    const uint64_t end = start + bytes;
    uint64_t start_hunk = start / m_header->hunkbytes;
    uint64_t end_hunk = end / m_header->hunkbytes;

    printf("chd: start %lu\n", start);
    printf("chd: start_hunk %lu\n", start_hunk);
    printf("chd: end %lu\n", end);
    printf("chd: end_hunk %lu\n", end_hunk);

    uint64_t total_read = 0;

    for (uint32_t i = (uint32_t)start_hunk; i <= end_hunk; i++)
    {
        //printf("chd: in the loop\n");
        if (i != m_current_hunk)
        {
            printf("chd: grabbing new hunk %d\n", i);
            chd_error err = chd_read(m_file, i, m_buf.get());
            if (err != CHDERR_NONE)
            {
                printf("chd: read %s\n", chd_error_string(err));
                return total_read;
            }

            m_current_hunk = i;
        }

        const uint64_t local_ofs = start - (uint64_t)start_hunk * m_header->hunkbytes;
        uint64_t readlen = m_header->hunkbytes;
        printf("chd: readlen %lu\n", readlen);
        printf("chd: local_ofs %lu\n", local_ofs);

        if (i == start_hunk)
            readlen -= local_ofs;
        if (i == end_hunk)
            readlen -= m_header->hunkbytes - (end - (uint64_t)end_hunk * m_header->hunkbytes);

        printf("chd: readlen %lu\n", readlen);

        assert(readlen <= bytes);

        memcpy(buff, m_buf.get() + local_ofs, readlen);
        total_read += readlen;
        //m_virtptr += readlen;
        m_virtptr += m_header->unitbytes;
        buff += readlen;
    }

    printf("chd: read %lu\n", total_read);

    return total_read;
}

void CHD_Reader::seek(size_t ofs, std::ios::seekdir whence)
{
    printf("chd: seek to sector %zd, bytes %lu\n", ofs, ofs*m_header->unitbytes);
    ofs *= m_header->unitbytes;
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

bool CHD_Reader::is_open()
{
    if(m_file)
        return true;

    return false;
}

size_t CHD_Reader::get_size()
{
    return m_size;
}
