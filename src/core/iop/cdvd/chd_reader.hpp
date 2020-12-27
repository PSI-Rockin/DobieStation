#ifndef __CHD_H_
#define __CHD_H_

#include <memory>
#include <libchdr/chd.h>
#include "cdvd_container.hpp"

class CHD_Reader : public CDVD_Container
{
    private:
        chd_file *m_file {nullptr};
        const chd_header *m_header {nullptr};


        uint64_t m_virtptr {0};
        size_t m_size {0};
        uint32_t m_current_hunk {0};
        std::unique_ptr<uint8_t[]> m_buf;

    public:
        bool open(std::string name);
        void close();
        size_t read(uint8_t* buff, size_t bytes);
        void seek(size_t pos, std::ios::seekdir whence);

        bool is_open();
        size_t get_size();
};



#endif // __CHD_H_
