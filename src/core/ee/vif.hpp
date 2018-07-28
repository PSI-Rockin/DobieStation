#ifndef VIF_HPP
#define VIF_HPP
#include <cstdint>
#include <queue>

#include "vu.hpp"

#include "../int128.hpp"

class GraphicsInterface;

struct MPG_Command
{
    uint32_t addr;
};

struct UNPACK_Command
{
    uint32_t addr;
    bool sign_extend;
    bool masked;
    int offset;
    int num;
    int cmd;
    int blocks_written;
    int words_per_op; //e.g. - V4-32 has four words per op
};

struct CYCLE_REG
{
    uint8_t CL, WL;
};

class VectorInterface
{
    private:
        GraphicsInterface* gif;
        VectorUnit* vu;
        std::queue<uint32_t> FIFO;
        uint16_t imm;
        uint8_t command;

        MPG_Command mpg;
        UNPACK_Command unpack;

        bool wait_for_VU;
        bool flush_stall;
        uint32_t wait_cmd_value;

        uint32_t buffer[4];
        int buffer_size;

        bool DBF;
        CYCLE_REG CYCLE;
        uint16_t OFST;
        uint16_t BASE;
        uint16_t TOPS, TOP, ITOPS, ITOP;
        uint8_t MODE;
        uint32_t MASK;
        uint32_t ROW[4];
        uint32_t COL[4];

        int command_len;
        void decode_cmd(uint32_t value);
        void handle_wait_cmd(uint32_t value);
        void MSCAL(uint32_t addr);
        void init_UNPACK(uint32_t value);
        void handle_UNPACK(uint32_t value);
        void handle_UNPACK_masking(uint128_t& quad);
        void process_UNPACK_quad(uint128_t& quad);

        void disasm_micromem();
    public:
        VectorInterface(GraphicsInterface* gif, VectorUnit* vu);

        void reset();
        void update();

        bool transfer_DMAtag(uint128_t tag);
        void feed_DMA(uint128_t quad);

        uint32_t get_stat();
};

#endif // VIF_HPP
