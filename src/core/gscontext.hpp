#ifndef GSCONTEXT_HPP
#define GSCONTEXT_HPP
#include <cstdint>

struct TEX0
{
    uint32_t texture_base;
    uint32_t width;
    uint8_t format;
    uint16_t tex_width;
    uint16_t tex_height;
    bool use_alpha;
    uint8_t color_function;
    uint32_t CLUT_base;
    uint8_t CLUT_format;
    bool use_CSM2;
    uint16_t CLUT_offset;
    uint8_t CLUT_control;
};

struct TEX1
{
    bool LOD_method; //LCM
    uint8_t max_MIP_level; //MXL 0 - 6
    bool filter_larger; //MMAG
    uint8_t filter_smaller; //MMIN 0 - 6
    bool MTBA; //base address specification of MIPMAPs (0 = specified, 1 = automatic)
    uint8_t L;//2 bits
    float K;//sign bit, 7 bits integer, 4 bits decimal.
};

struct CLAMP
{
    uint8_t wrap_s;
    uint8_t wrap_t;
    uint16_t min_u, max_u, min_v, max_v;
};

struct UV_REG
{
    uint16_t u, v;
};

struct ST_REG
{
    float s, t;
};

struct XYOFFSET
{
    uint16_t x;
    uint16_t y;
};

//Contains levels 1-6 (0 is specified in TEX0)
struct MIPTBL
{
    uint32_t texture_base[6];
    uint32_t width[6];
};

struct SCISSOR
{
    uint16_t x1, x2, y1, y2;

    const bool empty() const noexcept
    {
        return x1 == x2 && y1 == y2;
    }
};

struct ALPHA
{
    uint8_t spec_A;
    uint8_t spec_B;
    uint8_t spec_C;
    uint8_t spec_D;

    uint8_t fixed_alpha;
};

struct TEST
{
    bool alpha_test;
    uint8_t alpha_method;
    uint8_t alpha_ref;
    uint8_t alpha_fail_method;
    bool dest_alpha_test;
    bool dest_alpha_method;
    bool depth_test;
    uint8_t depth_method;
};

struct FRAME
{
    uint32_t base_pointer;
    uint32_t width;
    uint8_t format;
    uint32_t mask;
};

struct ZBUF
{
    uint32_t base_pointer;
    uint8_t format;
    bool no_update;
};

struct GSContext
{
    TEX0 tex0;
    TEX1 tex1;
    CLAMP clamp;
    XYOFFSET xyoffset;
    MIPTBL miptbl;
    SCISSOR scissor;
    ALPHA alpha;
    TEST test;
    FRAME frame;
    ZBUF zbuf;
    bool FBA;

    void reset();

    void set_tex0(uint64_t value);
    void set_tex1(uint64_t value);
    void set_tex2(uint64_t value);
    void set_clamp(uint64_t value);
    void set_xyoffset(uint64_t value);
    void set_miptbl1(uint64_t value);
    void set_miptbl2(uint64_t value);
    void set_scissor(uint64_t value);
    void set_alpha(uint64_t value);
    void set_test(uint64_t value);
    void set_frame(uint64_t value);
    void set_zbuf(uint64_t value);
};

#endif // GSCONTEXT_HPP
