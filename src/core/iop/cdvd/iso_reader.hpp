#ifndef ISO_READER_HPP
#define ISO_READER_HPP
#include "cdvd_container.hpp"

class ISO_Reader : public CDVD_Container
{
    protected:
        std::ifstream file;
    public:
        ISO_Reader();

        bool open(std::string name);
        void close();
        size_t read(uint8_t *buff, size_t bytes);
        void seek(size_t pos, std::ios::seekdir whence);

        bool is_open();
        size_t get_size();
};

#endif // ISO_READER_HPP
