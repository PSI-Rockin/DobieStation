#ifndef SPU_HPP
#define SPU_HPP
#include <cstdint>

struct Voice
{
    uint16_t left_vol, right_vol;

};

class SPU
{
    private:
        uint16_t stat;
    public:
        SPU();

        void reset();

        uint16_t get_stat();

        void set_int_flag();
};

#endif // SPU_HPP
