#ifndef IOP_DMA_HPP
#define IOP_DMA_HPP
#include <cstdint>

struct IOP_DMA_Chan_Control
{
    bool direction_from;
    bool unk8;
    uint8_t sync_mode;
    bool busy;
    bool unk30;
};

struct IOP_DMA_Channel
{
    uint32_t addr;
    uint16_t word_count;
    uint16_t block_size;
    IOP_DMA_Chan_Control control;

    bool tag_end;
};

struct DMA_DPCR
{
    uint8_t priorities[16]; //Is this correct?
    bool enable[16];
};

struct DMA_DICR
{
    uint8_t STAT[2];
    uint8_t MASK[2];
    bool master_int_enable[2];
};

class Emulator;
class SubsystemInterface;

class IOP_DMA
{
    private:
        uint8_t* RAM;
        Emulator* e;
        SubsystemInterface* sif;
        IOP_DMA_Channel channels[16];

        //Merge of DxCR, DxCR2, DxCR3 for easier processing
        DMA_DPCR DPCR;
        DMA_DICR DICR;

        void transfer_end(int index);
        void process_SIF1();
    public:
        static const char* CHAN(int index);
        IOP_DMA(Emulator* e, SubsystemInterface* sif);

        void reset(uint8_t* RAM);
        void run();

        uint32_t get_DPCR();
        uint32_t get_DPCR2();
        uint32_t get_DICR();
        uint32_t get_DICR2();

        void set_DPCR(uint32_t value);
        void set_DPCR2(uint32_t value);
        void set_DICR(uint32_t value);
        void set_DICR2(uint32_t value);

        void set_chan_addr(int index, uint32_t value);
        void set_chan_block(int index, uint32_t value);
        void set_chan_size(int index, uint16_t value);
        void set_chan_count(int index, uint16_t value);
        void set_chan_control(int index, uint32_t value);
};

#endif // IOP_DMA_HPP
