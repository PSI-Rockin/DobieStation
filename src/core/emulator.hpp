#ifndef EMULATOR_HPP
#define EMULATOR_HPP
#include "ee/bios_hle.hpp"
#include "ee/dmac.hpp"
#include "ee/emotion.hpp"

#include "iop/iop.hpp"
#include "iop/iop_dma.hpp"

#include "gs.hpp"
#include "gif.hpp"
#include "sif.hpp"

class Emulator
{
    private:
        BIOS_HLE bios_hle;
        DMAC dmac;
        EmotionEngine cpu;
        GraphicsSynthesizer gs;
        GraphicsInterface gif;
        IOP iop;
        IOP_DMA iop_dma;
        SubsystemInterface sif;

        uint8_t* RDRAM;
        uint8_t* IOP_RAM;
        uint8_t* BIOS;

        uint32_t MCH_RICM, MCH_DRD;
        uint8_t rdram_sdevid;

        uint32_t INTC_STAT;
        uint32_t INTC_MASK;
        uint32_t instructions_run;

        uint8_t IOP_POST;
        uint32_t IOP_I_STAT;
        uint32_t IOP_I_MASK;
        uint32_t IOP_I_CTRL;

        void iop_IRQ_check(uint32_t new_stat, uint32_t new_mask);
    public:
        Emulator();
        ~Emulator();
        void run();
        void reset();
        void load_BIOS(uint8_t* BIOS);
        void load_ELF(uint8_t* ELF);
        uint32_t* get_framebuffer();
        void get_resolution(int& w, int& h);

        uint8_t read8(uint32_t address);
        uint16_t read16(uint32_t address);
        uint32_t read32(uint32_t address);
        uint64_t read64(uint32_t address);
        void write8(uint32_t address, uint8_t value);
        void write16(uint32_t address, uint16_t value);
        void write32(uint32_t address, uint32_t value);
        void write64(uint32_t address, uint64_t value);

        uint8_t iop_read8(uint32_t address);
        uint16_t iop_read16(uint32_t address);
        uint32_t iop_read32(uint32_t address);
        void iop_write8(uint32_t address, uint8_t value);
        void iop_write16(uint32_t address, uint16_t value);
        void iop_write32(uint32_t address, uint32_t value);

        void iop_request_IRQ(int index);
};

#endif // EMULATOR_HPP
