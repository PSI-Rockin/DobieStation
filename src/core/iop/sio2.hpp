#ifndef SIO2_HPP
#define SIO2_HPP
#include <cstdint>

class SIO2
{
    private:

    public:
        SIO2();

        void reset();

        uint32_t get_control();
        uint32_t get_RECV1();
        void set_control(uint32_t value);
        void write_serial(uint8_t value);
};

#endif // SIO2_HPP
