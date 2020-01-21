#ifndef VIF_HPP
#define VIF_HPP
#include <cstdint>
#include <queue>
#include <fstream>
#include <unordered_set>

#include "intc.hpp"
#include "vu.hpp"

#include "../int128.hpp"

class GraphicsInterface;
class DMAC;

enum VIF_STALL
{
    STALL_IBIT = 1,
    STALL_MSKPATH3 = 2
};

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

struct VIF_ERR_REG
{
    bool mask_interrupt, mask_dmatag_error, mask_vifcode_error;
};

class VectorInterface
{
    private:
        GraphicsInterface* gif;
        VectorUnit* vu;
        INTC* intc;
        DMAC* dmac;
        std::queue<uint32_t> FIFO;
        int id;
        uint16_t imm;
        uint8_t command;

        MPG_Command mpg;
        UNPACK_Command unpack;

        bool vif_ibit_detected;
        uint8_t vif_stalled;
        bool vif_interrupt;
        bool vif_stop;
        bool fifo_reverse;
        
        bool wait_for_VU;
        bool wait_for_PATH3;
        bool flush_stall;
        uint32_t wait_cmd_value;
        uint16_t mem_mask;
        uint32_t fifo_size;

        uint32_t buffer[4];
        int buffer_size;

        bool DBF;
        bool mark_detected;
        VIF_ERR_REG VIF_ERR;
        CYCLE_REG CYCLE;
        uint16_t OFST;
        uint16_t BASE;
        uint16_t TOPS, TOP, ITOPS, ITOP;
        uint8_t MODE;
        uint32_t MASK;
        uint32_t CODE;
        uint32_t ROW[4];
        uint32_t COL[4];
        uint32_t MARK;

        int command_len;
        bool check_vif_stall(uint32_t value);
        void decode_cmd(uint32_t value);
        void handle_wait_cmd(uint32_t value);
        void MSCAL(uint32_t addr);
        void init_UNPACK(uint32_t value);
        bool is_filling_write();
        void handle_UNPACK();
        void handle_UNPACK_masking(uint128_t& quad);
        void handle_UNPACK_mode(uint128_t& quad);
        void process_UNPACK_quad(uint128_t& quad);

        bool process_data_word(uint32_t value);
    public:
        VectorInterface(GraphicsInterface* gif, VectorUnit* vu, INTC* intc, DMAC* dmac, int id);
        int get_id();

        void reset();
        void update(int cycles);

        bool transfer_DMAtag(uint128_t tag);
        bool feed_DMA(uint128_t quad);
        uint128_t readFIFO();

        uint32_t get_stat();
        uint32_t get_mark();
        uint32_t get_err();
        uint32_t get_mode();
        uint32_t get_row(uint32_t address);
        uint32_t get_code();
        uint32_t get_top();

        void set_stat(uint32_t value);
        void set_mark(uint32_t value);
        void set_err(uint32_t value);
        void set_fbrst(uint32_t value);

        void load_state(std::ifstream& state);
        void save_state(std::ofstream& state);
};

inline int VectorInterface::get_id()
{
    return id;
}
#endif // VIF_HPP
