#include <cstring>
#include <ctime>
#include <string>
#include "bincuereader.hpp"
#include "cdvd.hpp"
#include "cso_reader.hpp"
#include "iso_reader.hpp"

#include "../iop_dma.hpp"
#include "../iop_intc.hpp"

#include "../../errors.hpp"
#include "../../scheduler.hpp"

using namespace std;

//Values from PCSX2 - subject to change
static uint64_t IOP_CLOCK = 36864000;
static const int PSX_CD_READSPEED = 153600;
static const int PSX_DVD_READSPEED = 1382400;

//Values taken from PCSX2
static const uint8_t monthmap[13] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
#define btoi(b) ((b)/16*10 + (b)%16)    /* BCD to u_char */
#define itob(i) ((i)/10*16 + (i)%10)    /* u_char to BCD */

uint32_t CDVD_Drive::get_block_timing(bool mode_DVD)
{
    return (IOP_CLOCK * block_size) / (speed * (mode_DVD ? PSX_DVD_READSPEED : PSX_CD_READSPEED));
}

CDVD_Drive::CDVD_Drive(IOP_INTC* intc, IOP_DMA* dma, Scheduler* scheduler) :
    intc(intc),
    dma(dma),
    scheduler(scheduler),
    container(nullptr)
{

}

CDVD_Drive::~CDVD_Drive()
{
    if (container)
        container->close();
}

void CDVD_Drive::reset()
{
    speed = 4;
    current_sector = 0;
    cycle_count = 0;
    last_read = 0;
    drive_status = STOPPED;
    is_spinning = false;
    active_N_command = NCOMMAND::NONE;
    N_status = 0x40;
    N_params = 0;
    N_command = 0;
    S_params = 0;
    S_status = 0x40;
    S_out_params = 0;
    read_bytes_left = 0;
    ISTAT = 0;
    disc_type = CDVD_DISC_NONE;
    file_size = 0;
    time_t raw_time;
    struct tm * time;
    std::time(&raw_time);
    raw_time += 9 * 60 * 60; // Add 9 hours to change raw_time to Japan's timezone
    time = std::gmtime(&raw_time);
    rtc.vsyncs = 0;
    rtc.second = time->tm_sec;
    rtc.minute = time->tm_min;
    rtc.hour = time->tm_hour;
    rtc.day = time->tm_mday;
    rtc.month = time->tm_mon+1; //Jan = 0
    rtc.year = time->tm_year - 100; //Returns Years since 1900

    n_command_event_id = scheduler->register_function([this] (uint64_t param) { handle_N_command(); });
}

string CDVD_Drive::get_ps2_exec_path()
{
    if (!container->is_open())
        return "";

    uint32_t cnf_size;
    char* cnf = (char*)read_file("SYSTEM.CNF;1", cnf_size);
    int pos = 0;
    if (!cnf)
        return "h";
    while (strncmp("cdrom0:\\", cnf + pos, 7))
        pos++;

    string path;
    while (strncmp(";1", cnf + pos, 2))
    {
        path += cnf[pos];
        pos++;
    }

    return path + ";1";
}

string CDVD_Drive::get_serial()
{
    if (!container->is_open())
        return "";

    uint32_t cnf_size;
    char* cnf = (char*)read_file("SYSTEM.CNF;1", cnf_size);
    int pos = 0;
    if (!cnf)
        return "h";
    while (strncmp("cdrom0:\\", cnf + pos, 7))
        pos++;

    pos += 8;

    string blorp(cnf + pos, cnf + pos + 11);
    delete[] cnf;
    return blorp;
}

//TODO: Support PAL framerates
void CDVD_Drive::vsync()
{
    rtc.vsyncs++;
    if (rtc.vsyncs < 60) return;
    rtc.vsyncs = 0;

    rtc.second++;
    if (rtc.second < 60) return;
    rtc.second = 0;

    rtc.minute++;
    if (rtc.minute < 60) return;
    rtc.minute = 0;

    rtc.hour++;
    if (rtc.hour < 24) return;
    rtc.hour = 0;

    rtc.day++;
    if (rtc.day <= (rtc.month == 2 && rtc.year % 4 == 0 ? 29 : monthmap[rtc.month - 1])) return;
    rtc.day = 1;

    rtc.month++;
    if (rtc.month < 13) return;
    rtc.month = 1;

    rtc.year++;
    if (rtc.year < 100) return;
    rtc.year = 0;
}

int CDVD_Drive::get_block_size()
{
    return block_size;
}

int CDVD_Drive::bytes_left()
{
    return read_bytes_left;
}

uint32_t CDVD_Drive::read_to_RAM(uint8_t *RAM, uint32_t bytes)
{
    memcpy(RAM, read_buffer, block_size);
    dma->clear_DMA_request(IOP_CDVD);
    read_bytes_left -= block_size;
    if (read_bytes_left <= 0)
    {
        if (sectors_left == 0)
        {
            N_status = 0x4E;
            ISTAT |= 0x3;
            intc->assert_irq(2);
            drive_status = PAUSED;
            active_N_command = NCOMMAND::NONE;
        }
        else
        {
            active_N_command = NCOMMAND::READ;
            add_event(get_block_timing(N_command != 0x06));
        }
    }
    return block_size;
}

void CDVD_Drive::handle_N_command()
{
    printf("CDVD event!\n");
    switch (active_N_command)
    {
        case NCOMMAND::SEEK:
            drive_status = PAUSED;
            active_N_command = NCOMMAND::NONE;
            current_sector = sector_pos;
            N_status = 0x4E;
            ISTAT |= 0x2;
            intc->assert_irq(2);
            break;
        case NCOMMAND::STANDBY:
            drive_status = PAUSED;
            active_N_command = NCOMMAND::NONE;
            N_status = 0x40;
            ISTAT |= 0x2;
            intc->assert_irq(2);
            break;
        case NCOMMAND::STOP:
            is_spinning = false;
            drive_status = STOPPED;
            N_status = 0x40;
            ISTAT |= 0x2;
            intc->assert_irq(2);
            break;
        case NCOMMAND::READ:
            if (!read_bytes_left)
            {
                if (N_command == 0x06)
                    read_CD_sector();
                else if (N_command == 0x08)
                    read_DVD_sector();
            }
            else
                add_event(1000); //Check later to see if there's space in the buffer
            break;
        case NCOMMAND::READ_SEEK:
            drive_status = READING | SPINNING;
            active_N_command = NCOMMAND::READ;
            current_sector = sector_pos;
            add_event(get_block_timing(N_command != 0x06));
            break;
        case NCOMMAND::BREAK:
            printf("[CDVD] Break issued\n");
            drive_status = PAUSED;
            active_N_command = NCOMMAND::NONE;
            N_status = 0x4E;
            ISTAT |= 0x2;
            intc->assert_irq(2);
            break;
        case NCOMMAND::NONE:
            //Just do nothing
            //This can happen when a BREAK is sent while a command is currently executing
            //The BREAK will send its own event that clears active_N_command
            break;
        default:
            Errors::die("[CDVD] Unrecognized active N command %d\n", active_N_command);
    }
}

bool CDVD_Drive::load_disc(const char *name, CDVD_CONTAINER a_container)
{
    //container = a_container;
    switch (a_container)
    {
        case CDVD_CONTAINER::ISO:
            container = std::unique_ptr<CDVD_Container>(new ISO_Reader());
            break;
        case CDVD_CONTAINER::CISO:
            container = std::unique_ptr<CDVD_Container>(new CSO_Reader());
            break;
        case CDVD_CONTAINER::BIN_CUE:
            container = std::unique_ptr<CDVD_Container>(new BinCueReader());
            break;
        default:
            container = nullptr;
            return false;
    }
    if (!container->open(name))
        return false;

    file_size = container->get_size();

    printf("[CDVD] Disc size: %lu bytes\n", file_size);
    printf("[CDVD] Locating Primary Volume Descriptor\n");
    uint8_t type = 0;
    int sector = 0x0F;
    while (type != 1)
    {
        sector++;
        container->seek(sector, std::ios::beg);
        container->read(&type, sizeof(uint8_t));
    }
    printf("[CDVD] Primary Volume Descriptor found at sector %d\n", sector);

    container->seek(sector, std::ios::beg);
    container->read(pvd_sector, 2048);

    LBA = *(uint16_t*)&pvd_sector[128];
    if (*(uint16_t*)&pvd_sector[166] == 2048)
        disc_type = CDVD_DISC_PS2CD;
    else
        disc_type = CDVD_DISC_PS2DVD;
    printf("[CDVD] PVD LBA: $%08X\n", LBA);

    root_location = *(uint32_t*)&pvd_sector[156 + 2];
    root_len = *(uint32_t*)&pvd_sector[156 + 10];
    printf("[CDVD] Root dir len: %d\n", *(uint16_t*)&pvd_sector[156]);
    printf("[CDVD] Extent loc: $%08lX\n", root_location * LBA);
    printf("[CDVD] Extent len: $%08lX\n", root_len);

    return true;
}

uint8_t* CDVD_Drive::read_file(string name, uint32_t& file_size)
{
    uint8_t* root_extent = new uint8_t[root_len];
    container->seek(root_location, std::ios::beg);
    container->read(root_extent, root_len);
    uint32_t bytes = 0;
    uint64_t file_location = 0;
    uint8_t* file;
    file_size = 0;
    printf("[CDVD] Finding %s...\n", name.c_str());
    while (bytes < root_len)
    {
        uint64_t directory_len = root_extent[bytes + 32];
        if (name.length() == directory_len)
        {
            bool match = true;
            for (unsigned int i = 0; i < directory_len; i++)
            {
                if (name[i] != root_extent[bytes + 33 + i])
                {
                    match = false;
                    break;
                }
            }
            if (match)
            {
                printf("[CDVD] Match found!\n");
                file_location = *(uint32_t*)&root_extent[bytes + 2];
                file_size = *(uint32_t*)&root_extent[bytes + 10];
                printf("[CDVD] Location: $%08lX\n", file_location);
                printf("[CDVD] Size: $%08X\n", file_size);

                file = new uint8_t[file_size];
                container->seek(file_location, std::ios::beg);
                container->read(file, file_size);
                delete[] root_extent;
                return file;
            }
        }
        //Increment bytes by size of directory record
        bytes += root_extent[bytes];
    }
    delete[] root_extent;
    return nullptr;
}

uint8_t CDVD_Drive::read_N_command()
{
    printf("[CDVD] Read N_command: $%02X\n", N_command);
    return N_command;
}

uint8_t CDVD_Drive::read_disc_type()
{
    //Not sure what the exact limit is. We'll just go with 1 GB for now.
    printf("[CDVD] Read disc type\n");
    return disc_type;
}

uint8_t CDVD_Drive::read_N_status()
{
    //printf("[CDVD] Read N_status: $%02X\n", N_status);
    return N_status;
}

uint8_t CDVD_Drive::read_S_status()
{
    printf("[CDVD] Read S_status: $%02X\n", S_status);
    return S_status;
}

uint8_t CDVD_Drive::read_S_command()
{
    printf("[CDVD] Read S_command: $%02X\n", S_command);
    return S_command;
}

uint8_t CDVD_Drive::read_S_data()
{
    if (S_out_params <= 0)
        return 0;
    uint8_t value = S_outdata[S_params];
    printf("[CDVD] Read S data: $%02X\n", value);
    S_params++;
    S_out_params--;
    if (S_out_params == 0)
    {
        S_status |= 0x40;
        S_params = 0;
    }
    return value;
}

uint8_t CDVD_Drive::read_ISTAT()
{
    printf("[CDVD] Read ISTAT: $%02X\n", ISTAT);
    return ISTAT;
}

uint8_t CDVD_Drive::read_cdkey(int index)
{
    return cdkey[index];
}

void CDVD_Drive::send_N_command(uint8_t value)
{
    N_command = value;
    switch (value)
    {
        //NOPSync/NOP
        case 0x00:
        case 0x01:
            intc->assert_irq(2);
            break;
        case 0x02:
            //Standby
            sector_pos = 0;
            start_seek();
            active_N_command = NCOMMAND::STANDBY;
            break;
        case 0x03:
            add_event(IOP_CLOCK / 6);
            active_N_command = NCOMMAND::STOP;
            break;
        case 0x04:
            //Pause
            intc->assert_irq(2);
            drive_status = PAUSED;
            break;
        case 0x05:
            sector_pos = *(uint32_t*)&N_command_params[0];
            start_seek();
            active_N_command = NCOMMAND::SEEK;
            break;
        case 0x06:
            N_command_read();
            break;
        case 0x08:
            N_command_dvdread();
            break;
        case 0x09:
            printf("[CDVD] GetTOC\n");
            N_command_gettoc();
            break;
        case 0x0C:
        {
            uint32_t arg = N_command_params[3] |
                    (N_command_params[4]<<8) |
                    (N_command_params[5]<<16) |
                    (N_command_params[6]<<24);
            printf("[CDVD] ReadKey: $%08X\n", arg);
            N_command_readkey(arg);
        }
            break;
        default:
            Errors::die("[CDVD] Unrecognized N command $%02X\n", value);
    }
    N_params = 0;
}

uint8_t CDVD_Drive::read_drive_status()
{
    printf("[CDVD] Read drive status: $%02X\n", drive_status);
    return drive_status;
}

void CDVD_Drive::write_N_data(uint8_t value)
{
    printf("[CDVD] Write NDATA: $%02X\n", value);
    if (N_params > 10)
    {
        Errors::die("[CDVD] Excess NDATA params!\n");
    }
    else
    {
        N_command_params[N_params] = value;
        N_params++;
    }
}

void CDVD_Drive::write_BREAK()
{
    printf("[CDVD] Write BREAK\n");
    if (active_N_command == NCOMMAND::NONE || active_N_command == NCOMMAND::BREAK)
        return;

    add_event(64);
    active_N_command = NCOMMAND::BREAK;
    drive_status = CDVD_STATUS::STOPPED;
    read_bytes_left = 0;
}

void CDVD_Drive::send_S_command(uint8_t value)
{
    printf("[CDVD] Send S command: $%02X\n", value);
    S_status &= ~0x40;
    S_command = value;
    switch (value)
    {
        case 0x03:
            S_command_sub(S_command_params[0]);
            break;
        case 0x05:
            printf("[CDVD] Media Change?\n");
            prepare_S_outdata(1);
            S_outdata[0] = 0;
            break;
        case 0x08:
            printf("[CDVD] ReadClock\n");
            prepare_S_outdata(8);
            S_outdata[0] = 0;
            S_outdata[1] = itob(rtc.second);
            S_outdata[2] = itob(rtc.minute);
            S_outdata[3] = itob(rtc.hour);
            S_outdata[4] = 0;
            S_outdata[5] = itob(rtc.day);
            S_outdata[6] = itob(rtc.month);
            S_outdata[7] = itob(rtc.year);
            break;
        case 0x09:
            printf("[CDVD] WriteClock\n");
            prepare_S_outdata(1);
            S_outdata[0] = 0;
            rtc.second = S_command_params[0];
            rtc.minute = S_command_params[1] % 60;
            rtc.hour = S_command_params[2] % 24;
            rtc.day = S_command_params[4];
            rtc.month = S_command_params[5] & 0x7F;
            rtc.year = S_command_params[6];
            break;
        case 0x12:
            printf("[CDVD] sceCdReadILinkId\n");
            prepare_S_outdata(9);
            for (int i = 0; i < 9; i++)
                S_outdata[i] = 0;
            break;
        case 0x13:
            printf("[CDVD] sceCdWriteILinkId\n");
            prepare_S_outdata(1);
            S_outdata[0] = 0;
            break;
        case 0x15:
            printf("[CDVD] ForbidDVD\n");
            prepare_S_outdata(1);
            S_outdata[0] = 5;
            break;
        case 0x17:
            printf("[CDVD] ReadILinkModel\n");
            prepare_S_outdata(9);
            for (int i = 0; i < 9; i++)
                S_outdata[i] = 0;
            break;
        case 0x1A:
            printf("[CDVD] BootCertify\n");
            prepare_S_outdata(1);
            S_outdata[0] = 1; //means OK according to PCSX2
            break;
        case 0x1B:
            printf("[CDVD] CancelPwOffReady\n");
            prepare_S_outdata(1);
            S_outdata[0] = 0;
            break;
        case 0x1E:
            prepare_S_outdata(5);
            S_outdata[0] = 0x00;
            S_outdata[1] = 0x14;
            S_outdata[2] = 0x00;
            S_outdata[3] = 0x00;
            S_outdata[4] = 0x00;
            break;
        case 0x22:
            printf("[CDVD] CdReadWakeupTime\n");
            prepare_S_outdata(10);
            for (int i = 0; i < 10; i++)
                S_outdata[i] = 0;
            break;
        case 0x24:
            printf("[CDVD] CdRCBypassCtrl\n");
            prepare_S_outdata(1);
            S_outdata[0] = 0;
            break;
        case 0x40:
            printf("[CDVD] OpenConfig\n");
            prepare_S_outdata(1);
            S_outdata[0] = 0;
            break;
        case 0x41:
            printf("[CDVD] ReadConfig\n");
            prepare_S_outdata(16);
            for (int i = 0; i < 16; i++)
                S_outdata[i] = 0;
            break;
        case 0x42:
            printf("[CDVD] WriteConfig\n");
            prepare_S_outdata(1);
            S_outdata[0] = 0;
            break;
        case 0x43:
            printf("[CDVD] CloseConfig\n");
            prepare_S_outdata(1);
            S_outdata[0] = 0;
            break;
        default:
            Errors::die("[CDVD] Unrecognized S command $%02X\n", value);
    }
}

void CDVD_Drive::write_S_data(uint8_t value)
{
    printf("[CDVD] Write SDATA: $%02X (%d)\n", value, S_params);
    if (S_params > 15)
    {
        Errors::die("[CDVD] Excess SDATA params!\n");
    }
    else
    {
        S_command_params[S_params] = value;
        S_params++;
    }
}

void CDVD_Drive::prepare_S_outdata(int amount)
{
    if (amount > 16)
    {
        Errors::die("[CDVD] Excess S outdata! (%d)\n", amount);
    }
    S_out_params = amount;
    S_status &= ~0x40;
    S_params = 0;
}

void CDVD_Drive::start_seek()
{
    N_status = 0x40;
    drive_status = SEEKING;

    uint64_t cycles_to_seek = 0;

    if (!is_spinning)
    {
        //1/3 of a second
        cycles_to_seek = IOP_CLOCK / 3;
        //cycles_to_seek = 1000000;
        printf("[CDVD] Spinning\n");
        is_spinning = true;
    }
    else
    {
        printf("[CDVD] Seeking\n");
        bool is_DVD = N_command != 0x06;
        int delta = abs((int)current_sector - (int)sector_pos);
        printf("[CDVD] Seek delta: %d\n", delta);
        if ((is_DVD && delta < 16) || (!is_DVD && delta < 8))
        {
            printf("[CDVD] Contiguous read\n");
            cycles_to_seek = get_block_timing(is_DVD) * delta;
            if (!delta)
            {
                drive_status = READING | SPINNING;
                printf("Instant read!\n");
            }
        }
        else if ((is_DVD && delta < 14764) || (!is_DVD && delta < 4371))
        {
            cycles_to_seek = (IOP_CLOCK * 30) / 1000;
            printf("[CDVD] Fast seek\n");
        }
        else
        {
            cycles_to_seek = (IOP_CLOCK * 100) / 1000;
            printf("[CDVD] Full seek\n");
        }
    }

    //Seek anyway. The program won't know the difference
    uint64_t seek_to = sector_pos;
    uint32_t block_count = file_size / LBA;
    int32_t seek_to_int = (int32_t)seek_to;

    //If sector number is negative, start at end of disc and work our way backwards
    if (seek_to_int < 0)
    {
        seek_to = block_count + seek_to_int;
        Errors::print_warning("[CDVD] Negative sector seek, converting to end-of-disc offset\n");
    }

    //Some games (Simpsons Hit and Run, All Star Baseball 2005, etc) read sectors beyond the disc boundary.
    //I'm not sure how this is supposed to be handled, but the games seem to work fine otherwise.
    //TODO: Investigate what the hardware actually does on an invalid seek. Maybe sets some error flag?
    if (seek_to > block_count)
        Errors::print_warning("[CDVD] Invalid sector read $%08X (max size: $%08X)", seek_to, block_count);

    container->seek(seek_to, std::ios::beg);

    add_event(cycles_to_seek);
}

void CDVD_Drive::N_command_read()
{
    sector_pos = *(uint32_t*)&N_command_params[0];
    sectors_left = *(uint32_t*)&N_command_params[4];
    switch (N_command_params[10])
    {
        case 1:
            block_size = 2328;
            break;
        case 2:
            block_size = 2340;
            break;
        default:
            block_size = 2048;
    }
    speed = 24;
    printf("[CDVD] Read; Seek pos: %lu, Sectors: %lu\n", sector_pos, sectors_left);
    start_seek();
    active_N_command = NCOMMAND::READ_SEEK;
}

void CDVD_Drive::N_command_dvdread()
{
    sector_pos = *(uint32_t*)&N_command_params[0];
    sectors_left = *(uint32_t*)&N_command_params[4];
    printf("[CDVD] ReadDVD; Seek pos: %lu, Sectors: %lu\n", sector_pos, sectors_left);
    printf("Last read: %lu cycles ago\n", cycle_count - last_read);
    last_read = cycle_count;
    speed = 4;
    block_size = 2064;
    start_seek();
    active_N_command = NCOMMAND::READ_SEEK;
}

void CDVD_Drive::N_command_gettoc()
{
    printf("[CDVD] Get TOC\n");
    sectors_left = 0;
    block_size = 2064;
    read_bytes_left = 2064;
    memset(read_buffer, 0, 2064);
    read_buffer[0] = 0x04;
    read_buffer[1] = 0x02;
    read_buffer[2] = 0xF2;
    read_buffer[3] = 0x00;
    read_buffer[4] = 0x86;
    read_buffer[5] = 0x72;

    read_buffer[17] = 0x03;
    N_status = 0x40;
    drive_status = READING;
    dma->set_DMA_request(IOP_CDVD);
}

void CDVD_Drive::N_command_readkey(uint32_t arg)
{
    //Code referenced/taken from PCSX2
    //This performs some kind of encryption/checksum with the game's serial?
    memset(cdkey, 0, 16);

    string serial = get_serial();

    int32_t letters = (int32_t)((serial[3] & 0x7F) << 0) |
                (int32_t)((serial[2] & 0x7F) << 7) |
                (int32_t)((serial[1] & 0x7F) << 14) |
                (int32_t)((serial[0] & 0x7F) << 21);
    int32_t code = (int32_t)stoi(serial.substr(5, 3) + serial.substr(9, 2));

    uint32_t key_0_3 = ((code & 0x1FC00) >> 10) | ((0x01FFFFFF & letters) <<  7);
    uint32_t key_4   = ((code & 0x0001F) <<  3) | ((0x0E000000 & letters) >> 25);
    uint32_t key_14  = ((code & 0x003E0) >>  2) | 0x04;

    cdkey[0] = (key_0_3&0x000000FF)>> 0;
    cdkey[1] = (key_0_3&0x0000FF00)>> 8;
    cdkey[2] = (key_0_3&0x00FF0000)>>16;
    cdkey[3] = (key_0_3&0xFF000000)>>24;
    cdkey[4] = key_4;

    switch (arg)
    {
        case 75:
            cdkey[14] = key_14;
            cdkey[15] = 0x05;
            break;
        case 4246:
            cdkey[0] = 0x07;
            cdkey[1] = 0xF7;
            cdkey[2] = 0xF2;
            cdkey[3] = 0x01;
            cdkey[4] = 0x00;
            cdkey[15] = 0x01;
            break;
        default:
            cdkey[15] = 0x01;
            break;
    }

    ISTAT |= 0x2;
    intc->assert_irq(2);
}

void CDVD_Drive::read_CD_sector()
{
    printf("[CDVD] Read CD sector - Sector: %lu Size: %lu\n", current_sector, block_size);
    switch (block_size)
    {
        case 2340:
            fill_CDROM_sector();
            break;
        default:
            container->read(read_buffer, block_size);
            break;
    }
    read_bytes_left = block_size;
    current_sector++;
    sectors_left--;
    dma->set_DMA_request(IOP_CDVD);
}

void CDVD_Drive::fill_CDROM_sector()
{
    uint8_t temp_buffer[2352];

    uint64_t read_sector = current_sector + 150;
    uint32_t minutes = read_sector / 4500;
    read_sector -= (minutes * 4500);
    uint32_t seconds = read_sector / 75;
    uint32_t fragments = read_sector - (seconds * 75);

    printf("Minutes: %d Seconds: %d Fragments: %d\n", minutes, seconds, fragments);

    memset(temp_buffer, 0, 2340);
    for (int i = 0x1; i < 0xB; i++)
        temp_buffer[i] = 0xFF;
    temp_buffer[0xC] = itob(minutes);
    temp_buffer[0xD] = itob(seconds);
    temp_buffer[0xE] = itob(fragments);
    temp_buffer[0xF] = 1;
    container->read(&temp_buffer[0x10 + 0x8], 2048);

    memcpy(read_buffer, temp_buffer + 0xC, 2340);
}

void CDVD_Drive::read_DVD_sector()
{
    printf("[CDVD] Read DVD sector - Sector: %lu Size: %lu\n", current_sector, block_size);

    int layer_num;
    uint32_t lsn;
    bool dual_layer;
    uint64_t layer2_start;
    get_dual_layer_info(dual_layer, layer2_start);

    if (dual_layer && current_sector >= layer2_start)
    {
        layer_num = 1;
        lsn = current_sector - layer2_start + 0x30000;
    }
    else
    {
        layer_num = 0;
        lsn = current_sector + 0x30000;
    }
    read_buffer[0] = 0x20 | layer_num;
    read_buffer[1] = (lsn >> 16) & 0xFF;
    read_buffer[2] = (lsn >> 8) & 0xFF;
    read_buffer[3] = lsn & 0xFF;
    read_buffer[4] = 0;
    read_buffer[5] = 0;
    read_buffer[6] = 0;
    read_buffer[7] = 0;
    read_buffer[8] = 0;
    read_buffer[9] = 0;
    read_buffer[10] = 0;
    read_buffer[11] = 0;
    container->read(&read_buffer[12], 2048);
    read_buffer[2060] = 0;
    read_buffer[2061] = 0;
    read_buffer[2062] = 0;
    read_buffer[2063] = 0;
    read_bytes_left = 2064;
    current_sector++;
    sectors_left--;

    dma->set_DMA_request(IOP_CDVD);
}

void CDVD_Drive::get_dual_layer_info(bool &dual_layer, uint64_t &sector)
{
    dual_layer = false;
    sector = 0;

    uint64_t volume_size = pvd_sector[80]
            + (pvd_sector[81] << 8)
            + (pvd_sector[82] << 16)
            + (pvd_sector[83] << 24);

    //If the size of the volume is less than the size of the file, there must be more than one volume
    if (volume_size < (file_size / LBA))
    {
        dual_layer = true;
        sector = volume_size;
    }
}

void CDVD_Drive::S_command_sub(uint8_t func)
{
    switch (func)
    {
        case 0x00:
            printf("[CDVD] GetMecaconVersion\n");
            prepare_S_outdata(4);
            *(uint32_t*)&S_outdata[0] = 0x00020603;
            break;
        default:
            Errors::die("[CDVD] Unrecognized sub (0x3) S command $%02X\n", func);
    }
}

void CDVD_Drive::write_ISTAT(uint8_t value)
{
    printf("[CDVD] Write ISTAT: $%02X\n", value);
    ISTAT &= ~value;
}

void CDVD_Drive::add_event(uint64_t cycles)
{
    scheduler->add_event(n_command_event_id, cycles * 8);
}
