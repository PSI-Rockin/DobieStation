#ifndef COP1_HPP
#define COP1_HPP
#include <cstdint>

class Cop1
{
    private:
        uint32_t gpr[32];
    public:
        Cop1();

        uint32_t get_gpr(int index);
        void mtc(int index, uint32_t value);

        void cvt_s_w(int dest, int source);
};

#endif // COP1_HPP
