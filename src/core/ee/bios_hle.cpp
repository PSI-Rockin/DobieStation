#include <cstdio>
#include "bios_hle.hpp"
#include "emotionasm.hpp"

#include "../emulator.hpp"
#include "../gs.hpp"
#include "../logger.hpp"

BIOS_HLE::BIOS_HLE(Emulator* e, GraphicsSynthesizer* gs) : e(e), gs(gs)
{

}

void BIOS_HLE::reset()
{
    assemble_interrupt_handler();
}

//Store a word to memory and increment the address
void BIOS_HLE::sw(uint32_t& address, uint32_t value)
{
    e->write32(address, value);
    address += 4;
}

void BIOS_HLE::assemble_interrupt_handler()
{
    //Start of interrupt handler
    uint32_t addr = 0x00000200;

    //Set up stack pointer through a kernel register
    sw(addr, EmotionAssembler::lui(MIPS_REG::at, 0x00030000));
    sw(addr, EmotionAssembler::ori(MIPS_REG::k0, MIPS_REG::at, 0));
    sw(addr, EmotionAssembler::addiu(MIPS_REG::k0, MIPS_REG::k0, -0x230));

    //Save every register
    for (int i = 0; i < 32; i++)
        sw(addr, EmotionAssembler::sq(i, MIPS_REG::k0, i * 16));

    //Store kernel stack pointer in SP
    sw(addr, EmotionAssembler::add(MIPS_REG::sp, MIPS_REG::k0, MIPS_REG::zero));

    //Load INTC_MASK and INTC_STAT and AND the result
    sw(addr, EmotionAssembler::lui(MIPS_REG::at, 0x10000000));
    sw(addr, EmotionAssembler::ori(MIPS_REG::t0, MIPS_REG::at, 0xF000));
    sw(addr, EmotionAssembler::lw(MIPS_REG::t1, MIPS_REG::t0, 0));
    sw(addr, EmotionAssembler::lw(MIPS_REG::t2, MIPS_REG::t0, 0x10));
    sw(addr, EmotionAssembler::and_ee(MIPS_REG::t2, MIPS_REG::t2, MIPS_REG::t1));

    //Clear INTC_STAT
    sw(addr, EmotionAssembler::sw(MIPS_REG::t2, MIPS_REG::t0, 0));

    //Load address from INTC handler and jump
    //TODO: loop through all handlers and check for cause
    sw(addr, EmotionAssembler::lui(MIPS_REG::at, 0x80000000));
    sw(addr, EmotionAssembler::ori(MIPS_REG::t0, MIPS_REG::at, 0xA000));
    sw(addr, EmotionAssembler::lw(MIPS_REG::t1, MIPS_REG::t0, 0));
    sw(addr, EmotionAssembler::jalr(MIPS_REG::ra, MIPS_REG::t1));
    sw(addr, 0);

    //Restore location of saved registers
    sw(addr, EmotionAssembler::add(MIPS_REG::k0, MIPS_REG::sp, MIPS_REG::zero));

    //Load registers back
    for (int i = 0; i < 32; i++)
        sw(addr, EmotionAssembler::lq(i, MIPS_REG::k0, i * 16));

    //Return to program
    sw(addr, EmotionAssembler::eret());
}

void BIOS_HLE::store_INTC_handler(INTC_handler &h)
{
    uint32_t addr = 0x0000A000 + (intc_handlers.size() * 16);
    sw(addr, h.addr);
    sw(addr, h.cause);
    sw(addr, h.arg);
    sw(addr, h.next);
}

void BIOS_HLE::hle_syscall(EmotionEngine& cpu, int op)
{
    //TODO: what are "negative" BIOS functions, i.e. those with bit 7 set, for?
    op = (int8_t)op;
    if (op < 0)
        op = -op;
    switch (op)
    {
        case 0x01:
            reset_EE(cpu);
            break;
        case 0x02:
            set_GS_CRT(cpu);
            break;
        case 0x0F:
            set_VBLANK_handler(cpu);
            break;
        case 0x10:
            add_INTC_handler(cpu);
            break;
        case 0x14:
            enable_INTC(cpu);
            break;
        case 0x3C:
            init_main_thread(cpu);
            break;
        case 0x3D:
            init_heap(cpu);
            break;
        case 0x3E:
            get_heap_end(cpu);
            break;
        case 0x64:
            ds_log->main->info("SYSCALL: flush_cache\n");
            break;
        case 0x71:
            set_GS_IMR(cpu);
            break;
        case 0x7F:
            get_memory_size(cpu);
            break;
        default:
            ds_log->main->info("Unrecognized HLE syscall ${:02X}\n", op);
    }
}

void BIOS_HLE::reset_EE(EmotionEngine &cpu)
{
    ds_log->main->info("SYSCALL: reset_EE\n");
}

void BIOS_HLE::set_GS_CRT(EmotionEngine &cpu)
{
    ds_log->main->info("SYSCALL: set_GS_CRT\n");
    bool interlaced = cpu.get_gpr<uint64_t>(PARAM0);
    int mode = cpu.get_gpr<uint64_t>(PARAM1);
    bool frame_mode = cpu.get_gpr<uint64_t>(PARAM2);
    gs->set_CRT(interlaced, mode, frame_mode);
}

void BIOS_HLE::set_VBLANK_handler(EmotionEngine &cpu)
{
    ds_log->main->info("SYSCALL: set_VBLANK_handler\n");
    ds_log->main->info("PARAM0: ${:08X}\n", cpu.get_gpr<uint32_t>(PARAM0));
    ds_log->main->info("PARAM1: ${:08X}\n", cpu.get_gpr<uint32_t>(PARAM1));
    ds_log->main->info("PARAM2: ${:08X}\n", cpu.get_gpr<uint32_t>(PARAM2));
    ds_log->main->info("PARAM3: ${:08X}\n", cpu.get_gpr<uint32_t>(PARAM3));
    ds_log->main->info("PARAM4: ${:08X}\n", cpu.get_gpr<uint32_t>(PARAM4));
    ds_log->main->info("PARAM5: ${:08X}\n", cpu.get_gpr<uint32_t>(PARAM5));
}

void BIOS_HLE::add_INTC_handler(EmotionEngine &cpu)
{
    uint32_t cause = cpu.get_gpr<uint32_t>(PARAM0);
    uint32_t address = cpu.get_gpr<uint32_t>(PARAM1);
    uint32_t next = cpu.get_gpr<uint32_t>(PARAM2);
    uint32_t arg = cpu.get_gpr<uint32_t>(PARAM3);
    ds_log->main->info("SYSCALL: add_INTC_handler\n");
    ds_log->main->info("Cause: ${:08X}\n", cause);
    ds_log->main->info("Addr: ${:08X}\n", address);
    ds_log->main->info("Next: ${:08X}\n", next);
    ds_log->main->info("Arg: ${:08X}\n", arg);

    INTC_handler handler;
    handler.cause = 1 << cause;
    handler.addr = address;
    handler.next = next;
    handler.arg = arg;

    store_INTC_handler(handler);
    intc_handlers.push_back(handler);
}

void BIOS_HLE::enable_INTC(EmotionEngine &cpu)
{
    uint32_t cause = cpu.get_gpr<uint32_t>(PARAM0);
    ds_log->main->info("SYSCALL: enable_INTC ${:08X}\n", cause);
    uint32_t new_value = 1 << cause;
    uint32_t old_mask = e->read32(0x1000F010);
    bool has_changed = false;

    if (!(old_mask & new_value))
    {
        e->write32(0x1000F010, new_value);
        has_changed = true;
    }

    cpu.set_gpr<uint64_t>(RETURN, has_changed);
}

void BIOS_HLE::init_main_thread(EmotionEngine &cpu)
{
    ds_log->main->info("SYSCALL: init_main_thread\n");
    uint32_t stack_base = cpu.get_gpr<uint32_t>(PARAM1);
    uint32_t stack_size = cpu.get_gpr<uint32_t>(PARAM2);
    ds_log->main->info("Stack base: ${:08X}\n", stack_base);
    ds_log->main->info("Stack size: ${:08X}\n", stack_size);

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
    ds_log->main->info("SYSCALL: init_heap\n");
    thread_hle* thread = &threads[0];
    uint32_t heap_base = cpu.get_gpr<uint32_t>(PARAM0);
    uint32_t heap_size = cpu.get_gpr<uint32_t>(PARAM1);
    if (heap_size == 0xFFFFFFFF)
        thread->heap_base = thread->stack_base;
    else
        thread->heap_base = heap_base + heap_size;

    cpu.set_gpr<uint64_t>(RETURN, thread->heap_base);
}

void BIOS_HLE::get_heap_end(EmotionEngine &cpu)
{
    thread_hle* thread = &threads[0];
    ds_log->main->info("SYSCALL: get_heap_end: ${:08X}\n", thread->heap_base);
    cpu.set_gpr<uint64_t>(RETURN, thread->heap_base);
}

void BIOS_HLE::set_GS_IMR(EmotionEngine &cpu)
{
    uint32_t imr = cpu.get_gpr<uint32_t>(PARAM0);
    ds_log->main->info("SYSCALL: set_GS_IMR ${:08X}\n", imr);
}

void BIOS_HLE::get_memory_size(EmotionEngine &cpu)
{
    //size of EE RDRAM
    ds_log->main->info("SYSCALL: get_memory_size\n");
    cpu.set_gpr<uint64_t>(RETURN, 0x02000000);
}
