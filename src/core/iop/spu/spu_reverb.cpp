#include "spu.hpp"
#include "../../errors.hpp"

uint32_t SPU::translate_reverb_offset(int offset)
{
    uint32_t addr = reverb.effect_pos + offset;
    uint32_t size = reverb.effect_area_end - reverb.effect_area_start;
    addr = reverb.effect_area_start + (addr % size);

    if (addr < reverb.effect_area_start || addr > reverb.effect_area_end)
        Errors::die("[SPU%d] RDEBUG reverb addr outside of buffer - VERY BAD\n", id);

    return addr;
}


void SPU::run_reverb(stereo_sample wet)
{
    // Reverb runs at half the rate
    // In reality it alternates producing left and right samples
    auto &r = reverb;

    if (reverb.cycle > 0)
    {
        reverb.cycle--;
        return;
    }

    if (static_cast<int>(reverb.effect_area_end - reverb.effect_area_start) <= 0)
        return;

    int16_t Lin = 0, Rin = 0;
    int16_t Lout = 0, Rout = 0;


#define W(x, value) write(translate_reverb_offset((static_cast<int>(x))), static_cast<uint16_t>((value)))
#define R(x) static_cast<int16_t>(read(translate_reverb_offset((x))))
    //_Input from Mixer (Input volume multiplied with incoming data)_____________
    // Lin = vLIN * LeftInput    ;from any channels that have Reverb enabled
    // Rin = vRIN * RightInput   ;from any channels that have Reverb enabled
    Lin = mulvol(wet.left, r.vLIN);
    Rin = mulvol(wet.right, r.vRIN);

     // ____Same Side Reflection (left-to-left and right-to-right)___________________
    // [mLSAME] = (Lin + [dLSAME]*vWALL - [mLSAME-2])*vIIR + [mLSAME-2]  ;L-to-L
    // [mRSAME] = (Rin + [dRSAME]*vWALL - [mRSAME-2])*vIIR + [mRSAME-2]  ;R-to-R
    int16_t tLSAME = clamp16(Lin + mulvol(R(r.dLSAME), r.vWALL));
    tLSAME = mulvol(clamp16(tLSAME - R(r.mLSAME - 1)), r.vIIR);
    tLSAME = clamp16(tLSAME + R(r.mLSAME - 1));

    if (effect_enable)
        W(r.mLSAME, tLSAME);

    int16_t tRSAME = clamp16(Rin + mulvol(R(r.dRSAME), r.vWALL));
    tRSAME = mulvol(clamp16(tRSAME - R(r.mRSAME - 1)), r.vIIR);
    tRSAME = clamp16(tRSAME + R(r.mRSAME - 1));

    if (effect_enable)
        W(r.mRSAME, tRSAME);

    // ___Different Side Reflection (left-to-right and right-to-left)_______________
    // [mLDIFF] = (Lin + [dRDIFF]*vWALL - [mLDIFF-2])*vIIR + [mLDIFF-2]  ;R-to-L
    // [mRDIFF] = (Rin + [dLDIFF]*vWALL - [mRDIFF-2])*vIIR + [mRDIFF-2]  ;L-to-R
    int16_t tLDIFF = clamp16(Lin + mulvol(R(r.dRDIFF), r.vWALL));
    tLDIFF = mulvol(clamp16(tLDIFF- R(r.mLDIFF - 1)), r.vIIR);
    tLDIFF = clamp16(tLDIFF + R(r.mLDIFF - 1));

    if (effect_enable)
        W(r.mLDIFF, tLDIFF);

    int16_t tRDIFF = clamp16(Rin + mulvol(R(r.dLDIFF), r.vWALL));
    tRDIFF = mulvol(clamp16(tRDIFF- R(r.mRDIFF - 1)), r.vIIR);
    tRDIFF = clamp16(tRDIFF + R(r.mRDIFF - 1));

    if (effect_enable)
        W(r.mRDIFF, tRDIFF);

    // ___Early Echo (Comb Filter, with input from buffer)__________________________
    // Lout=vCOMB1*[mLCOMB1]+vCOMB2*[mLCOMB2]+vCOMB3*[mLCOMB3]+vCOMB4*[mLCOMB4]
    // Rout=vCOMB1*[mRCOMB1]+vCOMB2*[mRCOMB2]+vCOMB3*[mRCOMB3]+vCOMB4*[mRCOMB4]
    Lout = mulvol(r.vCOMB1, R(r.mLCOMB1));
    Lout = clamp16(Lout + mulvol(r.vCOMB2, R(r.mLCOMB2)));
    Lout = clamp16(Lout + mulvol(r.vCOMB3, R(r.mLCOMB3)));
    Lout = clamp16(Lout + mulvol(r.vCOMB4, R(r.mLCOMB4)));

    Rout = mulvol(r.vCOMB1, R(r.mRCOMB1));
    Rout = clamp16(Rout + mulvol(r.vCOMB2, R(r.mRCOMB2)));
    Rout = clamp16(Rout + mulvol(r.vCOMB3, R(r.mRCOMB3)));
    Rout = clamp16(Rout + mulvol(r.vCOMB4, R(r.mRCOMB4)));

    // ___Late Reverb APF1 (All Pass Filter 1, with input from COMB)________________
    // Lout=Lout-vAPF1*[mLAPF1-dAPF1], [mLAPF1]=Lout, Lout=Lout*vAPF1+[mLAPF1-dAPF1]
    // Rout=Rout-vAPF1*[mRAPF1-dAPF1], [mRAPF1]=Rout, Rout=Rout*vAPF1+[mRAPF1-dAPF1]
    Lout = clamp16(Lout - mulvol(r.vAPF1, R(r.mLAPF1-r.dAFP1)));
    if (effect_enable)
        W(r.mLAPF1, Lout);
    Lout = clamp16(mulvol(Lout, r.vAPF1) + R(r.mLAPF1-r.dAFP1));

    Rout = clamp16(Rout - mulvol(r.vAPF1, R(r.mRAPF1-r.dAFP1)));
    if (effect_enable)
        W(r.mRAPF1, Rout);
    Rout = clamp16(mulvol(Rout, r.vAPF1) + R(r.mRAPF1-r.dAFP1));

    // ___Late Reverb APF2 (All Pass Filter 2, with input from APF1)________________
    // Lout=Lout-vAPF2*[mLAPF2-dAPF2], [mLAPF2]=Lout, Lout=Lout*vAPF2+[mLAPF2-dAPF2]
    // Rout=Rout-vAPF2*[mRAPF2-dAPF2], [mRAPF2]=Rout, Rout=Rout*vAPF2+[mRAPF2-dAPF2]
    Lout = clamp16(Lout-mulvol(r.vAPF2, R(r.mLAPF2-r.dAFP2)));
    if (effect_enable)
        W(r.mLAPF2, Lout);
    Lout = clamp16(mulvol(Lout, r.vAPF2)+R(r.mLAPF2-r.dAFP2));

    Rout = clamp16(Rout-mulvol(r.vAPF2, R(r.mRAPF2-r.dAFP2)));
    if (effect_enable)
        W(r.mRAPF2, Rout);
    Rout = clamp16(mulvol(Rout, r.vAPF2)+R(r.mRAPF2-r.dAFP2));
    // ___Output to Mixer (Output volume multiplied with input from APF2)___________
    // LeftOutput  = Lout*vLOUT
    // RightOutput = Rout*vROUT
    //r.Eout.left = Lout;
    //r.Eout.right = Rout;
    r.Eout.left = mulvol(Lout, effect_volume_l);
    r.Eout.right = mulvol(Rout, effect_volume_r);
    // ___Finally, before repeating the above steps_________________________________
    // BufferAddress = MAX(mBASE, (BufferAddress+2) AND 7FFFEh)
    // Wait one 1T, then repeat the above stuff
    r.effect_pos += 1;
    if (r.effect_pos >= (r.effect_area_end-r.effect_area_start+1))
        r.effect_pos = 0;
#undef R
#undef W
#undef MUL

    reverb.cycle = 1;
}
