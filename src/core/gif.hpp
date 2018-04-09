#ifndef GIF_HPP
#define GIF_HPP
#include <cstdint>

class GraphicsSynthesizer;

struct GIFtag
{
    uint16_t NLOOP;
    bool end_of_packet;
    bool output_PRIM;
    uint16_t PRIM;
    uint8_t format;
    uint8_t reg_count;
    uint64_t regs;

    uint8_t regs_left;
    uint32_t data_left;
};

class GraphicsInterface
{
    private:
        GraphicsSynthesizer* gs;
        GIFtag current_tag;
        bool processing_GIF_prim;

        void process_PACKED(uint64_t data[2]);
        void process_REGLIST(uint64_t data[2]);
        void feed_GIF(uint64_t data[2]);
    public:
        GraphicsInterface(GraphicsSynthesizer* gs);
        void reset();
        void send_PATH3(uint64_t data[2]);
};

#endif // GIF_HPP
