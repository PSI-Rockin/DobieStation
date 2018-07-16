#include "logger.hpp"
#include <cstdio>
#include <cstdarg>
bool Logger::enabled[] = {};//sets them all to false

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

void Logger::set_all(bool data[LogOriginsCount]){
    for (int i = 0; i < LogOriginsCount; i++){
        enabled[i] = data[i];
    }
}
