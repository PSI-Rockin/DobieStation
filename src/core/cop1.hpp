#ifndef COP1_HPP
#define COP1_HPP
#include <cstdint>

struct COP1_CONTROL
{
    bool condition;
};

class Cop1
{
    private:
        COP1_CONTROL control;
        uint32_t gpr[32];
    public:
        Cop1();

        void reset();

        bool get_condition();

        uint32_t get_gpr(int index);
        void mtc(int index, uint32_t value);
        void ctc(int index, uint32_t value);

        void cvt_s_w(int dest, int source);
};

#endif // COP1_HPP
