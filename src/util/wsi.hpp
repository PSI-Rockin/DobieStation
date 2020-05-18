#pragma once

namespace Util
{
struct WSI
{
    enum class Type
    {
        Unknown,
        DWM,
        Quartz,
        X11
    };

    WSI() = default;
    ~WSI() = default;

    // runtime info about the display system
    Type type = Type::Unknown;
    void* connection;
    void* surface;
};
}