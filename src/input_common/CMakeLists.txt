
set(TARGET InputCommon)

set(CMAKE_CXX_STANDARD 14)

# Common
set(SOURCES_COMMON
    input_api.cpp)

set(HEADERS_COMMON
    input_api.hpp
    common_input.hpp)

# Linux
set(SOURCES_LINUX
    linux_input.cpp)

set(HEADERS_LINUX
    linux_input.hpp)

# MacOS
set(SOURCES_MACOS
    mac_input.cpp)


set(HEADERS_MACOS
    mac_input.hpp)


add_library(${TARGET} ${SOURCES_COMMON} ${HEADERS_COMMON}
    $<$<PLATFORM_ID:Linux>:${SOURCES_LINUX} ${HEADERS_LINUX}>
    $<$<PLATFORM_ID:Darwin>:${SOURCES_MACOS} ${HEADERS_MACOS}>)
add_library(Dobie::InputCommon ALIAS ${TARGET})

if (UNIX AND NOT APPLE)
    find_package(Libevdev REQUIRED)    
    target_link_libraries(InputCommon Libevdev::Libevdev)
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -O0")
    set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -O0")
endif (UNIX AND NOT APPLE)


