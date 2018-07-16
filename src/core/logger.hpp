#ifndef LOGGER_H
#define LOGGER_H

class Logger
{
public:
    enum LogOrigin{OTHER,
        IPU, EE, VIF, VU, GIF, GS, CDVD, IOP, Emulator,
        IOP_Debug, EE_Interpreter, FPU, DMAC, GS_CONTEXT,IOP_COP0,IOP_Interpreter,
        IOP_DMA,EE_Timing,IOP_Timing,INTC,SIO2,PAD,
        VU_Interpreter,
    OriginsCount
    };
    static constexpr const char* origin_names[] = { "Other (Needs categorization!)",
        "IPU", "EE", "VIF", "VU", "GIF", "GS", "CDVD", "IOP", "Emulator",
        "IOP Debug", "EE Interpreter", "FPU", "DMAC", "GS Context","IOP COP0","IOP Interpreter",
        "IOP_DMA","EE Timing","IOP Timing","INTC","SIO2","PAD",
        "VU_Interpreter",};

    //static void log(LogOrigin origin, char* format, ...);
    static void log(LogOrigin origin, const char* format, ...);
    static void toggle(LogOrigin origin, bool enable);
    static bool get_state(LogOrigin origin);
    static void set_all(bool data[OriginsCount]);

private:
    static bool enabled[OriginsCount];
};
#endif // LOGGER_H
