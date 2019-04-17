#ifndef COP0_HPP
#define COP0_HPP
#include <cstdint>
#include <fstream>

enum COP0_REG
{
    STATUS = 12,
    CAUSE = 13
};

struct COP0_STATUS
{
    bool int_enable;
    bool exception;
    bool error;
    uint8_t mode;
    bool bus_error;
    bool int0_mask;
    bool int1_mask;
    bool timer_int_mask;
    bool master_int_enable;
    bool edi;
    bool ch;
    bool bev;
    bool dev;
    uint8_t cu;
};

struct COP0_CAUSE
{
    uint8_t code;
    bool int0_pending;
    bool int1_pending;
    bool timer_int_pending;
    uint8_t code2;
    uint8_t ce;
    bool bd2;
    bool bd;
};

struct TLB_Entry
{
    bool valid[2];
    bool dirty[2];
    uint8_t cache_mode[2];
    uint32_t pfn[2];
    bool is_scratchpad;
    bool global;
    uint8_t asid;
    uint32_t vpn2;
    uint32_t page_size;
    uint32_t page_mask;

    uint8_t page_shift;
};

class DMAC;

class Cop0
{
    private:
        DMAC* dmac;
        uint8_t* RDRAM;
        uint8_t* BIOS;
        uint8_t* spr;

        uint8_t** kernel_vtlb;
        uint8_t** sup_vtlb;
        uint8_t** user_vtlb;

        void unmap_tlb(TLB_Entry* entry);
        void map_tlb(TLB_Entry* entry);

        uint8_t* get_mem_pointer(uint32_t paddr);
    public:
        TLB_Entry tlb[48];
        uint32_t gpr[32];
        COP0_STATUS status;
        COP0_CAUSE cause;
        uint32_t EPC, ErrorEPC;
        uint32_t PCCR, PCR0, PCR1;
        Cop0(DMAC* dmac);
        ~Cop0();

        uint8_t** get_vtlb_map();

        void reset();
        void init_mem_pointers(uint8_t* RDRAM, uint8_t* BIOS, uint8_t* spr);
        void init_tlb();

        uint32_t mfc(int index);
        void mtc(int index, uint32_t value);

        bool get_condition();
        bool int1_raised();
        bool int_enabled();
        bool int_pending();

        void count_up(int cycles);

        void set_tlb(int index);

        void load_state(std::ifstream &state);
        void save_state(std::ofstream& state);
};

inline bool Cop0::int_enabled()
{
    return status.master_int_enable && status.int_enable && !status.exception && !status.error;
}

#endif // COP0_HPP
