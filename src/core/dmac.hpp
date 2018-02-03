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

class Emulator;
class GraphicsSynthesizer;

class DMAC
{
    private:
        Emulator* e;
        GraphicsSynthesizer* gs;
        DMA_Channel channels[10];

        D_CTRL control;
        D_STAT interrupt_stat;

        void handle_source_chain(int index);
    public:
        DMAC(Emulator* e, GraphicsSynthesizer* gs);
        void reset();
        void run();
        void start_DMA(int index);

        uint32_t read32(uint32_t address);
        void write32(uint32_t address, uint32_t value);
};

#endif // DMAC_HPP
