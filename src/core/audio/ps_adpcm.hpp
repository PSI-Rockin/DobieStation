#ifndef __PS_ADPCM_H
#define __PS_ADPCM_H
#include <vector>
#include <cstdint>


// Integer math version of ps-adpcm coefs
static const int ps_adpcm_coefs_i[5][2] = {
        {   0 ,   0 },
        {  60 ,   0 },
        { 115 , -52 },
        {  98 , -55 },
        { 122 , -60 },
};

//struct BlockInfo
//{
//
//    uint8_t flags;
//    uint8_t shift_factor;
//    uint8_t coef_index;
//};
//
struct ADPCM_block
{
        uint8_t data[14];
        uint8_t pos;
        uint8_t flags;
        uint8_t shift_factor, coef_index;
};

struct ADPCM_info
{
        int32_t hist1, hist2;
        ADPCM_block block;
};

class ADPCM_decoder
{
    public:
        std::vector<int16_t> decode_samples(ADPCM_info *info, int num_samples);
        ADPCM_block read_block(uint8_t *inbuf);
};


#endif // __PS_ADPCM_H_
