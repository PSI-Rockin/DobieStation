cmake_minimum_required(VERSION 3.1)
project(DobieStation)

if (NOT MVSC AND CMAKE_BUILD_TYPE MATCHES "Release")
    message(WARNING
        "CMake's Release flags include -O3, which causes Dobie to become "
        "a very sick puppy, you are advised to use RelWithDebInfo instead.\n"
        "Do not expect any builds made in this configuration to work.")
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
