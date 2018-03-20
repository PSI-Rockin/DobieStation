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
    bool int_enable;
    bool exception;
    bool error;
    uint8_t mode;
    bool bus_error;
    bool int0_mask;
    bool int1_mask;
    bool timer_int_mask;
    bool master_int_enable;
    bool edi;
    bool ch;
    bool bev;
    bool dev;
    uint8_t cu;
};

struct COP0_CAUSE
{
    uint8_t code;
    bool int0_pending;
    bool int1_pending;
    bool timer_int_pending;
    uint8_t code2;
    uint8_t ce;
    bool bd2;
    bool bd;
};

class Cop0
{
    public:
        uint32_t gpr[32];
        COP0_STATUS status;
        COP0_CAUSE cause;
        Cop0();

        void reset();

        uint32_t mfc(int index);
        void mtc(int index, uint32_t value);

        bool int1_raised();
        bool int_enabled();
        bool int_pending();

        void count_up();
};

#endif // COP0_HPP
