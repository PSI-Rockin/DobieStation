#include <cstdio>
#include <cstdlib>
#include "ipu.hpp"

VLC_Entry ImageProcessingUnit::macroblock_increment[] =
{
    {0x1, 0x10001, 1},
    {0x3, 0x30002, 3},
    {0x2, 0x30003, 3}
};

VLC_Entry ImageProcessingUnit::macroblock_I_pic[] =
{
    {0x1, 0x10001, 1},
    {0x1, 0x20011, 2}
};

ImageProcessingUnit::ImageProcessingUnit()
{

}

void ImageProcessingUnit::reset()
{
    VDEC_table = nullptr;
    std::queue<uint128_t> empty, empty2;
    in_FIFO.swap(empty);
    out_FIFO.swap(empty);

    ctrl.error_code = false;
    ctrl.start_code = false;
    ctrl.intra_DC_precision = 0;
    ctrl.MPEG1 = false;
    ctrl.picture_type = 0;
    ctrl.busy = false;
    command = 0;
    command_option = 0;
    bytes_left = 0;
    command_decoding = false;
    bit_pointer = 0;
}

void ImageProcessingUnit::run()
{
    if (ctrl.busy)
    {
        switch (command)
        {
            case 0x00:
            {
                std::queue<uint128_t> empty;
                in_FIFO.swap(empty);
                ctrl.busy = false;
                bit_pointer = command_option & 0x7F;
            }
                break;
            case 0x01:
                command_decoding = false;
                ctrl.busy = false;
                break;
            case 0x02:
                if (in_FIFO.size())
                    process_BDEC();
                break;
            case 0x03:
                if (in_FIFO.size())
                    process_VDEC();
                break;
            case 0x04:
                if (in_FIFO.size())
                    process_FDEC();
                break;
            case 0x05:
                while (bytes_left && in_FIFO.size())
                {
                    in_FIFO.pop();
                    bytes_left -= 16;
                }
                if (bytes_left <= 0)
                    ctrl.busy = false;
                break;
            case 0x06:
                while (bytes_left && in_FIFO.size())
                {
                    uint128_t quad = in_FIFO.front();
                    in_FIFO.pop();
                    for (int i = 0; i < 8; i++)
                    {
                        int index = (32 - bytes_left) >> 1;
                        VQCLUT[index] = quad._u16[index];
                        bytes_left -= 2;
                    }
                }
                if (bytes_left <= 0)
                    ctrl.busy = false;
                break;
            case 0x09:
                ctrl.busy = false;
                break;
            default:
                printf("[IPU] Unrecognized command $%02X\n", command);
                exit(1);
        }
    }
}

void ImageProcessingUnit::advance_stream(uint8_t amount)
{
    if (amount > 32)
        amount = 32;

    bit_pointer += amount;

    if (bit_pointer > (in_FIFO.size() * 128))
    {
        printf("[IPU] Bit pointer exceeds FIFO size!\n");
        exit(1);
    }
    while (bit_pointer >= 128)
    {
        bit_pointer -= 128;
        in_FIFO.pop();
    }
}

bool ImageProcessingUnit::get_bits(uint32_t &data, int bits)
{
    int bits_available = (in_FIFO.size() * 128) - bit_pointer;

    if (bits_available < bits)
        return false;

    //MPEG is big-endian...
    uint8_t blorp[8];
    int index = (bit_pointer & ~0x1F) / 8;
    for (int i = 0; i < 8; i++)
        blorp[7 - i] = in_FIFO.front()._u8[index + i];

    int shift = 64 - (bit_pointer % 32) - bits;
    uint64_t mask = ~0x0ULL >> (64 - bits);
    data = (*(uint64_t*)&blorp[0] >> shift) & mask;

    return true;
}

void ImageProcessingUnit::process_BDEC()
{
    while (true)
    {
        switch (bdec.state)
        {
            case BDEC_STATE::ADVANCE:
                advance_stream(command_option & 0x3F);
                bdec.state = BDEC_STATE::GET_CBP;
                break;
            case BDEC_STATE::GET_CBP:
                printf("[IPU] Get CBP!\n");
                if (!bdec.intra)
                {
                    printf("[IPU] Non-intra macroblock in BDEC!\n");
                    exit(1);
                }
                else
                    bdec.coded_block_pattern = 0x3F;
                bdec.state = BDEC_STATE::RESET_DC;
                break;
            case BDEC_STATE::RESET_DC:
                if (command_option & (1 << 26))
                {
                    printf("[IPU] Reset DC!\n");
                }
                bdec.state = BDEC_STATE::BEGIN_DECODING;
                break;
            case BDEC_STATE::BEGIN_DECODING:
                printf("[IPU] Begin decoding!\n");
                if (bdec.coded_block_pattern & (1 << (5 - bdec.block_index)))
                    bdec.state = BDEC_STATE::READ_COEFFS;
                else
                    bdec.state = BDEC_STATE::LOAD_NEXT_BLOCK;
                break;
            case BDEC_STATE::READ_COEFFS:
                printf("[IPU] Read coeffs!\n");
                bdec.read_coeff_state = BDEC_Command::INIT;
                if (!BDEC_read_coeffs())
                    return;
                exit(1);
                bdec.state = BDEC_STATE::LOAD_NEXT_BLOCK;
                break;
            case BDEC_STATE::LOAD_NEXT_BLOCK:
                printf("[IPU] Load next block!\n");
                bdec.block_index++;
                if (bdec.block_index == 6)
                    bdec.state = BDEC_STATE::DONE;
                else
                    bdec.state = BDEC_STATE::BEGIN_DECODING;
                break;
            case BDEC_STATE::DONE:
                printf("[IPU] BDEC successful!\n");
                exit(1);
                break;
        }
    }
}

bool ImageProcessingUnit::BDEC_read_coeffs()
{
    while (true)
    {
        switch (bdec.read_coeff_state)
        {
            case BDEC_Command::INIT:
                printf("[IPU] READ_COEFF Init!\n");
                if (bdec.intra)
                    bdec.read_coeff_state = BDEC_Command::READ_DC_DIFF;
                else
                    bdec.read_coeff_state = BDEC_Command::CHECK_END;
                break;
            case BDEC_Command::READ_DC_DIFF:
                printf("[IPU] READ_COEFF Read DC diffs!\n");
                exit(1);
                break;
            case BDEC_Command::CHECK_END:
                printf("[IPU] READ_COEFF Check end of block!\n");
                exit(1);
                break;
        }
    }
    return true;
}

void ImageProcessingUnit::process_VDEC()
{
    advance_stream(command_option & 0x3F);
    int table = command_option >> 26;
    int entry_count;
    int max_bits;
    switch (table)
    {
        case 0:
            VDEC_table = macroblock_increment;
            entry_count = sizeof(macroblock_increment);
            max_bits = 11;
            break;
        case 1:
            switch (ctrl.picture_type)
            {
                case 0x1:
                    VDEC_table = macroblock_I_pic;
                    entry_count = sizeof(macroblock_I_pic);
                    max_bits = 2;
                    break;
                default:
                    printf("[IPU] Unrecognized Macroblock Type %d!\n", ctrl.picture_type);
                    exit(1);
            }
            break;
        default:
            printf("[IPU] Unrecognized table id %d in VDEC!\n", table);
            exit(1);
    }

    entry_count /= sizeof(VLC_Entry);

    printf("[IPU] Entries: %d\n", entry_count);

    bool entry_found = false;
    while (true)
    {
        switch (vdec_state)
        {
            case VDEC_STATE::ADVANCE:
                advance_stream(command_option & 0x3F);
                vdec_state = VDEC_STATE::DECODE;
                break;
            case VDEC_STATE::DECODE:
                for (int i = 0; i < max_bits; i++)
                {
                    uint32_t entry;
                    int bits = i + 1;
                    if (!get_bits(entry, bits))
                        return;
                    for (int j = 0; j < entry_count; j++)
                    {
                        if (bits != VDEC_table[j].bits)
                            continue;

                        if (entry == VDEC_table[j].key)
                        {
                            command_output = VDEC_table[j].value;
                            advance_stream(bits);
                            entry_found = true;
                            break;
                        }
                    }
                    if (entry_found)
                    {
                        vdec_state = VDEC_STATE::DONE;
                        break;
                    }
                }
                if (!entry_found)
                {
                    printf("[IPU] No matching entry found for VDEC type %d\n", table);
                    exit(1);
                }
                break;
            case VDEC_STATE::DONE:
                ctrl.busy = false;
                command_decoding = false;
                return;
        }
    }
}

void ImageProcessingUnit::process_FDEC()
{
    while (true)
    {
        switch (fdec_state)
        {
            case VDEC_STATE::ADVANCE:
                advance_stream(command_option & 0x3F);
                fdec_state = VDEC_STATE::DECODE;
                break;
            case VDEC_STATE::DECODE:
                if (!get_bits(command_output, 32))
                    return;
                fdec_state = VDEC_STATE::DONE;
                break;
            case VDEC_STATE::DONE:
                command_decoding = false;
                ctrl.busy = false;
                printf("[IPU] FDEC result: $%08X\n", command_output);
                return;
        }
    }
}

uint64_t ImageProcessingUnit::read_command()
{
    uint64_t reg = 0;
    reg |= command_output;
    reg |= (uint64_t)command_decoding << 63UL;
    return reg;
}

uint32_t ImageProcessingUnit::read_control()
{
    uint32_t reg = 0;
    reg |= in_FIFO.size();
    reg |= ctrl.error_code << 14;
    reg |= ctrl.start_code << 15;
    reg |= ctrl.intra_DC_precision << 16;
    reg |= ctrl.MPEG1 << 23;
    reg |= ctrl.picture_type << 24;
    reg |= ctrl.busy << 31;
    return reg;
}

uint32_t ImageProcessingUnit::read_BP()
{
    uint32_t reg = 0;
    uint8_t fifo_size = in_FIFO.size();

    //Check for FP bit
    if (bit_pointer && fifo_size)
    {
        reg |= 1 << 16;
        fifo_size--;
    }
    reg |= bit_pointer;
    reg |= fifo_size << 8;
    printf("[IPU] Read BP: $%08X\n", reg);
    return reg;
}

uint64_t ImageProcessingUnit::read_top()
{
    uint64_t reg = 0;
    int max_bits = (in_FIFO.size() * 128) - bit_pointer;
    if (max_bits > 32)
        max_bits = 32;
    uint32_t next_data;
    get_bits(next_data, max_bits);
    reg |= next_data;
    reg |= (uint64_t)command_decoding << 63UL;
    return reg;
}

void ImageProcessingUnit::write_command(uint32_t value)
{
    printf("[IPU] Write command: $%08X\n", value);
    if (!ctrl.busy)
    {
        ctrl.busy = true;
        command = (value >> 28);
        command_option = value & ~0xF0000000;
        switch (command)
        {
            case 0x00:
                printf("[IPU] BCLR\n");
                break;
            case 0x02:
                printf("[IPU] BDEC\n");
                command_decoding = true;
                bdec.state = BDEC_STATE::ADVANCE;
                bdec.coded_block_pattern = 0x3F;
                bdec.block_index = 0;
                bdec.intra = command_option & (1 << 27);
                break;
            case 0x03:
                printf("[IPU] VDEC\n");
                command_decoding = true;
                vdec_state = VDEC_STATE::ADVANCE;
                break;
            case 0x04:
                printf("[IPU] FDEC\n");
                command_decoding = true;
                fdec_state = VDEC_STATE::ADVANCE;
                break;
            case 0x05:
                printf("[IPU] SETIQ\n");
                bytes_left = 64;
                break;
            case 0x06:
                printf("[IPU] SETVQ\n");
                bytes_left = 32;
                break;
            case 0x09:
                printf("[IPU] SETTH\n");
                break;
        }
    }
    else
    {
        printf("[IPU] Error - command sent while busy!\n");
        exit(1);
    }
}

void ImageProcessingUnit::write_control(uint32_t value)
{
    printf("[IPU] Write control: $%08X\n", value);
    ctrl.MPEG1 = value & (1 << 23);
    ctrl.picture_type = (value >> 24) & 0x7;
    if (value & (1 << 30))
    {
        ctrl.busy = false;
        command_decoding = false;
        bit_pointer = 0;
        bytes_left = 0;
    }
}

bool ImageProcessingUnit::can_write_FIFO()
{
    return in_FIFO.size() < 8;
}

void ImageProcessingUnit::write_FIFO(uint128_t quad)
{
    printf("[IPU] Write FIFO: $%08X_%08X_%08X_%08X\n", quad._u32[3], quad._u32[2], quad._u32[1], quad._u32[0]);
    if (in_FIFO.size() >= 8)
    {
        printf("[IPU] Error: data sent to IPU exceeding FIFO limit!\n");
        exit(1);
    }
    in_FIFO.push(quad);
}
