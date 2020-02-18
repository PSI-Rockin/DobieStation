#ifndef DMAC_HPP
#define DMAC_HPP
#include <cstdint>
#include <fstream>
#include <list>

#include "../int128.hpp"

enum DMAC_CHANNELS
{
    VIF0,
    VIF1,
    GIF,
    IPU_FROM,
    IPU_TO,
    EE_SIF0,
    EE_SIF1,
    EE_SIF2,
    SPR_FROM,
    SPR_TO,
    DMA_STALL = 13,
    MFIFO_EMPTY = 14
};

class DMAC;

struct DMA_Channel
{
    uint32_t control;
    uint32_t address;
    uint32_t quadword_count;
    uint32_t tag_address;
    uint32_t tag_save0, tag_save1;
    uint32_t scratchpad_address;

    bool tag_end;
    bool paused;
    uint8_t interleaved_qwc;
    uint8_t tag_id;

    typedef int(DMAC::*dma_copy_func)();
    dma_copy_func func;

    bool started;
    bool can_stall_drain;
    bool has_dma_stalled;
    bool dma_req;
    bool is_spr;

    int index;
};

//Regs
struct D_CTRL
{
    bool master_enable;
    bool cycle_stealing;
    uint8_t mem_drain_channel;
    uint8_t stall_source_channel;
    uint8_t stall_dest_channel;
    uint8_t release_cycle;
};

struct D_STAT
{
    bool channel_stat[15];
    bool channel_mask[15];
};

struct D_SQWC
{
    uint8_t skip_qwc;
    uint8_t transfer_qwc;
};

class EmotionEngine;
class Emulator;
class GraphicsInterface;
class ImageProcessingUnit;
class SubsystemInterface;
class VectorInterface;
class VectorUnit;

class DMAC
{
    private:
        uint8_t* RDRAM, *scratchpad;
        EmotionEngine* cpu;
        Emulator* e;
        GraphicsInterface* gif;
        ImageProcessingUnit* ipu;
        SubsystemInterface* sif;
        VectorInterface* vif0, *vif1;
        VectorUnit* vu0, *vu1;
        DMA_Channel channels[15];

        DMA_Channel* active_channel;
        DMA_Channel* queued_VIF0; //TODO: VIF0 has a higher priority, so it needs its own slot
        std::list<DMA_Channel*> queued_channels;

        D_CTRL control;
        D_STAT interrupt_stat;
        uint32_t PCR;
        D_SQWC SQWC;
        uint32_t RBOR, RBSR;
        uint32_t STADR;
        bool mfifo_empty_triggered;
        int cycles_to_run;

        uint32_t master_disable;

        void apply_dma_funcs();

        int process_VIF0();
        int process_VIF1();
        int process_GIF();
        int process_IPU_FROM();
        int process_IPU_TO();
        int process_SIF0();
        int process_SIF1();
        int process_SPR_FROM();
        int process_SPR_TO();

        void handle_source_chain(int index);
        void advance_source_dma(int index);
        void advance_dest_dma(int index);
        bool mfifo_handler(int index);
        void transfer_end(int index);
        void int1_check();

        uint128_t fetch128(uint32_t addr);
        void store128(uint32_t addr, uint128_t data);

        void update_stadr(uint32_t addr);
        void check_for_activation(int index);
        void deactivate_channel(int index);
        void arbitrate();
        void find_new_active_channel();
    public:
        static const char* CHAN(int index);
        DMAC(EmotionEngine* cpu, Emulator* e, GraphicsInterface* gif, ImageProcessingUnit* ipu, SubsystemInterface* sif,
             VectorInterface* vif0, VectorInterface* vif1, VectorUnit* vu0, VectorUnit* vu1);
        void reset(uint8_t* RDRAM, uint8_t* scratchpad);
        void run(int cycles);
        void start_DMA(int index);

        uint32_t read_master_disable();
        void write_master_disable(uint32_t value);

        uint8_t read8(uint32_t address);
        uint16_t read16(uint32_t address);
        uint32_t read32(uint32_t address);
        void write8(uint32_t address, uint8_t value);
        void write16(uint32_t address, uint16_t value);
        void write32(uint32_t address, uint32_t value);

        void set_DMA_request(int index);
        void clear_DMA_request(int index);

        void load_state(std::ifstream& state);
        void save_state(std::ofstream& state);
};

#endif // DMAC_HPP
