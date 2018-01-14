#ifndef BIOS_HLE_HPP
#define BIOS_HLE_HPP
#include <vector>
#include "emotion.hpp"

enum CALL_PARAMS
{
    RETURN = 2,
    PARAM0 = 4,
    PARAM1 = 5,
    PARAM2 = 6,
    PARAM3 = 7,
    PARAM4 = 8,
    PARAM5 = 9
};

struct thread_hle
{
    uint32_t stack_base;
    uint32_t heap_base;
    uint8_t priority;
};

class BIOS_HLE
{
    private:
        Emulator* e;
        std::vector<thread_hle> threads;
    public:
        BIOS_HLE(Emulator* e);

        void hle_syscall(EmotionEngine& cpu, int op);

        void set_GS_CRT(EmotionEngine& cpu);
        void init_main_thread(EmotionEngine& cpu);
        void init_heap(EmotionEngine& cpu);
        void set_GS_IMR(EmotionEngine& cpu);
};

#endif // BIOS_HLE_HPP
