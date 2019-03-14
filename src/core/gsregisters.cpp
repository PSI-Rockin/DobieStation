#include <cstdio>
#include "gsregisters.hpp"
#include "errors.hpp"

void GS_REGISTERS::write64_privileged(uint32_t addr, uint64_t value)
{
    addr &= 0xFFFF;
    switch (addr)
    {
        case 0x0000:
            printf("[GS_r] Write PMODE: $%08lX_%08lX\n", value >> 32, value & 0xFFFFFFFF);
            PMODE.circuit1 = value & 0x1;
            PMODE.circuit2 = value & 0x2;
            PMODE.output_switching = (value >> 2) & 0x7;
            PMODE.use_ALP = value & (1 << 5);
            PMODE.out1_circuit2 = value & (1 << 6);
            PMODE.blend_with_bg = value & (1 << 7);
            PMODE.ALP = (value >> 8) & 0xFF;
            break;
        case 0x0020:
            printf("[GS_r] Write SMODE2: $%08lX_%08lX\n", value >> 32, value & 0xFFFFFFFF);
            SMODE2.interlaced = value & 0x1;
            SMODE2.frame_mode = value & 0x2;
            SMODE2.power_mode = (value >> 2) & 0x3;
            break;
        case 0x0070:
            printf("[GS_r] Write DISPFB1: $%08lX_%08lX\n", value >> 32, value & 0xFFFFFFFF);
            DISPFB1.frame_base = (value & 0x1FF) * 2048;
            DISPFB1.width = ((value >> 9) & 0x3F) * 64;
            DISPFB1.format = (value >> 15) & 0x1F;
            DISPFB1.x = (value >> 32) & 0x7FF;
            DISPFB1.y = (value >> 43) & 0x7FF;
            break;
        case 0x0080:
            printf("[GS_r] Write DISPLAY1: $%08lX_%08lX\n", value >> 32, value & 0xFFFFFFFF);
            DISPLAY1.x = value & 0xFFF;
            DISPLAY1.y = (value >> 12) & 0x7FF;
            DISPLAY1.magnify_x = ((value >> 23) & 0xF) + 1;
            DISPLAY1.magnify_y = ((value >> 27) & 0x3) + 1;
            DISPLAY1.width = ((value >> 32) & 0xFFF) + 1;
            DISPLAY1.height = ((value >> 44) & 0x7FF) + 1;
            break;
        case 0x0090:
            printf("[GS_r] Write DISPFB2: $%08lX_%08lX\n", value >> 32, value & 0xFFFFFFFF);
            DISPFB2.frame_base = (value & 0x1FF) * 2048;
            DISPFB2.width = ((value >> 9) & 0x3F) * 64;
            DISPFB2.format = (value >> 15) & 0x1F;
            DISPFB2.x = (value >> 32) & 0x7FF;
            DISPFB2.y = (value >> 43) & 0x7FF;
            break;
        case 0x00A0:
            printf("[GS_r] Write DISPLAY2: $%08lX_%08lX\n", value >> 32, value & 0xFFFFFFFF);
            DISPLAY2.x = value & 0xFFF;
            DISPLAY2.y = (value >> 12) & 0x7FF;
            DISPLAY2.magnify_x = ((value >> 23) & 0xF) + 1;
            DISPLAY2.magnify_y = ((value >> 27) & 0x3) + 1;
            DISPLAY2.width = ((value >> 32) & 0xFFF) + 1;
            DISPLAY2.height = ((value >> 44) & 0x7FF) + 1;
            printf("MAGH: %d\n", DISPLAY2.magnify_x);
            printf("MAGV: %d\n", DISPLAY2.magnify_y);
            break;
        case 0x00E0:
            printf("[GS_r] Write BGCOLOR: $%08lX\n", value);
            BGCOLOR = value & 0xFFFFFF;
            break;
        case 0x1000:
            printf("[GS_r] Write64 to GS_CSR: $%08lX_%08lX\n", value >> 32, value & 0xFFFFFFFF);
            CSR.SIGNAL_generated &= ~(value & 1);
            CSR.SIGNAL_stall &= ~(value & 0x1);
            if (value & 0x2)
            {
                CSR.FINISH_enabled = true;
                CSR.FINISH_generated = false;
            }
            if (value & 0x8)
            {
                CSR.VBLANK_enabled = true;
                CSR.VBLANK_generated = false;
            }
            break;
        case 0x1010:
            printf("[GS_r] Write64 GS_IMR: $%08lX_%08lX\n", value >> 32, value & 0xFFFFFFFF);
            IMR.signal = value & (1 << 8);
            IMR.finish = value & (1 << 9);
            IMR.hsync = value & (1 << 10);
            IMR.vsync = value & (1 << 11);
            IMR.rawt = value & (1 << 12);
            break;
        case 0x1040:
            printf("[GS_r] Write64 to GS_BUSDIR: $%08lX_%08lX\n", value >> 32, value & 0xFFFFFFFF);
            BUSDIR = (uint8_t)value;
            break;
        case 0x1080:
            SIGLBLID.sig_id = value & 0xFFFFFFFF;
            SIGLBLID.lbl_id = value >> 32;
            break;
        default:
            printf("[GS_r] Unrecognized privileged write64 to reg $%04X: $%08lX_%08lX\n", addr, value >> 32, value & 0xFFFFFFFF);
    }
}

void GS_REGISTERS::write32_privileged(uint32_t addr, uint32_t value)
{
    addr &= 0xFFFF;
    switch (addr)
    {
        case 0x0070:
            printf("[GS_r] Write DISPFB1: $%08X\n", value);
            DISPFB1.frame_base = (value & 0x1FF) * 2048;
            DISPFB1.width = ((value >> 9) & 0x3F) * 64;
            DISPFB1.format = (value >> 15) & 0x1F;
            break;
        case 0x0090:
            printf("[GS_r] Write DISPFB2: $%08X\n", value);
            DISPFB2.frame_base = (value & 0x1FF) * 2048;
            DISPFB2.width = ((value >> 9) & 0x3F) * 64;
            DISPFB2.format = (value >> 15) & 0x1F;
            break;
        case 0x00E0:
            printf("[GS_r] Write BGCOLOR: $%08X\n", value);
            BGCOLOR = value & 0xFFFFFF;
            break;
        case 0x1000:
            printf("[GS_r] Write32 to GS_CSR: $%08X\n", value);
            CSR.SIGNAL_generated &= ~(value & 1);
            CSR.SIGNAL_stall &= ~(value & 1);
            if (value & 0x2)
            {
                CSR.FINISH_enabled = true;
                CSR.FINISH_generated = false;
            }
            if (value & 0x8)
            {
                CSR.VBLANK_enabled = true;
                CSR.VBLANK_generated = false;
            }
            break;
        case 0x1010:
            printf("[GS_r] Write32 GS_IMR: $%08X\n", value);
            IMR.signal = value & (1 << 8);
            IMR.finish = value & (1 << 9);
            IMR.hsync = value & (1 << 10);
            IMR.vsync = value & (1 << 11);
            IMR.rawt = value & (1 << 12);
            break;
        case 0x1080:
            SIGLBLID.sig_id = value;
            break;
        default:
            printf("\n[GS_r] Unrecognized privileged write32 to reg $%04X: $%08X", addr, value);
    }
}

//reads from local copies only
uint32_t GS_REGISTERS::read32_privileged(uint32_t addr)
{
    addr &= 0xFFFF;
    switch (addr)
    {
        case 0x1000:
        {
            uint32_t reg = 0;
            reg |= CSR.SIGNAL_generated;
            reg |= CSR.FINISH_generated << 1;
            reg |= CSR.VBLANK_generated << 3;
            reg |= CSR.is_odd_frame << 13;
            //printf("[GS_r] read32_privileged!: CSR = %04X\n", reg);
            return reg;
        }
        case 0x1080:
            return SIGLBLID.sig_id;
        default:
            printf("[GS_r] Unrecognized privileged read32 from $%04X\n", addr);
            return 0;
    }
}

uint64_t GS_REGISTERS::read64_privileged(uint32_t addr)
{
    addr &= 0xFFFF;
    switch (addr)
    {
        case 0x1000:
        {
            uint64_t reg = 0;
            reg |= CSR.SIGNAL_generated;
            reg |= CSR.FINISH_generated << 1;
            reg |= CSR.VBLANK_generated << 3;
            reg |= CSR.is_odd_frame << 13;
            //printf("[GS_r] read64_privileged!: CSR = %08X\n", reg);
            return reg;
        }
        case 0x1080:
        {
            uint64_t reg = 0;
            reg |= SIGLBLID.sig_id;
            reg |= (uint64_t)SIGLBLID.lbl_id << 32UL;
            return reg;
        }
        default:
            printf("[GS_r] Unrecognized privileged read64 from $%04X\n", addr);
            return 0;
    }
}

bool GS_REGISTERS::write64(uint32_t addr, uint64_t value)
{
    addr &= 0xFFFF;
    switch (addr)
    {
        case 0x0060:
            printf("[GS] SIGNAL requested!\n");
        {
            uint32_t mask = value >> 32;
            uint32_t new_signal = value & mask;
            if (CSR.SIGNAL_generated)
            {
                printf("[GS] Second SIGNAL requested before acknowledged!\n");
                CSR.SIGNAL_stall = true;
                SIGLBLID.backup_sig_id &= ~mask;
                SIGLBLID.backup_sig_id |= new_signal;
            }
            else
            {
                CSR.SIGNAL_generated = true;
                SIGLBLID.sig_id &= ~mask;
                SIGLBLID.sig_id |= new_signal;
            }
        }
            return true;
        case 0x0061:
            printf("[GS] FINISH Write\n");
            CSR.FINISH_requested = true;
            return true;
        case 0x0062:
            printf("[GS] LABEL requested!\n");
        {
            uint32_t mask = value >> 32;
            uint32_t new_label = value & mask;
            SIGLBLID.lbl_id &= ~mask;
            SIGLBLID.lbl_id |= new_label;
        }
            return true;
    }
    return false;
}

void GS_REGISTERS::reset()
{
    DISPLAY1.x = 0;
    DISPLAY1.y = 0;
    DISPLAY1.width = 0;
    DISPLAY1.height = 0;
    DISPLAY2.x = 0;
    DISPLAY2.y = 0;
    DISPLAY2.width = 0;
    DISPLAY2.height = 0;
    DISPFB1.frame_base = 0;
    DISPFB1.width = 0;
    DISPFB1.x = 0;
    DISPFB1.y = 0;
    DISPFB2.frame_base = 0;
    DISPFB2.width = 0;
    DISPFB2.y = 0;
    DISPFB2.x = 0;
    IMR.signal = true;
    IMR.finish = true;
    IMR.hsync = true;
    IMR.vsync = true;
    IMR.rawt = true;
    CSR.is_odd_frame = false;
    CSR.SIGNAL_generated = false;
    CSR.SIGNAL_stall = false;
    CSR.VBLANK_enabled = true;
    CSR.VBLANK_generated = false;
    CSR.FINISH_enabled = false;
    CSR.FINISH_generated = false;
    CSR.FINISH_requested = false;

    SIGLBLID.lbl_id = 0;
    SIGLBLID.sig_id = 0;

    set_CRT(false, 0x2, false);
}

void GS_REGISTERS::set_CRT(bool interlaced, int mode, bool frame_mode)
{
    SMODE2.interlaced = interlaced;
    CRT_mode = mode;
    SMODE2.frame_mode = frame_mode;
}

void GS_REGISTERS::get_resolution(int &w, int &h)
{
    w = 640;
    switch (CRT_mode)
    {
        case 0x2:
            h = 448;
            break;
        case 0x3:
            h = 512;
            break;
        case 0x1C:
            h = 480;
            break;
        default:
            h = 448;
    }
}

void GS_REGISTERS::get_inner_resolution(int &w, int &h)
{
    DISPLAY &current_display = DISPLAY1;

    if (PMODE.circuit1 == true)
    {
        current_display = DISPLAY1;
    }
    else
    {
        current_display = DISPLAY2;
    }
    w = current_display.width >> 2;
    h = current_display.height;
}

void GS_REGISTERS::set_VBLANK(bool is_VBLANK)
{
    if (!is_VBLANK)
    {
        CSR.is_odd_frame = !CSR.is_odd_frame;
    }
}

bool GS_REGISTERS::assert_VSYNC()//returns true if the interrupt should be processed
{
    if (CSR.VBLANK_enabled)
    {
        CSR.VBLANK_generated = true;
        CSR.VBLANK_enabled = false;
        return !IMR.vsync;
    }
    return false;
}

bool GS_REGISTERS::assert_FINISH()//returns true if the interrupt should be processed
{
    if (CSR.FINISH_requested && CSR.FINISH_enabled)
    {
        CSR.FINISH_generated = true;
        CSR.FINISH_enabled = false;
        CSR.FINISH_requested = false;
        return !IMR.finish;
    }
    return false;
}
