#include "sif.hpp"

SubsystemInterface::SubsystemInterface()
{

}

void SubsystemInterface::reset()
{
    msflag = 0;
    smflag = 0;
}

uint32_t SubsystemInterface::get_msflag()
{
    return msflag;
}

uint32_t SubsystemInterface::get_smflag()
{
    return smflag;
}

void SubsystemInterface::set_msflag(uint32_t value)
{
    msflag = value;
}

void SubsystemInterface::set_smflag(uint32_t value)
{
    smflag = value;
}
