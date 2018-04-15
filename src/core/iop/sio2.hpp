#ifndef SIO2_HPP
#define SIO2_HPP
#include <cstdint>

class Emulator;

class SIO2
{
    private:
        Emulator* e;
    public:
        SIO2(Emulator* e);

        void reset();

        uint32_t get_control();
        uint32_t get_RECV1();
        void set_control(uint32_t value);
        void write_serial(uint8_t value);
};

#endif // SIO2_HPP
