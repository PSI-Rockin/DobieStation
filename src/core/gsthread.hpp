#ifndef GSTHREAD_HPP
#define GSTHREAD_HPP
#include <cstdint>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <cstdint>
#include "gscontext.hpp"
#include "gsregisters.hpp"
#include "circularFIFO.hpp"

//Commands sent from the main thread to the GS thread.
enum GSCommand:uint8_t 
{
    write64_t, write64_privileged_t, write32_privileged_t,
    set_rgba_t, set_st_t, set_uv_t, set_xyz_t, set_xyzf_t, set_crt_t,
    render_crt_t, assert_finish_t, assert_vsync_t, set_vblank_t, memdump_t, die_t,
    save_state_t, load_state_t, gsdump_t
};

union GSMessagePayload 
{
    struct 
    {
        uint32_t addr;
        uint64_t value;
    } write64_payload;
    struct 
    {
        uint32_t addr;
        uint32_t value;
    } write32_payload;
    struct 
    {
        uint8_t r, g, b, a;
        float q;
    } rgba_payload;
    struct 
    {
        uint32_t s, t;
    } st_payload;
    struct 
    {
        uint16_t u, v;
    } uv_payload;
    struct 
    {
        uint32_t x, y, z;
        bool drawing_kick;
    } xyz_payload;
    struct
    {
        uint32_t x, y, z;
        uint8_t fog;
        bool drawing_kick;
    } xyzf_payload;
    struct 
    {
        bool interlaced;
        int mode;
        bool frame_mode;
    } crt_payload;
    struct 
    {
        bool vblank;
    } vblank_payload;
    struct 
    {
        uint32_t* target;
        std::mutex* target_mutex;
    } render_payload;
    struct
    {
        std::ofstream* state;
    } save_state_payload;
    struct
    {
        std::ifstream* state;
    } load_state_payload;
    struct 
    {
        uint8_t BLANK; 
    } no_payload;//C++ doesn't like the empty struct
};

struct GSMessage
{
    GSCommand type;
    GSMessagePayload payload;
};

//Commands sent from the GS thread to the main thread.
enum GSReturn :uint8_t
{
    render_complete_t,
    death_error_t,
    save_state_done_t,
    load_state_done_t,
    gsdump_render_partial_done_t,
};

union GSReturnMessagePayload
{
    struct
    {
        const char* error_str;
    } death_error_payload;
    struct
    {
        uint16_t x, y;
    } xy_payload;
    struct
    {
        uint8_t BLANK;
    } no_payload;//C++ doesn't like the empty struct
};

struct GSReturnMessage
{
    GSReturn type;
    GSReturnMessagePayload payload;
};

typedef CircularFifo<GSMessage, 1024 * 1024 * 16> gs_fifo;
typedef CircularFifo<GSReturnMessage, 1024> gs_return_fifo;

struct PRMODE_REG
{
    bool gourand_shading;
    bool texture_mapping;
    bool fog;
    bool alpha_blend;
    bool antialiasing;
    bool use_UV;
    bool use_context2;
    bool fix_fragment_value;
    void reset()
    {
        gourand_shading = false;
        texture_mapping = false;
        fog = false;
        alpha_blend = false;
        antialiasing = false;
        use_UV = false;
        use_context2 = false;
        fix_fragment_value = false;
    }
};

struct RGBAQ_REG
{
    //We make them 16-bit to handle potential overflows
    int16_t r, g, b, a;
    float q;
};

struct BITBLTBUF_REG
{
    uint32_t source_base;
    uint32_t source_width;
    uint8_t source_format;
    uint32_t dest_base;
    uint32_t dest_width;
    uint8_t dest_format;
};

struct TRXREG_REG
{
    uint16_t width;
    uint16_t height;
};

struct TRXPOS_REG
{
    uint16_t source_x, source_y;
    uint16_t dest_x, dest_y;
    uint16_t int_source_x, int_dest_x;
    uint16_t int_source_y, int_dest_y;
    uint8_t trans_order;
};

struct TEXA_REG
{
    uint8_t alpha0, alpha1;
    bool trans_black;
};

struct TEXCLUT_REG
{
    uint16_t width, x, y;
};

struct Vertex
{
    int32_t x, y;
    uint32_t z;

    RGBAQ_REG rgbaq;
    UV_REG uv;
    float s, t;
    uint8_t fog;
    void to_relative(XYOFFSET xyoffset)
    {
        x -= xyoffset.x;
        y -= xyoffset.y;
    }
};

struct TexLookupInfo
{
    //int32_t u, v;
    RGBAQ_REG vtx_color, tex_color, srctex_color;
    float LOD;
    int mipmap_level;

    uint32_t tex_base;
    uint32_t buffer_width;
    uint16_t tex_width, tex_height;

    uint8_t fog;
    bool new_lookup;
    int16_t lastu, lastv;
};

class GraphicsSynthesizerThread
{
    private:
        //threading
        std::thread thread;
        std::condition_variable notifier;

        std::mutex data_mutex;

        bool send_data = false;
        bool recieve_data = false;

        gs_fifo* message_queue = nullptr;
        gs_return_fifo* return_queue = nullptr;

        bool frame_complete;
        int frame_count;
        uint8_t* local_mem;
        uint8_t CRT_mode;
        uint8_t clut_cache[1024];
        uint32_t CBP0, CBP1;

        //CSR/IMR stuff - to be merged into structs

        GS_IMR IMR;

        GSContext context1, context2;
        GSContext* current_ctx;

        uint8_t prim_type;
        uint16_t FOG;
        PRMODE_REG PRIM, PRMODE;
        PRMODE_REG* current_PRMODE;
        RGBAQ_REG RGBAQ;
        UV_REG UV;
        ST_REG ST;
        TEXA_REG TEXA;
        TEXCLUT_REG TEXCLUT;
        bool DTHE;
        bool COLCLAMP;
        RGBAQ_REG FOGCOL;
        bool PABE;

        uint8_t SCANMSK;

        uint8_t dither_mtx[4][4];

        BITBLTBUF_REG BITBLTBUF;
        TRXPOS_REG TRXPOS;
        TRXREG_REG TRXREG;
        uint8_t TRXDIR;
        uint8_t BUSDIR;
        int pixels_transferred;

        //Used for unpacking PSMCT24
        uint32_t PSMCT24_color;
        int PSMCT24_unpacked_count;

        GS_REGISTERS reg;

        Vertex current_vtx;
        Vertex vtx_queue[3];
        unsigned int num_vertices;

        static const unsigned int max_vertices[8];

        void event_loop();

        inline const uint32_t get_word(uint32_t addr) { return *(uint32_t*)&local_mem[addr]; };
        inline void set_word(uint32_t addr, uint32_t value) { *(uint32_t*)&local_mem[addr] = value; };

        //Swizzling routines
        uint32_t blockid_PSMCT32(uint32_t block, uint32_t width, uint32_t x, uint32_t y);
        uint32_t blockid_PSMCT32Z(uint32_t block, uint32_t width, uint32_t x, uint32_t y);
        uint32_t blockid_PSMCT16(uint32_t block, uint32_t width, uint32_t x, uint32_t y);
        uint32_t blockid_PSMCT16S(uint32_t block, uint32_t width, uint32_t x, uint32_t y);
        uint32_t blockid_PSMCT16Z(uint32_t block, uint32_t width, uint32_t x, uint32_t y);
        uint32_t blockid_PSMCT16SZ(uint32_t block, uint32_t width, uint32_t x, uint32_t y);
        uint32_t blockid_PSMCT8(uint32_t block, uint32_t width, uint32_t x, uint32_t y);
        uint32_t blockid_PSMCT4(uint32_t block, uint32_t width, uint32_t x, uint32_t y);

        uint32_t addr_PSMCT32(uint32_t block, uint32_t width, uint32_t x, uint32_t y);
        uint32_t addr_PSMCT32Z(uint32_t block, uint32_t width, uint32_t x, uint32_t y);
        uint32_t addr_PSMCT16(uint32_t block, uint32_t width, uint32_t x, uint32_t y);
        uint32_t addr_PSMCT16S(uint32_t block, uint32_t width, uint32_t x, uint32_t y);
        uint32_t addr_PSMCT16Z(uint32_t block, uint32_t width, uint32_t x, uint32_t y);
        uint32_t addr_PSMCT16SZ(uint32_t block, uint32_t width, uint32_t x, uint32_t y);
        uint32_t addr_PSMCT8(uint32_t block, uint32_t width, uint32_t x, uint32_t y);
        uint32_t addr_PSMCT4(uint32_t block, uint32_t width, uint32_t x, uint32_t y);

        uint32_t read_PSMCT32_block(uint32_t base, uint32_t width, uint32_t x, uint32_t y);
        uint32_t read_PSMCT32Z_block(uint32_t base, uint32_t width, uint32_t x, uint32_t y);
        uint16_t read_PSMCT16_block(uint32_t base, uint32_t width, uint32_t x, uint32_t y);
        uint16_t read_PSMCT16S_block(uint32_t base, uint32_t width, uint32_t x, uint32_t y);
        uint16_t read_PSMCT16Z_block(uint32_t base, uint32_t width, uint32_t x, uint32_t y);
        uint16_t read_PSMCT16SZ_block(uint32_t base, uint32_t width, uint32_t x, uint32_t y);
        uint8_t read_PSMCT8_block(uint32_t base, uint32_t width, uint32_t x, uint32_t y);
        uint8_t read_PSMCT4_block(uint32_t base, uint32_t width, uint32_t x, uint32_t y);

        void write_PSMCT32_block(uint32_t base, uint32_t width, uint32_t x, uint32_t y, uint32_t value);
        void write_PSMCT32Z_block(uint32_t base, uint32_t width, uint32_t x, uint32_t y, uint32_t value);
        void write_PSMCT24_block(uint32_t base, uint32_t width, uint32_t x, uint32_t y, uint32_t value);
        void write_PSMCT24Z_block(uint32_t base, uint32_t width, uint32_t x, uint32_t y, uint32_t value);
        void write_PSMCT16_block(uint32_t base, uint32_t width, uint32_t x, uint32_t y, uint16_t value);
        void write_PSMCT16S_block(uint32_t base, uint32_t width, uint32_t x, uint32_t y, uint16_t value);
        void write_PSMCT16Z_block(uint32_t base, uint32_t width, uint32_t x, uint32_t y, uint16_t value);
        void write_PSMCT16SZ_block(uint32_t base, uint32_t width, uint32_t x, uint32_t y, uint16_t value);
        void write_PSMCT8_block(uint32_t base, uint32_t width, uint32_t x, uint32_t y, uint8_t value);
        void write_PSMCT4_block(uint32_t base, uint32_t width, uint32_t x, uint32_t y, uint8_t value);

        uint8_t get_16bit_alpha(uint16_t color);
        int16_t multiply_tex_color(int16_t tex_color, int16_t frag_color);
        void calculate_LOD(TexLookupInfo& info);
        void tex_lookup(int16_t u, int16_t v, TexLookupInfo& info);
        void tex_lookup_int(int16_t u, int16_t v, TexLookupInfo& info, bool forced_lookup = false);
        void clut_lookup(uint8_t entry, RGBAQ_REG& tex_color);
        void clut_CSM2_lookup(uint8_t entry, RGBAQ_REG& tex_color);
        void reload_clut(const GSContext& context);

        void vertex_kick(bool drawing_kick);
        bool depth_test(int32_t x, int32_t y, uint32_t z);
        void draw_pixel(int32_t x, int32_t y, uint32_t z, RGBAQ_REG& color);
        void render_primitive();
        void render_point();
        void render_line();
        void render_triangle();
        void render_sprite();
        void write_HWREG(uint64_t data);
        void unpack_PSMCT24(uint64_t data, int offset, bool z_format);
        void local_to_local();

        int32_t orient2D(const Vertex &v1, const Vertex &v2, const Vertex &v3);

        void reset();
        void memdump(uint32_t* target, uint16_t& width, uint16_t& height);

        uint32_t get_CRT_color(DISPFB& dispfb, uint32_t x, uint32_t y);
        void render_single_CRT(uint32_t* target, DISPFB& dispfb, DISPLAY& display);
        void render_CRT(uint32_t* target);

        void dump_texture(uint32_t* target, uint32_t start_addr, uint32_t width);

        void write64(uint32_t addr, uint64_t value);

        void set_RGBA(uint8_t r, uint8_t g, uint8_t b, uint8_t a, float q);
        void set_ST(uint32_t s, uint32_t t);
        void set_UV(uint16_t u, uint16_t v);
        void set_XYZ(uint32_t x, uint32_t y, uint32_t z, bool drawing_kick);
        void set_XYZF(uint32_t x, uint32_t y, uint32_t z, uint8_t fog, bool drawing_kick);

        void load_state(std::ifstream* state);
        void save_state(std::ofstream* state);
    public:
        GraphicsSynthesizerThread();
        ~GraphicsSynthesizerThread();
        
        // safe to access from emu thread
        void send_message(GSMessage message);
        void wait_for_return(GSReturn type, GSReturnMessage &data);
        void reset_fifos();
        void exit();
};
#endif // GSTHREAD_HPP
