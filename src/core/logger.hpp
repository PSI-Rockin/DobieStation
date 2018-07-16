#ifndef LOGGER_H
#define LOGGER_H

enum LogOrigin{
    IPU, EE, VIF, VU, GIF, GS, CDVD, IOP, QT,
LogOriginsCount
};
static const char* LogOriginNames[] = { "IPU", "EE", "VIF", "VU", "GIF", "GS", "CDVD", "IOP", "QT"};
class Logger
{
public:
    static void log(LogOrigin origin, char* format, ...);
    static void toggle(LogOrigin origin, bool enable);
    static void set_all(bool data[LogOriginsCount]);
private:
    static bool enabled[LogOriginsCount];
};

#endif // LOGGER_H
