#ifndef IPU_HPP
#define IPU_HPP
#include <cstdint>
#include <queue>

#include "../int128.hpp"

struct IPU_CTRL
{
    bool error_code;
    bool start_code;
    uint8_t intra_DC_precision;
    bool MPEG1;
    uint8_t picture_type;
    bool busy;
};

//Contains information needed for the VDEC command
struct VLC_Entry
{
    uint32_t key;
    uint32_t value;
    uint8_t bits;
};

enum class VDEC_STATE
{
    ADVANCE,
    DECODE,
    DONE
};

enum class BDEC_STATE
{
    ADVANCE,
    GET_CBP,
    RESET_DC,
    BEGIN_DECODING,
    READ_COEFFS,
    LOAD_NEXT_BLOCK,
    DONE
};

struct BDEC_Command
{
    BDEC_STATE state;
    uint8_t coded_block_pattern;
    bool intra;
    int block_index;

    enum READ_COEFF_STATE
    {
        INIT,
        READ_DC_DIFF,
        CHECK_END,
        READ_COEFF,
        SKIP_EOB
    };

    READ_COEFF_STATE read_coeff_state;
};

class ImageProcessingUnit
{
    private:
        static VLC_Entry macroblock_increment[];
        static VLC_Entry macroblock_I_pic[];
        VLC_Entry* VDEC_table;
        std::queue<uint128_t> in_FIFO;
        std::queue<uint128_t> out_FIFO;

        uint16_t VQCLUT[16];

        IPU_CTRL ctrl;
        bool command_decoding;
        uint8_t command;
        uint32_t command_option;
        uint32_t command_output;
        int bytes_left;

        BDEC_Command bdec;
        VDEC_STATE vdec_state, fdec_state;

        int bit_pointer;
        void advance_stream(uint8_t amount);
        bool get_bits(uint32_t& data, int bits);

        void process_BDEC();
        bool BDEC_read_coeffs();

        void process_VDEC();
        void process_FDEC();
    public:
        ImageProcessingUnit();

        void reset();
        void run();

        uint64_t read_command();
        uint32_t read_control();
        uint32_t read_BP();
        uint64_t read_top();

        void write_command(uint32_t value);
        void write_control(uint32_t value);

        bool can_write_FIFO();
        void write_FIFO(uint128_t quad);
};

#endif // IPU_HPP
