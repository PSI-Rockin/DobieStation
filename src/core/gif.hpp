#ifndef GIF_HPP
#define GIF_HPP
#include <cstdint>
#include <fstream>

#include "int128.hpp"

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

        uint8_t active_path;
        uint8_t path_queue;
        bool path3_vif_masked;

        void process_PACKED(uint128_t quad);
        void process_REGLIST(uint128_t quad);
        void feed_GIF(uint128_t quad);
    public:
        GraphicsInterface(GraphicsSynthesizer* gs);
        void reset();

        bool path_active(int index);

        uint32_t read_STAT();

        void request_PATH(int index);
        void deactivate_PATH(int index);
        void set_path3_vifmask(int value);

        bool send_PATH(int index, uint128_t quad);

        bool send_PATH1(uint128_t quad);
        void send_PATH2(uint32_t data[4]);
        void send_PATH3(uint128_t quad);

        void load_state(std::ifstream& state);
        void save_state(std::ofstream& state);
};

inline bool GraphicsInterface::path_active(int index)
{
    return active_path == index;
}

#endif // GIF_HPP
