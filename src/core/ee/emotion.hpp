#ifndef EMOTION_HPP
#define EMOTION_HPP
#include <cstdint>
#include <fstream>
#include <functional>
#include "cop0.hpp"
#include "cop1.hpp"

#include "../int128.hpp"

class Emulator;
class VectorUnit;
class EE_JIT64;
class EmotionEngine;

//Handler used for Deci2Call (syscall 0x7C)
struct Deci2Handler
{
    bool active;
    uint32_t device;
    uint32_t addr;
};

struct EE_ICacheLine
{
    bool lfu[2];
    uint32_t tag[2];
};

//Taken from PS2SDK
struct EE_OsdConfigParam
{
    /** 0=enabled, 1=disabled */
    /*00*/uint32_t spdifMode:1;
        /** 0=4:3, 1=fullscreen, 2=16:9 */
    /*01*/uint32_t screenType:2;
        /** 0=rgb(scart), 1=component */
    /*03*/uint32_t videoOutput:1;
        /** 0=japanese, 1=english(non-japanese) */
    /*04*/uint32_t japLanguage:1;
        /** Playstation driver settings. */
    /*05*/uint32_t ps1drvConfig:8;
        /** 0 = early Japanese OSD, 1 = OSD2, 2 = OSD2 with extended languages.
         * Early kernels cannot retain the value set in this field (Hence always 0). */
    /*13*/uint32_t version:3;
        /** LANGUAGE_??? value */
    /*16*/uint32_t language:5;
        /** timezone minutes offset from gmt */
    /*21*/uint32_t timezoneOffset:11;
};

extern "C" uint8_t* exec_block_ee(EE_JIT64& jit, EmotionEngine& ee);

class EmotionEngine
{
    private:
        Emulator* e;

        uint64_t cycle_count;
        uint64_t cop2_last_cycle;
        int cycles_to_run;
        uint64_t run_event;

        Cop0* cp0;
        Cop1* fpu;
        VectorUnit* vu0;
        VectorUnit* vu1;

        uint8_t** tlb_map;

        EE_OsdConfigParam osd_config_param;

        //Each register is 128-bit
        alignas(16) uint8_t gpr[32 * sizeof(uint64_t) * 2];
        alignas(16) uint128_t LO, HI;
        uint32_t PC, new_PC;
        uint64_t SA;

        EE_ICacheLine icache[128];

        bool wait_for_IRQ, wait_for_VU0;
        bool branch_on;
        bool can_disassemble;
        int delay_slot;

        Deci2Handler deci2handlers[128];
        int deci2size;

        bool flush_jit_cache;

        std::function<void(EmotionEngine&)> run_func;

        uint32_t get_paddr(uint32_t vaddr);
        void handle_exception(uint32_t new_addr, uint8_t code);
        void deci2call(uint32_t func, uint32_t param);
    public:
        EmotionEngine(Cop0* cp0, Cop1* fpu, Emulator* e, VectorUnit* vu0, VectorUnit* vu1);
        static const char* REG(int id);
        static const char* SYSCALL(int id);
        void reset();
        void init_tlb();
        void run(int cycles);
        void run_interpreter();
        void run_jit();
        uint64_t get_cycle_count();
        uint64_t get_cop2_last_cycle();
        void set_cop2_last_cycle(uint64_t value);
        void halt();
        void unhalt();
        void print_state();
        void set_disassembly(bool dis);
        void set_run_func(std::function<void(EmotionEngine&)> func);

        template <typename T> T get_gpr(int id, int offset = 0);
        template <typename T> T get_LO(int offset = 0);
        template <typename T> void set_gpr(int id, T value, int offset = 0);
        template <typename T> void set_LO(int id, T value, int offset = 0);
        uint32_t get_PC();
        uint64_t get_LO();
        uint64_t get_LO1();
        uint64_t get_HI();
        uint64_t get_HI1();
        uint64_t get_SA();
        bool check_interlock();
        void clear_interlock();
        bool vu0_wait();

        uint32_t read_instr(uint32_t address);

        uint8_t read8(uint32_t address);
        uint16_t read16(uint32_t address);
        uint32_t read32(uint32_t address);
        uint64_t read64(uint32_t address);
        uint128_t read128(uint32_t address);

        void set_PC(uint32_t addr);
        void write8(uint32_t address, uint8_t value);
        void write16(uint32_t address, uint16_t value);
        void write32(uint32_t address, uint32_t value);
        void write64(uint32_t address, uint64_t value);
        void write128(uint32_t address, uint128_t value);

        void jp(uint32_t new_addr);
        void branch(bool condition, int offset);
        void branch_likely(bool condition, int offset);
        void cfc(int cop_id, int reg, int cop_reg, uint32_t instruction);
        void ctc(int cop_id, int reg, int cop_reg, uint32_t instruction);
        void mfc(int cop_id, int reg, int cop_reg);
        void mtc(int cop_id, int reg, int cop_reg);
        void lwc1(uint32_t addr, int index);
        void lqc2(uint32_t addr, int index);
        void swc1(uint32_t addr, int index);
        void sqc2(uint32_t addr, int index);

        void invalidate_icache_indexed(uint32_t addr);

        void mfhi(int index);
        void mthi(int index);
        void mflo(int index);
        void mtlo(int index);
        void mfhi1(int index);
        void mthi1(int index);
        void mflo1(int index);
        void mtlo1(int index);
        void mfsa(int index);
        void mtsa(int index);
        void pmfhi(int index);
        void pmflo(int index);
        void pmthi(int index);
        void pmtlo(int index);
        void set_SA(uint64_t value);
        void set_LO_HI(uint64_t a, uint64_t b, bool hi = false);

        void hle_syscall();
        void syscall_exception();
        void break_exception();
        void trap_exception();
        void int0();
        void int1();
        void set_int0_signal(bool value);
        void set_int1_signal(bool value);

        void tlbwi();
        void tlbp();
        void eret();
        void ei();
        void di();
        void cp0_bc0(int32_t offset, bool test_true, bool likely);
        void mtps(int reg);
        void mtpc(int pc_reg, int reg);
        void mfps(int reg);
        void mfpc(int pc_reg, int reg);

        void fpu_cop_s(uint32_t instruction);
        void fpu_bc1(int32_t offset, bool test_true, bool likely);
        void cop2_bc2(int32_t offset, bool test_true, bool likely);
        void fpu_cvt_s_w(int dest, int source);

        void qmfc2(int dest, int cop_reg);
        void qmtc2(int source, int cop_reg);
        void cop2_special(EmotionEngine &cpu, uint32_t instruction);
        void cop2_updatevu0();

        void load_state(std::ifstream& state);
        void save_state(std::ofstream& state);

        //Friends needed for JIT convenience
        friend class EE_JIT64;
        friend class EE_JitTranslator;

        friend void emit_dispatcher();
        friend uint8_t* exec_block_ee(EE_JIT64& jit, EmotionEngine& ee);
};

template <typename T>
inline T EmotionEngine::get_gpr(int id, int offset)
{
    return *(T*)&gpr[(id * sizeof(uint64_t) * 2) + (offset * sizeof(T))];
}

template <typename T>
inline void EmotionEngine::set_gpr(int id, T value, int offset)
{
    if (id)
        *(T*)&gpr[(id * sizeof(uint64_t) * 2) + (offset * sizeof(T))] = value;
}

inline uint64_t EmotionEngine::get_cycle_count()
{
    return cycle_count - cycles_to_run;
}

inline uint64_t EmotionEngine::get_cop2_last_cycle()
{
    return cop2_last_cycle;
}

inline void EmotionEngine::set_cop2_last_cycle(uint64_t value)
{
    cop2_last_cycle = value;
}

inline void EmotionEngine::halt()
{
    wait_for_IRQ = true;
    cycles_to_run = 0;
}

inline void EmotionEngine::unhalt()
{
    wait_for_IRQ = false;
    if (cycles_to_run < 0)
        cycles_to_run = 0;
}

#endif // EMOTION_HPP
