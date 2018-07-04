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
        COP1_REG accumulator;

        float convert(uint32_t value);
    public:
        Cop1();

        void reset();

        bool get_condition();

        uint32_t get_gpr(int index);
        void mtc(int index, uint32_t value);
        uint32_t cfc(int index);
        void ctc(int index, uint32_t value);

        void cvt_s_w(int dest, int source);
        void cvt_w_s(int dest, int source);

        void add_s(int dest, int reg1, int reg2);
        void sub_s(int dest, int reg1, int reg2);
        void mul_s(int dest, int reg1, int reg2);
        void div_s(int dest, int reg1, int reg2);
        void sqrt_s(int dest, int source);
        void abs_s(int dest, int source);
        void mov_s(int dest, int source);
        void neg_s(int dest, int source);
        void adda_s(int reg1, int reg2);
        void suba_s(int reg1, int reg2);
        void mula_s(int reg1, int reg2);
        void madd_s(int dest, int reg1, int reg2);
        void msub_s(int dest, int reg1, int reg2);
        void madda_s(int reg1, int reg2);
        void max_s(int dest, int reg1, int reg2);
        void min_s(int dest, int reg1, int reg2);
        void c_f_s();
        void c_lt_s(int reg1, int reg2);
        void c_eq_s(int reg1, int reg2);
        void c_le_s(int reg1, int reg2);
};

#endif // COP1_HPP
