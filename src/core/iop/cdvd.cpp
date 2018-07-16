#include <cstring>
#include "../emulator.hpp"
#include "cdvd.hpp"

using namespace std;

//Values from PCSX2 - subject to change
static uint64_t IOP_CLOCK = 36864000;
static const int PSX_CD_READSPEED = 153600;
static const int PSX_DVD_READSPEED = 1382400;

uint32_t CDVD_Drive::get_block_timing(bool mode_DVD)
{
    //TODO: handle block sizes and CD_ROM read speeds
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
    is_reading = false;
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
                    Logger::log(Logger::CDVD, "Unrecognized active N command\n");
                    exit(1);
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

    Logger::log(Logger::CDVD, "Disc size: %lld bytes\n", file_size);
    Logger::log(Logger::CDVD, "Locating Primary Volume Descriptor\n");
    uint8_t type = 0;
    int sector = 0x0F;
    while (type != 1)
    {
        sector++;
        cdvd_file.seekg(sector * 2048);
        cdvd_file >> type;
    }
    Logger::log(Logger::CDVD, "Primary Volume Descriptor found at sector %d\n", sector);

    cdvd_file.seekg(sector * 2048);
    cdvd_file.read((char*)pvd_sector, 2048);

    LBA = *(uint16_t*)&pvd_sector[128];
    Logger::log(Logger::CDVD, "PVD LBA: $%08X\n", LBA);

    root_location = *(uint32_t*)&pvd_sector[156 + 2] * LBA;
    root_len = *(uint32_t*)&pvd_sector[156 + 10];
    Logger::log(Logger::CDVD, "Root dir len: %d\n", *(uint16_t*)&pvd_sector[156]);
    Logger::log(Logger::CDVD, "Extent loc: $%08X\n", root_location);
    Logger::log(Logger::CDVD, "Extent len: $%08X\n", root_len);
    return true;
}

uint8_t* CDVD_Drive::read_file(string name, uint32_t& file_size)
{
    uint8_t* root_extent = new uint8_t[root_len];
    cdvd_file.seekg(root_location);
    cdvd_file.read((char*)root_extent, root_len);
    uint32_t bytes = 0;
    uint32_t file_location = 0;
    uint8_t* file;
    file_size = 0;
    Logger::log(Logger::CDVD, "Finding %s...\n", name.c_str());
    while (bytes < root_len)
    {
        int directory_len = root_extent[bytes + 32];
        if (name.length() == directory_len)
        {
            bool match = true;
            for (int i = 0; i < directory_len; i++)
            {
                if (name[i] != root_extent[bytes + 33 + i])
                {
                    match = false;
                    break;
                }
            }
            if (match)
            {
                Logger::log(Logger::CDVD, "Match found!\n");
                file_location = *(uint32_t*)&root_extent[bytes + 2] * LBA;
                file_size = *(uint32_t*)&root_extent[bytes + 10];
                Logger::log(Logger::CDVD, "Location: $%08X\n", file_location);
                Logger::log(Logger::CDVD, "Size: $%08X\n", file_size);

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
    Logger::log(Logger::CDVD, "Read N_command: $%02X\n", N_command);
    return N_command;
}

uint8_t CDVD_Drive::read_disc_type()
{
    //Not sure what the exact limit is. We'll just go with 1 GB for now.
    Logger::log(Logger::CDVD, "Read disc type\n");
    if (file_size > (1024 * 1024 * 1024))
        return 0x14;
    return 0x12;
}

uint8_t CDVD_Drive::read_N_status()
{
    Logger::log(Logger::CDVD, "Read N_status: $%02X\n", N_status);
    return N_status;
}

uint8_t CDVD_Drive::read_S_status()
{
    Logger::log(Logger::CDVD, "Read S_status: $%02X\n", S_status);
    return S_status;
}

uint8_t CDVD_Drive::read_S_command()
{
    Logger::log(Logger::CDVD, "Read S_command: $%02X\n", S_command);
    return S_command;
}

uint8_t CDVD_Drive::read_S_data()
{
    if (S_out_params <= 0)
        return 0;
    uint8_t value = S_outdata[S_params];
    Logger::log(Logger::CDVD, "Read S data: $%02X\n", value);
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
    Logger::log(Logger::CDVD, "Read ISTAT: $%02X\n", ISTAT);
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
            Logger::log(Logger::CDVD, "GetTOC\n");
            N_command_gettoc();
            break;
        default:
            Logger::log(Logger::CDVD, "Unrecognized N command $%02X\n", value);
            exit(1);
    }
    N_params = 0;
}

uint8_t CDVD_Drive::read_drive_status()
{
    Logger::log(Logger::CDVD, "Read drive status: $%02X\n", drive_status);
    return drive_status;
}

void CDVD_Drive::write_N_data(uint8_t value)
{
    Logger::log(Logger::CDVD, "Write NDATA: $%02X\n", value);
    if (N_params > 10)
    {
        Logger::log(Logger::CDVD, "Excess NDATA params!\n");
        exit(1);
    }
    else
    {
        N_command_params[N_params] = value;
        N_params++;
    }
}

void CDVD_Drive::write_BREAK()
{
    Logger::log(Logger::CDVD, "Write BREAK\n");
    if (N_status || active_N_command == NCOMMAND::BREAK)
        return;

    N_cycles_left = 64;
    active_N_command = NCOMMAND::BREAK;
    drive_status = CDVD_STATUS::STOPPED;
    read_bytes_left = 0;
}

void CDVD_Drive::send_S_command(uint8_t value)
{
    Logger::log(Logger::CDVD, "Send S command: $%02X\n", value);
    S_status &= ~0x40;
    S_command = value;
    switch (value)
    {
        case 0x03:
            S_command_sub(S_command_params[0]);
            break;
        case 0x05:
            Logger::log(Logger::CDVD, "Media Change?\n");
            prepare_S_outdata(1);
            S_outdata[0] = 0;
            break;
        case 0x08:
            Logger::log(Logger::CDVD, "ReadClock\n");
            prepare_S_outdata(8);
            for (int i = 0; i < 8; i++)
                S_outdata[i] = 0;
            break;
        case 0x15:
            Logger::log(Logger::CDVD, "ForbidDVD\n");
            prepare_S_outdata(1);
            S_outdata[0] = 5;
            break;
        case 0x17:
            Logger::log(Logger::CDVD, "ReadILinkModel\n");
            prepare_S_outdata(9);
            break;
        case 0x1A:
            Logger::log(Logger::CDVD, "BootCertify\n");
            prepare_S_outdata(1);
            S_outdata[0] = 1; //means OK according to PCSX2
            break;
        case 0x1B:
            Logger::log(Logger::CDVD, "CancelPwOffReady\n");
            prepare_S_outdata(1);
            S_outdata[0] = 0;
            break;
        case 0x40:
            Logger::log(Logger::CDVD, "OpenConfig\n");
            prepare_S_outdata(1);
            S_outdata[0] = 0;
            break;
        case 0x41:
            Logger::log(Logger::CDVD, "ReadConfig\n");
            prepare_S_outdata(16);
            memset(S_outdata, 0, 16);
            break;
        case 0x43:
            Logger::log(Logger::CDVD, "CloseConfig\n");
            prepare_S_outdata(1);
            S_outdata[0] = 0;
            break;
        default:
            Logger::log(Logger::CDVD, "Unrecognized S command $%02X\n", value);
            exit(1);
    }
}

void CDVD_Drive::write_S_data(uint8_t value)
{
    Logger::log(Logger::CDVD, "Write SDATA: $%02X (%d)\n", value, S_params);
    if (S_params > 15)
    {
        Logger::log(Logger::CDVD, "Excess SDATA params!\n");
        exit(1);
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
        Logger::log(Logger::CDVD, "Excess S outdata! (%d)\n", amount);
        exit(1);
    }
    S_out_params = amount;
    S_status &= ~0x40;
    S_params = 0;
}

void CDVD_Drive::start_seek()
{
    N_status = 0;
    is_reading = false;
    drive_status = PAUSED;

    if (!is_spinning)
    {
        //1/3 of a second
        N_cycles_left = IOP_CLOCK / 3;
        //N_cycles_left = 1000000;
        Logger::log(Logger::CDVD, "Spinning\n");
        is_spinning = true;
    }
    else
    {
        Logger::log(Logger::CDVD, "Seeking\n");
        bool is_DVD = N_command != 0x06;
        int delta = abs((int)current_sector - (int)sector_pos);
        if (delta < 16)
        {
            Logger::log(Logger::CDVD, "Contiguous read\n");
            N_cycles_left = get_block_timing(is_DVD) * delta;
            if (!delta)
            {
                drive_status = READING | SPINNING;
                Logger::log(Logger::OTHER, "Instant read!\n");
            }
        }
        else if ((is_DVD && delta < 14764) || (!is_DVD && delta < 4371))
        {
            N_cycles_left = (IOP_CLOCK * 30) / 1000;
            Logger::log(Logger::CDVD, "Fast seek\n");
        }
        else
        {
            N_cycles_left = (IOP_CLOCK * 100) / 1000;
            Logger::log(Logger::CDVD, "Full seek\n");
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
    if (N_command_params[10])
        exit(1);
    speed = 24;
    Logger::log(Logger::CDVD, "Read; Seek pos: %d, Sectors: %d\n", sector_pos, sectors_left);
    block_size = 2048;
    start_seek();
    active_N_command = NCOMMAND::READ_SEEK;
}

void CDVD_Drive::N_command_dvdread()
{
    sector_pos = *(uint32_t*)&N_command_params[0];
    sectors_left = *(uint32_t*)&N_command_params[4];
    Logger::log(Logger::CDVD, "ReadDVD; Seek pos: %d, Sectors: %d\n", sector_pos, sectors_left);
    Logger::log(Logger::OTHER, "Last read: %lld cycles ago\n", cycle_count - last_read);
    last_read = cycle_count;
    block_size = 2064;
    start_seek();
    active_N_command = NCOMMAND::READ_SEEK;
}

void CDVD_Drive::N_command_gettoc()
{
    Logger::log(Logger::CDVD, "Get TOC\n");
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
    Logger::log(Logger::CDVD, "Read CD sector - Sector: %d Size: %d\n", current_sector, block_size);
    cdvd_file.read((char*)read_buffer, block_size);
    read_bytes_left = block_size;
    current_sector++;
    sectors_left--;
}

void CDVD_Drive::read_DVD_sector()
{
    Logger::log(Logger::CDVD, "Read DVD sector - Sector: %d Size: %d\n", current_sector, block_size);
    uint32_t lsn = current_sector + 0x30000;
    read_buffer[0] = 0x20;
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

void CDVD_Drive::S_command_sub(uint8_t func)
{
    switch (func)
    {
        case 0x00:
            Logger::log(Logger::CDVD, "GetMecaconVersion\n");
            prepare_S_outdata(4);
            *(uint32_t*)&S_outdata[0] = 0x00020603;
            break;
        default:
            Logger::log(Logger::CDVD, "Unrecognized sub (0x3) S command $%02X\n", func);
            exit(1);
    }
}

void CDVD_Drive::write_ISTAT(uint8_t value)
{
    Logger::log(Logger::CDVD, "Write ISTAT: $%02X\n", value);
    ISTAT &= ~value;
}
