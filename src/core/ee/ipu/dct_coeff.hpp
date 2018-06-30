#ifndef DCT_COEFF_HPP
#define DCT_COEFF_HPP
#include "vlc_table.hpp"

struct RunLevelPair
{
    int run, level;
};

class DCT_Coeff : public VLC_Table
{
    protected:
        constexpr static int RUN_ESCAPE = 102;
    public:
        DCT_Coeff(VLC_Entry* table, int table_size, int max_bits);

        virtual bool get_end_of_block(IPU_FIFO& FIFO, uint32_t& result) = 0;
        virtual bool get_skip_block(IPU_FIFO& FIFO) = 0;
        virtual bool get_runlevel_pair(IPU_FIFO& FIFO, RunLevelPair& pair, bool MPEG1) = 0;

        bool peek_value(IPU_FIFO& FIFO, int bits, int& bit_count, uint32_t& result);
};

#endif // DCT_COEFF_HPP
