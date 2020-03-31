#ifndef IPU_HPP
#define IPU_HPP
#include <cstdint>
#include <queue>

#define BLOCK_SIZE 0x180

#include "../../int128.hpp"
#include "chromtable.hpp"
#include "codedblockpattern.hpp"
#include "dct_coeff_table0.hpp"
#include "dct_coeff_table1.hpp"
#include "lumtable.hpp"
#include "mac_addr_inc.hpp"
#include "mac_i_pic.hpp"
#include "mac_p_pic.hpp"
#include "mac_b_pic.hpp"
#include "motioncode.hpp"

struct IPU_CTRL
{
    uint8_t coded_block_pattern;
    bool error_code;
    bool start_code;
    uint8_t intra_DC_precision;
    bool alternate_scan;
    bool intra_VLC_table;
    bool nonlinear_Q_step;
    bool MPEG1;
    uint8_t picture_type;
    bool busy;
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
    DONE,
    CHECK_START_CODE
};

enum class IDEC_STATE
{
    DELAY,
    ADVANCE,
    MACRO_I_TYPE,
    DCT_TYPE,
    QSC,
    INIT_BDEC,
    READ_BLOCK,
    INIT_CSC,
    EXEC_CSC,
    CHECK_START_CODE,
    VALID_START_CODE,
    MACRO_INC,
    DONE
};

enum class SETIQ_STATE
{
    ADVANCE,
    POPULATE_TABLE
};

struct IDEC_Command
{
    IDEC_STATE state;
    uint32_t macro_type;

    bool decodes_dct;
    uint32_t qsc;

    IPU_FIFO temp_fifo;

    int blocks_decoded;
};

struct BDEC_Command
{
    BDEC_STATE state;
    IPU_FIFO* out_fifo;
    bool intra;
    bool reset_dc;
    bool check_start_code;
    int quantizer_step;

    int block_index;
    int subblock_index;

    //Four Y blocks, a Cb block, and a Cr block
    int16_t blocks[6][64];
    int16_t* cur_block;
    int cur_channel; //0 = Y, 1 = Cb, 2 = Cr

    enum class READ_COEFF
    {
        INIT,
        READ_DC_DIFF,
        CHECK_END,
        COEFF,
        SKIP_END
    };

    enum class READ_DIFF
    {
        SIZE,
        DIFF,
    };

    uint32_t dc_size;
    int16_t dc_diff;
    int16_t dc_predictor[3];

    READ_COEFF read_coeff_state;
    READ_DIFF read_diff_state;
};

enum class CSC_STATE
{
    BEGIN,
    READ,
    CONVERT,
    DONE
};

struct CSC_Command
{
    CSC_STATE state;
    int macroblocks;
    bool use_RGB16;

    uint8_t block[BLOCK_SIZE];
    int block_index;
};

class INTC;
class DMAC;

class ImageProcessingUnit
{
    private:
        INTC* intc;
        DMAC* dmac;
        DCT_Coeff_Table0 dct_coeff0;
        DCT_Coeff_Table1 dct_coeff1;
        DCT_Coeff* dct_coeff;
        ChromTable chrom_table;
        CodedBlockPattern cbp;
        LumTable lum_table;
        MacroblockAddrInc macroblock_increment;
        Macroblock_IPic macroblock_I_pic;
        Macroblock_PPic macroblock_P_pic;
        Macroblock_BPic macroblock_B_pic;
        MotionCode motioncode;
        VLC_Table* VDEC_table;
        IPU_FIFO in_FIFO, out_FIFO;

        int8_t dither_mtx[4][4];

        uint8_t intra_IQ[0x40], nonintra_IQ[0x40];
        uint16_t VQCLUT[16];
        uint32_t TH0, TH1;

        unsigned int crcb_map[0x100];

        static uint32_t inverse_scan_zigzag[0x40];
        static uint32_t inverse_scan_alternate[0x40];

        static uint32_t quantizer_linear[0x20];
        static uint32_t quantizer_nonlinear[0x20];

        IPU_CTRL ctrl;
        bool command_decoding;
        uint8_t command;
        uint32_t command_option;
        uint32_t command_output;
        int bytes_left;

        IDEC_Command idec;
        BDEC_Command bdec;
        VDEC_STATE vdec_state, fdec_state;
        CSC_Command csc;
        SETIQ_STATE setiq_state;

        double IDCT_table[8][8];

        void finish_command();

        bool process_IDEC();

        bool process_BDEC();
        void inverse_scan(int16_t* block);
        void dequantize(int16_t* block);
        void prepare_IDCT();
        void perform_IDCT(const int16_t* pUV, int16_t* pXY);
        bool BDEC_read_coeffs();
        bool BDEC_read_diff();

        void process_VDEC();
        void process_FDEC();
        bool process_CSC();
    public:
        ImageProcessingUnit(INTC* intc, DMAC* dmac);

        void reset();
        void run();

        uint64_t read_command();
        uint32_t read_control();
        uint32_t read_BP();
        uint64_t read_top();

        void write_command(uint32_t value);
        void write_control(uint32_t value);

        bool can_read_FIFO();
        bool can_write_FIFO();
        uint128_t read_FIFO();
        void write_FIFO(uint128_t quad);
};

#endif // IPU_HPP
