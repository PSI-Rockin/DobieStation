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
#include "int128.hpp"

#include "jitcommon/emitter64.hpp"

template<int XS, int YS, int ZS>
struct SwizzleTable
{
    SwizzleTable()
    {

    }

    uint32_t& get(int x, int y, int z)
    {
        return data[x*YS*ZS + y * ZS + z];
    }

    uint32_t data[XS*YS*ZS];
};

//Commands sent from the main thread to the GS thread.
enum GSCommand:uint8_t 
{
    write64_t, write64_privileged_t, write32_privileged_t,
    set_rgba_t, set_st_t, set_uv_t, set_xyz_t, set_xyzf_t, set_crt_t,
    render_crt_t, assert_finish_t, assert_vsync_t, set_vblank_t, memdump_t, die_t,
    save_state_t, load_state_t, gsdump_t, request_local_host_tx,
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
    local_host_transfer,
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
    struct
    {
        uint128_t quad_data;
        uint32_t status;
    } data_payload;
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

//NOTE: RGBAQ is used by the JIT. DO NOT TOUCH!
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

//NOTE: TexLookupInfo is used by the JIT. DO NOT TOUCH!
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

uint32_t addr_PSMCT32(uint32_t block, uint32_t width, uint32_t x, uint32_t y);
uint32_t addr_PSMCT32Z(uint32_t block, uint32_t width, uint32_t x, uint32_t y);
uint32_t addr_PSMCT16(uint32_t block, uint32_t width, uint32_t x, uint32_t y);
uint32_t addr_PSMCT16S(uint32_t block, uint32_t width, uint32_t x, uint32_t y);
uint32_t addr_PSMCT16Z(uint32_t block, uint32_t width, uint32_t x, uint32_t y);
uint32_t addr_PSMCT16SZ(uint32_t block, uint32_t width, uint32_t x, uint32_t y);
uint32_t addr_PSMCT8(uint32_t block, uint32_t width, uint32_t x, uint32_t y);
uint32_t addr_PSMCT4(uint32_t block, uint32_t width, uint32_t x, uint32_t y);

struct VertexF
{
    union {
        struct
        {
            float x,y,w,r,g,b,a,q,u,v,s,t,fog;
        };
        float data[13];
    };
    double z;

    VertexF()
    {

    }

    VertexF(Vertex& vert)
    {
        x = (float)vert.x / 16.f;
        y = (float)vert.y / 16.f;
        z = vert.z;
        r = vert.rgbaq.r;
        g = vert.rgbaq.g;
        b = vert.rgbaq.b;
        a = vert.rgbaq.a;
        q = vert.rgbaq.q;
        u = vert.uv.u;
        v = vert.uv.v;
        s = vert.s;
        t = vert.t;
        fog = vert.fog;
    }

    VertexF operator-(const VertexF& rhs)
    {
        VertexF result;
        for(int i = 0; i < 13; i++)
        {
            result.data[i] = data[i] - rhs.data[i];
        }
        result.z = z - rhs.z;
        return result;
    }

    VertexF operator+(const VertexF& rhs)
    {
        VertexF result;
        for(int i = 0; i < 13; i++)
        {
            result.data[i] = data[i] + rhs.data[i];
        }
        result.z = z + rhs.z;
        return result;
    }

    VertexF& operator+=(const VertexF& rhs)
    {
        for(int i = 0; i < 13; i++)
        {
            data[i] = data[i] + rhs.data[i];
        }
        z += rhs.z;
        return *this;
    }

    VertexF operator*(float mult)
    {
        VertexF result;
        for(int i = 0; i < 13; i++)
        {
            result.data[i] = data[i] * mult;
        }
        result.z = z * mult;
        return result;
    }
};

typedef void (*GSDrawPixelPrologue)(int32_t x, int32_t y, uint32_t z, RGBAQ_REG& color);
typedef void (*GSTexLookupPrologue)(int16_t u, int16_t v, TexLookupInfo* info);

class GraphicsSynthesizerThread
{
    private:
#ifdef _MSC_VER
        constexpr static REG_64 abi_args[] = {RCX, RDX, R8, R9};
#else
        constexpr static REG_64 abi_args[] = {RDI, RSI, RDX, RCX};
#endif
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
        uint32_t screen_buffer[2048 * 2048];
        uint8_t clut_cache[1024];
        uint32_t CBP0, CBP1;

        //CSR/IMR stuff - to be merged into structs

        GS_IMR IMR;

        GSContext context1, context2;
        GSContext* current_ctx;

        JitBlock jit_draw_pixel_block, jit_tex_lookup_block;
        Emitter64 emitter_dp, emitter_tex;

        GSPixelJitHeap jit_draw_pixel_heap;
        GSTextureJitHeap jit_tex_lookup_heap;

        uint8_t* jit_draw_pixel_func;
        uint8_t* jit_tex_lookup_func;

        GSTexLookupPrologue jit_tex_lookup_prologue;
        GSDrawPixelPrologue jit_draw_pixel_prologue;

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

        //Used for the JIT to determine the ID of the draw pixel block
        uint64_t draw_pixel_state;
        uint64_t tex_lookup_state;

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

        uint32_t frame_color;
        bool frame_color_looked_up;

        static const unsigned int max_vertices[8];

        float log2_lookup[32768][4];

        void event_loop();

        //Swizzling routines
        uint32_t blockid_PSMCT32(uint32_t block, uint32_t width, uint32_t x, uint32_t y);
        uint32_t blockid_PSMCT32Z(uint32_t block, uint32_t width, uint32_t x, uint32_t y);
        uint32_t blockid_PSMCT16(uint32_t block, uint32_t width, uint32_t x, uint32_t y);
        uint32_t blockid_PSMCT16S(uint32_t block, uint32_t width, uint32_t x, uint32_t y);
        uint32_t blockid_PSMCT16Z(uint32_t block, uint32_t width, uint32_t x, uint32_t y);
        uint32_t blockid_PSMCT16SZ(uint32_t block, uint32_t width, uint32_t x, uint32_t y);
        uint32_t blockid_PSMCT8(uint32_t block, uint32_t width, uint32_t x, uint32_t y);
        uint32_t blockid_PSMCT4(uint32_t block, uint32_t width, uint32_t x, uint32_t y);

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
        void update_draw_pixel_state();
        void update_tex_lookup_state();
        uint8_t* get_jitted_draw_pixel(uint64_t state);

        void recompile_draw_pixel_prologue();
        GSPixelJitBlockRecord* recompile_draw_pixel(uint64_t state);
        void recompile_alpha_test();
        void recompile_depth_test();
        void recompile_alpha_blend();
        void jit_call_func(Emitter64& emitter, uint64_t addr);
        void jit_epilogue_draw_pixel();

        void recompile_tex_lookup_prologue();
        uint8_t* get_jitted_tex_lookup(uint64_t state);
        GSTextureJitBlockRecord* recompile_tex_lookup(uint64_t state);
        void recompile_clut_lookup();
        void recompile_csm2_lookup();
        void recompile_convert_16bit_tex(REG_64 color, REG_64 temp, REG_64 temp2);

        void vertex_kick(bool drawing_kick);
        bool depth_test(int32_t x, int32_t y, uint32_t z);
        void draw_pixel(int32_t x, int32_t y, uint32_t z, RGBAQ_REG& color);
        uint32_t lookup_frame_color(int32_t x, int32_t y);
        void render_primitive();
        void render_point();
        void render_line();
        void render_triangle();
        void render_triangle2();
        void render_half_triangle(float x0, float x1, int y0, int y1, VertexF& x_step, VertexF& y_step, VertexF& init,
                float step_x0, float step_x1, float scx1, float scx2, TexLookupInfo& tex_info);
        void render_sprite();
        void write_HWREG(uint64_t data);
        uint128_t local_to_host();
        void unpack_PSMCT24(uint64_t data, int offset, bool z_format);
        uint64_t pack_PSMCT24(bool z_format);
        void local_to_local();

        int32_t orient2D(const Vertex &v1, const Vertex &v2, const Vertex &v3);

        void reset();
        void memdump(uint32_t* target, uint16_t& width, uint16_t& height);

        uint32_t get_CRT_color(DISPFB& dispfb, uint32_t x, uint32_t y);
        void render_CRT(uint32_t* target);

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
        void wake_thread();
        void wait_for_return(GSReturn type, GSReturnMessage &data);
        void reset_fifos();
        void exit();
};
#endif // GSTHREAD_HPP
