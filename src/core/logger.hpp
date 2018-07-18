#ifndef LOGGER_H
#define LOGGER_H

/**
Logs debug info only if the corresponding origin is enabled.
You can call Logger::set(origin, true) anywhere to enable logging for that origin.
QT and Emulator are enabled by default
**/

class Logger
{
public:
    enum LogOrigin{QT,Emulator,
        BIOS_HLE, IPU, EE, EE_Timing,EE_Interpreter,
        VIF, VU, GIF,VU_Interpreter,
        GS, GS_CONTEXT,
        CDVD, IOP,IOP_Debug, FPU, DMAC,IOP_COP0,IOP_Interpreter,
        IOP_DMA,IOP_Timing,INTC,SIO2,PAD,SPU,
    OriginsCount
    };
    static constexpr const char* origin_names[] = { "Qt", "Emulator",
        "BIOS HLE", "IPU", "EE", "EE Timing", "EE Interpreter",
        "VIF", "VU", "GIF","VU Interpreter",
        "GS", "GS Context",
        "CDVD", "IOP", "IOP Debug", "FPU", "DMAC", "IOP COP0","IOP Interpreter",
        "IOP DMA","IOP Timing","INTC","SIO2","PAD","SPU",};

    //static void log(LogOrigin origin, char* format, ...);
    static void log(LogOrigin origin, const char* format, ...);
    static void log_silent(LogOrigin origin, const char* format, ...);//logs but doesn't include the [Origin] prefix
    static void set(LogOrigin origin, bool enable);
    static bool get_state(LogOrigin origin);
    static void set_all(bool data[OriginsCount]);

private:
    static bool enabled[OriginsCount];
};
#endif // LOGGER_H
