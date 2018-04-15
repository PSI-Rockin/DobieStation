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

struct SCISSOR
{
    uint16_t x1, x2, y1, y2;
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
    XYOFFSET xyoffset;
    SCISSOR scissor;
    ALPHA alpha;
    TEST test;
    FRAME frame;
    ZBUF zbuf;

    void reset();

    void set_tex0(uint64_t value);
    void set_xyoffset(uint64_t value);
    void set_scissor(uint64_t value);
    void set_alpha(uint64_t value);
    void set_test(uint64_t value);
    void set_frame(uint64_t value);
    void set_zbuf(uint64_t value);
};

#endif // GSCONTEXT_HPP
