cmake_minimum_required(VERSION 3.1)
project(DobieStation)

if (${CMAKE_C_COMPILER_ID} STREQUAL "GNU" OR
    ${CMAKE_C_COMPILER_ID} STREQUAL "Clang" OR
    ${CMAKE_C_COMPILER_ID} STREQUAL "AppleClang")

    string(REPLACE "-O3" "-O2" CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE}")
    set(DOBIE_FLAGS
        -Wall -Wundef -Wsign-compare -Wconversion -Wstrict-aliasing -Wtype-limits

        # These probably should be fixed instead of disabled,
        # but doing so to keep the warning count more managable for now.
        -Wno-reorder -Wno-unused-variable -Wno-unused-value

        # Might be useful for debugging:
        #-fomit-frame-pointer -fwrapv -fno-delete-null-pointer-checks -fno-strict-aliasing -fvisibility=hidden
    )

    if (${CMAKE_C_COMPILER_ID} STREQUAL "AppleClang")
        # Needed to make a working build on some Macs,
        # Dobie crashes in the same way as using -O3
        set(DOBIE_FLAGS ${DOBIE_FLAGS} -fomit-frame-pointer)
    else()
        set(DOBIE_FLAGS ${DOBIE_FLAGS} -Wno-unused-but-set-variable)
    endif()

    set(THREADS_PREFER_PTHREAD_FLAG ON) # -pthreads on GNU-like compilers

elseif (MSVC)
    set(DOBIE_FLAGS /W4) # Warning level 4

endif()


# Externals
add_subdirectory(ext/libdeflate)

# Shared packages
find_package(Threads REQUIRED)

# Modules
add_subdirectory(src/core)
add_subdirectory(src/qt)


if (MSVC)
    # Use DobieQt as Startup Project instead of ALL_BUILD
    set_property(DIRECTORY ${CMAKE_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT DobieQt)
endif()

install(TARGETS DobieQt DESTINATION bin)
