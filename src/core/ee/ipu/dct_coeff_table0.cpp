#include <cstdio>
#include <cstdlib>
#include "dct_coeff_table0.hpp"

VLC_Entry DCT_Coeff_Table0::table[] =
{
    {0x3, 0, 2},
    {0x3, 1, 3},
    {0x4, 2, 4},
    {0x5, 3, 4},

    {0x5, 4, 5},
    {0x7, 5, 5},
    {0x6, 6, 5},
    {0x6, 7, 6},

    {0x7, 8, 6},
    {0x5, 9, 6},
    {0x4, 10, 6},
    {0x1, 15, 6},

    {0x6, 11, 7},
    {0x4, 12, 7},
    {0x7, 13, 7},
    {0x5, 14, 7}
};

RunLevelPair DCT_Coeff_Table0::runlevel_table[] =
{
    { 0,			1	},
    { 1,			1	},
    { 0,			2	},
    { 2,			1	},
    { 0,			3	},
    { 3,			1	},
    { 4,			1	},
    { 1,			2	},
    { 5,			1	},
    { 6,			1	},
    { 7,			1	},
    { 0,			4	},
    { 2,			2	},
    { 8,			1	},
    { 9,			1	},
    { RUN_ESCAPE,	0	},
    { 0,			5	},
    { 0,			6	},
    { 1,			3	},
    { 3,			2	},
    { 10,			1	},
    { 11,			1	},
    { 12,			1	},
    { 13,			1	},
    { 0,			7	},
    { 1,			4	},
    { 2,			3	},
    { 4,			2	},
    { 5,			2	},
    { 14,			1	},
    { 15,			1	},
    { 16,			1	},

    { 0,			8	},
    { 0,			9	},
    { 0,			10	},
    { 0,			11	},
    { 1,			5	},
    { 2,			4	},
    { 3,			3	},
    { 4,			3	},
    { 6,			2	},
    { 7,			2	},
    { 8,			2	},
    { 17,			1	},
    { 18,			1	},
    { 19,			1	},
    { 20,			1	},
    { 21,			1	},
    { 0,			12	},
    { 0,			13	},
    { 0,			14	},
    { 0,			15	},
    { 1,			6	},
    { 1,			7	},
    { 2,			5	},
    { 3,			4	},
    { 5,			3	},
    { 9,			2	},
    { 10,			2	},
    { 22,			1	},
    { 23,			1	},
    { 24,			1	},
    { 25,			1	},
    { 26,			1	},

    { 0,			16	},
    { 0,			17	},
    { 0,			18	},
    { 0,			19	},
    { 0,			20	},
    { 0,			21	},
    { 0,			22	},
    { 0,			23	},
    { 0,			24	},
    { 0,			25	},
    { 0,			26	},
    { 0,			27	},
    { 0,			28	},
    { 0,			29	},
    { 0,			30	},
    { 0,			31	},
    { 0,			32	},
    { 0,			33	},
    { 0,			34	},
    { 0,			35	},
    { 0,			36	},
    { 0,			37	},
    { 0,			38	},
    { 0,			39	},
    { 0,			40	},
    { 1,			8	},
    { 1,			9	},
    { 1,			10	},
    { 1,			11	},
    { 1,			12	},
    { 1,			13	},
    { 1,			14	},

    { 1,			15	},
    { 1,			16	},
    { 1,			17	},
    { 1,			18	},
    { 6,			3	},
    { 11,			2	},
    { 12,			2	},
    { 13,			2	},
    { 14,			2	},
    { 15,			2	},
    { 16,			2	},
    { 27,			1	},
    { 28,			1	},
    { 29,			1	},
    { 30,			1	},
    { 31,			1	},
};

DCT_Coeff_Table0::DCT_Coeff_Table0() :
    DCT_Coeff(table, SIZE, 32)
{

}

bool DCT_Coeff_Table0::get_end_of_block(IPU_FIFO &FIFO, uint32_t &result)
{
    if (!FIFO.get_bits(result, 2))
        return false;

    printf("[DCT_Coeff_Table0] EOB: $%08X\n", result);
    result = (result == 2);
    return true;
}

bool DCT_Coeff_Table0::get_skip_block(IPU_FIFO &FIFO)
{
    uint32_t blorp;
    return FIFO.get_bits(blorp, 2);
}

bool DCT_Coeff_Table0::get_runlevel_pair(IPU_FIFO &FIFO, RunLevelPair &pair, bool MPEG1)
{
    VLC_Entry entry;
    if (!peek_symbol(FIFO, entry))
    {
        printf("[DCT_Coeff_Table0] Symbol not found\n");
        exit(1);
        return false;
    }

    int bit_count = entry.bits;
    RunLevelPair cur_pair = runlevel_table[entry.value];
    printf("Run level pair index: %d (Key: $%02X)\n", entry.value, entry.key);
    if (cur_pair.run == RUN_ESCAPE)
    {
        printf("[DCT_Coeff_Table0] Run escape\n");
        exit(1);
    }
    else
    {
        uint32_t sign = 0;
        if (!peek_value(FIFO, 1, bit_count, sign))
            return false;

        pair.run = cur_pair.run;
        if (sign)
            pair.level = -cur_pair.level;
        else
            pair.level = cur_pair.level;
    }

    FIFO.advance_stream(entry.bits);
    return true;
}
