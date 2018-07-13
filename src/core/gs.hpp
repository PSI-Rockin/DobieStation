#ifndef GS_HPP
#define GS_HPP
#include <cstdint>
#include <thread>
#include <mutex>
#include "gscontext.hpp"
#include "circularFIFO.hpp"


//reg reg
struct TRXREG_REG
{
    uint16_t width;
    uint16_t height;
};

struct PMODE_REG
{
    bool circuit1;
    bool circuit2;
    uint8_t output_switching;
    bool use_ALP;
    bool out1_circuit2;
    bool blend_with_bg;
    uint8_t ALP;
};

struct SMODE
{
    bool interlaced;
    bool frame_mode;
    uint8_t power_mode;
};

struct DISPFB
{
    uint32_t frame_base;
    uint32_t width;
    uint8_t format;
    uint16_t x, y;
};

struct DISPLAY
{
    uint16_t x, y;
    uint8_t magnify_x, magnify_y;
    uint16_t width, height;
};

struct TEXCLUT_REG
{
    uint16_t width, x, y;
};
struct GS_IMR //Interrupt masking
{
    bool signal;
    bool finish;
    bool hsync;
    bool vsync;
    bool rawt; //Rectangular Area Write Termination
};

class INTC;

enum GS_command:uint8_t { write64_t, write64_privileged_t, write32_privileged_t,
    set_rgba_t, set_stq_t, set_uv_t, set_xyz_t, set_q_t, render_crt_t, set_crt_t, memdump_t, die_t
};
union GS_message_payload {
    struct {
        uint32_t addr;
        uint64_t value;
    } write64_payload;
    struct {
        uint32_t addr;
        uint32_t value;
    } write32_payload;
    struct {
        uint8_t r, g, b, a;
    } rgba_payload;
    struct {
        uint32_t s, t, q;
    } stq_payload;
    struct {
        uint16_t u, v;
    } uv_payload;
    struct {
        uint32_t x, y, z;
        bool drawing_kick;
    } xyz_payload;
    struct {
        float q;
    } q_payload;
    struct {
        bool interlaced;
        int mode;
        bool frame_mode;
    } crt_payload;
    struct {
        uint32_t* target;
        std::mutex* target_mutex;
    } render_payload;
    struct {} no_payload;
};
struct GS_message {
    GS_command type;
    GS_message_payload payload;
};
typedef CircularFifo<GS_message, 1024*1024*16> gs_fifo;


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
        uint8_t CRT_mode;

        //general registers that generate interrupts- used outside of GS
        bool SIGNAL, FINISH, LABEL;

        //Privileged registers
        PMODE_REG PMODE;
        SMODE SMODE2;
        DISPFB DISPFB1, DISPFB2;
        DISPLAY DISPLAY1, DISPLAY2;

        //CSR/IMR stuff - to be merged into structs
        bool VBLANK_generated;
        bool VBLANK_enabled;
        bool is_odd_frame;
        bool FINISH_enabled;
        bool FINISH_generated;
        bool FINISH_requested;
        GS_IMR IMR;
        uint8_t BUSDIR;
        gs_fifo* MessageQueue; //ring buffer size
        //EXTBUF, EXTDATA, EXTWRITE, BGCOLOR, SIGLBLID not currently implemented

        std::thread gsthread_id;
		
    public:
        GraphicsSynthesizer(INTC* intc);
        ~GraphicsSynthesizer();
        void reset();
        void memdump();
        void start_frame();
        bool is_frame_complete();
        uint32_t* get_framebuffer();
        void render_CRT();
        void get_resolution(int& w, int& h);
        void get_inner_resolution(int& w, int& h);

        void set_VBLANK(bool is_VBLANK);
        void assert_FINISH();

        void set_CRT(bool interlaced, int mode, bool frame_mode);

        uint32_t read32_privileged(uint32_t addr);
        uint64_t read64_privileged(uint32_t addr);
        void write32_privileged(uint32_t addr, uint32_t value);
        void write64_privileged(uint32_t addr, uint64_t value);
        void write64(uint32_t addr, uint64_t value);

        void set_RGBA(uint8_t r, uint8_t g, uint8_t b, uint8_t a);
        void set_STQ(uint32_t s, uint32_t t, uint32_t q);
        void set_UV(uint16_t u, uint16_t v);
        void set_Q(float q);
        void set_XYZ(uint32_t x, uint32_t y, uint32_t z, bool drawing_kick);
};

#endif // GS_HPP
