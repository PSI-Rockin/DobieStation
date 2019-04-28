#ifndef ERRORS_HPP
#define ERRORS_HPP

#include <stdexcept>

#define ERROR_STRING_MAX_LENGTH 255

class Errors
{
    public:
        static void die(const char* format, ...);
        static void non_fatal(const char* format, ...);
        static void print_warning(const char* format, ...);//ignores the error, just print it.
};

class Emulation_error : public std::runtime_error
{
    using std::runtime_error::runtime_error;
};

class non_fatal_error : public std::runtime_error
{
    using std::runtime_error::runtime_error;
};

#include <time.h>
#include <stdint.h>
#include <assert.h>

class CTimer {
public:
    CTimer() {
        start();
    }

    void start() {
        clock_gettime(CLOCK_MONOTONIC, &_startTime);
    }

    double getMs() {
        return (double)getNs() / 1.e6;
    }

    int64_t getNs() {
        struct timespec now;
        clock_gettime(CLOCK_MONOTONIC, &now);
        return (int64_t)(now.tv_nsec - _startTime.tv_nsec) + 1000000000 * (now.tv_sec - _startTime.tv_sec);

    }

    double getSeconds() {
        return (double)getNs() / 1.e9;
    }

    struct timespec _startTime;
};


#endif
