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
    Logger();
    void log(LogOrigin origin, char* format, ...);
    void toggle(LogOrigin origin, bool enable);
private:
    bool enabled[COUNT_OF_ORIGINS];
};

#endif // LOGGER_H
