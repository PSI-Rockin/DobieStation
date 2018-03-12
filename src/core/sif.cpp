#include <cstdio>
#include "sif.hpp"

/**
 * TODO: What are the sizes of the SIF0/SIF1 DMAC FIFOs?
 */

SubsystemInterface::SubsystemInterface()
{

}

void SubsystemInterface::reset()
{
    std::queue<uint32_t> empty;
    SIF1_FIFO.swap(empty);
    mscom = 0;
    smcom = 0;
    msflag = 0;
    smflag = 0;
}

int SubsystemInterface::get_SIF1_size()
{
    return SIF1_FIFO.size();
}

void SubsystemInterface::write_SIF1(uint64_t *quad)
{
    SIF1_FIFO.push(quad[0] & 0xFFFFFFFF);
    SIF1_FIFO.push(quad[0] >> 32);
    SIF1_FIFO.push(quad[1] & 0xFFFFFFFF);
    SIF1_FIFO.push(quad[1] >> 32);
    printf("[SIF1] Quad: $%08X_%08X_%08X_%08X\n", quad[1] >> 32, quad[1], quad[0] >> 32, quad[0]);
}

uint32_t SubsystemInterface::read_SIF1()
{
    uint32_t value = SIF1_FIFO.front();
    SIF1_FIFO.pop();
    return value;
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
