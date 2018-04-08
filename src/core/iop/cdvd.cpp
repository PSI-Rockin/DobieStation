#include "../emulator.hpp"
#include "cdvd.hpp"

using namespace std;

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
    N_status = 0x40;
    N_params = 0;
    N_callback = 0;
    S_params = 0;
    S_status = 0x40;
    S_out_params = 0;
    read_bytes_left = 0;
    ISTAT = 0;
}

int CDVD_Drive::bytes_left()
{
    return read_bytes_left;
}

void CDVD_Drive::read_to_RAM(uint8_t *RAM, uint32_t bytes)
{
    bytes = min(read_bytes_left, (int)bytes);
    cdvd_file.read((char*)RAM, bytes);
    read_bytes_left -= bytes;
    if (read_bytes_left <= 0)
    {
        ISTAT |= 1;
        e->iop_request_IRQ(2);
    }
}

bool CDVD_Drive::load_disc(const char *name)
{
    if (cdvd_file.is_open())
        cdvd_file.close();

    cdvd_file.open(name, ios::in | ios::binary);
    if (!cdvd_file.is_open())
        return false;

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
    uint32_t file_location = 0;
    uint8_t* file;
    file_size = 0;
    printf("[CDVD] Finding %s...\n", name.c_str());
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
                printf("[CDVD] Match found!\n");
                file_location = *(uint32_t*)&root_extent[bytes + 2] * LBA;
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

uint8_t CDVD_Drive::read_N_callback()
{
    return N_callback;
}

uint8_t CDVD_Drive::read_disc_type()
{
    //Assume it's a DVD for now
    return 0x14;
}

uint8_t CDVD_Drive::read_N_status()
{
    return N_status;
}

uint8_t CDVD_Drive::read_S_status()
{
    return S_status;
}

uint8_t CDVD_Drive::read_S_command()
{
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
    return ISTAT;
}

void CDVD_Drive::send_N_command(uint8_t value)
{
    switch (value)
    {
        case 0x06:
            N_command_read();
            break;
        default:
            printf("[CDVD] Unrecognized N command $%02X\n", value);
            exit(1);
    }
    N_params = 0;
}

void CDVD_Drive::write_N_data(uint8_t value)
{
    printf("[CVD] Write NDATA: $%02X\n", value);
    if (N_params > 10)
    {
        printf("[CDVD] Excess NDATA params!\n");
        exit(1);
    }
    else
    {
        N_command_params[N_params] = value;
        N_params++;
    }
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
            for (int i = 0; i < 8; i++)
                S_outdata[i] = 0;
            break;
        default:
            printf("[CDVD] Unrecognized S command $%02X\n", value);
            exit(1);
    }
}

void CDVD_Drive::write_S_data(uint8_t value)
{
    printf("[CDVD] Write SDATA: $%02X (%d)\n", value, S_params);
    if (S_params > 15)
    {
        printf("[CDVD] Excess SDATA params!\n");
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
    if (amount > 15)
    {
        printf("[CDVD] Excess S outdata! (%d)\n", amount);
        exit(1);
    }
    S_out_params = amount;
    S_status &= ~0x40;
    S_params = 0;
}

void CDVD_Drive::N_command_read()
{
    uint32_t seek_pos = *(uint32_t*)&N_command_params[0];
    uint32_t sectors = *(uint32_t*)&N_command_params[4];
    printf("[CDVD] Read; Seek pos: $%08X, Data: $%08X\n", seek_pos * 2048, sectors * 2048);
    read_bytes_left = sectors * 2048;
    N_callback = 0;
    cdvd_file.seekg(seek_pos * 2048);
    e->iop_request_IRQ(2);
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
            printf("[CDVD] Unrecognized sub (0x3) S command $%02X\n", func);
            exit(1);
    }
}

void CDVD_Drive::write_ISTAT(uint8_t value)
{
    ISTAT &= ~value;
}
