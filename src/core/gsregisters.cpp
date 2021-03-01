#include <algorithm>
#include <cstdio>
#include "gsregisters.hpp"
#include "errors.hpp"

void GS_REGISTERS::write64_privileged(uint32_t addr, uint64_t value)
{
    addr &= 0x13F0;
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
        case 0x0010:
            SMODE1.PLL_reference_divider = value & 0x7;
            SMODE1.PLL_loop_divider = (value >> 3) & 0x7F;
            SMODE1.PLL_output_divider = (value >> 10) & 0x3;
            SMODE1.color_system = (value >> 13) & 0x3;
            SMODE1.PLL_reset = (value >> 16) & 0x1;
            SMODE1.YCBCR_color = (value >> 25) & 0x1;
            {
                uint8_t SPML = (value >> 21) & 0xF;

                // no divide by zero
                if (SPML && SMODE1.PLL_loop_divider)
                {
                    float VCK = (13500000 * SMODE1.PLL_loop_divider) / ((SMODE1.PLL_output_divider + 1) * SMODE1.PLL_reference_divider);
                    float PCK = VCK / SPML;

                    printf("[GS_r] Unimplemented privileged write64 to SMODE1: $%08lX_%08lX\n"
                        "\tPLL Reference Divider: %d\n"
                        "\tPLL Loop Divider: %d\n"
                        "\tPLL Output Divider: %d\n"
                        "\tPLL Reset: %d\n"
                        "\tColor System: %d\n"
                        "\tYCBCR enabled: %d\n"
                        "\tSPML: %d\n"
                        "\tCalculated VCK: %4.6lf MHz\n"
                        "\tCalculated PCK: %4.6lf MHz\n",
                        value >> 32, value & 0xFFFFFFFF,
                        SMODE1.PLL_reference_divider, SMODE1.PLL_loop_divider,
                        SMODE1.PLL_output_divider, SMODE1.PLL_reset,
                        SMODE1.color_system, SMODE1.YCBCR_color,
                        SPML, VCK / 1000000, PCK / 1000000
                    );
                }
            }
            break;
        case 0x0020:
            printf("[GS_r] Write SMODE2: $%08lX_%08lX\n", value >> 32, value & 0xFFFFFFFF);
            SMODE2.interlaced = value & 0x1;
            SMODE2.frame_mode = value & 0x2;
            SMODE2.power_mode = (value >> 2) & 0x3;
            if(SMODE2.interlaced && SMODE2.frame_mode) //Interlace - Read Every Line
                deinterlace_method = NO_DEINTERLACE;
            else
                deinterlace_method = BOB_DEINTERLACE;
            break;
        case 0x0030: // this register is undocumented, it's fields are unknown
            printf("[GS_r] Unimplemented privileged write64 to SRFSH: $%08lX_%08lX\n", value >> 32, value & 0xFFFFFFFF);
            break;
        case 0x0040:
            SYNCH1.horizontal_front_porch = value & 0x7FF;
            SYNCH1.horizontal_back_porch = (value >> 11) & 0x7FF;

            printf("[GS_r] Unimplemented privileged write64 to SYNCH1: $%08lX_%08lX\n"
                "\tHorizontal Front Porch: %d\n"
                "\tHorizontal Back Porch: %d\n",
                value >> 32, value & 0xFFFFFFFF,
                SYNCH1.horizontal_front_porch, SYNCH1.horizontal_back_porch
            );
            break;
        case 0x0050:
            printf("[GS_r] Unimplemented privileged write64 to SYNCH2: $%08lX_%08lX\n", value >> 32, value & 0xFFFFFFFF);
            break;
        case 0x0060:
            SYNCV.vertical_front_porch = value & 0x3FF;
            SYNCV.halflines_after_front_porch = (value >> 10) & 0x3FF;
            SYNCV.vertical_back_porch = (value >> 20) & 0xFFF;
            SYNCV.halflines_after_back_porch = (value >> 32) & 0x3FF;
            SYNCV.halflines_with_video = (value >> 42) & 0x7FF;
            SYNCV.halflines_with_vsync = (value >> 53) & 0x7FF;

            printf("[GS_r] Unimplemented privileged write64 to SYNCV: $%08lX_%08lX\n"
                "\tVertical Front Porch: %d\n"
                "\tHalflines After Front Porch: %d\n"
                "\tVertical Back Porch: %d\n"
                "\tHalflines After Back Porch: %d\n"
                "\tHalflines with Video: %d\n"
                "\tHalflines with VSYNC: %d\n",
                value >> 32, value & 0xFFFFFFFF,
                SYNCV.vertical_front_porch, SYNCV.halflines_after_front_porch,
                SYNCV.halflines_after_back_porch, SYNCV.halflines_after_back_porch,
                SYNCV.halflines_with_video, SYNCV.halflines_with_vsync
            );
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

            //Calculate actual values
            DISPLAY1.width /= DISPLAY1.magnify_x;
            DISPLAY1.height /= DISPLAY1.magnify_y;
            DISPLAY1.x /= DISPLAY1.magnify_x;
            DISPLAY1.y /= DISPLAY1.magnify_y;
            printf("MAGH: %d\n", DISPLAY1.magnify_x);
            printf("MAGV: %d\n", DISPLAY1.magnify_y);
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

            //Calculate actual values
            DISPLAY2.width /= DISPLAY2.magnify_x;
            DISPLAY2.height /= DISPLAY2.magnify_y;
            DISPLAY2.x /= DISPLAY2.magnify_x;
            DISPLAY2.y /= DISPLAY2.magnify_y;
            printf("MAGH: %d\n", DISPLAY2.magnify_x);
            printf("MAGV: %d\n", DISPLAY2.magnify_y);
            break;
        case 0x00B0:
            printf("[GS_r] Unimplemented privileged write64 to EXTBUF: $%08lX_%08lX\n", value >> 32, value & 0xFFFFFFFF);
            break;
        case 0x00C0:
            printf("[GS_r] Unimplemented privileged write64 to EXTDATA: $%08lX_%08lX\n", value >> 32, value & 0xFFFFFFFF);
            break;
        case 0x00D0:
            if (value & 0x1)
                Errors::die("[GS_r] Feedback Write activated but not implemented");
            break;
        case 0x00E0:
            printf("[GS_r] Write BGCOLOR: $%08lX\n", value);
            BGCOLOR = value & 0xFFFFFF;
            break;
        case 0x1000:
            printf("[GS_r] Write64 to GS_CSR: $%08lX_%08lX\n", value >> 32, value & 0xFFFFFFFF);
            CSR.SIGNAL_generated &= ~(value & 1);
            if (value & 1)
            {
                if (CSR.SIGNAL_stall)
                {
                    CSR.SIGNAL_stall = false;
                    //Stalled SIGNAL event is processed and generated as soon as the first is cancelled
                    //but the IRQ is delayed until IMR SIGMSK is toggled
                    SIGLBLID.sig_id = SIGLBLID.backup_sig_id;
                    CSR.SIGNAL_generated = true;
                    CSR.SIGNAL_irq_pending = true;
                }
                else //Stalled SIGNAL interrupt has been cancelled (MGS3 for testing)
                    CSR.SIGNAL_irq_pending = false;
            }
            if (value & 0x2)
            {
                CSR.FINISH_generated = false;
            }
            if (value & 0x4)
            {
                CSR.HBLANK_generated = false;
            }
            if (value & 0x8)
            {
                CSR.VBLANK_generated = false;
            }
            if (value & 0x200)
            {
                reset(true);
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
            Errors::die("[GS_r] Unrecognized privileged write64\nAddr: $%04X\nValue: $%08lX_%08lX\n", addr, value >> 32, value & 0xFFFFFFFF);
    }
}

void GS_REGISTERS::write32_privileged(uint32_t addr, uint32_t value)
{
    addr &= 0x13F0;
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
            if (value & 1)
            {
                if (CSR.SIGNAL_stall)
                {
                    CSR.SIGNAL_stall = false;
                    //Stalled SIGNAL event is processed and generated as soon as the first is cancelled
                    //but the IRQ is delayed until IMR SIGMSK is toggled
                    SIGLBLID.sig_id = SIGLBLID.backup_sig_id;
                    CSR.SIGNAL_generated = true;
                    CSR.SIGNAL_irq_pending = true;
                }
                else //Stalled SIGNAL interrupt has been cancelled (MGS3 for testing)
                    CSR.SIGNAL_irq_pending = false;
            }
            if (value & 0x2)
            {
                CSR.FINISH_generated = false;
            }
            if (value & 0x4)
            {
                CSR.HBLANK_generated = false;
            }
            if (value & 0x8)
            {
                CSR.VBLANK_generated = false;
            }
            if (value & 0x200)
            {
                reset(true);
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
            Errors::die("[GS_r] Unrecognized privileged write32\nAddr: $%04X\nValue: $%08X", addr, value);
    }
}

//reads from local copies only
uint32_t GS_REGISTERS::read32_privileged(uint32_t addr)
{
    return read64_privileged(addr & ~0x4) >> (32 * ((addr & 0x4) != 0));
}

uint64_t GS_REGISTERS::read64_privileged(uint32_t addr)
{
    addr &= 0x13F0;

    uint64_t reg = 0;
    switch (addr)
    {
        case 0x1080:
            reg |= SIGLBLID.sig_id;
            reg |= (uint64_t)SIGLBLID.lbl_id << 32UL;
            break;
        default:
            // All write only addresses return CSR
            // but for completeness CSR is 0x1000
            reg |= CSR.SIGNAL_generated << 0;
            reg |= CSR.FINISH_generated << 1;
            reg |= CSR.HBLANK_generated << 2;
            reg |= CSR.VBLANK_generated << 3;
            reg |= CSR.is_odd_frame << 13;
            reg |= (uint32_t)CSR.FIFO_status << 14;

            //GS revision and ID respectively, taken from PCSX2.
            //Shadow Hearts needs this.
            reg |= 0x1B << 16;
            reg |= 0x55 << 24;
    }

    return reg;
}

bool GS_REGISTERS::write64(uint32_t addr, uint64_t value)
{
    addr &= 0xFFFF;
    switch (addr)
    {
        case 0x0060:
            
        {
            uint32_t mask = value >> 32;
            uint32_t new_signal = value & mask;
            if (CSR.SIGNAL_generated)
            {
                printf("[GS] Second SIGNAL requested before acknowledged!\n");
                CSR.SIGNAL_stall = true;
                SIGLBLID.backup_sig_id = SIGLBLID.sig_id;
                SIGLBLID.backup_sig_id &= ~mask;
                SIGLBLID.backup_sig_id |= new_signal;
            }
            else
            {
                printf("[GS] SIGNAL requested!\n");
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

void GS_REGISTERS::reset(bool soft_reset)
{
    SMODE1.color_system = 0;
    SMODE1.PLL_loop_divider = 0;
    SMODE1.PLL_output_divider = 0;
    SMODE1.PLL_reference_divider = 0;
    SMODE1.PLL_reset = false;
    SMODE1.YCBCR_color = false;

    SMODE2.frame_mode = false;
    SMODE2.interlaced = false;
    SMODE2.power_mode = 0;

    deinterlace_method = BOB_DEINTERLACE;

    SYNCH1.horizontal_back_porch = 0;
    SYNCH1.horizontal_front_porch = 0;

    SYNCV.halflines_after_back_porch = 0;
    SYNCV.halflines_after_front_porch = 0;
    SYNCV.halflines_with_video = 0;
    SYNCV.halflines_with_vsync = 0;
    SYNCV.vertical_back_porch = 0;
    SYNCV.vertical_front_porch = 0;

    DISPFB1.frame_base = 0;
    DISPFB1.width = 0;
    DISPFB1.x = 0;
    DISPFB1.y = 0;
    DISPFB2.frame_base = 0;
    DISPFB2.width = 0;
    DISPFB2.y = 0;
    DISPFB2.x = 0;

    DISPLAY1.magnify_x = 1;
    DISPLAY1.magnify_y = 1;
    DISPLAY1.width = 0;
    DISPLAY1.height = 0;
    DISPLAY1.x = 0;
    DISPLAY1.y = 0;

    DISPLAY2.magnify_x = 1;
    DISPLAY2.magnify_y = 1;
    DISPLAY2.width = 0;
    DISPLAY2.height = 0;
    DISPLAY2.x = 0;
    DISPLAY2.y = 0;

    IMR.signal = true;
    IMR.finish = true;
    IMR.hsync = true;
    IMR.vsync = true;
    IMR.rawt = true;

    CSR.FIFO_status = 0x1; //Empty
    CSR.SIGNAL_generated = false;
    CSR.SIGNAL_stall = false;
    CSR.SIGNAL_irq_pending = false;
    CSR.HBLANK_generated = false;
    CSR.VBLANK_generated = false;
    CSR.FINISH_generated = false;
    CSR.FINISH_requested = false;

    SIGLBLID.lbl_id = 0;
    SIGLBLID.sig_id = 0;
    SIGLBLID.backup_sig_id = 0;
    PMODE.circuit1 = false;
    PMODE.circuit2 = false;

    if (soft_reset == false)
        CSR.is_odd_frame = true; //First frame is always odd

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
    /*switch (CRT_mode)
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
    }*/

    //Force the window to display everything in 4:3 for now
    w = 640;
    h = 480;
}

void GS_REGISTERS::get_inner_resolution(int &w, int &h)
{
    if (PMODE.circuit1 && PMODE.circuit2)
    {
        h = std::max(DISPLAY1.height, DISPLAY2.height);
        w = std::max(DISPLAY1.width, DISPLAY2.width);
    }
    else if (PMODE.circuit1)
    {
        h = DISPLAY1.height;
        w = DISPLAY1.width;
    }
    else //Circuit 2 only or none
    {
        h = DISPLAY2.height;
        w = DISPLAY2.width;
    }
    //TODO - Find out why some games double their height
    if (h >= (w * 1.3))
        h /= 2;
}

void GS_REGISTERS::swap_FIELD()
{
    if (SYNCV.vertical_front_porch & 0x1)
        CSR.is_odd_frame = !CSR.is_odd_frame;
    else
        CSR.is_odd_frame = true;
}

bool GS_REGISTERS::assert_HBLANK()//returns true if the interrupt should be processed
{
    if (!CSR.HBLANK_generated)
    {
        CSR.HBLANK_generated = true;
        return !IMR.hsync;
    }
    return false;
}

bool GS_REGISTERS::assert_VSYNC()//returns true if the interrupt should be processed
{
    if (!CSR.VBLANK_generated)
    {
        CSR.VBLANK_generated = true;
        return !IMR.vsync;
    }
    return false;
}

bool GS_REGISTERS::assert_FINISH()//returns true if the interrupt should be processed
{
    if (CSR.FINISH_requested && !CSR.FINISH_generated)
    {
        CSR.FINISH_requested = false;
        CSR.FINISH_generated = true;
        return !IMR.finish;
    }
    return false;
}
