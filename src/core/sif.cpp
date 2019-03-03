#include <cstdio>
#include "sif.hpp"
#include "logger.hpp"

#include "iop/iop_dma.hpp"

SubsystemInterface::SubsystemInterface(IOP_DMA* iop_dma) : iop_dma(iop_dma)
{

}

void SubsystemInterface::reset()
{
    std::queue<uint32_t> empty, empty2;
    SIF0_FIFO.swap(empty);
    SIF1_FIFO.swap(empty2);
    mscom = 0;
    smcom = 0;
    msflag = 0;
    smflag = 0;
    control = 0;

    iop_dma->set_DMA_request(IOP_SIF0);
    iop_dma->clear_DMA_request(IOP_SIF1);
}

void SubsystemInterface::write_SIF0(uint32_t word)
{
    SIF0_FIFO.push(word);
    if (SIF0_FIFO.size() >= MAX_FIFO_SIZE)
        iop_dma->clear_DMA_request(IOP_SIF0);
}

void SubsystemInterface::write_SIF1(uint128_t quad)
{
    //ds_log->sif->debug("Write SIF1: ${:08X}_%{:08X}_{:08X}_{:08X}\n", quad._u32[3], quad._u32[2], quad._u32[1], quad._u32[0]);
    for (int i = 0; i < 4; i++)
        SIF1_FIFO.push(quad._u32[i]);
    iop_dma->set_DMA_request(IOP_SIF1);
}

uint32_t SubsystemInterface::read_SIF0()
{
    uint32_t value = SIF0_FIFO.front();
    //ds_log->sif->debug("Read SIF0: ${:08X}\n", value);
    SIF0_FIFO.pop();
    iop_dma->set_DMA_request(IOP_SIF0);
    return value;
}

uint32_t SubsystemInterface::read_SIF1()
{
    uint32_t value = SIF1_FIFO.front();
    SIF1_FIFO.pop();
    if (!SIF1_FIFO.size())
        iop_dma->clear_DMA_request(IOP_SIF1);
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

uint32_t SubsystemInterface::get_control()
{
    return control;
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
    msflag |= value;
}

void SubsystemInterface::reset_msflag(uint32_t value)
{
    msflag &= ~value;
}

void SubsystemInterface::set_smflag(uint32_t value)
{
    smflag |= value;
}

void SubsystemInterface::reset_smflag(uint32_t value)
{
    smflag &= ~value;
}

void SubsystemInterface::set_control_EE(uint32_t value)
{
    if (!(value & 0x100))
        control &= ~0x100;
    else
        control |= 0x100;
}

void SubsystemInterface::set_control_IOP(uint32_t value)
{
    uint8_t bark = value & 0xF0;

    if (value & 0xA0)
    {
        control &= ~0xF000;
        control |= 0x2000;
    }

    if (control & bark)
        control &= ~bark;
    else
        control |= bark;
}
