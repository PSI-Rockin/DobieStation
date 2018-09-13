#ifndef SPU_HPP
#define SPU_HPP
#include <cstdint>

struct Voice
{
    uint16_t left_vol, right_vol;
    uint16_t pitch;
    uint16_t adsr1, adsr2;
    uint16_t current_envelope;

    uint32_t start_addr;
    uint32_t current_addr;
    uint32_t loop_addr;

    uint32_t counter;
    int block_pos;
    int loop_code;
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

        static uint16_t spdif_irq;

        uint32_t transfer_addr;

        uint32_t current_addr;

        uint32_t voice_mixdry_left;
        uint32_t voice_mixdry_right;
        uint32_t voice_mixwet_left;
        uint32_t voice_mixwet_right;
        //ADMA bullshit
        int ADMA_left;
        int input_pos;

        int cycles;

        uint32_t IRQA;
        uint32_t ENDX;
        uint32_t key_on;
        uint32_t key_off;

        void gen_sample();

        void key_on_voice(int v);
        void key_off_voice(int v);

        void spu_irq();

        uint16_t read_voice_reg(uint32_t addr);
        void write_voice_reg(uint32_t addr, uint16_t value);
        void write_voice_addr(uint32_t addr, uint16_t value);
    public:

        uint16_t autodma_ctrl;
        SPU(int id, Emulator* e);

        bool running_ADMA();
        bool can_write_ADMA();

        void reset(uint8_t* RAM);
        void update(int cycles_to_run);

        void start_DMA(int size);
        void finish_DMA();

        uint16_t read_mem();
        void process_ADMA();
        void write_DMA(uint32_t value);
        void write_ADMA(uint8_t* RAM);
        void write_mem(uint16_t value);

        uint16_t read16(uint32_t addr);
        void write16(uint32_t addr, uint16_t value);
};

#endif // SPU_HPP
