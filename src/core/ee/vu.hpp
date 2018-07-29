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

class GraphicsInterface;

class VectorUnit
{
    private:
        GraphicsInterface* gif;
        int id;

        uint16_t *VIF_TOP, *VIF_ITOP;

        uint8_t instr_mem[1024 * 16];
        uint8_t data_mem[1024 * 16];

        bool running;
        uint16_t PC, new_PC;
        bool branch_on;
        bool finish_on;
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
        VU_GPR ACC;
        uint32_t status;
        uint32_t clip_flags;
        VU_R R;
        VU_R I;
        VU_R Q;
        VU_R P;

        uint16_t MAC_pipeline[4];       
        uint16_t* MAC_flags; //pointer to last element in the pipeline; the register accessible to programs
        uint16_t new_MAC_flags; //to be placed in the pipeline

        float Q_Pipeline[6];
        VU_R new_Q_instance;

        float update_mac_flags(float value, int index);
        void clear_mac_flags(int index);
        void update_mac_pipeline();
        void update_div_pipeline();
        void advance_r();
        void print_vectors(uint8_t a, uint8_t b);
        float convert();
    public:
        VectorUnit(int id);

        void set_TOP_regs(uint16_t* TOP, uint16_t* ITOP);
        void set_GIF(GraphicsInterface* gif);

        void flush_pipes();

        void run(int cycles);
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

        void branch(bool condition, int32_t imm);
        void jp(uint16_t addr);

        void abs(uint8_t field, uint8_t dest, uint8_t source);
        void add(uint8_t field, uint8_t dest, uint8_t reg1, uint8_t reg2);
        void adda(uint8_t field, uint8_t reg1, uint8_t reg2);
        void addabc(uint8_t bc, uint8_t field, uint8_t source, uint8_t bc_reg);
        void addbc(uint8_t bc, uint8_t field, uint8_t dest, uint8_t source, uint8_t bc_reg);
        void addi(uint8_t field, uint8_t dest, uint8_t source);
        void addq(uint8_t field, uint8_t dest, uint8_t source);
        void clip(uint8_t reg1, uint8_t reg2);
        void div(uint8_t ftf, uint8_t fsf, uint8_t reg1, uint8_t reg2);
        void eleng(uint8_t source);
        void esqrt(uint8_t fsf, uint8_t source);
        void erleng(uint8_t source);
        void fcand(uint32_t value);
        void fcget(uint8_t dest);
        void fcor(uint32_t value);
        void fcset(uint32_t value);
        void fmand(uint8_t dest, uint8_t source);
        void ftoi0(uint8_t field, uint8_t dest, uint8_t source);
        void ftoi4(uint8_t field, uint8_t dest, uint8_t source);
        void ftoi12(uint8_t field, uint8_t dest, uint8_t source);
        void ftoi15(uint8_t field, uint8_t dest, uint8_t source);
        void iadd(uint8_t dest, uint8_t reg1, uint8_t reg2);
        void iaddi(uint8_t dest, uint8_t source, int8_t imm);
        void iaddiu(uint8_t dest, uint8_t source, uint16_t imm);
        void iand(uint8_t dest, uint8_t reg1, uint8_t reg2);
        void ilw(uint8_t field, uint8_t dest, uint8_t base, int32_t offset);
        void ilwr(uint8_t field, uint8_t dest, uint8_t base);
        void ior(uint8_t dest, uint8_t reg1, uint8_t reg2);
        void isub(uint8_t dest, uint8_t reg1, uint8_t reg2);
        void isubiu(uint8_t dest, uint8_t source, uint16_t imm);
        void isw(uint8_t field, uint8_t source, uint8_t base, int32_t offset);
        void iswr(uint8_t field, uint8_t source, uint8_t base);
        void itof0(uint8_t field, uint8_t dest, uint8_t source);
        void itof4(uint8_t field, uint8_t dest, uint8_t source);
        void itof12(uint8_t field, uint8_t dest, uint8_t source);
        void lq(uint8_t field, uint8_t dest, uint8_t base, int32_t offset);
        void lqd(uint8_t field, uint8_t dest, uint8_t base);
        void lqi(uint8_t field, uint8_t dest, uint8_t base);
        void madd(uint8_t field, uint8_t dest, uint8_t reg1, uint8_t reg2);
        void madda(uint8_t field, uint8_t reg1, uint8_t reg2);
        void addai(uint8_t field, uint8_t source);
        void maddai(uint8_t field, uint8_t source);
        void maddabc(uint8_t bc, uint8_t field, uint8_t source, uint8_t bc_reg);
        void maddbc(uint8_t bc, uint8_t field, uint8_t dest, uint8_t source, uint8_t bc_reg);
        void maddq(uint8_t field, uint8_t dest, uint8_t source);
        void maddi(uint8_t field, uint8_t dest, uint8_t source);
        void max(uint8_t field, uint8_t dest, uint8_t reg1, uint8_t reg2);
        void maxbc(uint8_t bc, uint8_t field, uint8_t dest, uint8_t source, uint8_t bc_reg);
        void mfir(uint8_t field, uint8_t dest, uint8_t source);
        void mfp(uint8_t field, uint8_t dest);
        void mini(uint8_t field, uint8_t dest, uint8_t reg1, uint8_t reg2);
        void minibc(uint8_t bc, uint8_t field, uint8_t dest, uint8_t source, uint8_t bc_reg);
        void minii(uint8_t field, uint8_t dest, uint8_t source);
        void move(uint8_t field, uint8_t dest, uint8_t source);
        void mr32(uint8_t field, uint8_t dest, uint8_t source);
        void msubabc(uint8_t bc, uint8_t field, uint8_t source, uint8_t bc_reg);
        void msubai(uint8_t field, uint8_t source);
        void msub(uint8_t field, uint8_t dest, uint8_t reg1, uint8_t reg2);
        void msubbc(uint8_t bc, uint8_t field, uint8_t dest, uint8_t source, uint8_t bc_reg);
        void msubi(uint8_t field, uint8_t dest, uint8_t source);
        void mtir(uint8_t fsf, uint8_t dest, uint8_t source);
        void mul(uint8_t field, uint8_t dest, uint8_t reg1, uint8_t reg2);
        void mula(uint8_t field, uint8_t reg1, uint8_t reg2);
        void mulabc(uint8_t bc, uint8_t field, uint8_t source, uint8_t bc_reg);
        void mulai(uint8_t field, uint8_t source);
        void mulbc(uint8_t bc, uint8_t field, uint8_t dest, uint8_t source, uint8_t bc_reg);
        void muli(uint8_t field, uint8_t dest, uint8_t source);
        void mulq(uint8_t field, uint8_t dest, uint8_t source);
        void opmsub(uint8_t dest, uint8_t reg1, uint8_t reg2);
        void opmula(uint8_t reg1, uint8_t reg2);
        void rinit(uint8_t fsf, uint8_t source);
        void rget(uint8_t field, uint8_t dest);
        void rnext(uint8_t field, uint8_t dest);
        void rsqrt(uint8_t ftf, uint8_t fsf, uint8_t reg1, uint8_t reg2);
        void rxor(uint8_t fsf, uint8_t source);
        void sq(uint8_t field, uint8_t source, uint8_t base, int32_t offset);
        void sqd(uint8_t field, uint8_t source, uint8_t base);
        void sqi(uint8_t field, uint8_t source, uint8_t base);
        void vu_sqrt(uint8_t ftf, uint8_t source);
        void sub(uint8_t field, uint8_t dest, uint8_t reg1, uint8_t reg2);
        void subai(uint8_t field, uint8_t source);
        void subbc(uint8_t bc, uint8_t field, uint8_t dest, uint8_t source, uint8_t bc_reg);
        void subi(uint8_t field, uint8_t dest, uint8_t source);
        void subq(uint8_t field, uint8_t dest, uint8_t source);
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
    return *(T*)&instr_mem[addr & 0x3FFF];
}

template <typename T>
inline T VectorUnit::read_data(uint32_t addr)
{
    return *(T*)&data_mem[addr & 0x3FFF];
}

template <typename T>
inline void VectorUnit::write_instr(uint32_t addr, T data)
{
    *(T*)&instr_mem[addr & 0x3FFF] = data;
}

template <typename T>
inline void VectorUnit::write_data(uint32_t addr, T data)
{
    *(T*)&data_mem[addr & 0x3FFF] = data;
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
