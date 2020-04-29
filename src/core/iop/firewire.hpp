#ifndef FIREWIRE_HPP
#define FIREWIRE_HPP
#include <cstdint>
#include <fstream>

class IOP_INTC;
class IOP_DMA; //Will be needed later for Firewire DMA's

class Firewire
{
    private:
        IOP_INTC* intc;
        IOP_DMA* dma;

        uint32_t intr0, intr1, intr2;
        uint32_t intr0mask, intr1mask, intr2mask;
        uint32_t ctrl0, ctrl1, ctrl2;
        uint32_t dmaCtrlSR0, dmaCtrlSR1;
        uint32_t PHYAccess;
        uint8_t  PHYregisters[16];

        void readPHY();
        void writePHY();

    public:
        Firewire(IOP_INTC* intc, IOP_DMA* dma);

        void reset();
        void write32(uint32_t addr, uint32_t value);
        uint32_t read32(uint32_t addr);
        /*
        void load_state(std::ifstream& state);
        void save_state(std::ofstream& state);
        */
};

#endif // Firewire_HPP
