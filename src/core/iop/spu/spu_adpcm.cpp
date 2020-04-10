#include "spu_adpcm.hpp"
#include <cstdint>
#include <algorithm>

std::array<int16_t, 28> ADPCM_decoder::decode_block(uint8_t *block)
{
    std::array<int16_t, 28> pcm;

    shift_factor = (*block & 0xF);
    coef_index = ((*block >> 4) & 0xF);
    flags = *(block+1);

    block += 2;

    for (uint8_t i = 0; i < 28; i++)
    {
        int32_t sample;
        uint8_t byte = block[i/2];

        switch (i % 2)
        {
            case 0:
                sample = byte & 0x0F;
                break;
            case 1:
                sample = (byte >> 4) & 0x0F;
                break;
        }

        sample = (int16_t)((sample << 12) & 0xf000) >> shift_factor;
        sample += (ps_adpcm_coefs_i[coef_index][0]*hist1
                + ps_adpcm_coefs_i[coef_index][1]*hist2) >> 6;

        pcm[i] = clamp16(sample);

        hist2 = hist1;
        hist1 = sample;
    }

    return pcm;
}
