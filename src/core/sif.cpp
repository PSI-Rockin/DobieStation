#include "sif.hpp"

SubsystemInterface::SubsystemInterface()
{

}

void SubsystemInterface::reset()
{
    mscom = 0;
    smcom = 0;
    msflag = 0;
    smflag = 0;
}

uint32_t SubsystemInterface::get_mscom()
{
    return mscom;
}

uint32_t SubsystemInterface::get_smcom()
{
    return smcom;
}

uint32_t SubsystemInterface::get_msflag()
{
    return msflag;
}

uint32_t SubsystemInterface::get_smflag()
{
    return smflag;
}

void SubsystemInterface::set_mscom(uint32_t value)
{
    mscom = value;
}

void SubsystemInterface::set_smcom(uint32_t value)
{
    smcom = value;
}

void SubsystemInterface::set_msflag(uint32_t value)
{
    msflag = value;
}

void SubsystemInterface::set_smflag(uint32_t value)
{
    smflag = value;
}
