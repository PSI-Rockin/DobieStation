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
    {0x5, 14, 7},

    //20
    {0x26, 16, 8},
    {0x21, 17, 8},
    {0x25, 18, 8},
    {0x24, 19, 8},

    //24
    {0x27, 20, 8},
    {0x23, 21, 8},
    {0x22, 22, 8},
    {0x20, 23, 8},

    //28
    {0xA, 24, 10},
    {0xC, 25, 10},
    {0xB, 26, 10},
    {0xF, 27, 10},

    //32
    {0x9, 28, 10},
    {0xE, 29, 10},
    {0xD, 30, 10},
    {0x8, 31, 10},

    //36
    {0x1D, 32, 12},
    {0x18, 33, 12},
    {0x13, 34, 12},
    {0x10, 35, 12},

    //40
    {0x1B, 36, 12},
    {0x14, 37, 12},
    {0x1C, 38, 12},
    {0x12, 39, 12},

    //44
    {0x1E, 40, 12},
    {0x15, 41, 12},
    {0x11, 42, 12},
    {0x1F, 43, 12},

    //48
    {0x1A, 44, 12},
    {0x19, 45, 12},
    {0x17, 46, 12},
    {0x16, 47, 12},

    //52
    {0x1A, 48, 13},
    {0x19, 49, 13},
    {0x18, 50, 13},
    {0x17, 51, 13},

    //56
    {0x16, 52, 13},
    {0x15, 53, 13},
    {0x14, 54, 13},
    {0x13, 55, 13},

    //60
    {0x12, 56, 13},
    {0x11, 57, 13},
    {0x10, 58, 13},
    {0x1F, 59, 13},

    //64
    {0x1E, 60, 13},
    {0x1D, 61, 13},
    {0x1C, 62, 13},
    {0x1B, 63, 13},

    //68
    {0x1F, 64, 14},
    {0x1E, 65, 14},
    {0x1D, 66, 14},
    {0x1C, 67, 14},

    //72
    {0x1B, 68, 14},
    {0x1A, 69, 14},
    {0x19, 70, 14},
    {0x18, 71, 14},

    //76
    {0x17, 72, 14},
    {0x16, 73, 14},
    {0x15, 74, 14},
    {0x14, 75, 14},

    //80
    {0x13, 76, 14},
    {0x12, 77, 14},
    {0x11, 78, 14},
    {0x10, 79, 14},

    //84
    {0x18, 80, 15},
    {0x17, 81, 15},
    {0x16, 82, 15},
    {0x15, 83, 15},

    //88
    {0x14, 84, 15},
    {0x13, 85, 15},
    {0x12, 86, 15},
    {0x11, 87, 15},

    //92
    {0x10, 88, 15},
    {0x1F, 89, 15},
    {0x1E, 90, 15},
    {0x1D, 91, 15},

    //96
    {0x1C, 92, 15},
    {0x1B, 93, 15},
    {0x1A, 94, 15},
    {0x19, 95, 15},

    //100
    {0x13, 96, 16},
    {0x12, 97, 16},
    {0x11, 98, 16},
    {0x10, 99, 16},

    //104
    {0x14, 100, 16},
    {0x1A, 101, 16},
    {0x19, 102, 16},
    {0x18, 103, 16},

    //108
    {0x17, 104, 16},
    {0x16, 105, 16},
    {0x15, 106, 16},
    {0x1F, 107, 16},

    //102
    {0x1E, 108, 16},
    {0x1D, 109, 16},
    {0x1C, 110, 16},
    {0x1B, 111, 16},
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
    DCT_Coeff(table, SIZE, 16)
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
    if (!FIFO.get_bits(blorp, 2))
        return false;
    FIFO.advance_stream(2);
    return true;
}

bool DCT_Coeff_Table0::get_runlevel_pair(IPU_FIFO &FIFO, RunLevelPair &pair, bool MPEG1)
{
    VLC_Entry entry;
    if (!peek_symbol(FIFO, entry))
        return false;

    int bit_count = entry.bits;
    RunLevelPair cur_pair = runlevel_table[entry.value];
    printf("Run level pair index: %d (Key: $%02X)\n", entry.value, entry.key);
    if (cur_pair.run == RUN_ESCAPE)
    {
        uint32_t run;
        if (!peek_value(FIFO, 6, bit_count, run))
            return false;

        pair.run = run;

        uint32_t level;
        if (MPEG1)
        {
            if (!peek_value(FIFO, 8, bit_count, level))
                return false;

            if (!level)
            {
                if (!peek_value(FIFO, 8, bit_count, level))
                    return false;
            }
            else if ((uint8_t)level == 128)
            {
                if (!peek_value(FIFO, 8, bit_count, level))
                    return false;
                level -= 256;
            }
            else if (level > 128)
                level -= 256;
        }
        else
        {
            if (!peek_value(FIFO, 12, bit_count, level))
                return false;

            if (level & 0x800)
            {
                level |= 0xF000;
                level = (int16_t)level;
            }
        }
        pair.level = level;
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

    FIFO.advance_stream(bit_count);
    return true;
}

bool DCT_Coeff_Table0::get_runlevel_pair_dc(IPU_FIFO &FIFO, RunLevelPair &pair, bool MPEG1)
{
    uint32_t special;
    if (!FIFO.get_bits(special, 1))
        return false;

    if (special)
    {
        uint32_t sign;
        if (!FIFO.get_bits(sign, 2))
            return false;
        FIFO.advance_stream(2);
        pair.run = 0;

        if (sign & 0x1)
            pair.level = 0 - 1;
        else
            pair.level = 1;
        return true;
    }
    else
        return get_runlevel_pair(FIFO, pair, MPEG1);
}
