#ifndef COP1_HPP
#define COP1_HPP
#include <cstdint>

struct COP1_CONTROL
{
    bool condition;
};

union COP1_REG
{
    float f;
    uint32_t u;
    int32_t s;
};

class Cop1
{
    private:
        COP1_CONTROL control;
        COP1_REG gpr[32];
    public:
        Cop1();

        void reset();

        bool get_condition();

        uint32_t get_gpr(int index);
        void mtc(int index, uint32_t value);
        void ctc(int index, uint32_t value);

        void cvt_s_w(int dest, int source);
        void cvt_w_s(int dest, int source);

        void add_s(int dest, int reg1, int reg2);
        void sub_s(int dest, int reg1, int reg2);
        void mul_s(int dest, int reg1, int reg2);
        void div_s(int dest, int reg1, int reg2);
        void mov_s(int dest, int source);
        void neg_s(int dest, int source);
        void c_lt_s(int reg1, int reg2);
};

#endif // COP1_HPP
