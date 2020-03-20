#ifndef BINCUEREADER_HPP
#define BINCUEREADER_HPP
#include "cdvd_container.hpp"

class BinCueReader : public CDVD_Container
{
    protected:
        std::ifstream bin_file, cue_file;
    public:
        BinCueReader();

        bool open(std::string name);
        void close();
        size_t read(uint8_t *buff, size_t bytes);
        void seek(size_t pos, std::ios::seekdir whence);

        bool is_open();
        size_t get_size();
};

#endif // BINCUEREADER_HPP
