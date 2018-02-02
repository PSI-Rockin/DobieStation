#include <cstdio>
#include "gscontext.hpp"

void GSContext::reset()
{
    set_tex0(0);
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
    tex0.texture_base = (value & 0x3FFF) * 64;
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
    tex0.CLUT_base = ((value >> 37) & 0x3FFF) * 64;
    tex0.CLUT_format = (value >> 51) & 0xF;
    tex0.use_CSM2 = value & (1UL << 55);
    tex0.CLUT_offset = ((value >> 56) & 0x1F) * 16;
    tex0.CLUT_control = (value >> 61) & 0x7;

    printf("\nTEX0: $%08X_%08X", value >> 32, value & 0xFFFFFFFF);
}

void GSContext::set_xyoffset(uint64_t value)
{
    xyoffset.x = value & 0xFFFF;
    xyoffset.y = (value >> 32) & 0xFFFF;
    printf("\nXYOFFSET: $%08X_%08X", value >> 32, value & 0xFFFFFFFF);
}

void GSContext::set_scissor(uint64_t value)
{
    scissor.x1 = value & 0x7FF;
    scissor.x2 = (value >> 16) & 0x7FF;
    scissor.y1 = (value >> 32) & 0x7FF;
    scissor.y2 = (value >> 48) & 0x7FF;
    printf("\nSCISSOR: $%08X_%08X", value >> 32, value & 0xFFFFFFFF);
}

void GSContext::set_alpha(uint64_t value)
{
    alpha.spec_A = value & 0x3;
    alpha.spec_B = (value >> 2) & 0x3;
    alpha.spec_C = (value >> 4) & 0x3;
    alpha.spec_D = (value >> 6) & 0x3;
    alpha.fixed_alpha = (value >> 32) & 0xFF;
    printf("\nALPHA: $%08X_%08X", value >> 32, value & 0xFFFFFFFF);
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
    printf("\nTEST: $%08X", value & 0xFFFFFFFF);
}

void GSContext::set_frame(uint64_t value)
{
    frame.base_pointer = (value & 0x1FF) * 2048;
    frame.width = ((value >> 16) & 0x1F) * 64;
    frame.format = (value >> 24) & 0x3F;
    frame.mask = value >> 32;
    printf("\nFRAME: $%08X_%08X", value >> 32, value & 0xFFFFFFFF);
}

void GSContext::set_zbuf(uint64_t value)
{
    zbuf.base_pointer = (value & 0x1FF) * 2048;
    zbuf.format = (value >> 24) & 0xF;
    zbuf.no_update = value & (1UL << 32);
    printf("\nZBUF: $%08X_%08X", value >> 32, value & 0xFFFFFFFF);
}
