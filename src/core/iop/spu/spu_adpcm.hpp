#ifndef __PS_ADPCM_H
#define __PS_ADPCM_H
#include <array>
#include <cstdint>
#include "spu_utils.hpp"


// Integer math version of ps-adpcm coefs
static const int ps_adpcm_coefs_i[5][2] = {
        {   0 ,   0 },
        {  60 ,   0 },
        { 115 , -52 },
        {  98 , -55 },
        { 122 , -60 },
};

class ADPCM_decoder
{
    public:
        std::array<int16_t, 28> decode_block(uint8_t *block);
        uint8_t flags;
    private:
        uint8_t block[14];
        uint8_t shift_factor, coef_index;
        int32_t hist1, hist2;
};


#endif // __PS_ADPCM_H_
