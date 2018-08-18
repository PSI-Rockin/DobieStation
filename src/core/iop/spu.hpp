#ifndef SPU_HPP
#define SPU_HPP
#include <cstdint>

struct Voice
{
    uint16_t left_vol, right_vol;
    uint16_t pitch;
};

struct SPU_STAT
{
    bool DMA_finished;
    bool DMA_busy;
};

class Emulator;

class SPU
{
    private:
        int id;
        Emulator* e;

        uint16_t* RAM;
        Voice voices[24];
        uint16_t core_att;
        SPU_STAT status;

        uint32_t transfer_addr;

        uint32_t current_addr;

        //ADMA bullshit
        static uint16_t autodma_ctrl;
        int ADMA_left;
        bool can_write_adma;
        int input_pos;

        int cycles;

        void gen_sample();
    public:
        SPU(int id, Emulator* e);

        bool running_ADMA();
        bool can_write_ADMA();

        void reset(uint8_t* RAM);
        void update(int cycles_to_run);

        void start_DMA(int size);
        void finish_DMA();

        uint16_t read_mem();
        void write_DMA(uint32_t value);
        void write_ADMA(uint8_t* RAM);
        void write_mem(uint16_t value);

        uint16_t read16(uint32_t addr);
        void write16(uint32_t addr, uint16_t value);
};

#endif // SPU_HPP
