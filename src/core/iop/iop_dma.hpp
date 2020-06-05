#ifndef IOP_DMA_HPP
#define IOP_DMA_HPP
#include <cstdint>
#include <fstream>
#include <list>

class IOP_DMA;

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
    uint32_t word_count;
    uint32_t size;
    uint16_t block_size;
    IOP_DMA_Chan_Control control;
    uint32_t tag_addr;

    bool tag_end;

    typedef void(IOP_DMA::*dma_copy_func)();
    dma_copy_func func;

    bool dma_req;

    int delay;
    int index;
};

struct DMA_DPCR
{
    uint8_t priorities[16]; //Is this correct?
    bool enable[16];
};

struct DMA_DICR
{
    bool force_IRQ[2];
    uint8_t STAT[2];
    uint8_t MASK[2];
    bool master_int_enable[2];
};

class CDVD_Drive;
class IOP_INTC;
class SubsystemInterface;
class SIO2;
class SPU;

enum IOP_DMA_CHANNELS
{
    IOP_MDECin,
    IOP_MDECout,
    IOP_GPU,
    IOP_CDVD,
    IOP_SPU,
    IOP_PIO,
    IOP_OTC,
    IOP_SPU2 = 8,
    IOP_unk,
    IOP_SIF0,
    IOP_SIF1,
    IOP_SIO2in,
    IOP_SIO2out
};

class IOP_DMA
{
    private:
        uint8_t* RAM;
        IOP_INTC* intc;
        CDVD_Drive* cdvd;
        SubsystemInterface* sif;
        SIO2* sio2;
        SPU *spu, *spu2;
        IOP_DMA_Channel channels[16];
        IOP_DMA_Channel* active_channel;
        std::list<IOP_DMA_Channel*> queued_channels;

        //Merge of DxCR, DxCR2, DxCR3 for easier processing
        DMA_DPCR DPCR;
        DMA_DICR DICR;

        void transfer_end(int index);
        void process_CDVD();
        void process_SPU();
        void process_SPU2();
        void process_SIF0();
        void process_SIF1();
        void process_SIO2in();
        void process_SIO2out();

        void active_dma_check(int index);
        void deactivate_dma(int index);
        void find_new_active_channel();
        void apply_dma_functions();
    public:
        static const char* CHAN(int index);
        IOP_DMA(IOP_INTC* intc, CDVD_Drive* cdvd, SubsystemInterface* sif, SIO2* sio2, SPU* spu, SPU* spu2);

        void reset(uint8_t* RAM);
        void run(int cycles);

        uint32_t get_DPCR();
        uint32_t get_DPCR2();
        uint32_t get_DICR();
        uint32_t get_DICR2();

        uint32_t get_chan_addr(int index);
        uint32_t get_chan_block(int index);
        uint32_t get_chan_control(int index);

        void set_DPCR(uint32_t value);
        void set_DPCR2(uint32_t value);
        void set_DICR(uint32_t value);
        void set_DICR2(uint32_t value);

        void set_DMA_request(int index);
        void clear_DMA_request(int index);

        void set_chan_addr(int index, uint32_t value);
        void set_chan_block(int index, uint32_t value);
        void set_chan_size(int index, uint16_t value);
        void set_chan_count(int index, uint16_t value);
        void set_chan_control(int index, uint32_t value);
        void set_chan_tag_addr(int index, uint32_t value);

        void load_state(std::ifstream& state);
        void save_state(std::ofstream& state);
};

#endif // IOP_DMA_HPP
