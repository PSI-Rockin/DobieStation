#ifndef SPU_HPP
#define SPU_HPP
#include <cstdint>
#include <fstream>
#include "../audio/utils.hpp"
#include "../audio/ps_adpcm.hpp"

struct stereo_sample
{
    int16_t left;
    int16_t right;
};

struct Voice
{
    int16_t left_vol, right_vol;
    uint16_t pitch;
    uint16_t adsr1, adsr2;
    uint16_t current_envelope;

    uint32_t start_addr;
    uint32_t current_addr;
    uint32_t loop_addr;
    bool loop_addr_specified;

    bool on;
    int key_switch_timeout;

    uint32_t counter;
    int block_pos;
    int loop_code;

    ADPCM_info adpcm;

    int16_t last_sample = 0;
    int16_t next_sample = 0;

    std::vector<int16_t> last_pcm;
    std::vector<int16_t> current_pcm;

    std::vector<int16_t> left_out_pcm;
    std::vector<int16_t> right_out_pcm;
    WAVWriter* wavout;

    void reset()
    {
        delete wavout;
        left_vol = 0;
        right_vol = 0;
        pitch = 0;
        adsr1 = 0;
        adsr2 = 0;
        current_envelope = 0;
        start_addr = 0;
        current_addr = 0;
        loop_addr = 0;
        loop_addr_specified = false;
        counter = 0;
        block_pos = 0;
        loop_code = 0;
    }
};

struct SPU_STAT
{
    bool DMA_ready;
    bool DMA_busy;
};

class IOP_INTC;
class IOP_DMA;

class SPU
{
    private:
        int id;
        IOP_INTC* intc;
        IOP_DMA* dma;

        uint16_t* RAM;
        Voice voices[24];
        static uint16_t core_att[2];
        SPU_STAT status;

        static uint16_t spdif_irq;

        uint32_t transfer_addr;

        uint32_t current_addr;

        uint32_t voice_mixdry_left;
        uint32_t voice_mixdry_right;
        uint32_t voice_mixwet_left;
        uint32_t voice_mixwet_right;
        //ADMA bullshit
        uint16_t autodma_ctrl;
        int ADMA_left;
        int input_pos;

        static uint32_t IRQA[2];
        uint32_t ENDX;
        uint32_t key_on;
        uint32_t key_off;
        ADPCM_decoder dec;

        stereo_sample voice_gen_sample(int voice_id);

        void key_on_voice(int v);
        void key_off_voice(int v);

        void spu_check_irq(uint32_t address);
        void spu_irq(int index);

        uint16_t read_voice_reg(uint32_t addr);
        void write_voice_reg(uint32_t addr, uint16_t value);
        void write_voice_addr(uint32_t addr, uint16_t value);

        void clear_dma_req();
        void set_dma_req();
    public:
        SPU(int id, IOP_INTC* intc, IOP_DMA* dma);

        void dump_voice_data();

        bool running_ADMA();
        bool can_write_ADMA();

        void reset(uint8_t* RAM);
        void gen_sample();

        void start_DMA(int size);
        void pause_DMA();
        void finish_DMA();

        uint16_t read_mem();
        void process_ADMA();
        uint32_t read_DMA();
        void write_DMA(uint32_t value);
        void write_ADMA(uint8_t* RAM);
        void write_mem(uint16_t value);

        uint16_t read16(uint32_t addr);
        void write16(uint32_t addr, uint16_t value);

        void load_state(std::ifstream& state);
        void save_state(std::ofstream& state);
};

inline bool SPU::running_ADMA()
{
    return (autodma_ctrl & (1 << (id - 1)));
}

inline bool SPU::can_write_ADMA()
{
    return (input_pos <= 128 || input_pos >= 384) && (ADMA_left < 0x400);
}

#endif // SPU_HPP
