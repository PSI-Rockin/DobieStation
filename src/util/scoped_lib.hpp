#ifndef DYNALIB_HPP
#define DYNALIB_HPP

#include <type_traits>
#ifdef WIN32
#include <Windows.h>
#else
#include <dlfcn.h>
#endif

namespace Util
{
// small class for managing dynamic
// libraries using RAII
class Dynalib
{
#ifdef WIN32
    HMODULE handle = nullptr;
#else
    void* handle = nullptr;
#endif

public:
    Dynalib() = default;
    Dynalib(const char* filename);
    ~Dynalib(); // calls close to unload

    // no copying
    Dynalib(Dynalib const&) = delete;
    Dynalib& operator=(Dynalib const&) = delete;

    bool is_open() const { return handle != nullptr; };
    bool open(const char* filename);

    void close();

    // TODO: this could probably be expanded with templates
    // to return the correct type based on the function
    // but for now a dynamic cast from void* is good enough
    void* get_sym(const char* name) const;
};

// TODO: maybe allow moves?
static_assert(!std::is_copy_constructible<Dynalib>::value, "Cannot copy a dynalib");
static_assert(!std::is_copy_assignable<Dynalib>::value, "Cannot copy a dynalib");
}
#endif
