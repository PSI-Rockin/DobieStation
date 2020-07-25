/** These are the GS registers that are (often indirectly) accessible from the EE/GIF
As such they are evaluated on both the GS thread and the GS manager.
**/
#ifndef GSREGISTERS_HPP
#define GSREGISTERS_HPP
#include <cstdint>

struct PMODE_REG
{
    bool circuit1;
    bool circuit2;
    uint8_t output_switching;
    bool use_ALP;
    bool out1_circuit2;
    bool blend_with_bg;
    uint8_t ALP;
};

// SMODE1 (undocumented)
//| Field  | Pos   | Format     | Notes                           |
//| ------ | ----- | ---------- | ------------------------------- |
//| RC     | 2:0   | int 0:3:0  | PLL Reference Divider           |
//| LC     | 9:3   | int 0:7:0  | PLL Loop Divider                |
//| T1248  | 11:10 | int 0:2:0  | PLL Output divider              |
//| SLCK   | 12    | int 0:1:0  |                                 |
//| CMOD   | 14:13 | int 0:2:0  | 0=VESA 1=RESERVED 2=NTSC, 3=PAL |
//| EX     | 15    | int 0:1:0  |                                 |
//| PRST   | 16    | int 0:1:0  | PLL Reset                       |
//| SINT   | 17    | int 0:1:0  | PLL                             |
//| XPCK   | 18    | int 0:1:0  |                                 |
//| PCK2   | 20:19 | int 0:2:0? |                                 |
//| SPML   | 24:21 | int 0:4:0? |                                 |
//| GCONT  | 25    | int 0:1:0  | 0 = RGBYC, 1 = YCRCB            |
//| PHS    | 26    | int 0:1:0  | HSync Output                    |
//| PVS    | 27    | int 0:1:0  | VSync Output                    |
//| PEHS   | 28    | int 0:1:0  |                                 |
//| PEVS   | 29    | int 0:1:0  |                                 |
//| CLKSEL | 31:30 | int 0:2:0? |                                 |
//| NVCK   | 32    | int 0:1:0  |                                 |
//| SLCK2  | 33    | int 0:1:0  |                                 |
//| VCKSEL | 35:34 | int 0:2:0? |                                 |
//| VHP    | 36    | int 0:1:0  |                                 |
struct SMODE1_REG
{
    bool PLL_reset;
    bool YCBCR_color;
    uint8_t PLL_reference_divider;
    uint8_t PLL_loop_divider;
    uint8_t PLL_output_divider;
    uint8_t color_system;
};

struct SMODE2_REG
{
    bool interlaced;
    bool frame_mode;
    uint8_t power_mode;
};

// SYNCH1 (undocumented)
//| Field | Pos   | Format      | Notes                  |
//| ----- | ----- | ----------- | ---------------------- |
//| HFP   | 10:0  | int 0:11:0? | Horizontal front porch |
//| HBP   | 21:11 | int 0:11:0? | Horizontal back porch  |
//| HSEQ  | 31:22 | int 0:10:0? |                        |
//| HSVS  | 42:32 | int 0:11:0? |                        |
//| HS    | ??    | int 0:??:0? |                        |
struct SYNCH1_REG
{
    uint16_t horizontal_front_porch;
    uint16_t horizontal_back_porch;
};

// SYNCH2 (undocumented)
//| Field | Pos  | Format      | Notes |
//| ----- | ---- | ----------- | ----- |
//| HF    | 10:0 | int 0:11:0? |       |
//| HB    | 21:0 | int 0:11:0? |       |
//struct SYNCH2_REG
//{
//
//};

// SYNCV
//| Field | Pos   | Format      | Notes                                                             |
//| ----- | ----- | ----------- | ----------------------------------------------------------------- |
//| VFP   | 9:0   | int 0:10:0? | Vertical front porch, halflines with color burst after video data |
//| VFPE  | 19:10 | int 0:10:0? | Halflines without color burst after VFP                           |
//| VBP   | 31:20 | int 0:12:0? | Vertical back porch, halflines with color burst after VFPE        |
//| VBPE  | 41:32 | int 0:10:0? | Halflines without color burst after VBP                           |
//| VDP   | 43:33 | int 0:11:0? | Halflines with video data                                         |
//| VS    | 54:44 | int 0:11:0? | Halflines with VSYNC                                              |
struct SYNCV_REG
{
    uint16_t vertical_front_porch;
    uint16_t vertical_back_porch;
    uint16_t halflines_after_front_porch;
    uint16_t halflines_after_back_porch;
    uint16_t halflines_with_video;
    uint16_t halflines_with_vsync;
};

struct DISPFB
{
    uint32_t frame_base;
    uint32_t width;
    uint8_t format;
    uint16_t x, y;
};

struct DISPLAY
{
    int32_t x, y;
    int32_t magnify_x, magnify_y;
    int32_t width, height;
};

struct GS_IMR //Interrupt masking
{
    bool signal;
    bool finish;
    bool hsync;
    bool vsync;
    bool rawt; //Rectangular Area Write Termination
};

struct GS_CSR
{
    bool SIGNAL_generated;
    bool SIGNAL_stall; //Used only by main/GIF thread
    bool SIGNAL_irq_pending; //As above
    bool HBLANK_generated;
    bool VBLANK_generated;
    bool is_odd_frame;
    bool FINISH_generated;
    bool FINISH_requested;
    uint8_t FIFO_status;
};

struct GS_SIGLBLID
{
    uint32_t sig_id;
    uint32_t backup_sig_id;
    uint32_t lbl_id;
};

//0 = No Deinterlacing, 1 = Merge Field (blurry/low res), 2 = Blend Scanline (ghosting), 3 = Bob (Best quality, can still have interlace bob)
enum DeinterlaceMethod
{
    NO_DEINTERLACE,
    MERGE_FIELD_DEINTERLACE,
    BLEND_SCANLINE_DEINTERLACE,
    BOB_DEINTERLACE,
};

struct GS_REGISTERS
{
    //Privileged registers
    PMODE_REG PMODE;
    SMODE1_REG SMODE1;
    SMODE2_REG SMODE2;
    SYNCH1_REG SYNCH1;
    SYNCV_REG SYNCV;
    DISPFB DISPFB1, DISPFB2;
    DISPLAY DISPLAY1, DISPLAY2;
    GS_IMR IMR;
    GS_CSR CSR;
    uint8_t BUSDIR;
    uint8_t CRT_mode;
    GS_SIGLBLID SIGLBLID;
    uint32_t BGCOLOR;
    DeinterlaceMethod deinterlace_method;
    
    //EXTBUF, EXTDATA, EXTWRITE not currently implemented

    uint32_t read32_privileged(uint32_t addr);
    uint64_t read64_privileged(uint32_t addr);
    void write32_privileged(uint32_t addr, uint32_t value);
    void write64_privileged(uint32_t addr, uint64_t value);
    bool write64(uint32_t addr, uint64_t value);//returns false if address not part of these registers

    void reset(bool soft_reset);

    void set_CRT(bool interlaced, int mode, bool frame_mode);
    void get_resolution(int &w, int &h);
    void get_inner_resolution(int &w, int &h);
    void swap_FIELD();
    bool assert_FINISH();
    bool assert_HBLANK();
    bool assert_VSYNC();

};
#endif // GSREGISTERS_HPP
