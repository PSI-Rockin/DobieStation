#ifndef CDVD_HPP
#define CDVD_HPP

#include <fstream>
#include <memory>
#include "cdvd_container.hpp"

class IOP_INTC;
class IOP_DMA;
class Scheduler;

enum CDVD_CONTAINER
{
    ISO,
    CISO,
    BIN_CUE
};

enum CDVD_STATUS
{
    STOPPED = 0x00,
    SPINNING = 0x02,
    READING = 0x06,
    PAUSED = 0x0A,
    SEEKING = 0x12
};

enum class NCOMMAND
{
    NONE,
    SEEK,
    STANDBY,
    STOP,
    READ_SEEK,
    READ,
    BREAK
};

enum CDVD_DISC_TYPE
{
    CDVD_DISC_NONE = 0,
    CDVD_DISC_DETECTING = 1,
    CDVD_DISC_DETECTING_CD = 2,
    CDVD_DISC_DETECTING_DVD = 3,
    CDVD_DISC_DETECTING_DUAL_DVD = 4,
    CDVD_DISC_UNK = 5,
    CDVD_DISC_PSCD = 0x10,
    CDVD_DISC_PSCDDA = 0x11,
    CDVD_DISC_PS2CD = 0x12,
    CDVD_DISC_PS2CDDA = 0x13,
    CDVD_DISC_PS2DVD = 0x14,
    CDVD_DISC_CDDA = 0xFD,
    CDVD_DISC_DVDV = 0xFE,
    CDVD_DISC_ILL = 0xFF
};

struct RTC
{
    int vsyncs;
    int second;
    int minute;
    int hour;
    int day;
    int month;
    int year;
};

class CDVD_Drive
{
    private:
        uint64_t last_read;
        uint64_t cycle_count;
        IOP_INTC* intc;
        IOP_DMA* dma;
        CDVD_DISC_TYPE disc_type;
        std::unique_ptr<CDVD_Container> container;
        size_t file_size;
        Scheduler* scheduler;
        int read_bytes_left;
        int speed;

        uint8_t pvd_sector[2048];
        uint16_t LBA;
        uint64_t root_location;
        uint64_t root_len;

        uint64_t current_sector;
        uint64_t sector_pos;
        uint64_t sectors_left;
        uint64_t block_size;

        uint8_t read_buffer[4096];

        uint8_t ISTAT;

        uint8_t drive_status;

        bool is_spinning;
        NCOMMAND active_N_command;
        uint8_t N_command;
        uint8_t N_command_params[11];
        uint8_t N_params;
        uint8_t N_status;
        RTC rtc;

        uint8_t S_command;
        uint8_t S_command_params[16];
        uint8_t S_outdata[16];
        uint8_t S_params;
        uint8_t S_out_params;
        uint8_t S_status;

        uint8_t cdkey[16];

        int n_command_event_id;

        uint32_t get_block_timing(bool mode_DVD);

        void start_seek();
        void prepare_S_outdata(int amount);

        void read_CD_sector();
        void fill_CDROM_sector();
        void read_DVD_sector();
        void get_dual_layer_info(bool& dual_layer, uint64_t& sector);

        void N_command_read();
        void N_command_dvdread();
        void N_command_gettoc();
        void N_command_readkey(uint32_t arg);
        void S_command_sub(uint8_t func);
        void add_event(uint64_t cycles);
    public:
        CDVD_Drive(IOP_INTC* intc, IOP_DMA* dma, Scheduler* scheduler);
        ~CDVD_Drive();

        std::string get_ps2_exec_path();
        std::string get_serial();

        void reset();
        void vsync();
        void handle_N_command();
        int get_block_size();
        int bytes_left();

        uint32_t read_to_RAM(uint8_t* RAM, uint32_t bytes);
        uint8_t* read_file(std::string name, uint32_t& file_size);
        bool load_disc(const char* name, CDVD_CONTAINER container);

        uint8_t read_drive_status();
        uint8_t read_N_command();
        uint8_t read_disc_type();
        uint8_t read_N_status();
        uint8_t read_S_status();
        uint8_t read_S_command();
        uint8_t read_S_data();
        uint8_t read_ISTAT();
        uint8_t read_cdkey(int index);

        void send_N_command(uint8_t value);
        void write_N_data(uint8_t value);
        void write_BREAK();
        void send_S_command(uint8_t value);
        void write_S_data(uint8_t value);
        void write_ISTAT(uint8_t value);

        void load_state(std::ifstream& state);
        void save_state(std::ofstream& state);
};

#endif // CDVD_HPP
