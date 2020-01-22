#include <cstdio>
#include <cstdlib>
#include "dmac.hpp"
#include "vu_jit.hpp"
#include "vif.hpp"

#include "../gif.hpp"
#include "../errors.hpp"

#define printf(fmt, ...)(0)

VectorInterface::VectorInterface(GraphicsInterface* gif, VectorUnit* vu, INTC* intc, DMAC* dmac, int id) :
    gif(gif), vu(vu), intc(intc), dmac(dmac), id(id)
{

}

void VectorInterface::reset()
{
    std::queue<uint32_t> empty;
    FIFO.swap(empty);
    command = 0;
    command_len = 0;
    buffer_size = 0;
    DBF = false;
    MODE = 0;
    MASK = 0;
    CODE = 0;
    vu->set_TOP_regs(&TOP, &ITOP);
    vu->set_GIF(gif);
    wait_for_VU = false;
    wait_for_PATH3 = false;
    vif_stalled = 0;
    vif_ibit_detected = false;
    vif_interrupt = false;
    vif_stop = false;
    mark_detected = false;
    flush_stall = false;
    fifo_reverse = false;

    if (id)
        mem_mask = 0x3FF;
    else
        mem_mask = 0xFF;

    if (id)
        fifo_size = 64;
    else
        fifo_size = 32;

    if(vu->get_id() == 1)
        VU_JIT::reset();
}

bool VectorInterface::check_vif_stall(uint32_t value)
{
    if (command == 0)
    {
        //Acknowledge the stall on the next command when triggered on MARK
        if (vif_ibit_detected && ((value >> 24) & 0x7f) != 0x7)
        {
            printf("[VIF] VIF%x Stalled\n", get_id());
            vif_ibit_detected = false;
            vif_interrupt = true;

            if (get_id())
                intc->assert_IRQ((int)Interrupt::VIF1);
            else
                intc->assert_IRQ((int)Interrupt::VIF0);

            vif_stalled |= STALL_IBIT;
            return true;
        }
        if (vif_stop)
        {
            vif_stalled |= STALL_IBIT;
            return true;
        }
    }
    return false;
}

void VectorInterface::update(int cycles)
{
    //Since the loop processes per-word, we need to multiply cycles by 4
    //This allows us to process one quadword per bus cycle
    if (fifo_reverse)
    {
        if (FIFO.size() <= (fifo_size - 4))
        {
            uint128_t fifo_data;
            while (cycles--)
            {
                fifo_data = gif->read_GSFIFO();
                for (int i = 0; i < 4; i++)
                    FIFO.push(fifo_data._u32[i]);
            }
        }
        return;
    }
        
    int run_cycles = cycles << 2;
   
    if (vif_stalled & STALL_MSKPATH3)
    {
        gif->resume_path3();
        vif_stalled &= ~STALL_MSKPATH3;
    }

    while (!vif_stalled && run_cycles--)
    {
        if (wait_for_VU)
        {
            if (vu->is_running())
                return;
            wait_for_VU = false;
            handle_wait_cmd(wait_cmd_value);
        }

        if (flush_stall)
        {
            int active_path = gif->get_active_path();
            if (active_path == 1 || active_path == 2)
                return;
            flush_stall = false;
        }

        if (wait_for_PATH3)
        {
            if (!gif->path3_done())
                return;
            gif->deactivate_PATH(3);
            wait_for_PATH3 = false;
        }

        if ((command & 0x60) == 0x60)
        {
            handle_UNPACK();
        }

        if(check_vif_stall(CODE) || !FIFO.size())
            return;

        uint32_t value = FIFO.front();

        //If process_data_word returns false, this means the word was not processed, so don't pop the FIFO.
        if (process_data_word(value))
        {
            if (command_len)
            {
                command_len--;
                FIFO.pop();
                if (FIFO.size() <= (fifo_size - 4))
                    dmac->set_DMA_request(id);
            }
        }
    }
}

bool VectorInterface::process_data_word(uint32_t value)
{
    if (command == 0)
    {
        buffer_size = 0;
        decode_cmd(value);
    }
    else
    {
        switch (command)
        {
            case 0x20:
                //STMASK
                printf("[VIF] New MASK: $%08X\n", value);
                MASK = value;
                command = 0;
                break;
            case 0x30:
                //STROW
                printf("[VIF] ROW%d: $%08X\n", 4 - command_len, value);
                ROW[4 - command_len] = value;
                if (command_len <= 1)
                    command = 0;
                break;
            case 0x31:
                //STCOL
                printf("[VIF] COL%d: $%08X\n", 4 - command_len, value);
                COL[4 - command_len] = value;
                if (command_len <= 1)
                    command = 0;
                break;
            case 0x4A:
                //MPG
                vu->write_instr(mpg.addr, value);
                mpg.addr += 4;
                if (command_len <= 1)
                    command = 0;
                break;
            case 0x50:
            case 0x51:
                //DIRECT/DIRECTHL
                if (!gif->path_active(2, command == 0x50))
                    return false;

                buffer[buffer_size] = value;
                buffer_size++;
                if (buffer_size == 4)
                {
                    gif->send_PATH2(buffer);
                    buffer_size = 0;
                }

                //If we've run out of data, deactivate the PATH2 transfer
                if (command_len <= 1)
                {
                    gif->deactivate_PATH(2);
                    command = 0;
                }
                break;
            default:
                if ((command & 0x60) == 0x60)
                {
                    buffer[buffer_size] = value;
                    buffer_size++;
                }
                else
                {
                    Errors::die("[VIF] Unhandled data for command $%02X\n", command);
                }
        }
    }

    return true;
}

void VectorInterface::decode_cmd(uint32_t value)
{
    command = (value >> 24) & 0x7F;
    imm = value & 0xFFFF;
    CODE = value;
    if (value & 0x80000000)
    {
        if (!VIF_ERR.mask_interrupt)
            vif_ibit_detected = true;
    }

    command_len = 1;

    switch (command)
    {
        case 0x00:
            //printf("[VIF] NOP\n");
            command = 0;
            break;
        case 0x01:
            printf("[VIF] Set CYCLE: $%08X\n", value);
            CYCLE.CL = imm & 0xFF;
            CYCLE.WL = imm >> 8;
            command = 0;
            break;
        case 0x02:
            printf("[VIF] Set OFFSET: $%08X\n", value);
            OFST = value & 0x3FF;
            TOPS = BASE;
            DBF = false;
            command = 0;
            break;
        case 0x03:
            printf("[VIF] Set BASE: $%08X\n", value);
            BASE = value & 0x3FF;
            command = 0;
            break;
        case 0x04:
            printf("[VIF] Set ITOP: $%08X\n", value);
            ITOPS = value & 0x3FF;
            command = 0;
            break;
        case 0x05:
            printf("[VIF] Set MODE: $%08X\n", value);
            MODE = value & 0x3;
            command = 0;
            break;
        case 0x06:
            printf("[VIF] MSKPATH3: %d\n", (value >> 15) & 0x1);
            gif->set_path3_vifmask((value >> 15) & 0x1);
            command = 0;
            vif_stalled |= STALL_MSKPATH3;
            break;
        case 0x07:
            printf("[VIF] Set MARK: $%08X\n", value);
            MARK = imm;
            mark_detected = true;
            command = 0;
            break;
        case 0x10:
            printf("[VIF] FLUSHE\n");
            wait_for_VU = true;
            wait_cmd_value = value;
            command = 0;
            break;
        case 0x11:
            printf("[VIF] FLUSH\n");
            wait_for_VU = true;
            flush_stall = true;
            wait_cmd_value = value;
            command = 0;
            break;
        case 0x13:
            printf("[VIF] FLUSHA\n");
            wait_for_VU = true;
            flush_stall = true;
            wait_for_PATH3 = true;
            wait_cmd_value = value;
            command = 0;
            break;
        case 0x14:
            printf("[VIF] MSCAL\n");
            wait_for_VU = true;
            wait_cmd_value = value;
            command = 0;
            break;
        case 0x15:
            printf("[VIF] MSCALF\n");
            wait_for_VU = true;
            flush_stall = true;
            wait_cmd_value = value;
            command = 0;
            break;
        case 0x17:
            printf("[VIF] MSCNT\n");
            wait_for_VU = true;
            wait_cmd_value = value;
            command = 0;
            break;
        case 0x20:
            printf("[VIF] Set MASK: $%08X\n", value);
            command_len++;
            break;
        case 0x30:
            printf("[VIF] Set ROW: $%08X\n", value);
            command_len += 4;
            break;
        case 0x31:
            printf("[VIF] Set COL: $%08X\n", value);
            command_len += 4;
            break;
        case 0x4A:
            printf("[VIF] MPG: $%08X\n", value);
            {
                int num = (value >> 16) & 0xFF;
                if (!num)
                    command_len += 512;
                else
                    command_len += num << 1;
                //printf("Command len: %d\n", command_len);
                mpg.addr = imm * 8;
            }

            wait_for_VU = true;
            wait_cmd_value = value;
            break;
        case 0x50:
        case 0x51:
            if (!imm)
                command_len += 65536;
            else
                command_len += (imm * 4);
            printf("[VIF] DIRECT: %d\n", command_len);
            gif->request_PATH(2, command == 0x50);
            break;
        default:
            if ((command & 0x60) == 0x60)
                init_UNPACK(value);
            else
            {
                Errors::die("[VIF] Unrecognized command $%02X\n", command);
            }
    }
}

void VectorInterface::handle_wait_cmd(uint32_t value)
{
    command = (value >> 24) & 0x7F;
    imm = value & 0xFFFF;
    switch (command)
    {
        case 0x14:
        case 0x15:
            MSCAL(imm * 8);
            break;
        case 0x17:
            MSCAL(vu->get_PC());
            break;
        default:
            break;
    }
    if (command_len == 0)
    {
        command = 0;
    }
    
}

void VectorInterface::MSCAL(uint32_t addr)
{
    vu->start_program(addr);

    ITOP = ITOPS & mem_mask;

    if (vu->get_id())
    {
        //Double buffering
        TOP = TOPS & mem_mask;
        if (DBF)
            TOPS = BASE;
        else
            TOPS = (BASE + OFST);
        DBF = !DBF;
    }
}

void VectorInterface::init_UNPACK(uint32_t value)
{
    uint32_t data_read;
    printf("[VIF] UNPACK: $%08X\n", value);
    unpack.addr = (imm & 0x3FF) * 16;
    unpack.sign_extend = !(imm & (1 << 14));
    unpack.masked = (command >> 4) & 0x1;
    unpack.num = (value >> 16) & 0xFF;
    if (!unpack.num)
        unpack.num = 256;
    if (imm & (1 << 15))
        unpack.addr += TOPS * 16;
    unpack.cmd = command & 0xF;
    unpack.blocks_written = 0;
    buffer_size = 0;
    unpack.offset = 0;
    int vl = command & 0x3;
    int vn = (command >> 2) & 0x3;

    if (vl == 3 && vn == 3)
        unpack.words_per_op = 16;
    else
        unpack.words_per_op = (32 >> vl) * (vn + 1);

    if (CYCLE.WL <= CYCLE.CL)
    {
        //Skip write
        //printf("[VIF] Command len: %d\n", command_len);
        data_read = unpack.num;
    }
    else
    {
        //Fill write
       // Errors::die("[VIF] WL > CL!\n");
        data_read = CYCLE.CL * (unpack.num / CYCLE.WL) + std::min((int)(unpack.num % CYCLE.WL), (int)CYCLE.CL);
    }

    data_read = unpack.words_per_op * data_read;
    unpack.words_per_op = (unpack.words_per_op + 0x1F) & ~0x1F; //round up to nearest 32 before dividing
    data_read = (data_read + 0x1F) & ~0x1F;
    unpack.words_per_op /= 32;
    data_read /= 32;

    command_len += data_read;

    //printf("[VIF] UNPACK V%d-%d addr: %x num: %d masked: %d word per op: %d command_len = %d\n", (vn + 1), (32 >> vl), unpack.addr, unpack.num, unpack.masked, unpack.words_per_op, command_len);
}

void VectorInterface::handle_UNPACK_masking(uint128_t& quad)
{
    if (unpack.masked || is_filling_write())
    {
        uint8_t tempmask;
        
        for (int i = 0; i < 4; i++)
        {
            tempmask = (MASK >> ((i * 2) + std::min(unpack.blocks_written * 8, 24))) & 0x3;
            
            switch (tempmask)
            {
                case 1:
                    //printf("[VIF] Writing ROW to position %d\n", i);
                    quad._u32[i] = ROW[i];
                    break;
                case 2:
                    //printf("[VIF] Writing COL to position %d\n", i);
                    quad._u32[i] = COL[std::min(unpack.blocks_written, 3)];
                    break;
                case 3:
                    //printf("[VIF] Write Protecting to position %d\n", i);
                    quad._u32[i] = vu->read_mem<uint32_t>(unpack.addr + (i * 4));
                    break;
                default:
                    //No masking, ignore
                    break;
            }
        }
    }
}

void VectorInterface::handle_UNPACK_mode(uint128_t& quad)
{
    //Do not apply addition decompression when the format is V4-5
    if (MODE && unpack.cmd != 0xF)
    {
        uint8_t tempmask;

        for (int i = 0; i < 4; i++)
        {
            tempmask = (MASK >> ((i * 2) + std::min(unpack.blocks_written * 8, 24))) & 0x3;

            if (!tempmask || !unpack.masked)
            {
                switch (MODE)
                {
                    case 1:
                        //Offset mode - VU Mem = Input + Row
                        quad._u32[i] += ROW[i];
                        break;
                    case 2:
                        //Difference mode - VU Mem = Row = Input + Row
                        quad._u32[i] += ROW[i];
                        ROW[i] = quad._u32[i];
                        break;
                    case 3:
                        ROW[i] = quad._u32[i];
                        break;
                    default:
                        //Do nothing
                        break;
                }
            }
        }
    }
}

bool VectorInterface::is_filling_write()
{
    if (CYCLE.CL < CYCLE.WL && unpack.blocks_written >= CYCLE.CL)
        return true;

    return false;
}

void VectorInterface::handle_UNPACK()
{
    int bufferpos = 0;

    while((is_filling_write() || (buffer_size >= unpack.words_per_op)) && unpack.num)
    {
        uint128_t quad;
        if (unpack.blocks_written < CYCLE.CL)
        {
            switch (unpack.cmd)
            {
                case 0x0:
                    //S-32
                    for (int i = 0; i < 4; i++)
                        quad._u32[i] = buffer[0];
                    buffer_size -= 1;
                    break;
                case 0x1:
                    //S-16
                    for (int i = 0; i < 4; i++)
                    {
                        if (unpack.sign_extend)
                        {
                            quad._u32[i] = (int32_t)(int16_t)(unpack.offset ? (buffer[0] >> 16) : buffer[0]);
                        }
                        else
                        {
                            quad._u32[i] = (uint16_t)(unpack.offset ? (buffer[0] >> 16) : buffer[0]);
                        }
                    }

                    if (unpack.offset++)
                    {
                        unpack.offset = 0;
                        buffer_size -= 1;
                    }
                    break;
                case 0x2:
                    //S-8
                    for (int i = 0; i < 4; i++)
                    {
                        if (unpack.sign_extend)
                        {
                            quad._u32[i] = (int32_t)(int8_t)(unpack.offset ? (buffer[0] >> (unpack.offset * 8)) : buffer[0]);
                        }
                        else
                        {
                            quad._u32[i] = (uint8_t)(unpack.offset ? (buffer[0] >> (unpack.offset * 8)) : buffer[0]);
                        }
                    }

                    if (unpack.offset++ == 3)
                    {
                        unpack.offset = 0;
                        buffer_size -= 1;
                    }
                    break;
                case 0x4:
                    //V2-32 - Z and W are "indeterminate"
                    //Indeterminate data is the x vector and 0 respectively
                    quad._u32[0] = buffer[0];
                    quad._u32[1] = buffer[1];
                    quad._u32[2] = buffer[0];
                    quad._u32[3] = buffer[1];

                    buffer_size -= 2;
                    break;
                case 0x5:
                    //V2-16 - Z and W are "indeterminate"
                    //Indeterminate data is just the x and y vectors respectively
                    {
                        uint16_t x = buffer[0] & 0xFFFF;
                        uint16_t y = buffer[0] >> 16;
                        if (unpack.sign_extend)
                        {
                            quad._u32[0] = (int32_t)(int16_t)x;
                            quad._u32[1] = (int32_t)(int16_t)y;
                            quad._u32[2] = (int32_t)(int16_t)x;
                            quad._u32[3] = (int32_t)(int16_t)y;
                        }
                        else
                        {
                            quad._u32[0] = x;
                            quad._u32[1] = y;
                            quad._u32[2] = x;
                            quad._u32[3] = y;
                        }

                        buffer_size -= 1;
                    }
                    break;
                case 0x6:
                    //V2-8 - Z and W are "indeterminate"
                    //Indeterminate data is just the x and y vectors respectively
                    {
                        uint8_t x = (buffer[0] >> (unpack.offset * 16)) & 0xFF;
                        uint8_t y = (buffer[0] >> ((unpack.offset * 16) + 8));
                        if (unpack.sign_extend)
                        {
                            quad._u32[0] = (int32_t)(int8_t)x;
                            quad._u32[1] = (int32_t)(int8_t)y;
                            quad._u32[2] = (int32_t)(int8_t)x;
                            quad._u32[3] = (int32_t)(int8_t)y;
                        }
                        else
                        {
                            quad._u32[0] = x;
                            quad._u32[1] = y;
                            quad._u32[2] = x;
                            quad._u32[3] = y;
                        }

                        if (unpack.offset++)
                        {
                            unpack.offset = 0;
                            buffer_size -= 1;
                        }
                    }
                    break;
                case 0x8:
                    //V3-32 - W is "indeterminate"
                    //Indeterminate data is the first vector of the next num
                    //The first num uses 0, the last num uses the next VIFCode
                    //We can't read the next VIFCode so we will just use 0
                    if (buffer_size < 4 && unpack.num > 1 && unpack.offset > 0)
                        return;

                    for (int i = 0; i < 3; i++)
                        quad._u32[i] = buffer[i];

                    if (buffer_size >= 4)
                    {
                        quad._u32[3] = buffer[3];
                        buffer[0] = buffer[3];
                    }
                    else
                        quad._u32[3] = 0;

                    buffer_size -= 3;
                    unpack.offset++;
                    break;
                case 0x9:
                    //V3-16 - W is "indeterminate"
                    //Indeterminate data is the first vector of the next num
                    //If the data is aligned to the word write 0
                    //FIXME: It could be the first vector of the next unpack
                    for (int i = 0; i < 3; i++)
                    {
                        if (unpack.sign_extend)
                        {
                            quad._u32[i] = (int32_t)(int16_t)(unpack.offset ? (buffer[bufferpos] >> 16) : buffer[bufferpos]);
                        }
                        else
                        {
                            quad._u32[i] = (uint16_t)(unpack.offset ? (buffer[bufferpos] >> 16) : buffer[bufferpos]);
                        }

                        if (unpack.offset++)
                        {
                            unpack.offset = 0;
                            bufferpos++;
                            buffer_size -= 1;
                        }
                    }

                    if (buffer_size >= 1)
                    {
                        if (unpack.sign_extend)
                        {
                            quad._u32[3] = (int32_t)(int16_t)(unpack.offset ? (buffer[bufferpos] >> 16) : buffer[bufferpos]);
                        }
                        else
                        {
                            quad._u32[3] = (uint16_t)(unpack.offset ? (buffer[bufferpos] >> 16) : buffer[bufferpos]);
                        }
                    }
                    else
                    {
                        quad._u32[3] = 0;
                    }

                    if (buffer_size < 2 && unpack.offset)
                    {
                        buffer[0] = buffer[bufferpos];
                    }
                    break;
                case 0xA:
                    //V3-8 - W is "indeterminate"
                    //Indeterminate data is the first vector of the next num
                    //If the data is aligned to the word write 0
                    //FIXME: It could be the first vector of the next unpack

                    //Check we have enough data before continuing
                    if ((buffer_size * 4) - unpack.offset < 3)
                        return;

                    for (int i = 0; i < 3; i++)
                    {
                        if (unpack.sign_extend)
                        {
                            quad._u32[i] = (int32_t)(int8_t)(unpack.offset ? (buffer[bufferpos] >> (unpack.offset * 8)) : buffer[bufferpos]);
                        }
                        else
                        {
                            quad._u32[i] = (uint8_t)(unpack.offset ? (buffer[bufferpos] >> (unpack.offset * 8)) : buffer[bufferpos]);
                        }

                        if (unpack.offset++ == 3)
                        {
                            unpack.offset = 0;
                            bufferpos++;
                            buffer_size -= 1;
                        }
                    }

                    if (buffer_size >= 1)
                    {
                        if (unpack.sign_extend)
                        {
                            quad._u32[3] = (int32_t)(int8_t)(unpack.offset ? (buffer[bufferpos] >> ((unpack.offset) * 8)) : buffer[bufferpos]);
                        }
                        else
                        {
                            quad._u32[3] = (uint8_t)(unpack.offset ? (buffer[bufferpos] >> (unpack.offset * 8)) : buffer[bufferpos]);
                        }
                    }
                    else
                    {
                        quad._u32[3] = 0;
                    }

                    if (buffer_size == 1 && unpack.offset)
                    {
                        buffer[0] = buffer[bufferpos];
                    }
                    break;
                case 0xC:
                    //V4-32
                    for (int i = 0; i < 4; i++)
                        quad._u32[i] = buffer[i];
                    buffer_size -= 4;
                    break;
                case 0xD:
                    //V4-16
                    for (int i = 0; i < 4; i++)
                    {
                        uint16_t value = (buffer[i / 2] >> ((i % 2) * 16)) & 0xFFFF;
                        if (unpack.sign_extend)
                            quad._u32[i] = (int32_t)(int16_t)value;
                        else
                            quad._u32[i] = value;
                    }

                    buffer_size -= 2;
                    break;
                case 0xE:
                    //V4-8
                    for (int i = 0; i < 4; i++)
                    {
                        uint8_t value = (buffer[0] >> (i * 8)) & 0xFF;
                        if (unpack.sign_extend)
                            quad._u32[i] = (int32_t)(int8_t)value;
                        else
                            quad._u32[i] = value;
                    }

                    buffer_size -= 1;
                    break;
                case 0xF:
                    //V4-5
                    {
                        uint16_t data = (buffer[0] >> (unpack.offset * 16)) & 0xFFFF;
                        quad._u32[0] = (data & 0x1F) << 3;
                        quad._u32[1] = ((data >> 5) & 0x1F) << 3;
                        quad._u32[2] = ((data >> 10) & 0x1F) << 3;
                        quad._u32[3] = ((data >> 15) & 0x1) << 7;

                        if (unpack.offset++)
                        {
                            unpack.offset = 0;
                            buffer_size -= 1;
                        }
                    }
                    break;
                default:
                    Errors::die("[VIF] Unhandled UNPACK cmd $%02X!\n", unpack.cmd);
            }
        }
        else //Filling Write
        {
            quad._u64[0] = 0;
            quad._u64[1] = 0;
        }

        process_UNPACK_quad(quad);
    }
    if (unpack.num == 0)
        command = 0;
}

void VectorInterface::process_UNPACK_quad(uint128_t &quad)
{
    handle_UNPACK_masking(quad);
    handle_UNPACK_mode(quad);

    unpack.blocks_written++;

    //printf("[VIF] Write data mem $%08X: $%08X_%08X_%08X_%08X\n", unpack.addr,
    //quad._u32[3], quad._u32[2], quad._u32[1], quad._u32[0]);
    vu->write_mem<uint128_t>(unpack.addr, quad);
    unpack.addr += 16;
    unpack.num -= 1;
    
    if (unpack.blocks_written >= CYCLE.WL)
    {
        if (CYCLE.CL > CYCLE.WL)
            unpack.addr += (CYCLE.CL - unpack.blocks_written) * 16;

        unpack.blocks_written = 0;
    }
}

bool VectorInterface::transfer_DMAtag(uint128_t tag)
{
    //This should return false if the transfer stalls due to the FIFO filling up
    if (FIFO.size() > (fifo_size - 2))
    {
        dmac->clear_DMA_request(id);
        return false;
    }
    printf("[VIF] Transfer tag: $%08X_%08X_%08X_%08X\n", tag._u32[3], tag._u32[2], tag._u32[1], tag._u32[0]);
    for (int i = 2; i < 4; i++)
        FIFO.push(tag._u32[i]);
    return true;
}

bool VectorInterface::feed_DMA(uint128_t quad)
{
    if (FIFO.size() > (fifo_size - 4))
    {
        dmac->clear_DMA_request(id);
        return false;
    }
    printf("[VIF] Feed DMA: $%08X_%08X_%08X_%08X\n", quad._u32[3], quad._u32[2], quad._u32[1], quad._u32[0]);
    for (int i = 0; i < 4; i++)
        FIFO.push(quad._u32[i]);
    return true;
}

uint128_t VectorInterface::readFIFO()
{
    uint128_t quad;
    if (FIFO.empty())
        return gif->read_GSFIFO();

    for (int i = 0; i < 4; i++)
    {
        quad._u32[i] = FIFO.front();
        FIFO.pop();
    }
    return quad;
}

uint32_t VectorInterface::get_stat()
{
    uint32_t reg = 0;
    reg |= (vif_stalled & STALL_IBIT) ? 0 : ((FIFO.size() != 0) * 3);
    reg |= vu->is_running() << 2;
    reg |= mark_detected << 6;
    reg |= DBF << 7;
    reg |= vif_stop << 8;
    reg |= (vif_stalled & STALL_IBIT) << 10;
    reg |= vif_interrupt << 11;
    reg |= fifo_reverse << 23;
    reg |= ((FIFO.size() + 3) / 4) << 24;
    //printf("[VIF] Get STAT: $%08X\n", reg);
    return reg;
}

uint32_t VectorInterface::get_mark()
{
    printf("[VIF] Get MARK: $%x\n", MARK);
    return MARK;
}

uint32_t VectorInterface::get_mode()
{
    return MODE;
}

uint32_t VectorInterface::get_row(uint32_t address)
{
    return ROW[(address & 0xF0) >> 4];
}

uint32_t VectorInterface::get_code()
{
    return CODE;
}

uint32_t VectorInterface::get_top()
{
    return TOP;
}

uint32_t VectorInterface::get_err()
{
    uint32_t reg = 0;
    reg |= VIF_ERR.mask_interrupt;
    reg |= VIF_ERR.mask_dmatag_error << 1;
    reg |= VIF_ERR.mask_vifcode_error << 2;
    printf("[VIF] Get ERR: $%08X\n", reg);
    return reg;
}

void VectorInterface::set_stat(uint32_t value)
{
    if ((!fifo_reverse && ((value >> 23) & 0x1)) || (fifo_reverse && !((value >> 23) & 0x1)))
    {
        while (!FIFO.empty())
            FIFO.pop();
    }
    fifo_reverse = (value >> 23) & 0x1;
}
void VectorInterface::set_mark(uint32_t value)
{
    MARK = value;
    mark_detected = false;
    printf("[VIF] Set MARK: $%x\n", MARK);
}

void VectorInterface::set_err(uint32_t value)
{
    VIF_ERR.mask_interrupt = value & 0x1;
    VIF_ERR.mask_dmatag_error = value & 0x2;
    VIF_ERR.mask_vifcode_error = value & 0x4;
    printf("[VIF] Set ERR: $%x\n", value);
}

void VectorInterface::set_fbrst(uint32_t value)
{
    printf("[VIF] Set FBRST: $%x\n", value);

    if (value & 0x8)
    {
        printf("[VIF] VIF%x Resumed\n", get_id());
        vif_stalled &= ~STALL_IBIT;
        vif_interrupt = false;
        vif_stop = false;
    }
    if (value & 0x4)
    {
        printf("[VIF] VIF%x Stopped\n", get_id());
        vif_stop = true;
    }
}
