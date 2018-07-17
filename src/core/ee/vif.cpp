#include "../logger.hpp"
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <cstring>
#include "vu_disasm.hpp"
#include "vif.hpp"

#include "../gif.hpp"

VectorInterface::VectorInterface(GraphicsInterface* gif, VectorUnit* vu) : gif(gif), vu(vu)
{

}

void VectorInterface::reset()
{
    std::queue<uint32_t> empty;
    FIFO.swap(empty);
    command_len = 0;
    command = 0;
    buffer_size = 0;
    DBF = false;
    vu->set_TOP_regs(&TOP, &ITOP);
    vu->set_GIF(gif);
    wait_for_VU = false;
}

void VectorInterface::update()
{
    int runcycles = 8;
    if (wait_for_VU)
    {
        if (vu->is_running())
            return;
        wait_for_VU = false;
        handle_wait_cmd(wait_cmd_value);
    }
    while (FIFO.size() && runcycles--)
    {        
        if (wait_for_VU)
        {
            if (vu->is_running())
                return;
            wait_for_VU = false;
            handle_wait_cmd(wait_cmd_value);
        }
        uint32_t value = FIFO.front();
        if (command_len <= 0)
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
                    Logger::log(Logger::VIF,"New MASK: $%08X\n", value);
                    MASK = value;
                    break;
                case 0x30:
                    //STROW
                    Logger::log(Logger::VIF,"ROW%d: $%08X\n", 4-command_len,value);
                    ROW[4 - command_len] = value;
                    break;
                case 0x31:
                    //STCOL
                    Logger::log(Logger::VIF,"COL%d: $%08X\n", 4 - command_len, value);
                    COL[4 - command_len] = value;
                    break;
                case 0x4A:
                    //MPG
                    vu->write_instr(mpg.addr, value);
                    mpg.addr += 4;
                    if (command_len <= 1)
                        disasm_micromem();
                    break;
                case 0x50:
                case 0x51:
                    //DIRECT/DIRECTHL
                    if (!gif->path_active(2))
                        return;

                    buffer[buffer_size] = value;
                    buffer_size++;
                    if (buffer_size == 4)
                    {
                        gif->send_PATH2(buffer);
                        buffer_size = 0;
                    }

                    //If we've run out of data, deactivate the PATH2 transfer
                    if (command_len <= 1)
                        gif->deactivate_PATH(2);
                    break;
                default:
                    if ((command & 0x60) == 0x60)
                        handle_UNPACK(value);
                    else
                    {
                        Logger::log(Logger::VIF,"Unhandled data for command $%02X\n", command);
                        exit(1);
                    }
            }
            command_len--;
        }
        FIFO.pop();
    }
}

void VectorInterface::decode_cmd(uint32_t value)
{
    command = value >> 24;
    imm = value & 0xFFFF;
    switch (command)
    {
        case 0x00:
            Logger::log(Logger::VIF,"NOP\n");
            break;
        case 0x01:
            Logger::log(Logger::VIF,"Set CYCLE: $%08X\n", value);
            CYCLE.CL = imm & 0xFF;
            CYCLE.WL = imm >> 8;
            break;
        case 0x02:
            Logger::log(Logger::VIF,"Set OFFSET: $%08X\n", value);
            OFST = value & 0x3FF;
            TOPS = BASE;
            DBF = false;
            break;
        case 0x03:
            Logger::log(Logger::VIF,"Set BASE: $%08X\n", value);
            BASE = value & 0x3FF;
            break;
        case 0x04:
            Logger::log(Logger::VIF,"Set ITOP: $%08X\n", value);
            ITOPS = value & 0x3FF;
            break;
        case 0x05:
            Logger::log(Logger::VIF,"Set MODE: $%08X\n", value);
            MODE = value & 0x3;
            break;
        case 0x06:
            Logger::log(Logger::VIF,"MSKPATH3: %d\n", (value >> 15) & 0x1);
            break;
        case 0x07:
            Logger::log(Logger::VIF,"Set MARK: $%08X\n", value);
            break;
        case 0x10:
            Logger::log(Logger::VIF,"FLUSHE\n");
            wait_for_VU = true;
            wait_cmd_value = value;
            break;
        case 0x11:
            Logger::log(Logger::VIF,"FLUSH\n");
            wait_for_VU = true;
            wait_cmd_value = value;
            break;
        case 0x13:
            Logger::log(Logger::VIF,"FLUSHA\n");
            wait_for_VU = true;
            wait_cmd_value = value;
            break;
        case 0x14:
            Logger::log(Logger::VIF,"MSCAL\n");
            wait_for_VU = true;
            wait_cmd_value = value;
            break;
        case 0x17:
            Logger::log(Logger::VIF,"MSCNT\n");
            wait_for_VU = true;
            wait_cmd_value = value;
            break;
        case 0x20:
            Logger::log(Logger::VIF,"Set MASK: $%08X\n", value);
            command_len = 1;
            break;
        case 0x30:
            Logger::log(Logger::VIF,"Set ROW: $%08X\n", value);
            command_len = 4;
            break;
        case 0x31:
            Logger::log(Logger::VIF,"Set COL: $%08X\n", value);
            command_len = 4;
            break;
        case 0x4A:
            Logger::log(Logger::VIF,"MPG: $%08X\n", value);
            {
                command_len = 0;
                int num = (value >> 16) & 0xFF;
                if (!num)
                    command_len += 512;
                else
                    command_len += num << 1;
                Logger::log(Logger::VIF,"Command len: %d\n", command_len);
                mpg.addr = imm * 8;
            }
            break;
        case 0x50:
        case 0x51:
            command_len = 0;
            if (!imm)
                command_len += 65536;
            else
                command_len += (imm * 4);
            Logger::log(Logger::VIF,"DIRECT: %d\n", command_len);
            gif->request_PATH(2);
            break;
        default:
            if ((command & 0x60) == 0x60)
                init_UNPACK(value);
            else
            {
                Logger::log(Logger::VIF,"Unrecognized command $%02X\n", command);
                exit(1);
            }
    }
}

void VectorInterface::handle_wait_cmd(uint32_t value)
{
    command = value >> 24;
    imm = value & 0xFFFF;
    switch (command)
    {
        case 0x14:
            MSCAL(imm * 8);
            break;
        case 0x17:
            MSCAL(vu->get_PC());
            break;
        default:
            command_len = 0;
    }
}

void VectorInterface::MSCAL(uint32_t addr)
{
    vu->mscal(addr);

    ITOP = ITOPS;

    if (vu->get_id())
    {
        //Double buffering
        TOP = TOPS;
        if (DBF)
            TOPS = BASE;
        else
            TOPS = BASE + OFST;
        DBF = !DBF;
    }
}

void VectorInterface::init_UNPACK(uint32_t value)
{
    Logger::log(Logger::VIF,"UNPACK: $%08X\n", value);
    unpack.addr = (imm & 0x3FF) * 16;
    unpack.sign_extend = !(imm & (1 << 14));
    unpack.masked = (command >> 4) & 0x1;
    if (imm & (1 << 15))
        unpack.addr += TOPS * 16;
    unpack.cmd = command & 0xF;
    unpack.blocks_written = 0;
    buffer_size = 0;
    int vl = command & 0x3;
    int vn = (command >> 2) & 0x3;
    int num = (value >> 16) & 0xFF;
    Logger::log(Logger::VIF,"vl: %d vn: %d num: %d masked: %d\n", vl, vn, num, unpack.masked);
    unpack.words_per_op = (32 >> vl) * (vn + 1);
    unpack.words_per_op = (unpack.words_per_op + 0x1F) & ~0x1F; //round up to nearest 32 before dividing
    unpack.words_per_op /= 32;

    if (MODE)
    {
        Logger::log(Logger::VIF,"MODE == %d!\n", MODE);
        exit(1);
    }
    if (CYCLE.WL <= CYCLE.CL)
    {
        //Skip write
        command_len = unpack.words_per_op * num;
        Logger::log(Logger::VIF,"Command len: %d\n", command_len);
    }
    else
    {
        //Fill write
        Logger::log(Logger::VIF,"WL > CL!\n");
        exit(1);
    }
}

void VectorInterface::handle_UNPACK_masking(uint128_t& quad)
{
    if (unpack.masked)
    {
        uint8_t tempmask;
        
        for (int i = 0; i < 4; i++)
        {
            tempmask = (MASK >> ((i * 2) + std::min(unpack.blocks_written * 8, 24))) & 0x3;
            
            switch (tempmask)
            {
                case 1:
                    //Logger::log(Logger::VIF,"Writing ROW to position %d\n", i);
                    quad._u32[i] = ROW[i];
                    break;
                case 2:
                    //Logger::log(Logger::VIF,"Writing COL to position %d\n", i);
                    quad._u32[i] = COL[std::min(unpack.blocks_written, 3)];
                    break;
                case 3:
                    //Logger::log(Logger::VIF,"Write Protecting to position %d\n", i);
                    quad._u32[i] = vu->read_data<uint32_t>(unpack.addr + (i * 4));
                    break;
                default:
                    //No masking, ignore
                    break;
            }
        }
    }
}

void VectorInterface::handle_UNPACK(uint32_t value)
{
    buffer[buffer_size] = value;
    buffer_size++;
    if (buffer_size >= unpack.words_per_op)
    {
        uint128_t quad;
        buffer_size = 0;
        switch (unpack.cmd)
        {
            case 0x0:
                //S-32
                for (int i = 0; i < 4; i++)
                    quad._u32[i] = buffer[0];
                break;
            case 0x5:
                //V2-16 - Z and W are "indeterminate" (but actually determinate)
                //TODO: find the needed values for Z and W
            {
                uint16_t x = buffer[0] & 0xFFFF;
                uint16_t y = buffer[0] >> 16;
                if (unpack.sign_extend)
                {
                    quad._u32[0] = (int32_t)(int16_t)x;
                    quad._u32[1] = (int32_t)(int16_t)y;
                }
                else
                {
                    quad._u32[0] = x;
                    quad._u32[1] = y;
                }
                quad._u32[2] = 0;
                quad._u32[3] = 0;  
            }
                break;
            case 0x8:
                //V3-32 - W is "indeterminate"
                for (int i = 0; i < 3; i++)
                    quad._u32[i] = buffer[i];
                quad._u32[3] = 0;
                break;
            case 0xC:
                //V4-32
                for (int i = 0; i < 4; i++)
                    quad._u32[i] = buffer[i];
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
                break;
            default:
                Logger::log(Logger::VIF,"Unhandled UNPACK cmd $%02X!\n", unpack.cmd);
                exit(1);
        }

        process_UNPACK_quad(quad);
    }
}

void VectorInterface::process_UNPACK_quad(uint128_t &quad)
{
    handle_UNPACK_masking(quad);

    unpack.blocks_written++;
    if (CYCLE.CL >= CYCLE.WL)
    {
        if (unpack.blocks_written == CYCLE.CL)
            unpack.blocks_written = 0;
        else if (unpack.blocks_written > CYCLE.WL)
        {
            Logger::log(Logger::VIF, "Skip write!\n");
            exit(1);
        }
    }

    //Logger::log(Logger::VIF, "Write data mem $%08X: $%08X_%08X_%08X_%08X\n", unpack.addr,
           //quad._u32[3], quad._u32[2], quad._u32[1], quad._u32[0]);
    vu->write_data(unpack.addr, quad);
    unpack.addr += 16;
}

bool VectorInterface::transfer_DMAtag(uint128_t tag)
{
    //This should return false if the transfer stalls due to the FIFO filling up
    Logger::log(Logger::VIF,"Transfer tag: $%08X_%08X_%08X_%08X\n", tag._u32[3], tag._u32[2], tag._u32[1], tag._u32[0]);
    for (int i = 2; i < 4; i++)
        FIFO.push(tag._u32[i]);
    return true;
}

void VectorInterface::feed_DMA(uint128_t quad)
{
    //Logger::log(Logger::VIF,"Feed DMA: $%08X_%08X_%08X_%08X\n", quad._u32[3], quad._u32[2], quad._u32[1], quad._u32[0]);
    for (int i = 0; i < 4; i++)
        FIFO.push(quad._u32[i]);
}

void VectorInterface::disasm_micromem()
{
    using namespace std;
    ofstream file("microprogram.txt");
    if (!file.is_open())
    {
        Logger::log(Logger::VIF,"Failed to open\n");
        exit(1);
    }

    //Check for branch targets
    bool is_branch_target[0x4000 / 8];
    memset(is_branch_target, 0, 0x4000 / 8);
    for (int i = 0; i < 0x4000; i += 8)
    {
        uint32_t lower = vu->read_instr<uint32_t>(i);

        //If the lower instruction is a branch, set branch target to true for the location it points to
        if (VU_Disasm::is_branch(lower))
        {
            int32_t imm = lower & 0x7FF;
            imm = ((int16_t)(imm << 5)) >> 5;
            imm *= 8;

            uint32_t addr = i + imm + 8;
            if (addr < 0x4000)
                is_branch_target[addr / 8] = true;
        }
    }

    for (int i = 0; i < 0x4000; i += 8)
    {
        if (is_branch_target[i / 8])
        {
            file << endl;
            file << "Branch target $" << setfill('0') << setw(4) << right << hex << i << ":";
            file << endl;
        }
        //PC
        file << "[$" << setfill('0') << setw(4) << right << hex << i << "] ";

        //Raw instructions
        uint32_t lower = vu->read_instr<uint32_t>(i);
        uint32_t upper = vu->read_instr<uint32_t>(i + 4);

        file << setw(8) << hex << upper << ":" << setw(8) << lower << " ";
        file << setfill(' ') << setw(30) << left << VU_Disasm::upper(i, upper);

        if (upper & (1 << 31))
            file << VU_Disasm::loi(lower);
        else
            file << VU_Disasm::lower(i, lower);
        file << endl;
    }
    file.close();
}

uint32_t VectorInterface::get_stat()
{
    uint32_t reg = 0;
    reg |= ((FIFO.size() != 0) * 3);
    reg |= vu->is_running() << 2;
    reg |= DBF << 7;
    reg |= ((FIFO.size() != 0) * 16) << 24;
    //Logger::log(Logger::VIF, "Get STAT: $%08X\n", reg);
    return reg;
}
