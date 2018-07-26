#include "errors.hpp"
#include <cstdarg>
#include <string>
void Errors::die(const char* format, ...){
    char* output = new char[ERROR_STRING_MAX_LENGTH];
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    vsnprintf(output, ERROR_STRING_MAX_LENGTH, format, args);
    va_end(args);
    std::string error_str(output);
    throw Emulation_error(error_str);
}

void Errors::dont_die(const char* format, ...) {
    /*this is 100% identical to printf
    but is intended for erorrs that we want to ignore for now*/
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
}