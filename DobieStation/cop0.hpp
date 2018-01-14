#ifndef COP0_HPP
#define COP0_HPP
#include <cstdint>

class Cop0
{
    private:
        uint32_t gpr[32];
    public:
        Cop0();

        uint32_t mfc(int index);
        void mtc(int index, uint32_t value);

        void count_up();
};

#endif // COP0_HPP
