#include "cdvd.hpp"

using namespace std;

CDVD_Drive::CDVD_Drive()
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
    S_status = 0x40;
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

void CDVD_Drive::send_S_command(uint8_t value)
{
    switch (value)
    {
        case 0x05:
            printf("[CDVD] Media Change?\n");
            break;
        default:
            printf("[CDVD] Unrecognized S command $%02X\n", value);
            exit(1);
    }
}
