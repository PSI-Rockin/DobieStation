#include "chd_reader.hpp"
#include <cassert>
#include <cstring>

bool CHD_Reader::open(std::string name)
{
    chd_error err = chd_open(name.c_str(), CHD_OPEN_READ, nullptr, &m_file);
    if (err != CHDERR_NONE)
    {
        fprintf(stderr, "chd: chd_open: %s\n", chd_error_string(err));
        return false;
    }

    m_header = chd_get_header(m_file);
    m_size = m_header->logicalbytes;
    m_buf = std::make_unique<uint8_t[]>(m_header->hunkbytes);

    find_offset();

    // read first hunk in advance
    err = chd_read(m_file, 0, m_buf.get());
    if (err != CHDERR_NONE)
    {
        fprintf(stderr, "chd: chd_read: %s\n", chd_error_string(err));
        return false;
    }

    return true;
}

void CHD_Reader::find_offset()
{
    // TODO: If we want to eventually read multiple tracks they each have separate metadata.
    char metadata_str[256];
    uint32_t metadata_len;
    chd_error err = chd_get_metadata(m_file, CDROM_TRACK_METADATA2_TAG, 0, metadata_str, sizeof(metadata_str), &metadata_len, nullptr, nullptr);
    if (err != CHDERR_NONE)
    {
        fprintf(stderr, "chd: read_metadata: %s\n", chd_error_string(err));
    }

    char type_str[256];
    char subtype_str[256];
    char pgtype_str[256];
    char pgsub_str[256];
    int track_num = 0, frames = 0, pregap_frames = 0, postgap_frames = 0;

    std::sscanf(metadata_str, CDROM_TRACK_METADATA2_FORMAT, &track_num, type_str, subtype_str, &frames,
                &pregap_frames, pgtype_str, pgsub_str, &postgap_frames);

    if (std::strncmp(type_str, "MODE2_FORM_MIX", 14) == 0)
        m_sector_offset = 8;
    else if (std::strncmp(type_str, "MODE2_FORM1", 10) == 0)
        m_sector_offset = 0;
    else if (std::strncmp(type_str, "MODE2_FORM2", 10) == 0)
        m_sector_offset = 0;
    else if (std::strncmp(type_str, "MODE2_RAW", 9) == 0)
        m_sector_offset = 24;
    else if (std::strncmp(type_str, "MODE1_RAW", 9) == 0)
        m_sector_offset = 16;
    else if (std::strncmp(type_str, "MODE1", 5) == 0)
        m_sector_offset = 0;
    else if (std::strncmp(type_str, "MODE2", 5) == 0)
        m_sector_offset = 0; // IDK
    else if (std::strncmp(type_str, "AUDIO", 5) == 0)
        m_sector_offset = 0; // IDK
}

void CHD_Reader::close()
{
    chd_close(m_file);
}

size_t CHD_Reader::read(uint8_t* buff, size_t bytes)
{
    assert(bytes);
    assert(m_virtptr + bytes <= m_size);

    const uint64_t start = m_virtptr;
    const uint64_t end = start + bytes;
    uint64_t start_hunk = start / m_header->hunkbytes;
    uint64_t end_hunk = end / m_header->hunkbytes;
    uint64_t total_read = 0;

    for (uint32_t i = (uint32_t)start_hunk; i <= end_hunk; i++)
    {
        if (i != m_current_hunk)
        {
            chd_error err = chd_read(m_file, i, m_buf.get());
            if (err != CHDERR_NONE)
            {
                fprintf(stderr, "chd: read: %s\n", chd_error_string(err));
                return total_read;
            }

            m_current_hunk = i;
        }

        const uint64_t local_ofs = start - (uint64_t)start_hunk * m_header->hunkbytes;
        uint64_t readlen = m_header->hunkbytes;

        if (i == start_hunk)
            readlen -= local_ofs;
        if (i == end_hunk)
            readlen -= m_header->hunkbytes - (end - (uint64_t)end_hunk * m_header->hunkbytes);

        assert(readlen <= bytes);

        memcpy(buff, m_buf.get() + local_ofs + m_sector_offset, readlen);
        total_read += readlen;

        //m_virtptr += readlen;
        // Need to advance the full sector here to not get misaligned
        // other cdvd containers behave inconsistently anyway regarding seek
        // by read,
        m_virtptr += m_header->unitbytes;
        buff += readlen;
    }

    return total_read;
}

void CHD_Reader::seek(size_t ofs, std::ios::seekdir whence)
{
    ofs *= m_header->unitbytes;
    if (whence == std::ios::beg)
    {
        if ((uint64_t)ofs < m_size)
            m_virtptr = (uint64_t)ofs;
    }
    else if (whence == std::ios::cur)
    {
        if (m_virtptr + ofs < m_size)
            m_virtptr = m_virtptr + ofs;
    }
    else if (whence == std::ios::end)
    {
        if (m_size - ofs < m_size)
            m_virtptr = m_size - ofs;
    }
}

bool CHD_Reader::is_open()
{
    if (m_file)
        return true;

    return false;
}

size_t CHD_Reader::get_size()
{
    return m_size;
}
