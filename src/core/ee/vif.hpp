#ifndef VIF_HPP
#define VIF_HPP
#include <cstdint>
#include <queue>

#include "../int128.hpp"

class GraphicsInterface;

class VectorInterface
{
    private:
        GraphicsInterface* gif;
        std::queue<uint32_t> FIFO;
        uint8_t command;

        uint32_t buffer[4];
        int buffer_size;

        int command_len;
    public:
        VectorInterface(GraphicsInterface* gif);

        void reset();
        void update();

        bool transfer_DMAtag(uint128_t tag);
        void feed_DMA(uint128_t quad);
};

#endif // VIF_HPP
