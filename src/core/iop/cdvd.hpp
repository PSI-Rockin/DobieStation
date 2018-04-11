#ifndef CDVD_HPP
#define CDVD_HPP
#include <fstream>

class Emulator;

enum CDVD_STATUS
{
    READING = 0x06,
    PAUSED = 0x0A
};

class CDVD_Drive
{
    private:
        Emulator* e;
        std::ifstream cdvd_file;
        int read_bytes_left;

        uint8_t pvd_sector[2048];
        uint16_t LBA;
        uint32_t root_location;
        uint32_t root_len;

        uint32_t sector_pos;
        uint32_t sectors_left;
        uint32_t block_size;

        uint8_t read_buffer[4096];

        uint8_t ISTAT;

        CDVD_STATUS drive_status;

        bool N_active;
        uint8_t N_command;
        uint8_t N_command_params[11];
        uint8_t N_params;
        uint8_t N_status;

        uint8_t S_command;
        uint8_t S_command_params[16];
        uint8_t S_outdata[16];
        uint8_t S_params;
        uint8_t S_out_params;
        uint8_t S_status;

        void prepare_S_outdata(int amount);

        void read_CD_sector();
        void read_DVD_sector();

        void N_command_read();
        void N_command_dvdread();
        void N_command_gettoc();
        void S_command_sub(uint8_t func);
    public:
        CDVD_Drive(Emulator* e);
        ~CDVD_Drive();

        void reset();
        void N_command_check();
        int bytes_left();

        uint32_t read_to_RAM(uint8_t* RAM, uint32_t bytes);
        uint8_t* read_file(std::string name, uint32_t& file_size);
        bool load_disc(const char* name);

        uint8_t read_drive_status();
        uint8_t read_N_command();
        uint8_t read_disc_type();
        uint8_t read_N_status();
        uint8_t read_S_status();
        uint8_t read_S_command();
        uint8_t read_S_data();
        uint8_t read_ISTAT();

        void send_N_command(uint8_t value);
        void write_N_data(uint8_t value);
        void write_BREAK();
        void send_S_command(uint8_t value);
        void write_S_data(uint8_t value);
        void write_ISTAT(uint8_t value);
};

#endif // CDVD_HPP
