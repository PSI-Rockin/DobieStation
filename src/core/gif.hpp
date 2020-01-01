#ifndef GIF_HPP
#define GIF_HPP
#include <cstdint>
#include <queue>
#include <fstream>

#include "gs.hpp"
#include "int128.hpp"

class DMAC;

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
        DMAC* dmac;
        
        GIFPath path[4];

        std::queue<uint128_t> FIFO;

        uint8_t active_path;
        uint8_t path_queue;
        //4 = Idle, Others match tag ID's
        uint8_t path_status[4];
        bool path3_vif_masked;
        bool path3_mode_masked;
        bool intermittent_mode;
        bool intermittent_active;
        bool path3_dma_waiting;

        float internal_Q;

        void process_PACKED(uint128_t quad);
        void process_REGLIST(uint128_t quad);
        void feed_GIF(uint128_t quad);

        void flush_path3_fifo();
    public:
        GraphicsInterface(GraphicsSynthesizer* gs, DMAC* dmac);
        void reset();
        void run(int cycles);

        bool fifo_full();
        bool fifo_empty();
        bool fifo_draining();
        void dma_waiting(bool dma_waiting);
        int get_active_path();
        bool path_active(int index, bool canInterruptPath3);
        void resume_path3();

        uint32_t read_STAT();
        void write_MODE(uint32_t value);

        void request_PATH(int index, bool canInterruptPath3);
        void deactivate_PATH(int index);
        void set_path3_vifmask(int value);
        bool path3_masked(int index);
        bool interrupt_path3(int index);
        bool path3_done();
        void arbitrate_paths();

        bool send_PATH(int index, uint128_t quad);

        bool send_PATH1(uint128_t quad);
        void send_PATH2(uint32_t data[4]);
        void send_PATH3(uint128_t quad);
        uint128_t read_GSFIFO();

        void intermittent_check();

        void load_state(std::ifstream& state);
        void save_state(std::ofstream& state);
};

inline int GraphicsInterface::get_active_path()
{
    return active_path;
}

inline bool GraphicsInterface::path_active(int index, bool canInterruptPath3)
{
    if (index != 3 && canInterruptPath3)
    {
        interrupt_path3(index);
    }
    return (active_path == index) && !gs->stalled();
}

inline void GraphicsInterface::resume_path3()
{
    if (path3_vif_masked || path3_mode_masked)
        return;
    if ((path3_dma_waiting || FIFO.size()) && path_status[3] == 4)
    {
        //printf("[GIF] Resuming PATH3\n");
        path_status[3] = 0; //Force it to be busy so if VIF puts the mask back on quickly, it doesn't instantly mask it
        request_PATH(3, false);
    }
}

#endif // GIF_HPP
