#ifndef GSTHREAD_HPP
#define GSTHREAD_HPP
#include <cstdint>
#include "gscontext.hpp"
#include "gs.hpp"

struct PRMODE
{
    bool gourand_shading;
    bool texture_mapping;
    bool fog;
    bool alpha_blend;
    bool antialiasing;
    bool use_UV;
    bool use_context2;
    bool fix_fragment_value;
};

struct PRIM_REG
{
    uint8_t prim_type;
    bool gourand_shading;
    bool texture_mapping;
    bool fog;
    bool alpha_blend;
    bool antialiasing;
    bool use_UV;
    bool use_context2;
    bool fix_fragment_value;
};

struct RGBAQ_REG
{
    uint8_t r, g, b, a;
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
    uint8_t trans_order;
};
struct TEXCLUT_REG
{
    uint16_t width, x, y;
};


struct Vertex
{
    int32_t x, y, z;

    RGBAQ_REG rgbaq;
    UV_REG uv;
    float s, t;
    void to_relative(XYOFFSET xyoffset)
    {
        x -= xyoffset.x;
        y -= xyoffset.y;
    }
};

class GraphicsSynthesizerThread
{
    private:
        bool frame_complete;
        int frame_count;
        uint8_t* local_mem;
        uint8_t CRT_mode;

        //CSR/IMR stuff - to be merged into structs

        GS_IMR IMR;

        GSContext context1, context2;
        GSContext* current_ctx;

        PRIM_REG PRIM;
        RGBAQ_REG RGBAQ;
        UV_REG UV;
        ST_REG ST;
        TEXCLUT_REG TEXCLUT;
        bool DTHE;
        bool COLCLAMP;
        bool use_PRIM;

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

        uint32_t get_word(uint32_t addr);
        void set_word(uint32_t addr, uint32_t value);

        uint32_t read_PSMCT32_block(uint32_t base, uint32_t width, uint32_t x, uint32_t y);
        uint16_t read_PSMCT16_block(uint32_t base, uint32_t width, uint32_t x, uint32_t y);
        void write_PSMCT32_block(uint32_t base, uint32_t width, uint32_t x, uint32_t y, uint32_t value);
        void write_PSMCT16_block(uint32_t base, uint32_t width, uint32_t x, uint32_t y, uint16_t value);

        bool depth_test(int32_t x, int32_t y, uint32_t z);

        void tex_lookup(int16_t u, int16_t v, const RGBAQ_REG& vtx_color, RGBAQ_REG& tex_color);
        void clut_lookup(uint8_t entry, RGBAQ_REG& tex_color, bool eight_bit);
        void clut_CSM2_lookup(uint8_t entry, RGBAQ_REG& tex_color);
        void vertex_kick(bool drawing_kick);
        void draw_pixel(int32_t x, int32_t y, uint32_t z, RGBAQ_REG& color, bool alpha_blending);
        void render_primitive();
        void render_point();
        void render_line();
        void render_triangle();
        void render_sprite();
        void write_HWREG(uint64_t data);
        void unpack_PSMCT24(uint64_t data, int offset);
        void host_to_host();

        int32_t orient2D(const Vertex &v1, const Vertex &v2, const Vertex &v3);


        //called from event loop
        void reset();
        void memdump();
        void render_CRT(uint32_t* target);

        void set_VBLANK(bool is_VBLANK);
        void assert_FINISH();
        void dump_texture(uint32_t* target, uint32_t start_addr, uint32_t width);


        void write64(uint32_t addr, uint64_t value);

        void set_RGBA(uint8_t r, uint8_t g, uint8_t b, uint8_t a);
        void set_STQ(uint32_t s, uint32_t t, uint32_t q);
        void set_UV(uint16_t u, uint16_t v);
        void set_Q(float q);
        void set_XYZ(uint32_t x, uint32_t y, uint32_t z, bool drawing_kick);

    public:
        GraphicsSynthesizerThread();
        ~GraphicsSynthesizerThread();
		
		static void event_loop(gs_fifo* fifo, gs_return_fifo* return_fifo);
};

inline uint32_t GraphicsSynthesizerThread::get_word(uint32_t addr)
{
    return *(uint32_t*)&local_mem[addr];
}

inline void GraphicsSynthesizerThread::set_word(uint32_t addr, uint32_t value)
{
    *(uint32_t*)&local_mem[addr] = value;
}

#endif // GSTHREAD_HPP
