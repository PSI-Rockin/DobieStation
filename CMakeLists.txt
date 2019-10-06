cmake_minimum_required(VERSION 3.1)
project(DobieStation)

add_subdirectory(ext/libdeflate)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

add_subdirectory(src/core)
add_subdirectory(src/qt)

install(TARGETS DobieQt DESTINATION bin)
