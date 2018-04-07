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

        uint8_t N_status;
        uint8_t S_status;
    public:
        CDVD_Drive();
        ~CDVD_Drive();

        void reset();

        uint8_t* read_file(std::string name, uint32_t& file_size);
        bool load_disc(const char* name);

        uint8_t read_disc_type();
        uint8_t read_N_status();
        uint8_t read_S_status();

        void send_S_command(uint8_t value);
};

#endif // CDVD_HPP
