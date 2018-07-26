#include "errors.hpp"
#include <cstdarg>
#include <string>
void Errors::die(const char* format, ...){
    char* output = new char[255];
    va_list args;
    va_start(args, format);
    snprintf(output, 255, format, args);
    va_end(args);
    std::string error_str(output);
    throw Emulation_error(error_str);
}

void Errors::dont_die(const char* format, ...) {
    va_list args;
    va_start(args, format);
    printf(format, args);
    va_end(args);
}