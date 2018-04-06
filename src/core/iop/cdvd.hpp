#ifndef CDVD_HPP
#define CDVD_HPP
#include <fstream>

class CDVD_Drive
{
    private:
        std::ifstream cdvd_file;

        uint8_t pvd_sector[2048];
        uint16_t LBA;
        uint32_t root_location;
        uint32_t root_len;
    public:
        CDVD_Drive();
        ~CDVD_Drive();

        uint8_t* read_file(std::string name, uint32_t& file_size);

        bool load_disc(const char* name);
};

#endif // CDVD_HPP
