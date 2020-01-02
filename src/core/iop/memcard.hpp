#ifndef MEMCARD_HPP
#define MEMCARD_HPP
#include <cstdint>
#include <string>

class Memcard
{
    private:
        uint8_t* mem;

        int cmd_length;
        uint8_t cmd;
        uint8_t terminator;

        bool file_opened;

        uint8_t response_buffer[1024];
        unsigned int response_read_pos;
        unsigned int response_write_pos;
        unsigned int response_size;

        uint8_t read_response();
        void write_response(uint8_t value);
        void response_end();
    public:
        Memcard();
        ~Memcard();

        void reset();
        bool open(std::string file_name);
        bool is_connected();

        void start_transfer();
        uint8_t write_serial(uint8_t data);
};

#endif // MEMCARD_HPP
