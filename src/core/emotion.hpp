#ifndef EMOTION_HPP
#define EMOTION_HPP
#include <cstdint>
#include "cop0.hpp"
#include "cop1.hpp"

class Emulator;
class BIOS_HLE;

class EmotionEngine
{
    private:
        BIOS_HLE* bios;
        Emulator* e;

        Cop0 cp0;
        Cop1 fpu;

        //Each register is 128-bit
        uint8_t gpr[32 * sizeof(uint64_t) * 2];
        //uint64_t gpr_lo[32];
        //uint64_t gpr_hi[32];
        uint32_t PC, new_PC;
        uint64_t LO, LO1, HI, HI1;

        bool branch_on;
        int delay_slot;

        uint8_t scratchpad[1024 * 16];

        uint32_t get_paddr(uint32_t vaddr);
    public:
        EmotionEngine(BIOS_HLE* b, Emulator* e);
        static const char* REG(int id);
        void reset();
        void run();
        void print_state();
        template <typename T> T get_gpr(int id, int offset = 0);
        template <typename T> void set_gpr(int id, T value, int offset = 0);
        uint32_t get_PC();
        uint8_t read8(uint32_t address);
        uint16_t read16(uint32_t address);
        uint32_t read32(uint32_t address);
        uint64_t read64(uint32_t address);
        //void set_gpr_lo(int index, uint64_t value);

        void set_PC(uint32_t addr);
        void write8(uint32_t address, uint8_t value);
        void write16(uint32_t address, uint16_t value);
        void write32(uint32_t address, uint32_t value);
        void write64(uint32_t address, uint64_t value);

        void jp(uint32_t new_addr);
        void branch(bool condition, int offset);
        void branch_likely(bool condition, int offset);
        void mfc(int cop_id, int reg, int cop_reg);
        void mtc(int cop_id, int reg, int cop_reg);
        void lwc1(uint32_t addr, int index);
        void swc1(uint32_t addr, int index);

        void mfhi(int index);
        void mflo(int index);
        void mflo1(int index);
        void set_LO_HI(uint64_t a, uint64_t b, bool hi = false);

        void hle_syscall();
        void syscall_exception();

        void fpu_mov_s(int dest, int source);
        void fpu_cvt_s_w(int dest, int source);
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

#endif // EMOTION_HPP
