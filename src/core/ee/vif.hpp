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
    uint8_t write_cycle_counter;
};

class VectorInterface
{
    private:
        int id;

        // registers
        uint32_t VIF_R[4];
        uint32_t VIF_CYCLE;
        uint32_t VIF_MODE;
        uint32_t VIF1_TOPS;

        GraphicsInterface* gif;
        VectorUnit* vu;
        std::queue<uint32_t> FIFO;
        uint8_t command;
        uint32_t value;

        MPG_Command mpg;
        UNPACK_Command unpack;

        uint32_t buffer[4];
        int buffer_size;

        int command_len;
    public:
        VectorInterface(GraphicsInterface* gif, VectorUnit* vu, int id);

        void reset();
        void update();

        bool transfer_DMAtag(uint128_t tag);
        void feed_DMA(uint128_t quad);
        void perform_unpack_v4_32();
};

#endif // VIF_HPP
