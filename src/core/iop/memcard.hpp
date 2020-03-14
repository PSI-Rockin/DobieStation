#ifndef MEMCARD_HPP
#define MEMCARD_HPP
#include <cstdint>
#include <string>

class Memcard
{
    private:
        uint8_t* mem;

        int cmd_length;
        int cmd_params;
        uint8_t cmd;
        uint8_t terminator;

        std::string file_name;
        bool file_opened;
        bool is_dirty;

        uint8_t response_buffer[1024];
        unsigned int response_read_pos;
        unsigned int response_write_pos;
        unsigned int response_size;

        uint32_t mem_addr;

        uint8_t mem_write_size;

        bool auth_f0_do_xor;
        uint8_t auth_f0_checksum;

        uint8_t read_response();
        void write_response(uint8_t value);
        void response_end();

        void do_auth_f0(uint8_t value);
        uint8_t do_checksum(uint8_t* buff, unsigned int size);

        void sector_op(uint8_t value);
        void read_mem(uint32_t addr, uint8_t size);
        void write_mem(uint8_t data);
    public:
        Memcard();
        ~Memcard();

        void reset();
        bool open(std::string file_name);
        bool is_connected();
        void save_if_dirty();

        void start_transfer();
        uint8_t write_serial(uint8_t data);
};

#endif // MEMCARD_HPP
