#ifndef COP2_HPP
#define COP2_HPP
#include "vu.hpp"

//This is essentially a wrapper around VU0 with access to VU1
class Cop2
{
    private:
        VectorUnit *vu0, *vu1;
    public:
        Cop2(VectorUnit *vu0, VectorUnit *vu1);

        uint32_t cfc2(int index);
        void ctc2(int index, uint32_t value);
};

#endif // COP2_HPP
