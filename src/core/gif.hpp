#ifndef GIF_HPP
#define GIF_HPP
#include <cstdint>
#include <fstream>

#include "gs.hpp"
#include "int128.hpp"

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

struct GIFPath
{
    GIFtag current_tag;
};

class GraphicsInterface
{
    private:
        GraphicsSynthesizer* gs;
        
        GIFPath path[4];

        uint8_t active_path;
        uint8_t path_queue;
        //4 = Idle, Others match tag ID's
        uint8_t path_status[4];
        bool path3_vif_masked;
        bool path3_mode_masked;
        bool intermittent_mode;

        void process_PACKED(uint128_t quad);
        void process_REGLIST(uint128_t quad);
        void feed_GIF(uint128_t quad);
    public:
        GraphicsInterface(GraphicsSynthesizer* gs);
        void reset();

        bool path_active(int index);
        bool path_activepath3(int index);
        void resume_path3();

        uint32_t read_STAT();
        void write_MODE(uint32_t value);

        void request_PATH(int index, bool canInterruptPath3);
        void deactivate_PATH(int index);
        void set_path3_vifmask(int value);
        bool path3_masked(int index);
        bool interrupt_path3(int index);

        bool send_PATH(int index, uint128_t quad);

        bool send_PATH1(uint128_t quad);
        void send_PATH2(uint32_t data[4]);
        void send_PATH3(uint128_t quad);

        void load_state(std::ifstream& state);
        void save_state(std::ofstream& state);
};

inline bool GraphicsInterface::path_active(int index)
{
    if (index != 3)
    {
        interrupt_path3(index);
    }
    return (active_path == index) && !path3_masked(index) && !gs->stalled();
}

inline bool GraphicsInterface::path_activepath3(int index)
{
    if (index != 3)
    {
        interrupt_path3(index);
    }
    return ((active_path == index) || (path_queue & (1 << 3))) && !path3_masked(index) && !gs->stalled();
}

inline void GraphicsInterface::resume_path3()
{
    if (path3_vif_masked || path3_mode_masked)
        return;
    if (active_path == 3 || (path_queue & (1 << 3)) && path_status[3] == 4)
    {
        //printf("[GIF] Resuming PATH3\n");
        path_status[3] = 0; //Force it to be busy so if VIF puts the mask back on quickly, it doesn't instantly mask it
    }
}

#endif // GIF_HPP
