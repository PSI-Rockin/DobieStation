#include <cstdint>
#include "cop2.hpp"

Cop2::Cop2(VectorUnit *vu0, VectorUnit *vu1) : vu0(vu0), vu1(vu1)
{

}

uint32_t Cop2::cfc2(int index)
{
    if (index < 16)
        return vu0->get_int(index);

    switch (index)
    {
        default:
            return 0;
    }
}

void Cop2::ctc2(int index, uint32_t value)
{
    if (index < 16)
    {
        vu0->set_int(index, value);
        return;
    }

    switch (index)
    {
        case 20:
            vu0->set_R(value);
            break;
        case 21:
            vu0->set_I(value);
            break;
        case 22:
            vu0->set_Q(value);
            break;
        default:
            printf("[COP2] Unrecognized ctc2 of $%08X to reg %d\n", value, index);
    }
}
