#include "logger.h"
#include <cstdio>
#include <cstdarg>
Logger::Logger() : enabled()
{

}
void Logger::log(LogOrigin origin, char* fmt, ...){
    va_list args;
    va_start(args,fmt);
    if (enabled[origin])
        vprintf(fmt, args);
    va_end(args);
}
void Logger::toggle(LogOrigin origin, bool enable)
{
    enabled[origin] = enable;
}
