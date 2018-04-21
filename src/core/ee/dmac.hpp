#ifndef DMAC_HPP
#define DMAC_HPP
#include <cstdint>

struct DMA_Channel
{
    uint32_t control;
    uint32_t address;
    uint32_t quadword_count;
    uint32_t tag_address;
    uint32_t tag_save0, tag_save1;

    bool tag_end;
    bool paused;
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
    bool channel_stat[10];
    bool stall_stat;
    bool mfifo_stat;
    bool bus_stat;

    bool channel_mask[10];
    bool stall_mask;
    bool mfifo_mask;
};

class EmotionEngine;
class Emulator;
class GraphicsInterface;
class SubsystemInterface;

class DMAC
{
    private:
        uint8_t* RDRAM, *scratchpad;
        EmotionEngine* cpu;
        Emulator* e;
        GraphicsInterface* gif;
        SubsystemInterface* sif;
        DMA_Channel channels[10];

        D_CTRL control;
        D_STAT interrupt_stat;
        uint32_t PCR;

        uint32_t master_disable;

        void process_GIF();
        void process_SIF0();
        void process_SIF1();

        void handle_source_chain(int index);

        void transfer_end(int index);
        void int1_check();

        void fetch128(uint32_t addr, uint64_t* quad);
    public:
        DMAC(EmotionEngine* cpu, Emulator* e, GraphicsInterface* gif, SubsystemInterface* sif);
        void reset(uint8_t* RDRAM, uint8_t* scratchpad);
        void run();
        void run(int cycles);
        void start_DMA(int index);

        uint32_t read_master_disable();
        void write_master_disable(uint32_t value);

        uint32_t read32(uint32_t address);
        void write32(uint32_t address, uint32_t value);
};

#endif // DMAC_HPP
