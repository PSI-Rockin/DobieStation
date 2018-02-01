#include <cstdio>
#include "bios_hle.hpp"
#include "gs.hpp"

BIOS_HLE::BIOS_HLE(Emulator* e, GraphicsSynthesizer* gs) : e(e), gs(gs)
{

}

void BIOS_HLE::hle_syscall(EmotionEngine& cpu, int op)
{
    switch (op)
    {
        case 0x02:
            set_GS_CRT(cpu);
            break;
        case 0x3C:
            init_main_thread(cpu);
            break;
        case 0x3D:
            init_heap(cpu);
            break;
        case 0x71:
            set_GS_IMR(cpu);
            break;
        default:
            printf("\nUnrecognized HLE syscall $%02X", op);
    }
}

void BIOS_HLE::set_GS_CRT(EmotionEngine &cpu)
{
    printf("\nSYSCALL: set_GS_CRT");
    bool interlaced = cpu.get_gpr<uint64_t>(PARAM0);
    int mode = cpu.get_gpr<uint64_t>(PARAM1);
    bool frame_mode = cpu.get_gpr<uint64_t>(PARAM2);
    gs->set_CRT(interlaced, mode, frame_mode);
}

void BIOS_HLE::init_main_thread(EmotionEngine &cpu)
{
    printf("\nSYSCALL: init_main_thread");
    uint32_t stack_base = cpu.get_gpr<uint32_t>(PARAM1);
    uint32_t stack_size = cpu.get_gpr<uint32_t>(PARAM2);
    uint32_t stack_addr;
    if (stack_base == 0xFFFFFFFF)
        stack_addr = 0x02000000;
    else
        stack_addr = stack_base + stack_size;

    stack_addr -= 0x2A0;

    thread_hle main_thread;
    main_thread.priority = 0;
    main_thread.stack_base = stack_addr;
    threads.push_back(main_thread);
    cpu.set_gpr<uint64_t>(RETURN, stack_addr);
}

void BIOS_HLE::init_heap(EmotionEngine &cpu)
{
    printf("\nSYSCALL: init_heap");
    thread_hle* thread = &threads[0];
    uint32_t heap_base = cpu.get_gpr<uint32_t>(PARAM0);
    uint32_t heap_size = cpu.get_gpr<uint32_t>(PARAM1);
    if (heap_size == 0xFFFFFFFF)
        thread->heap_base = thread->stack_base;
    else
        thread->heap_base = heap_base + heap_size;

    cpu.set_gpr<uint64_t>(RETURN, thread->heap_base);
}

void BIOS_HLE::set_GS_IMR(EmotionEngine &cpu)
{
    uint32_t imr = cpu.get_gpr<uint32_t>(PARAM0);
    printf("\nSYSCALL: set_GS_IMR $%08X", imr);
}
