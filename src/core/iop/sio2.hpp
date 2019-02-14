#ifndef SIO2_HPP
#define SIO2_HPP
#include <cstdint>
#include <fstream>
#include <queue>

enum class SIO_DEVICE
{
    NONE,
    PAD,
    MEMCARD,
    DUMMY
};

class Emulator;
class Gamepad;
class Memcard;

class SIO2
{
    private:
        Emulator* e;
        Gamepad* pad;
        Memcard* memcard;

        uint32_t send1[4];
        uint32_t send2[4];
        uint32_t send3[16];

        uint32_t RECV1;
        uint32_t RECV3;

        int port;

        std::queue<uint8_t> FIFO;
        uint32_t control;

        bool new_command;
        SIO_DEVICE active_command;
        int command_length;
        int send3_port;

        void write_device(uint8_t value);
    public:
        SIO2(Emulator* e, Gamepad* pad, Memcard* memcard);

        void reset();

        uint8_t read_serial();
        uint32_t get_control();
        uint32_t get_RECV1();
        uint32_t get_RECV2();
        uint32_t get_RECV3();

        void set_send1(int index, uint32_t value);
        void set_send2(int index, uint32_t value);
        void set_send3(int index, uint32_t value);

        void set_control(uint32_t value);
        void write_serial(uint8_t value);
};

#endif // SIO2_HPP
