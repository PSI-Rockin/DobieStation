#ifndef __CHD_H_
#define __CHD_H_

#include <memory>
#include <libchdr/chd.h>
#include "cdvd_container.hpp"

class CHD_Reader : public CDVD_Container
{
    public:
        bool open(std::string name);
        void close();
        size_t read(uint8_t* buff, size_t bytes);
        void seek(size_t pos, std::ios::seekdir whence);

        bool is_open();
        size_t get_size();

    private:
        chd_file *m_file {nullptr};
        const chd_header *m_header {nullptr};

        uint32_t m_sector_offset {0};
        uint64_t m_virtptr {0};
        size_t m_size {0};
        uint32_t m_current_hunk {0};
        std::unique_ptr<uint8_t[]> m_buf;

        void find_offset();
};



#endif // __CHD_H_
