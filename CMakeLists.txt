cmake_minimum_required(VERSION 3.1)
project(DobieStation)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake)
string(REPLACE "-O3" "-O2" CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE}") # fix Release build
set(THREADS_PREFER_PTHREAD_FLAG ON) # -pthreads on GNU-like compilers


# Externals
add_subdirectory(ext/zlib)
add_subdirectory(ext/lzma)
add_subdirectory(ext/libFLAC)
add_subdirectory(ext/libchdr)

# Shared packages
find_package(Threads REQUIRED)

# Modules
add_subdirectory(src/core)
add_subdirectory(src/qt)


if (MSVC)
    # Use DobieQt as Startup Project instead of ALL_BUILD
    set_property(DIRECTORY ${CMAKE_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT DobieQt)
endif()

if (UNIX AND NOT APPLE)
    configure_file(
        ${CMAKE_CURRENT_SOURCE_DIR}/data/DobieStation.desktop.in
        ${CMAKE_CURRENT_BINARY_DIR}/DobieStation.desktop)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/DobieStation.desktop DESTINATION share/applications/)
endif()
