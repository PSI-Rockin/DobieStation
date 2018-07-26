#ifndef ERRORS_HPP
#define ERRORS_HPP

#include <stdexcept>

class Errors {
    public:
        static void die(const char* format, ...);
        static void dont_die(const char* format, ...);//ignores the error, just print it.
};
class Emulation_error : public std::runtime_error {
    using std::runtime_error::runtime_error;
};
#endif