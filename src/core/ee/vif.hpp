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

class VectorInterface
{
    private:
        GraphicsInterface* gif;
        VectorUnit* vu;
        std::queue<uint32_t> FIFO;
        uint8_t command;

        MPG_Command mpg;

        uint32_t buffer[4];
        int buffer_size;

        int command_len;
    public:
        VectorInterface(GraphicsInterface* gif, VectorUnit* vu);

        void reset();
        void update();

        bool transfer_DMAtag(uint128_t tag);
        void feed_DMA(uint128_t quad);
};

#endif // VIF_HPP
