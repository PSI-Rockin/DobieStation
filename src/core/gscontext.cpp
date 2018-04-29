#include <cstdio>
#include "gscontext.hpp"

void GSContext::reset()
{
    set_tex0(0);
    set_clamp(0);
    set_xyoffset(0);
    set_scissor(0);
    set_alpha(0);
    set_test(0);
    set_frame(0);
    set_zbuf(0);
    zbuf.no_update = true;
}

void GSContext::set_tex0(uint64_t value)
{
    tex0.texture_base = (value & 0x3FFF) * 64 * 4;
    tex0.width = ((value >> 14) & 0x3F) * 64;
    tex0.format = (value >> 20) & 0x3F;
    tex0.tex_width = (value >> 26) & 0xF;
    if (tex0.tex_width > 10)
        tex0.tex_width = 1 << 10;
    else
        tex0.tex_width = 1 << tex0.tex_width;
    tex0.tex_height = (value >> 30) & 0xF;
    if (tex0.tex_height > 10)
        tex0.tex_height = 1 << 10;
    else
        tex0.tex_height = 1 << tex0.tex_height;

    tex0.use_alpha = value & (1UL << 34);
    tex0.color_function = (value >> 35) & 0x3;
    tex0.CLUT_base = ((value >> 37) & 0x3FFF) * 64 * 4;
    tex0.CLUT_format = (value >> 51) & 0xF;
    tex0.use_CSM2 = value & (1UL << 55);
    tex0.CLUT_offset = ((value >> 56) & 0x1F) * 16;
    tex0.CLUT_control = (value >> 61) & 0x7;

    printf("TEX0: $%08X_%08X\n", value >> 32, value & 0xFFFFFFFF);
    printf("Tex base: $%08X\n", tex0.texture_base);
    printf("Buffer width: %d\n", tex0.width);
    printf("Tex width: %d Height: %d\n", tex0.tex_width, tex0.tex_height);
    printf("Use alpha: %d\n", tex0.use_alpha);
    printf("Color function: $%02X\n", tex0.color_function);
    printf("CLUT base: $%08X\n", tex0.CLUT_base);
    printf("CLUT format: $%02X\n", tex0.CLUT_format);
    printf("Use CSM2: %d\n", tex0.use_CSM2);
    printf("CLUT offset: $%08X\n", tex0.CLUT_offset);
}

void GSContext::set_tex2(uint64_t value)
{
    tex0.format = (value >> 20) & 0x3F;
    tex0.CLUT_base = ((value >> 37) & 0x3FFF) * 64 * 4;
    tex0.CLUT_format = (value >> 51) & 0xF;
    tex0.use_CSM2 = value & (1UL << 55);
    tex0.CLUT_offset = ((value >> 56) & 0x1F) * 16;
    tex0.CLUT_control = (value >> 61) & 0x7;

    printf("TEX2: $%08X_%08X\n", value >> 32, value);
    printf("CLUT base: $%08X\n", tex0.CLUT_base);
    printf("CLUT format: $%02X\n", tex0.CLUT_format);
    printf("Use CSM2: %d\n", tex0.use_CSM2);
    printf("CLUT offset: $%08X\n", tex0.CLUT_offset);
}

void GSContext::set_clamp(uint64_t value)
{
    clamp.wrap_s = value & 0x3;
    clamp.wrap_t = (value >> 2) & 0x3;
    clamp.min_u = (value >> 4) & 0x3FF;
    clamp.max_u = (value >> 14) & 0x3FF;
    clamp.min_v = (value >> 24) & 0x3FF;
    clamp.max_v = (value >> 34) & 0x3FF;
    printf("CLAMP: $%08X_%08X\n", value >> 32, value);
}

void GSContext::set_xyoffset(uint64_t value)
{
    xyoffset.x = value & 0xFFFF;
    xyoffset.y = (value >> 32) & 0xFFFF;
    printf("XYOFFSET: $%08X_%08X\n", value >> 32, value & 0xFFFFFFFF);
}

void GSContext::set_scissor(uint64_t value)
{
    //Shift by four to compensate for the 4 decimal bits in vertex coords
    scissor.x1 = (value & 0x7FF) << 4;
    scissor.x2 = ((value >> 16) & 0x7FF) << 4;
    scissor.y1 = ((value >> 32) & 0x7FF) << 4;
    scissor.y2 = ((value >> 48) & 0x7FF) << 4;
    printf("SCISSOR: $%08X_%08X\n", value >> 32, value & 0xFFFFFFFF);
    printf("Coords: (%d, %d), (%d, %d)\n", scissor.x1, scissor.y1, scissor.x2, scissor.y2);
}

void GSContext::set_alpha(uint64_t value)
{
    alpha.spec_A = value & 0x3;
    alpha.spec_B = (value >> 2) & 0x3;
    alpha.spec_C = (value >> 4) & 0x3;
    alpha.spec_D = (value >> 6) & 0x3;
    alpha.fixed_alpha = (value >> 32) & 0xFF;
    printf("ALPHA: $%08X_%08X\n", value >> 32, value & 0xFFFFFFFF);
}

void GSContext::set_test(uint64_t value)
{
    test.alpha_test = value & 0x1;
    test.alpha_method = (value >> 1) & 0x7;
    test.alpha_ref = (value >> 4) & 0xFF;
    test.alpha_fail_method = (value >> 12) & 0x3;
    test.dest_alpha_test = value & (1 << 14);
    test.dest_alpha_method = value & (1 << 15);
    test.depth_test = value & (1 << 16);
    test.depth_method = (value >> 17) & 0x3;
    printf("TEST: $%08X\n", value & 0xFFFFFFFF);
}

void GSContext::set_frame(uint64_t value)
{
    frame.base_pointer = (value & 0x1FF) * 2048 * 4;
    frame.width = ((value >> 16) & 0x1F) * 64;
    frame.format = (value >> 24) & 0x3F;
    frame.mask = value >> 32;
    printf("FRAME: $%08X_%08X\n", value >> 32, value & 0xFFFFFFFF);
    printf("Width: %d\n", frame.width);
    printf("Format: %d\n", frame.format);
}

void GSContext::set_zbuf(uint64_t value)
{
    zbuf.base_pointer = (value & 0x1FF) * 2048 * 4;
    zbuf.format = (value >> 24) & 0xF;
    zbuf.no_update = value & (1UL << 32);
    printf("ZBUF: $%08X_%08X\n", value >> 32, value & 0xFFFFFFFF);
    printf("Base pointer: $%08X\n", zbuf.base_pointer);
    printf("Format: $%02X\n", zbuf.format);
}
