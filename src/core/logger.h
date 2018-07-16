#ifndef LOGGER_H
#define LOGGER_H

enum LogOrigin{
    IPU, EE, VIF, VU, GIF, GS, CDVD, IOP,
COUNT_OF_ORIGINS
};
const char* LogOriginNames[] = { "IPU", "EE", "VIF", "VU", "GIF", "GS", "CDVD", "IOP"};
class Logger
{
public:
    static void log(LogOrigin origin, char* format, ...);
    static void toggle(LogOrigin origin, bool enable);
    static void set_all(bool data[COUNT_OF_ORIGINS]);
private:
    static bool enabled[COUNT_OF_ORIGINS];
};

#endif // LOGGER_H
