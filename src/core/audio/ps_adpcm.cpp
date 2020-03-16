#include "ps_adpcm.hpp"
#include <cstdint>
#include <vector>

int16_t clamp16(int32_t val)
{
    return std::max(std::min(INT16_MAX, val), INT16_MIN);
}


std::vector<int16_t> ADPCM_decoder::decode_samples(ADPCM_info &info, int num_samples)
{
    std::vector<int16_t> pcm;


    for (int i = 0; i < num_samples; i++)
    {

        uint8_t byte = info.block.data[info.block.pos+i];

        for(int j = 0; j < 2; j++)
        {
            int32_t sample;

            switch (j)
            {
                case 0:
                    sample = byte & 0x0F;
                    break;
                case 1:
                    sample = (byte >> 4) & 0x0F;
                    break;
            }

            sample = (int16_t)((sample << 12) & 0xf000) >> info.block.shift_factor;
            sample += (ps_adpcm_coefs_i[info.block.coef_index][0]*info.hist1
                       + ps_adpcm_coefs_i[info.block.coef_index][1]*info.hist2) >> 6;

            sample = clamp16(sample);
            pcm.push_back(sample);

            info.hist2 = info.hist1;
            info.hist1 = sample;

        }
    }

    return pcm;
}

ADPCM_block ADPCM_decoder::read_block(uint8_t *inbuf)
{
    ADPCM_block block;
    uint8_t *inp = inbuf;
    block.shift_factor = (*inp & 0xF);
    block.coef_index = ((*inp >> 4) & 0xF);
    block.flags = *(inp+1);

    inp += 2;

    for(int i = 0; i < 14; i++)
    {
        block.data[i] = *(inp+i);
    }
    return block;
}

