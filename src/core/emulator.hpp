#ifndef EMULATOR_HPP
#define EMULATOR_HPP
#include <fstream>

#include "ee/dmac.hpp"
#include "ee/emotion.hpp"
#include "ee/intc.hpp"
#include "ee/ipu/ipu.hpp"
#include "ee/timers.hpp"
#include "ee/vif.hpp"
#include "ee/vu.hpp"

#include "iop/cdvd.hpp"
#include "iop/gamepad.hpp"
#include "iop/iop.hpp"
#include "iop/iop_dma.hpp"
#include "iop/iop_timers.hpp"
#include "iop/sio2.hpp"
#include "iop/spu.hpp"

#include "int128.hpp"
#include "gs.hpp"
#include "gif.hpp"
#include "sif.hpp"

enum SKIP_HACK
{
    NONE,
    LOAD_ELF,
    LOAD_DISC
};

class Emulator
{
    private:
        std::atomic_bool save_requested, load_requested, gsdump_requested, gsdump_single_frame, gsdump_running;
        std::string savestate_path;
        int frames;
        Cop0 cp0;
        Cop1 fpu;
        CDVD_Drive cdvd;
        DMAC dmac;
        EmotionEngine cpu;
        EmotionTiming timers;
        Gamepad pad;
        GraphicsSynthesizer gs;
        GraphicsInterface gif;
        IOP iop;
        IOP_DMA iop_dma;
        IOPTiming iop_timers;
        INTC intc;
        ImageProcessingUnit ipu;
        SIO2 sio2;
        SPU spu, spu2;
        SubsystemInterface sif;
        VectorInterface vif0, vif1;
        VectorUnit vu0, vu1;

        int INTC_read_count;
        bool VBLANK_sent;

        std::ofstream ee_log;
        std::string ee_stdout;

        uint8_t* RDRAM;
        uint8_t* IOP_RAM;
        uint8_t* BIOS;
        uint8_t* SPU_RAM;

        uint8_t scratchpad[1024 * 16];

        uint32_t MCH_RICM, MCH_DRD;
        uint8_t rdram_sdevid;

        uint32_t instructions_run;

        uint8_t IOP_POST;
        uint32_t IOP_I_STAT;
        uint32_t IOP_I_MASK;
        uint32_t IOP_I_CTRL;
        int iop_i_ctrl_delay;

        SKIP_HACK skip_BIOS_hack;

        uint8_t* ELF_file;
        uint32_t ELF_size;

        void iop_IRQ_check(uint32_t new_stat, uint32_t new_mask);
    public:
        Emulator();
        ~Emulator();
        void run();
        void reset();
        void press_button(PAD_BUTTON button);
        void release_button(PAD_BUTTON button);
        bool skip_BIOS();
        void fast_boot();
        void set_skip_BIOS_hack(SKIP_HACK type);
        void load_BIOS(uint8_t* BIOS);
        void load_ELF(uint8_t* ELF, uint32_t size);
        bool load_CDVD(const char* name);
        void execute_ELF();
        uint32_t* get_framebuffer();
        void get_resolution(int& w, int& h);
        void get_inner_resolution(int& w, int& h);

        bool request_load_state(const char* file_name);
        bool request_save_state(const char* file_name);
        void request_gsdump_toggle();
        void request_gsdump_single_frame();
        void load_state(const char* file_name);
        void save_state(const char* file_name);

        uint8_t read8(uint32_t address);
        uint16_t read16(uint32_t address);
        uint32_t read32(uint32_t address);
        uint64_t read64(uint32_t address);
        uint128_t read128(uint32_t address);
        void write8(uint32_t address, uint8_t value);
        void write16(uint32_t address, uint16_t value);
        void write32(uint32_t address, uint32_t value);
        void write64(uint32_t address, uint64_t value);
        void write128(uint32_t address, uint128_t value);

        void ee_kputs(uint32_t param);
        void ee_deci2send(uint32_t addr, int len);

        uint8_t iop_read8(uint32_t address);
        uint16_t iop_read16(uint32_t address);
        uint32_t iop_read32(uint32_t address);
        void iop_write8(uint32_t address, uint8_t value);
        void iop_write16(uint32_t address, uint16_t value);
        void iop_write32(uint32_t address, uint32_t value);

        void iop_request_IRQ(int index);
        void iop_ksprintf();
        void iop_puts();

        void test_iop();
        GraphicsSynthesizer& get_gs();//used for gs dumps
};

#endif // EMULATOR_HPP
