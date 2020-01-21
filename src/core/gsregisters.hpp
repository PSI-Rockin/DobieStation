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

struct SMODE
{
    bool interlaced;
    bool frame_mode;
    uint8_t power_mode;
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
    uint16_t x, y;
    uint8_t magnify_x, magnify_y;
    uint16_t width, height;
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
    bool VBLANK_generated;
    bool VBLANK_enabled;
    bool is_odd_frame;
    bool FINISH_enabled;
    bool FINISH_generated;
    bool FINISH_requested;
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
    SMODE SMODE2;
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

    void reset();

    void set_CRT(bool interlaced, int mode, bool frame_mode);
    void get_resolution(int &w, int &h);
    void get_inner_resolution(int &w, int &h);
    void set_VBLANK(bool is_VBLANK);
    bool assert_FINISH();
    bool assert_VSYNC();

};
#endif // GSREGISTERS_HPP
