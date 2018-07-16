#include "logger.hpp"
#include <cstdio>
#include <cstdarg>
constexpr const char* Logger::origin_names[Logger::OriginsCount];
bool Logger::enabled[] = {1};//sets them all to false except for the "other" category

void Logger::log(Logger::LogOrigin origin, const char* fmt, ...){
    va_list args;
    va_start(args,fmt);
    if (enabled[origin]){
        printf("[%s] ",origin_names[origin]);
        vprintf(fmt, args);
    }
    va_end(args);
}
void Logger::toggle(LogOrigin origin, bool enable)
{
    enabled[origin] = enable;
}

void Logger::set_all(bool data[OriginsCount]){
    for (int i = 0; i < OriginsCount; i++){
        enabled[i] = data[i];
    }
}

bool Logger::get_state(LogOrigin origin){
    return enabled[origin];
}
