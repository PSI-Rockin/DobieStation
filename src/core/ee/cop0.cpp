#include <cstdio>
#include <cstring>
#include "cop0.hpp"
#include "dmac.hpp"

Cop0::Cop0(DMAC* dmac) : dmac(dmac)
{
    RDRAM = nullptr;
    BIOS = nullptr;
    spr = nullptr;
    kernel_vtlb = nullptr;
    sup_vtlb = nullptr;
    user_vtlb = nullptr;
    vtlb_info = nullptr;
}

Cop0::~Cop0()
{
    delete[] kernel_vtlb;
    delete[] sup_vtlb;
    delete[] user_vtlb;
    delete[] vtlb_info;
}

uint8_t** Cop0::get_vtlb_map()
{
    if (status.exception || status.error)
        return kernel_vtlb;

    switch (status.mode)
    {
        case 0:
            return kernel_vtlb;
        case 1:
            return sup_vtlb;
        case 2:
            return user_vtlb;
        default:
            return kernel_vtlb;
    }
}

void Cop0::reset()
{
    for (int i = 0; i < 32; i++)
        gpr[i] = 0;

    memset(tlb, 0, sizeof(tlb));

    status.int_enable = false;
    status.exception = false;
    status.error = false;
    status.mode = 0;
    status.int0_mask = false;
    status.int1_mask = false;
    status.bus_error = false;
    status.master_int_enable = false;
    status.edi = false;
    status.ch = false;
    status.bev = true;
    status.dev = false;
    status.cu = 0x7;

    cause.code = 0;
    cause.int0_pending = false;
    cause.int1_pending = false;
    cause.timer_int_pending = false;
    cause.code2 = 0;
    cause.ce = 0;
    cause.bd2 = false;
    cause.bd = false;

    PCCR = 0;
    PCR0 = 0;
    PCR1 = 0;

    //Set processor revision id
    gpr[15] = 0x00002E20;
}

void Cop0::init_mem_pointers(uint8_t *RDRAM, uint8_t *BIOS, uint8_t *spr)
{
    this->RDRAM = RDRAM;
    this->BIOS = BIOS;
    this->spr = spr;
}

void Cop0::init_tlb()
{
    if (!kernel_vtlb)
        kernel_vtlb = new uint8_t*[1024 * 1024];
    if (!sup_vtlb)
        sup_vtlb = new uint8_t*[1024 * 1024];
    if (!user_vtlb)
        user_vtlb = new uint8_t*[1024 * 1024];
    if (!vtlb_info)
        vtlb_info = new VTLB_Info[1024 * 1024];

    memset(kernel_vtlb, 0, 1024 * 1024 * sizeof(uint8_t*));
    memset(sup_vtlb, 0, 1024 * 1024 * sizeof(uint8_t*));
    memset(user_vtlb, 0, 1024 * 1024 * sizeof(uint8_t*));
    memset(vtlb_info, 0, 1024 * 1024 * sizeof(VTLB_Info));

    //Kernel segments are unmapped to TLB, so we must define them explicitly
    uint32_t unmapped_start = 0x80000000, unmapped_end = 0xC0000000;
    for (uint32_t i = unmapped_start; i < unmapped_end; i += 4096)
    {
        int map_index = i / 4096;
        kernel_vtlb[map_index] = get_mem_pointer(i & 0x1FFFFFFF);
        if (i < 0xA0000000)
            vtlb_info[map_index].cache_mode = CACHED;
        else
            vtlb_info[map_index].cache_mode = UNCACHED;
    }
}

uint32_t Cop0::mfc(int index)
{
    //printf("[COP0] Move from reg%d\n", index);
    switch (index)
    {
        case 12:
        {
            uint32_t reg = 0;
            reg |= status.int_enable;
            reg |= status.exception << 1;
            reg |= status.error << 2;
            reg |= status.mode << 3;
            reg |= status.int0_mask << 10;
            reg |= status.int1_mask << 11;
            reg |= status.bus_error << 12;
            reg |= status.timer_int_mask << 15;
            reg |= status.master_int_enable << 16;
            reg |= status.edi << 17;
            reg |= status.ch << 18;
            reg |= status.bev << 22;
            reg |= status.dev << 23;
            reg |= status.cu << 28;
            return reg;
        }
        case 13:
        {
            uint32_t reg = 0;
            reg |= cause.code << 2;
            reg |= cause.int0_pending << 10;
            reg |= cause.int1_pending << 11;
            reg |= cause.timer_int_pending << 15;
            reg |= cause.code2 << 16;
            reg |= cause.ce << 28;
            reg |= cause.bd2 << 30;
            reg |= cause.bd << 31;
            return reg;
        }
        case 14:
            return EPC;
        case 30:
            return ErrorEPC;
        default:
            return gpr[index];
    }
}

//Returns true if we need to check for interrupts
void Cop0::mtc(int index, uint32_t value)
{
    //printf("[COP0] Move to reg%d: $%08X\n", index, value);
    switch (index)
    {
        case 12:
            status.int_enable = value & 1;
            status.exception = value & (1 << 1);
            status.error = value & (1 << 2);
            status.mode = (value >> 3) & 0x3;
            status.int0_mask = value & (1 << 10);
            status.int1_mask = value & (1 << 11);
            status.bus_error = value & (1 << 12);
            status.timer_int_mask = value & (1 << 15);
            status.master_int_enable = value & (1 << 16);
            status.edi = value & (1 << 17);
            status.ch = value & (1 << 18);
            status.bev = value & (1 << 22);
            status.dev = value & (1 << 23);
            status.cu = (value >> 28);
            break;
        case 13:
            //No bits in CAUSE are writable
            break;
        case 14:
            EPC = value;
            break;
        case 30:
            ErrorEPC = value;
            break;
        case 11:
            cause.timer_int_pending = false; //Interrupt is cleared on COMPARE write
            //Fall through
        default:
            gpr[index] = value;
    }
}

/**
 * Coprocessor 0 is wired to the DMAC. CP0COND is true when all DMA transfers indicated in PCR have completed.
 */
bool Cop0::get_condition()
{
    uint32_t STAT = dmac->read32(0x1000E010) & 0x3FF;
    uint32_t PCR = dmac->read32(0x1000E020) & 0x3FF;
    return ((~PCR | STAT) & 0x3FF) == 0x3FF;
}

bool Cop0::int1_raised()
{
    return int_enabled() && (gpr[CAUSE] & (1 << 11));
}

bool Cop0::int_pending()
{
    if (status.int0_mask && cause.int0_pending)
        return true;
    if (status.int1_mask && cause.int1_pending)
        return true;
    return false;
}

void Cop0::read_tlb(int index)
{
    TLB_Entry* entry = &tlb[index];
    //PageMask
    gpr[5] = entry->page_mask << 13;
    //EntryHi  (VPN | ASID) & ~PageMask
    gpr[10] = ((entry->vpn2 << 13) | entry->asid) & ~gpr[5];
    //EntryLo0
    gpr[2] = (entry->is_scratchpad << 31) | (entry->pfn[1] << 6) | (entry->cache_mode[1] << 3) | (entry->dirty[1] << 2) | (entry->valid[1] << 1) | entry->global;
    //EntryLo1
    gpr[3] = (entry->pfn[0] << 6) | (entry->cache_mode[0] << 3) | (entry->dirty[0] << 2) | (entry->valid[0] << 1) | entry->global;
}

void Cop0::set_tlb(int index)
{
    TLB_Entry* new_entry = &tlb[index];

    unmap_tlb(new_entry);

    new_entry->is_scratchpad = gpr[2] >> 31;

    new_entry->page_mask = (gpr[5] >> 13) & 0xFFF;
    new_entry->page_shift = 0;
    switch (new_entry->page_mask)
    {
        case 0x000:
            new_entry->page_size = 1024 * 4;
            new_entry->page_shift = 0;
            break;
        case 0x003:
            new_entry->page_size = 1024 * 16;
            new_entry->page_shift = 2;
            break;
        case 0x00F:
            new_entry->page_size = 1024 * 64;
            new_entry->page_shift = 4;
            break;
        case 0x03F:
            new_entry->page_size = 1024 * 256;
            new_entry->page_shift = 6;
            break;
        case 0x0FF:
            new_entry->page_size = 1024 * 1024;
            new_entry->page_shift = 8;
            break;
        case 0x3FF:
            new_entry->page_size = 1024 * 1024 * 4;
            new_entry->page_shift = 10;
            break;
        case 0xFFF:
            new_entry->page_size = 1024 * 1024 * 16;
            new_entry->page_shift = 12;
            break;
        default:
            new_entry->page_size = 0;
    }

    //EntryHi
    new_entry->asid = gpr[10] & 0xFF;
    new_entry->vpn2 = gpr[10] >> 13;

    new_entry->global = true;

    for (int i = 0; i < 2; i++)
    {
        uint32_t entry_lo = gpr[i + 2];

        //Both G bits in the EntryLo registers must be set for global to be set
        new_entry->global &= entry_lo & 0x1;
        new_entry->valid[i] = (entry_lo >> 1) & 0x1;
        new_entry->dirty[i] = (entry_lo >> 2) & 0x1;
        new_entry->cache_mode[i] = (entry_lo >> 3) & 0x7;
        new_entry->pfn[i] = (entry_lo >> 6) & 0xFFFFF;
    }

    uint32_t real_virt = new_entry->vpn2 * 2;
    real_virt >>= new_entry->page_shift;
    real_virt *= new_entry->page_size;

    for (int i = 0; i < 2; i++)
    {
        uint32_t real_phy = new_entry->pfn[i] >> new_entry->page_shift;
        real_phy *= new_entry->page_size;
    }

    map_tlb(new_entry);
}

void Cop0::count_up(int cycles)
{
    //Check if TIMER interrupt has triggered
    if (gpr[9] < gpr[11] && (gpr[9] + cycles) >= gpr[11])
    {
        cause.timer_int_pending = true;
    }

    gpr[9] += cycles;

    if (PCCR & (1 << 31))
    {
        uint32_t PCR_MASK = (1 << (status.mode + 2)) | (status.exception << 1);

        //Performance counter registers
        if (PCCR & PCR_MASK)
        {
            int event0 = (PCCR >> 5) & 0x1F;
            bool can_count0 = false;

            switch (event0)
            {
                case 1: //Processor cycle
                case 2: //Single/double instruction issue
                case 3: //Branch issued/mispredicted
                case 12: //Instruction completed
                case 13: //Non-delay slot instruction completed
                case 14: //COP2/COP1 instruction completed
                case 15: //Load/store instruction completed
                    can_count0 = true;
                    break;
            }

            if (can_count0)
                PCR0 += cycles;
        }

        PCR_MASK <<= 10;

        if (PCCR & PCR_MASK)
        {
            int event1 = (PCCR >> 15) & 0x1F;
            bool can_count1 = false;

            switch (event1)
            {
                case 1: //Processor cycle
                case 2: //Single/double instruction issue
                case 3: //Branch issued/mispredicted
                case 12: //Instruction completed
                case 13: //Non-delay slot instruction completed
                case 14: //COP2/COP1 instruction completed
                case 15: //Load/store instruction completed
                    can_count1 = true;
                    break;
            }

            if (can_count1)
                PCR1 += cycles;
        }
    }
}

void Cop0::unmap_tlb(TLB_Entry *entry)
{
    uint32_t real_vpn = entry->vpn2 * 2;
    real_vpn >>= entry->page_shift;

    uint32_t even_page = (real_vpn * entry->page_size) / 4096;
    uint32_t odd_page = ((real_vpn + 1) * entry->page_size) / 4096;

    if (entry->is_scratchpad)
    {
        if (entry->valid[0])
        {
            for (uint32_t i = 0; i < 1024 * 16; i += 4096)
            {
                int map_index = i / 4096;
                kernel_vtlb[even_page + map_index] = nullptr;
                sup_vtlb[even_page + map_index] = nullptr;
                user_vtlb[even_page + map_index] = nullptr;
            }
        }
    }
    else
    {
        if (entry->valid[0])
        {
            for (uint32_t i = 0; i < entry->page_size; i += 4096)
            {
                int map_index = i / 4096;
                kernel_vtlb[even_page + map_index] = nullptr;
                sup_vtlb[even_page + map_index] = nullptr;
                user_vtlb[even_page + map_index] = nullptr;
            }
        }

        if (entry->valid[1])
        {
            for (uint32_t i = 0; i < entry->page_size; i += 4096)
            {
                int map_index = i / 4096;
                kernel_vtlb[odd_page + map_index] = nullptr;
                sup_vtlb[odd_page + map_index] = nullptr;
                user_vtlb[odd_page + map_index] = nullptr;
            }
        }
    }
}

void Cop0::map_tlb(TLB_Entry *entry)
{
    uint32_t real_vpn = entry->vpn2 * 2;
    real_vpn >>= entry->page_shift;

    uint32_t even_virt_page = (real_vpn * entry->page_size) / 4096;
    uint32_t even_virt_addr = even_virt_page * 4096;
    uint32_t even_phy_addr = (entry->pfn[0] >> entry->page_shift) * entry->page_size;
    uint32_t odd_virt_page = ((real_vpn + 1) * entry->page_size) / 4096;
    uint32_t odd_virt_addr = odd_virt_page * 4096;
    uint32_t odd_phy_addr = (entry->pfn[1] >> entry->page_shift) * entry->page_size;

    if (entry->is_scratchpad)
    {
        if (entry->valid[0])
        {
            for (uint32_t i = 0; i < 1024 * 16; i += 4096)
            {
                int map_index = i / 4096;
                kernel_vtlb[even_virt_page + map_index] = spr + i;
                sup_vtlb[even_virt_page + map_index] = spr + i;
                user_vtlb[even_virt_page + map_index] = spr + i;
                vtlb_info[even_virt_page + map_index].cache_mode = SPR;
            }
        }
    }
    else
    {
        if (entry->valid[0])
        {
            for (uint32_t i = 0; i < entry->page_size; i += 4096)
            {
                int map_index = i / 4096;
                uint8_t* mem = get_mem_pointer(even_phy_addr + i);
                kernel_vtlb[even_virt_page + map_index] = mem;

                if (even_virt_addr < 0x80000000)
                {
                    sup_vtlb[even_virt_page + map_index] = mem;
                    user_vtlb[even_virt_page + map_index] = mem;
                }
                else if (even_virt_addr >= 0xC0000000 && even_virt_addr < 0xE0000000)
                    sup_vtlb[even_virt_page + map_index] = mem;

                vtlb_info[even_virt_page + map_index].cache_mode = entry->cache_mode[0];
            }
        }

        if (entry->valid[1])
        {
            for (uint32_t i = 0; i < entry->page_size; i += 4096)
            {
                int map_index = i / 4096;
                uint8_t* mem = get_mem_pointer(odd_phy_addr + i);
                kernel_vtlb[odd_virt_page + map_index] = mem;

                if (odd_virt_addr < 0x80000000)
                {
                    sup_vtlb[odd_virt_page + map_index] = mem;
                    user_vtlb[odd_virt_page + map_index] = mem;
                }
                else if (odd_virt_addr >= 0xC0000000 && odd_virt_addr < 0xE0000000)
                    sup_vtlb[odd_virt_page + map_index] = mem;

                vtlb_info[odd_virt_page + map_index].cache_mode = entry->cache_mode[1];
            }
        }
    }
}

void Cop0::clear_tlb_modified(size_t page)
{
    vtlb_info[page].modified = false;
}

void Cop0::set_tlb_modified(size_t page)
{
    vtlb_info[page].modified = true;
}

uint8_t* Cop0::get_mem_pointer(uint32_t paddr)
{
    if (paddr < 0x10000000)
    {
        paddr &= (1024 * 1024 * 32) - 1;
        return RDRAM + paddr;
    }

    if (paddr >= 0x1FC00000 && paddr < 0x20000000)
    {
        paddr &= (1024 * 1024 * 4) - 1;
        return BIOS + paddr;
    }

    //Indicates that the region is MMIO and must be handled specially
    return (uint8_t*)1;
}
