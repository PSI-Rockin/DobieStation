#include <cstdio>
#include "firewire.hpp"
#include "iop_dma.hpp"
#include "iop_intc.hpp"

Firewire::Firewire(IOP_INTC* intc, IOP_DMA* dma) : intc(intc), dma(dma)
{

}

void Firewire::reset()
{
    intr0 = 0;
    intr1 = 0;
    intr2 = 0;
    intr0mask = 0;
    intr1mask = 0;
    intr2mask = 0;
    ctrl0 = 0;
    ctrl1 = 0;
    ctrl2 = 0;
    dmaCtrlSR0 = 0;
    PHYAccess = 0;

    for (int i = 0; i < 16; i++)
        PHYregisters[i] = 0;
}

void Firewire::readPHY()
{
    uint8_t reg = (PHYAccess >> 24) & 0xF;

    PHYAccess &= ~0x80000000; //Cancel read request
    PHYAccess |= PHYregisters[reg] | ((uint16_t)reg << 8);

    if (intr0mask & 0x40000000) //PhyRRx
    {
        intr0 |= 0x40000000;
        intc->assert_irq(24);
    }
    printf("[FW] PHY Read from reg %d value %x\n", reg, PHYAccess & 0xFF);
}

void Firewire::writePHY()
{
    uint8_t reg = (PHYAccess >> 8) & 0xF;
    uint8_t value = PHYAccess & 0xFF;

    PHYregisters[reg] = value;

    PHYAccess &= ~0x4000ffff; //Cancel write req and clear data and reg
    printf("[FW] PHY Write to reg %d value %x\n", reg, value);
}

void Firewire::write32(uint32_t addr, uint32_t value)
{
    uint32_t address = addr & 0x1ff;
    
    switch (address)
    {
        case 0x8:
            ctrl0 = value;
            ctrl0 &= ~0x3800000; //Tx, Rx, BusID Reset bits
            printf("[FW] Write32 to Ctrl0 value %x\n", value);
            break;
        case 0x10:
            if(value & 0x2) //Power On
                ctrl2 |= 0x8; //SCLK OK
            printf("[FW] Write32 to Ctrl2 value %x\n", value);
            break;
        case 0x14:
            PHYAccess = value;
            printf("[FW] Write32 to PSYAccess value %x\n", value);
            if (PHYAccess & 0x40000000) //Write Req
            {
                writePHY();
            }
            else if (PHYAccess & 0x80000000) //Read Req
            {
                readPHY();
            }
            break;
        case 0x20:
            intr0 &= ~value;
            printf("[FW] Write32 to Intr0 value %x\n", value);
            break;
        case 0x24:
            intr0mask = value;
            printf("[FW] Write32 to Intr0 Mask value %x\n", value);
            break;
        case 0x28:
            intr1 &= ~value;
            printf("[FW] Write32 to Intr1 value %x\n", value);
            break;
        case 0x2C:
            intr1mask = value;
            printf("[FW] Write32 to Intr1 Mask value %x\n", value);
            break;
        case 0x30:
            intr2 &= ~value;
            printf("[FW] Write32 to Intr2 value %x\n", value);
            break;
        case 0x34:
            intr2mask = value;
            printf("[FW] Write32 to Intr2 Mask value %x\n", value);
            break;
        case 0x7C:
            printf("[FW] Write32 to Unknown Reg 7C value %x\n", value);
            break;
        case 0xB8:
            dmaCtrlSR0 = value;
            printf("[FW] Write32 to DMA Control 0 value %x\n", value);
            break;
        case 0x138:
            dmaCtrlSR1 = value;
            printf("[FW] Write32 to DMA Control 1 value %x\n", value);
            break;
        default:
            printf("[FW] Unrecognized Write32 to %x value %x\n", addr, value);
            break;
    }
}

uint32_t Firewire::read32(uint32_t addr)
{
    uint32_t address = addr & 0x1ff;
    uint32_t reg = 0;

    switch (address)
    {
        case 0x0:
            reg = 0xffc00001;
            printf("[FW] Read32 from NoteID value %x\n", reg);
            break;
        case 0x8:
            reg = ctrl0;
            printf("[FW] Read32 from Ctrl0 value %x\n", reg);
            break;
        case 0x10:
            reg = ctrl2;
            printf("[FW] Read32 from Ctrl2 value %x\n", reg);
            break;
        case 0x14:
            reg = PHYAccess;
            printf("[FW] Read32 from PSYAccess value %x\n", reg);
            break;
        case 0x20:
            reg = intr0;
            printf("[FW] Read32 from Intr0 value %x\n", reg);
            break;
        case 0x24:
            reg = intr0mask;
            printf("[FW] Read32 from Intr0 Mask value %x\n", reg);
            break;
        case 0x28:
            reg = intr1;
            printf("[FW] Read32 from Intr1 value %x\n", reg);
            break;
        case 0x2C:
            reg = intr1mask;
            printf("[FW] Read32 from Intr1 Mask value %x\n", reg);
            break;
        case 0x30:
            reg = intr2;
            printf("[FW] Read32 from Intr2 value %x\n", reg);
            break;
        case 0x34:
            reg = intr2mask;
            printf("[FW] Read32 from Intr2 Mask value %x\n", reg);
            break;
        case 0x7C:
            reg = 0x10000001; //Value related to NodeID somehow
            printf("[FW] Read32 from Unknown Reg 7C value %x\n", reg);
            break;
        default:
            printf("[FW] Unrecognized Read32 from %x value %x\n", addr, reg);
            break;
    }

    return reg;
}