#ifndef COP0_HPP
#define COP0_HPP
#include <cstdint>

enum COP0_REG
{
    STATUS = 12,
    CAUSE = 13
};

struct COP0_STATUS
{

};

struct COP0_CAUSE
{

};

class Cop0
{
    private:
        uint32_t gpr[32];
    public:
        Cop0();

        void reset();

        uint32_t mfc(int index);
        void mtc(int index, uint32_t value);

        bool int_enabled();
        void set_EIE(bool IE);

        void set_exception_cause(uint8_t cause);

        void count_up();
};

#endif // COP0_HPP
