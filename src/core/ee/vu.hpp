#ifndef VU_HPP
#define VU_HPP
#include <cstdint>
#include <cstdio>
#include <fstream>

#include "../int128.hpp"

union VU_R
{
    float f;
    uint32_t u;
};

union VU_GPR
{
    float f[4];
    uint32_t u[4];
    int32_t s[4];
};

union VU_I
{
    uint16_t u;
    int16_t s;
};

struct VPU_STAT
{
    bool vu1_running;
};

struct DecodedRegs
{
    //0 = lower, 1 = upper
    uint8_t vf_write[2];
    uint8_t vf_write_field[2];

    uint8_t vf_read0[2], vf_read1[2];
    uint8_t vf_read0_field[2], vf_read1_field[2];

    uint8_t vi_read0, vi_read1, vi_write;
};

class GraphicsInterface;

class VectorUnit
{
    private:
        GraphicsInterface* gif;
        int id;

        uint64_t cycle_count;

        uint16_t *VIF_TOP, *VIF_ITOP;

        uint8_t instr_mem[1024 * 16];
        uint8_t data_mem[1024 * 16];

        bool running;
        uint16_t PC, new_PC, secondbranch_PC;
        bool branch_on;
        bool finish_on;
        bool second_branch_pending;
        int delay_slot;

        int XGKICK_cycles;
        uint16_t GIF_addr;
        bool transferring_GIF;
        bool XGKICK_stall;
        uint16_t stalled_GIF_addr;

        //GPR
        VU_GPR gpr[32];
        VU_I int_gpr[16];

        //Control registers
        uint32_t CMSAR0;
        VU_GPR ACC;
        uint32_t status;
        uint32_t clip_flags;
        VU_R R;
        VU_R I;
        VU_R Q;
        VU_R P;

        uint32_t CLIP_pipeline[4];
        uint64_t MAC_pipeline[4];
        uint32_t* CLIP_flags;
        uint64_t* MAC_flags; //pointer to last element in the pipeline; the register accessible to programs
        uint16_t new_MAC_flags; //to be placed in the pipeline

        VU_R new_Q_instance;
        uint64_t finish_DIV_event;

        float update_mac_flags(float value, int index);
        void clear_mac_flags(int index);

        //Updates new_Q_instance
        void start_DIV_unit(int latency);
        
        void update_status();
        void advance_r();
        void print_vectors(uint8_t a, uint8_t b);
        float convert();
    public:
        VectorUnit(int id);

        DecodedRegs decoder;

        void set_TOP_regs(uint16_t* TOP, uint16_t* ITOP);
        void set_GIF(GraphicsInterface* gif);

        void update_mac_pipeline();
        void check_for_FMAC_stall();
        void flush_pipes();

        void run(int cycles);
        void handle_XGKICK();
        void callmsr();
        void mscal(uint32_t addr);
        void end_execution();
        void reset();

        static float convert(uint32_t value);

        template <typename T> T read_instr(uint32_t addr);
        template <typename T> T read_data(uint32_t addr);
        template <typename T> void write_instr(uint32_t addr, T data);
        template <typename T> void write_data(uint32_t addr, T data);

        bool is_running();
        uint16_t get_PC();
        uint32_t get_gpr_u(int index, int field);
        uint16_t get_int(int index);
        int get_id();
        void set_gpr_f(int index, int field, float value);
        void set_gpr_u(int index, int field, uint32_t value);
        void set_int(int index, uint16_t value);
        void set_status(uint32_t value);
        void set_R(uint32_t value);
        void set_I(uint32_t value);
        void set_Q(uint32_t value);

        uint32_t cfc(int index);
        void ctc(int index, uint32_t value);

        void branch(bool condition, int16_t imm, bool link, uint8_t linkreg = 0);
        void jp(uint16_t addr, bool link, uint8_t linkreg = 0);

        void abs(uint32_t instr);
        void add(uint32_t instr);
        void adda(uint32_t instr);
        void addabc(uint32_t instr);
        void addai(uint32_t instr);
        void addbc(uint32_t instr);
        void addi(uint32_t instr);
        void addq(uint32_t instr);
        void clip(uint32_t instr);
        void div(uint32_t instr);
        void eexp(uint32_t instr);
        void eleng(uint32_t instr);
        void esqrt(uint32_t instr);
        void erleng(uint32_t instr);
        void ersqrt(uint32_t instr);
        void fcand(uint32_t value);
        void fcget(uint8_t dest);
        void fcor(uint32_t value);
        void fcset(uint32_t value);
        void fmeq(uint8_t dest, uint8_t source);
        void fmand(uint8_t dest, uint8_t source);
        void fmor(uint8_t dest, uint8_t source);
        void fsset(uint32_t value);
        void fsand(uint8_t dest, uint32_t value);
        void ftoi0(uint32_t instr);
        void ftoi4(uint32_t instr);
        void ftoi12(uint32_t instr);
        void ftoi15(uint32_t instr);
        void iadd(uint8_t dest, uint8_t reg1, uint8_t reg2);
        void iaddi(uint8_t dest, uint8_t source, int8_t imm);
        void iaddiu(uint8_t dest, uint8_t source, uint16_t imm);
        void iand(uint8_t dest, uint8_t reg1, uint8_t reg2);
        void ilw(uint8_t field, uint8_t dest, uint8_t base, int16_t offset);
        void ilwr(uint8_t field, uint8_t dest, uint8_t base);
        void ior(uint8_t dest, uint8_t reg1, uint8_t reg2);
        void isub(uint8_t dest, uint8_t reg1, uint8_t reg2);
        void isubiu(uint8_t dest, uint8_t source, uint16_t imm);
        void isw(uint8_t field, uint8_t source, uint8_t base, int16_t offset);
        void iswr(uint8_t field, uint8_t source, uint8_t base);
        void itof0(uint32_t instr);
        void itof4(uint32_t instr);
        void itof12(uint32_t instr);
        void itof15(uint32_t instr);
        void lq(uint32_t instr);
        void lqd(uint32_t instr);
        void lqi(uint32_t instr);
        void madd(uint32_t instr);
        void madda(uint32_t instr);
        void maddabc(uint32_t instr);
        void maddai(uint32_t instr);
        void maddaq(uint32_t instr);
        void maddbc(uint32_t instr);
        void maddi(uint32_t instr);
        void maddq(uint32_t instr);
        void max(uint32_t instr);
        void maxbc(uint32_t instr);
        void maxi(uint32_t instr);
        void mfir(uint32_t instr);
        void mfp(uint32_t instr);
        void mini(uint32_t instr);
        void minibc(uint32_t instr);
        void minii(uint32_t instr);
        void move(uint32_t instr);
        void mr32(uint32_t instr);
        void msuba(uint32_t instr);
        void msubabc(uint32_t instr);
        void msubaq(uint32_t instr);
        void msubai(uint32_t instr);
        void msub(uint32_t instr);
        void msubbc(uint32_t instr);
        void msubi(uint32_t instr);
        void msubq(uint32_t instr);
        void mtir(uint32_t instr);
        void mul(uint32_t instr);
        void mula(uint32_t instr);
        void mulabc(uint32_t instr);
        void mulai(uint32_t instr);
        void mulaq(uint32_t instr);
        void mulbc(uint32_t instr);
        void muli(uint32_t instr);
        void mulq(uint32_t instr);
        void opmsub(uint32_t instr);
        void opmula(uint32_t instr);
        void rinit(uint32_t instr);
        void rget(uint32_t instr);
        void rnext(uint32_t instr);
        void rsqrt(uint32_t instr);
        void rxor(uint32_t instr);
        void sq(uint32_t instr);
        void sqd(uint32_t instr);
        void sqi(uint32_t instr);
        void vu_sqrt(uint32_t instr);
        void sub(uint32_t instr);
        void suba(uint32_t instr);
        void subabc(uint32_t instr);
        void subai(uint32_t instr);
        void subbc(uint32_t instr);
        void subi(uint32_t instr);
        void subq(uint32_t instr);
        void waitq();
        void xgkick(uint8_t is);
        void xitop(uint8_t it);
        void xtop(uint8_t it);

        void load_state(std::ifstream& state);
        void save_state(std::ofstream& state);
};

template <typename T>
inline T VectorUnit::read_instr(uint32_t addr)
{
    uint16_t mask = 0x3fff;
    if (get_id() == 0)
        mask = 0xfff;
    return *(T*)&instr_mem[addr & mask];
}

template <typename T>
inline T VectorUnit::read_data(uint32_t addr)
{
    uint16_t mask = 0x3fff;
    if (get_id() == 0)
        mask = 0xfff;
    return *(T*)&data_mem[addr & mask];
}

template <typename T>
inline void VectorUnit::write_instr(uint32_t addr, T data)
{
    uint16_t mask = 0x3fff;
    if (get_id() == 0)
        mask = 0xfff;
     *(T*)&instr_mem[addr & mask] = data;
}

template <typename T>
inline void VectorUnit::write_data(uint32_t addr, T data)
{
    uint16_t mask = 0x3fff;
    if (get_id() == 0)
        mask = 0xfff;
    *(T*)&data_mem[addr & mask] = data;
}

inline bool VectorUnit::is_running()
{
    return running;
}

inline int VectorUnit::get_id()
{
    return id;
}

inline uint16_t VectorUnit::get_PC()
{
    return PC;
}

inline uint32_t VectorUnit::get_gpr_u(int index, int field)
{
    return gpr[index].u[field];
}

inline uint16_t VectorUnit::get_int(int index)
{
    return int_gpr[index].u;
}

inline void VectorUnit::set_gpr_f(int index, int field, float value)
{
    if (index)
        gpr[index].f[field] = value;
}

inline void VectorUnit::set_gpr_u(int index, int field, uint32_t value)
{
    if (index)
        gpr[index].u[field] = value;
}

inline void VectorUnit::set_int(int index, uint16_t value)
{
    if (index)
        int_gpr[index].u = value;
}

inline void VectorUnit::set_status(uint32_t value)
{
    status = value;
}

inline void VectorUnit::set_R(uint32_t value)
{
    R.u = value;
}

inline void VectorUnit::set_I(uint32_t value)
{
    I.u = value;
}

inline void VectorUnit::set_Q(uint32_t value)
{
    new_Q_instance.u = value;
}

#endif // VU_HPP
