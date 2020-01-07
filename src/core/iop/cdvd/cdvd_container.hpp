#ifndef CDVD_CONTAINER_HPP
#define CDVD_CONTAINER_HPP
#include <fstream>

class CDVD_Container
{
    public:
        virtual bool open(std::string name) = 0;
        virtual void close() = 0;
        virtual size_t read(uint8_t* buff, size_t bytes) = 0;
        virtual void seek(size_t pos, std::ios::seekdir whence) = 0;

        virtual bool is_open() = 0;
        virtual size_t get_size() = 0;
};

#endif // CDVD_CONTAINER_HPP
