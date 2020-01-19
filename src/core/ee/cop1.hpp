#ifndef COP1_HPP
#define COP1_HPP
#include <cstdint>
#include <fstream>

class EE_JIT64;
class EmotionEngine;

struct COP1_CONTROL
{
    bool su;
    bool so;
    bool sd;
    bool si;
    bool u;
    bool o;
    bool d;
    bool i;
    bool condition;
};

extern "C" uint8_t* exec_block_ee(EE_JIT64& jit, EmotionEngine& ee);

extern "C" uint8_t* exec_block_ee(EE_JIT64& jit, EmotionEngine& ee);

class Cop1
{
    private:
        COP1_CONTROL control;
        float gpr[32];
        float accumulator;

        float convert(float value) const noexcept;
        void check_overflow(float& dest, bool set_flags) noexcept;
        void check_underflow(float& dest, bool set_flags) noexcept;
    public:
        Cop1();

        void reset() noexcept;

        bool get_condition() const noexcept;

        uint32_t get_gpr(int index) const noexcept;
        void mtc(int index, uint32_t value) noexcept;
        uint32_t cfc(int index) noexcept;
        void ctc(int index, uint32_t value) noexcept;

        void cvt_s_w(int dest, int source) noexcept;
        void cvt_w_s(int dest, int source) noexcept;

        void add_s(int dest, int reg1, int reg2) noexcept;
        void sub_s(int dest, int reg1, int reg2) noexcept;
        void mul_s(int dest, int reg1, int reg2) noexcept;
        void div_s(int dest, int reg1, int reg2) noexcept;
        void sqrt_s(int dest, int source) noexcept;
        void abs_s(int dest, int source) noexcept;
        void mov_s(int dest, int source) noexcept;
        void neg_s(int dest, int source) noexcept;
        void rsqrt_s(int dest, int reg1, int reg2) noexcept;
        void adda_s(int reg1, int reg2) noexcept;
        void suba_s(int reg1, int reg2) noexcept;
        void mula_s(int reg1, int reg2) noexcept;
        void madd_s(int dest, int reg1, int reg2) noexcept;
        void msub_s(int dest, int reg1, int reg2) noexcept;
        void madda_s(int reg1, int reg2) noexcept;
        void msuba_s(int reg1, int reg2) noexcept;
        void max_s(int dest, int reg1, int reg2) noexcept;
        void min_s(int dest, int reg1, int reg2) noexcept;
        void c_f_s() noexcept;
        void c_lt_s(int reg1, int reg2) noexcept;
        void c_eq_s(int reg1, int reg2) noexcept;
        void c_le_s(int reg1, int reg2) noexcept;

        void load_state(std::ifstream& state);
        void save_state(std::ofstream& state);

        //Friends needed for JIT convenience
        friend class EE_JIT64;
        friend class EE_JitTranslator;

        friend uint8_t* exec_block_ee(EE_JIT64& jit, EmotionEngine& ee);
};

#endif // COP1_HPP
