#include "scoped_lib.hpp"

namespace Util
{

// https://docs.microsoft.com/en-us/windows/win32/api/libloaderapi/nf-libloaderapi-loadlibrarya
// https://docs.microsoft.com/en-us/windows/win32/api/libloaderapi/nf-libloaderapi-freelibrary
// https://docs.microsoft.com/en-us/windows/win32/api/libloaderapi/nf-libloaderapi-getprocaddress
// http://man7.org/linux/man-pages/man3/dlopen.3.html
// http://man7.org/linux/man-pages/man3/dlsym.3.html

Dynalib::Dynalib(const char* filename)
{
    open(filename);
}

bool Dynalib::open(const char* filename)
{
#ifdef WIN32
    handle = LoadLibraryA(filename);
#else // macOS/Unix
    handle = dlopen(filename, RTLD_NOW);
#endif
    return handle != nullptr;
}

void Dynalib::close()
{
    if (!is_open())
        return;

#ifdef WIN32
    FreeLibrary(handle);
#else // macOS/Unix
    dlclose(handle);
#endif
    handle = nullptr;
}

void* Dynalib::get_sym(const char* name) const
{
#ifdef WIN32
    return reinterpret_cast<void*>(GetProcAddress(handle, name));
#else // macOS/Unix
    return reinterpret_cast<void*>(dlsym(handle, name));
#endif
}

Dynalib::~Dynalib()
{
    close();
}

}