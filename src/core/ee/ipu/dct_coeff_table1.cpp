#include <cstdio>
#include <cstdlib>
#include "dct_coeff_table1.hpp"
#include "../../errors.hpp"

#define printf(fmt, ...)(0)

VLC_Entry DCT_Coeff_Table1::table[] =
{
    {0x2, 0, 2},
    {0x2, 1, 3},
    {0x6, 2, 3},
    {0x7, 4, 4},

    {0x5, 3, 5},
    {0x7, 5, 5},
    {0x6, 7, 5},
    {0x1C, 11, 5},

    {0x1D, 16, 5},
    {0x6, 6, 6},
    {0x7, 8, 6},
    {0x5, 17, 6},

    {0x4, 24, 6},
    {0x1, 15, 6},
    {0x79, 18, 7},
    {0x7A, 20, 7},

    {0x7, 12, 7},
    {0x5, 13, 7},
    {0x78, 14, 7},
    {0x6, 9, 7},

    {0x4, 10, 7},
    {0x7B, 32, 7},
    {0x7C, 33, 7},
    {0x26, 19, 8},

    {0x21, 21, 8},
    {0x25, 22, 8},
    {0x24, 23, 8},
    {0x27, 25, 8},

    {0xFC, 26, 8},
    {0xFD, 27, 8},
    {0xFA, 48, 8},
    {0xFB, 49, 8},

    {0xFE, 50, 8},
    {0xFF, 51, 8},
    {0x23, 34, 8},
    {0x22, 35, 8},

    {0x20, 36, 8},
    {0x4, 28, 9},
    {0x5, 29, 9},
    {0x7, 30, 9}, //40

    {0xD, 31, 10},
    {0xC, 37, 10},
    {0x1C, 38, 12},
    {0x12, 39, 12}, //44

    {0x1E, 40, 12},
    {0x15, 41, 12},
    {0x11, 42, 12},
    {0x1F, 43, 12}, //48

    {0x1A, 44, 12},
    {0x19, 45, 12},
    {0x17, 46, 12},
    {0x16, 47, 12}, //52

    {0x16, 52, 13},
    {0x15, 53, 13},
    {0x14, 54, 13},
    {0x13, 55, 13}, //56

    {0x12, 56, 13},
    {0x11, 57, 13},
    {0x10, 58, 13},
    {0x1F, 59, 13}, //60

    {0x1E, 60, 13},
    {0x1D, 61, 13},
    {0x1C, 62, 13},
    {0x1B, 63, 13}, //64

    {0x1F, 64, 14},
    {0x1E, 65, 14},
    {0x1D, 66, 14},
    {0x1C, 67, 14}, //68

    {0x1B, 68, 14},
    {0x1A, 69, 14},
    {0x19, 70, 14},
    {0x18, 71, 14}, //72

    {0x17, 72, 14},
    {0x16, 73, 14},
    {0x15, 74, 14},
    {0x14, 75, 14}, //76

    {0x13, 76, 14},
    {0x12, 77, 14},
    {0x11, 78, 14},
    {0x10, 79, 14}, //80

    {0x18, 80, 15},
    {0x17, 81, 15},
    {0x16, 82, 15},
    {0x15, 83, 15}, //84

    {0x14, 84, 15},
    {0x13, 85, 15},
    {0x12, 86, 15},
    {0x11, 87, 15}, //88

    {0x10, 88, 15},
    {0x1F, 89, 15},
    {0x1E, 90, 15},
    {0x1D, 91, 15}, //92

    {0x1C, 92, 15},
    {0x1B, 93, 15},
    {0x1A, 94, 15},
    {0x19, 95, 15}, //96

    {0x13, 96, 16},
    {0x12, 97, 16},
    {0x11, 98, 16},
    {0x10, 99, 16}, //100

    {0x14, 100, 16},
    {0x1A, 101, 16},
    {0x19, 102, 16},
    {0x18, 103, 16}, //104

    {0x17, 104, 16},
    {0x16, 105, 16},
    {0x15, 106, 16},
    {0x1F, 107, 16}, //108

    {0x1E, 108, 16},
    {0x1D, 109, 16},
    {0x1C, 110, 16},
    {0x1B, 111, 16}  //112
};

RunLevelPair DCT_Coeff_Table1::runlevel_table[] =
{
    //Page 134
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

    //Page 135
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

    //Page 132
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

unsigned int DCT_Coeff_Table1::index_table[16] =
{
    0,
    0,
    1,
    3,		//4
    4,		//5
    9,		//6
    14,		//7
    23,		//8
    37,		//9
    40,		//10
    40,		//11
    42,		//12
    52,		//13
    64,		//14
    80,		//15
    96,		//16
};

DCT_Coeff_Table1::DCT_Coeff_Table1() :
    DCT_Coeff(table, SIZE, 16, index_table)
{

}

bool DCT_Coeff_Table1::get_end_of_block(IPU_FIFO &FIFO, uint32_t &result)
{
    if (!FIFO.get_bits(result, 4))
        return false;

    printf("[DCT_Coeff_Table1] EOB: $%08X\n", result);
    result = (result == 6);
    return true;
}

bool DCT_Coeff_Table1::get_skip_block(IPU_FIFO &FIFO)
{
    uint32_t blorp;
    if (!FIFO.get_bits(blorp, 4))
        return false;
    FIFO.advance_stream(4);
    return true;
}

bool DCT_Coeff_Table1::get_runlevel_pair(IPU_FIFO &FIFO, RunLevelPair &pair, bool MPEG1)
{
    VLC_Entry entry;
    if (!peek_symbol(FIFO, entry))
        return false;

    printf("Key: $%08X Value: $%08X Bits: %d\n", entry.key, entry.value, entry.bits);
    int bit_count = entry.bits;
    RunLevelPair cur_pair = runlevel_table[entry.value];
    if (cur_pair.run == RUN_ESCAPE)
    {
        printf("[DCT_Coeff_Table1] RUN_ESCAPE\n");
        uint32_t run = 0;
        if (!peek_value(FIFO, 6, bit_count, run))
            return false;

        pair.run = run;

        if (MPEG1)
        {
            Errors::die("MPEG1???\n");
        }

        uint32_t level = 0;
        if (!peek_value(FIFO, 12, bit_count, level))
            return false;

        if (level & 0x800)
        {
            level |= 0xF000;
            level = (int16_t)level;
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

bool DCT_Coeff_Table1::get_runlevel_pair_dc(IPU_FIFO &FIFO, RunLevelPair &pair, bool MPEG1)
{
    //DCT_Coeff_Table1 only gets called by intra macroblocks, so this should never happen
    Errors::die("get_runlevel_pair_dc should never happen");
}
