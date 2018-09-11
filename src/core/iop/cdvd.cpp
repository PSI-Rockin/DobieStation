#include <cstring>
#include <ctime>
#include "../emulator.hpp"
#include "cdvd.hpp"
#include "../errors.hpp"

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

CDVD_Drive::CDVD_Drive(Emulator* e) : e(e)
{

}

CDVD_Drive::~CDVD_Drive()
{
    if (cdvd_file.is_open())
        cdvd_file.close();
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
    N_cycles_left = 0;
    N_status = 0x40;
    N_params = 0;
    N_command = 0;
    S_params = 0;
    S_status = 0x40;
    S_out_params = 0;
    read_bytes_left = 0;
    ISTAT = 0;
    file_size = 0;
    time_t raw_time;
    struct tm * time;
    std::time(&raw_time);
    time = std::gmtime(&raw_time);
    rtc.vsyncs = 0;
    rtc.second = time->tm_sec;
    rtc.minute = time->tm_min;
    rtc.hour = time->tm_hour+9;
    rtc.day = time->tm_mday;
    rtc.month = time->tm_mon+1; //Jan = 0
    rtc.year = time->tm_year - 100; //Returns Years since 1900

    if (rtc.hour >= 24)
    {
        rtc.hour -= 24;
        rtc.day++;
        if (rtc.day > (rtc.month == 2 && rtc.year % 4 == 0 ? 29 : monthmap[rtc.month - 1]))
        {
            rtc.month++;
            if (rtc.month > 12)
            {
                rtc.month = 1;
                rtc.year++;
            }
            rtc.day = 1;
        }
    }
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
    read_bytes_left -= block_size;
    if (read_bytes_left <= 0)
    {
        if (sectors_left == 0)
        {
            N_status = 0x4E;
            ISTAT |= 0x3;
            e->iop_request_IRQ(2);
            drive_status = PAUSED;
            active_N_command = NCOMMAND::NONE;
        }
        else
        {
            active_N_command = NCOMMAND::READ;
            N_cycles_left = get_block_timing(N_command != 0x06);
        }
    }
    return block_size;
}

void CDVD_Drive::update(int cycles)
{
    cycle_count += cycles;
    if (N_cycles_left >= 0 && active_N_command != NCOMMAND::NONE)
    {
        N_cycles_left -= cycles;
        if (N_cycles_left <= 0)
        {
            switch (active_N_command)
            {
                case NCOMMAND::SEEK:
                    drive_status = PAUSED;
                    active_N_command = NCOMMAND::NONE;
                    current_sector = sector_pos;
                    N_status = 0x4E;
                    ISTAT |= 0x2;
                    e->iop_request_IRQ(2);
                    break;
                case NCOMMAND::STANDBY:
                    drive_status = PAUSED;
                    active_N_command = NCOMMAND::NONE;
                    N_status = 0x40;
                    ISTAT |= 0x2;
                    e->iop_request_IRQ(2);
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
                        N_cycles_left = 1000; //Check later to see if there's space in the buffer
                    break;
                case NCOMMAND::READ_SEEK:
                    drive_status = READING | SPINNING;
                    active_N_command = NCOMMAND::READ;
                    current_sector = sector_pos;
                    N_cycles_left = get_block_timing(N_command != 0x06);
                    break;
                case NCOMMAND::BREAK:
                    drive_status = PAUSED;
                    active_N_command = NCOMMAND::NONE;
                    N_status = 0x4E;
                    ISTAT |= 0x2;
                    e->iop_request_IRQ(2);
                    break;
                default:
                    Errors::die("[CDVD] Unrecognized active N command\n");
            }
        }
    }
}

bool CDVD_Drive::load_disc(const char *name)
{
    if (cdvd_file.is_open())
        cdvd_file.close();

    cdvd_file.open(name, ios::in | ios::binary | ifstream::ate);
    if (!cdvd_file.is_open())
        return false;

    file_size = cdvd_file.tellg();

    printf("[CDVD] Disc size: %lld bytes\n", file_size);
    printf("[CDVD] Locating Primary Volume Descriptor\n");
    uint8_t type = 0;
    int sector = 0x0F;
    while (type != 1)
    {
        sector++;
        cdvd_file.seekg(sector * 2048);
        cdvd_file >> type;
    }
    printf("[CDVD] Primary Volume Descriptor found at sector %d\n", sector);

    cdvd_file.seekg(sector * 2048);
    cdvd_file.read((char*)pvd_sector, 2048);

    LBA = *(uint16_t*)&pvd_sector[128];
    printf("[CDVD] PVD LBA: $%08X\n", LBA);

    root_location = *(uint32_t*)&pvd_sector[156 + 2] * LBA;
    root_len = *(uint32_t*)&pvd_sector[156 + 10];
    printf("[CDVD] Root dir len: %d\n", *(uint16_t*)&pvd_sector[156]);
    printf("[CDVD] Extent loc: $%08X\n", root_location);
    printf("[CDVD] Extent len: $%08X\n", root_len);
    return true;
}

uint8_t* CDVD_Drive::read_file(string name, uint32_t& file_size)
{
    uint8_t* root_extent = new uint8_t[root_len];
    cdvd_file.seekg(root_location);
    cdvd_file.read((char*)root_extent, root_len);
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
                file_location *= LBA;
                file_size = *(uint32_t*)&root_extent[bytes + 10];
                printf("[CDVD] Location: $%08X\n", file_location);
                printf("[CDVD] Size: $%08X\n", file_size);

                file = new uint8_t[file_size];
                cdvd_file.seekg(file_location);
                cdvd_file.read((char*)file, file_size);
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
    if (file_size > (1024 * 1024 * 1024))
        return 0x14;
    return 0x12;
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

void CDVD_Drive::send_N_command(uint8_t value)
{
    N_command = value;
    switch (value)
    {
        //NOPSync/NOP
        case 0x00:
        case 0x01:
            e->iop_request_IRQ(2);
            break;
        case 0x02:
            //Standby
            sector_pos = 0;
            start_seek();
            active_N_command = NCOMMAND::STANDBY;
            break;
        case 0x04:
            //Pause
            e->iop_request_IRQ(2);
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
    if (N_status || active_N_command == NCOMMAND::BREAK)
        return;

    N_cycles_left = 64;
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
    N_status = 0;
    drive_status = PAUSED;

    if (!is_spinning)
    {
        //1/3 of a second
        N_cycles_left = IOP_CLOCK / 3;
        //N_cycles_left = 1000000;
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
            N_cycles_left = get_block_timing(is_DVD) * delta;
            if (!delta)
            {
                drive_status = READING | SPINNING;
                printf("Instant read!\n");
            }
        }
        else if ((is_DVD && delta < 14764) || (!is_DVD && delta < 4371))
        {
            N_cycles_left = (IOP_CLOCK * 30) / 1000;
            printf("[CDVD] Fast seek\n");
        }
        else
        {
            N_cycles_left = (IOP_CLOCK * 100) / 1000;
            printf("[CDVD] Full seek\n");
        }
        //N_cycles_left = 10000;
    }

    //Seek anyway. The program won't know the difference
    cdvd_file.seekg(sector_pos * 2048);
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
    printf("[CDVD] Read; Seek pos: %lld, Sectors: %lld\n", sector_pos, sectors_left);
    start_seek();
    active_N_command = NCOMMAND::READ_SEEK;
}

void CDVD_Drive::N_command_dvdread()
{
    sector_pos = *(uint32_t*)&N_command_params[0];
    sectors_left = *(uint32_t*)&N_command_params[4];
    printf("[CDVD] ReadDVD; Seek pos: %lld, Sectors: %lld\n", sector_pos, sectors_left);
    printf("Last read: %lld cycles ago\n", cycle_count - last_read);
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
    N_status = 0;
    drive_status = READING;
}

void CDVD_Drive::read_CD_sector()
{
    printf("[CDVD] Read CD sector - Sector: %lld Size: %lld\n", current_sector, block_size);
    cdvd_file.read((char*)read_buffer, block_size);
    read_bytes_left = block_size;
    current_sector++;
    sectors_left--;
}

void CDVD_Drive::read_DVD_sector()
{
    printf("[CDVD] Read DVD sector - Sector: %lld Size: %lld\n", current_sector, block_size);

    int layer_num;
    uint32_t lsn;
    bool dual_layer;
    uint64_t layer2_start;
    get_dual_layer_info(dual_layer, layer2_start);

    if (dual_layer && current_sector >= layer2_start && false)
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
    cdvd_file.read((char*)&read_buffer[12], 2048);
    read_buffer[2060] = 0;
    read_buffer[2061] = 0;
    read_buffer[2062] = 0;
    read_buffer[2063] = 0;
    read_bytes_left = 2064;
    current_sector++;
    sectors_left--;
}

void CDVD_Drive::get_dual_layer_info(bool &dual_layer, uint64_t &sector)
{
    dual_layer = false;
    sector = 0;

    uint64_t volume_size = *(uint32_t*)&pvd_sector[80] * 2048;

    //If the size of the volume is less than the size of the file, there must be more than one volume
    if (volume_size < file_size)
    {
        dual_layer = true;
        sector = volume_size / 2048;
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
