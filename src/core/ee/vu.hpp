#ifndef VU_HPP
#define VU_HPP
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <unordered_set>
#include "emotion.hpp"
#include "../int128.hpp"

union alignas(16) VU_R
{
    float f;
    uint32_t u;
    char padding[16];
};

//The GPRs need to be aligned on a 16-byte boundary so that SSE instructions can work on them.
union alignas(16) VU_GPR
{
    float f[4];
    uint32_t u[4];
    int32_t s[4];
};

union alignas(8) VU_I
{
    uint16_t u;
    int16_t s;
};

struct alignas(16) VU_Mem
{
    uint8_t m[1024 * 16];
};

struct VPU_STAT
{
    bool vu1_running;
};

struct DecodedRegs
{
    //0 = upper, 1 = lower
    uint8_t vf_write[2];
    uint8_t vf_write_field[2];

    uint8_t vf_read0[2], vf_read1[2];
    uint8_t vf_read0_field[2], vf_read1_field[2];

    uint8_t vi_read0, vi_read1, vi_write;
    uint8_t vi_write_from_load; // register which written from a load-from-memory instruction

    void reset();
};

struct VuIntBranchPipelineEntry
{
    uint8_t write_reg;   // reg that was overwritten
    VU_I    old_value;   // value that was overwritten
    bool    read_and_write;

    void clear();
};

struct VuIntBranchPipeline
{
    static constexpr int length = 5;           // how far back to go behind the branch
    VuIntBranchPipelineEntry pipeline[length]; // the previous integer operations (0 is most recent)
    VuIntBranchPipelineEntry next;             // the currently executing integer op

    void reset();
    void write_reg(uint8_t reg, VU_I old_value, bool also_read);
    void update();
    void flush();
    VU_I get_branch_condition_reg(uint8_t reg, VU_I current_value, uint8_t vu_id, uint16_t PC);
};

class GraphicsInterface;
class Emulator;
class VU_JIT64;
class VectorUnit;
class INTC;
class EmotionEngine;

extern "C" uint8_t* exec_block_vu(VU_JIT64& jit, VectorUnit& vu);
extern "C" uint8_t* exec_block_ee(EE_JIT64& jit, EmotionEngine& ee);

class VectorUnit
{
    private:
        GraphicsInterface* gif;
        int id;
        uint16_t mem_mask; //0xFFF for VU0, 0x3FFF for VU1
        Emulator* e;
        INTC* intc;
        EmotionEngine* eecpu;
        VectorUnit* other_vu; //Pointer to VU1 for VU0, vice versa for VU1

        uint64_t cycle_count; //Increments when "running" is true
        uint64_t run_event; //If less than cycle_count, the VU is allowed to run

        uint16_t *VIF_TOP, *VIF_ITOP;

        VU_Mem instr_mem, data_mem;

        std::unordered_set<uint32_t> seen_microprogram_crcs;

        bool running;
        bool tbit_stop;
        bool vumem_is_dirty;
        uint16_t PC, new_PC, secondbranch_PC;
        bool branch_on, branch_on_delay;
        bool finish_on;
        bool second_branch_pending;
        int branch_delay_slot, ebit_delay_slot;

        uint16_t GIF_addr;
        bool transferring_GIF;
        bool XGKICK_stall;
        uint16_t stalled_GIF_addr;

        //GPR
        VU_GPR backup_newgpr;
        VU_GPR backup_oldgpr;
        VU_GPR gpr[32];
        VU_I int_gpr[16];

        //Control registers
        static uint32_t FBRST;
        uint32_t CMSAR0;
        VU_GPR ACC;
        uint32_t status;
        int status_pipe;
        uint32_t status_value;
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
        uint64_t pipeline_state[2];
        VuIntBranchPipeline int_branch_pipeline;
        uint16_t ILW_pipeline[4]; // for integer load stalls

        int int_branch_delay;
        uint16_t int_backup_reg;
        uint8_t int_backup_id;
        uint8_t int_backup_id_rec;

        VU_R new_Q_instance;
        VU_R new_P_instance;
        uint64_t finish_DIV_event;
        bool DIV_event_started;
        uint64_t finish_EFU_event;
        bool EFU_event_started;
        int mbit_wait;

        float update_mac_flags(float value, int index);
        void clear_mac_flags(int index);

        //Updates new_Q_instance
        void start_DIV_unit(int latency);
        //Updates new_P_Instance
        void start_EFU_unit(int latency);
        void write_int(uint8_t reg, uint8_t read0 = 0, uint8_t readq1 = 0);
        VU_I read_int_for_branch_condition(uint8_t reg);
        void disasm_micromem();
        uint32_t crc_microprogram();
        
        void update_status();
        void advance_r();
        void print_vectors(uint8_t a, uint8_t b);
    public:
        VectorUnit(int id, Emulator* e, INTC* intc, EmotionEngine* eecpu, VectorUnit* other_vu);

        DecodedRegs decoder;

        void clear_interlock();
        bool check_interlock();
        bool is_interlocked();

        void set_TOP_regs(uint16_t* TOP, uint16_t* ITOP);
        void set_GIF(GraphicsInterface* gif);

        void update_mac_pipeline();
        void update_DIV_EFU_pipes();
        void check_for_FMAC_stall();
        void check_for_COP2_FMAC_stall();
        void flush_pipes();
        void cop2_updatepipes(int cycles);
        void handle_cop2_stalls();

        void run(int cycles);
        void correct_jit_pipeline(int cycles);
        void run_jit(int cycles);
        void handle_XGKICK();
        void start_program(uint32_t addr);
        void end_execution();
        void stop();
        void stop_by_tbit();
        void reset();

        void backup_vf(bool newvf, int index);
        void restore_vf(bool newvf, int index);
        uint32_t read_fbrst();
        uint32_t read_CMSAR0();

        static float convert(uint32_t value);

        uint32_t read_reg(uint32_t addr);
        template <typename T> T read_instr(uint32_t addr);
        template <typename T> T read_data(uint32_t addr);
        template <typename T> T read_mem(uint32_t addr);

        void write_reg(uint32_t addr, uint32_t data);
        template <typename T> void write_instr(uint32_t addr, T data);
        template <typename T> void write_data(uint32_t addr, T data);
        template <typename T> void write_mem(uint32_t addr, T data);

        bool is_running();
        bool stopped_by_tbit();
        bool is_dirty();
        void clear_dirty();
        uint16_t get_PC();
        void set_PC(uint32_t newPC);
        uint32_t get_gpr_u(int index, int field);
        uint16_t get_int(int index);
        int get_id();
        uint64_t get_cycle_count();
        void set_gpr_f(int index, int field, float value);
        void set_gpr_u(int index, int field, uint32_t value);
        void set_gpr_s(int index, int field, int32_t value);
        void set_int(int index, uint16_t value);
        void set_status(uint32_t value);
        void set_R(uint32_t value);
        void set_I(uint32_t value);
        void set_Q(uint32_t value);

        uint8_t* get_instr_mem();

        uint32_t cfc(int index);
        void ctc(int index, uint32_t value);

        void branch(bool condition, int16_t imm, bool link, uint8_t linkreg = 0);
        void jp(uint16_t addr, bool link, uint8_t linkreg = 0);

        void abs(uint32_t instr);
        void add(uint32_t instr);
        void adda(uint32_t instr);
        void addabc(uint32_t instr);
        void addai(uint32_t instr);
        void addaq(uint32_t instr);
        void addbc(uint32_t instr);
        void addi(uint32_t instr);
        void addq(uint32_t instr);
        void b(uint32_t instr);
        void bal(uint32_t instr);
        void clip(uint32_t instr);
        void div(uint32_t instr);
        void eexp(uint32_t instr);
        void esin(uint32_t instr);
        void ercpr(uint32_t instr);
        void eleng(uint32_t instr);
        void esqrt(uint32_t instr);
        void esum(uint32_t instr);
        void erleng(uint32_t instr);
        void ersadd(uint32_t instr);
        void ersqrt(uint32_t instr);
        void esadd(uint32_t instr);
        void fcand(uint32_t instr);
        void fceq(uint32_t instr);
        void fcget(uint32_t instr);
        void fcor(uint32_t instr);
        void fcset(uint32_t instr);
        void fmeq(uint32_t instr);
        void fmand(uint32_t instr);
        void fmor(uint32_t instr);
        void fsset(uint32_t instr);
        void fsand(uint32_t instr);
        void ftoi0(uint32_t instr);
        void ftoi4(uint32_t instr);
        void ftoi12(uint32_t instr);
        void ftoi15(uint32_t instr);
        void iadd(uint32_t instr);
        void iaddi(uint32_t instr);
        void iaddiu(uint32_t instr);
        void iand(uint32_t instr);
        void ibeq(uint32_t instr);
        void ibgez(uint32_t instr);
        void ibgtz(uint32_t instr);
        void iblez(uint32_t instr);
        void ibltz(uint32_t instr);
        void ibne(uint32_t instr);
        void ilw(uint32_t instr);
        void ilwr(uint32_t instr);
        void ior(uint32_t instr);
        void isub(uint32_t instr);
        void isubiu(uint32_t instr);
        void isw(uint32_t instr);
        void iswr(uint32_t instr);
        void itof0(uint32_t instr);
        void itof4(uint32_t instr);
        void itof12(uint32_t instr);
        void itof15(uint32_t instr);
        void jr(uint32_t instr);
        void jalr(uint32_t instr);
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
        void nop(uint32_t instr);
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
        void waitp(uint32_t instr);
        void waitq(uint32_t instr);
        void xgkick(uint32_t instr);
        void xitop(uint32_t instr);
        void xtop(uint32_t instr);

        void load_state(std::ifstream& state);
        void save_state(std::ofstream& state);

        //Friends needed for JIT convenience
        friend class VU_JIT64;
        friend class VU_JitTranslator;
        friend class EE_JIT64;
        friend class EE_JitTranslator;

        friend void vu_update_xgkick(VectorUnit& vu, int cycles);
        friend void vu_update_pipelines(VectorUnit& vu, int cycles);
        friend uint8_t* exec_block_vu(VU_JIT64& jit, VectorUnit& vu);
        friend uint8_t* exec_block_ee(EE_JIT64& jit, EmotionEngine& ee);
};

template <typename T>
inline T VectorUnit::read_instr(uint32_t addr)
{
    return *(T*)&instr_mem.m[addr & mem_mask];
}

template <typename T>
inline T VectorUnit::read_data(uint32_t addr)
{
    if (id == 1)
        return *(T*)&data_mem.m[addr & 0x3FFF];
    if (addr & 0x4000)
        return other_vu->read_reg(addr);
    return *(T*)&data_mem.m[addr & 0xFFF];
}

template <typename T>
inline T VectorUnit::read_mem(uint32_t addr)
{
    return *(T*)&data_mem.m[addr & mem_mask];
}

template <typename T>
inline void VectorUnit::write_instr(uint32_t addr, T data)
{
    *(T*)&instr_mem.m[addr & mem_mask] = data;
    vumem_is_dirty = true;
}

template <typename T>
inline void VectorUnit::write_data(uint32_t addr, T data)
{
    if (id == 1)
        *(T*)&data_mem.m[addr & 0x3FFF] = data;
    else if (addr & 0x4000)
        other_vu->write_reg(addr, data);
    else
        *(T*)&data_mem.m[addr & 0xFFF] = data;
}

template <typename T>
inline void VectorUnit::write_mem(uint32_t addr, T data)
{
    *(T*)&data_mem.m[addr & mem_mask] = data;
}

inline bool VectorUnit::is_running()
{
    return running;
}

inline bool VectorUnit::stopped_by_tbit()
{
    return tbit_stop;
}

inline bool VectorUnit::is_dirty()
{
    return vumem_is_dirty;
}
 
inline void VectorUnit::clear_dirty()
{
    vumem_is_dirty = false;
}

inline int VectorUnit::get_id()
{
    return id;
}

inline uint16_t VectorUnit::get_PC()
{
    return PC;
}

inline void VectorUnit::set_PC(uint32_t newPC)
{
    PC = newPC;
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

inline void VectorUnit::set_gpr_s(int index, int field, int32_t value)
{
    if (index)
        gpr[index].s[field] = value;
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
    Q.u = new_Q_instance.u;
}

inline uint8_t* VectorUnit::get_instr_mem()
{
    return (uint8_t*)instr_mem.m;
}

inline uint64_t VectorUnit::get_cycle_count()
{
    return cycle_count;
}

#endif // VU_HPP
