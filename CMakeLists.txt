cmake_minimum_required(VERSION 3.1)
project(DobieStation)

if (${CMAKE_C_COMPILER_ID} STREQUAL "GNU" OR
    ${CMAKE_C_COMPILER_ID} STREQUAL "Clang")

    if (CMAKE_BUILD_TYPE MATCHES "Release")
        message(WARNING
            "CMake's Release flags include -O3, which causes Dobie to become "
            "a very sick puppy, you are advised to use RelWithDebInfo instead.\n"
            "Do not expect any builds made in this configuration to work.")
    endif()

    set(DOBIE_FLAGS
        -Wall -Wundef -Wsign-compare -Wconversion -Wstrict-aliasing -Wtype-limits

        # These probably should be fixed instead of disabled,
        # but doing so to keep the warning count more managable for now.
        -Wno-reorder -Wno-unused-variable -Wno-unused-value -Wno-unused-but-set-variable

        # Might be useful for debugging:
        #-fomit-frame-pointer -fwrapv -fno-delete-null-pointer-checks -fno-strict-aliasing -fvisibility=hidden
    )

elseif (MSVC)
    set(DOBIE_FLAGS /W4)

endif()

add_subdirectory(ext/libdeflate)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

add_subdirectory(src/core)
add_subdirectory(src/qt)

if (MSVC)
    set_property(DIRECTORY ${CMAKE_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT DobieQt)
endif()

install(TARGETS DobieQt DESTINATION bin)
