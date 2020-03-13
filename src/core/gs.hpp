#ifndef GS_HPP
#define GS_HPP
#include <cstdint>
#include <thread>
#include <mutex>
#include "gsthread.hpp"
#include "gsregisters.hpp"

class INTC;

class GraphicsSynthesizer
{
    private:
        INTC* intc;
        bool frame_complete;
        int frame_count;
        uint32_t* output_buffer1;
        uint32_t* output_buffer2;//double buffered to prevent mutex lock
        std::mutex output_buffer1_mutex, output_buffer2_mutex;
        bool using_first_buffer;
        std::unique_lock<std::mutex> current_lock;

        GS_REGISTERS reg;

        GraphicsSynthesizerThread gs_thread;
    public:
        GraphicsSynthesizer(INTC* intc);
        ~GraphicsSynthesizer();

        void reset();
        void start_frame();
        bool is_frame_complete() const;
        uint32_t* get_framebuffer();
        void render_CRT();
        uint32_t* render_partial_frame(uint16_t& width, uint16_t& height);
        void get_resolution(int& w, int& h);
        void get_inner_resolution(int& w, int& h);

        inline bool stalled() { return reg.CSR.SIGNAL_stall; }

        void set_CSR_FIFO(uint8_t value);
        void set_VBLANK(bool is_VBLANK);
        void assert_FINISH();
        void assert_VSYNC();

        void set_CRT(bool interlaced, int mode, bool frame_mode);

        uint32_t read32_privileged(uint32_t addr);
        uint64_t read64_privileged(uint32_t addr);
        void write32_privileged(uint32_t addr, uint32_t value);
        void write64_privileged(uint32_t addr, uint64_t value);
        void write64(uint32_t addr, uint64_t value);

        void set_RGBA(uint8_t r, uint8_t g, uint8_t b, uint8_t a, float q);
        void set_ST(uint32_t s, uint32_t t);
        void set_UV(uint16_t u, uint16_t v);
        void set_XYZ(uint32_t x, uint32_t y, uint32_t z, bool drawing_kick);
        void set_XYZF(uint32_t x, uint32_t y, uint32_t z, uint8_t fog, bool drawing_kick);

        void load_state(std::ifstream& state);
        void save_state(std::ofstream& state);
        void send_dump_request();

        void send_message(GSMessage message);
        void wake_gs_thread();

        std::tuple<uint128_t, uint32_t>request_gs_download();
};
#endif // GS_HPP
