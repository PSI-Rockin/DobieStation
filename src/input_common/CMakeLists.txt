set(TARGET InputCommon)

set(CMAKE_CXX_STANDARD 14)

<<<<<<< HEAD
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
=======
find_package(Libevdev REQUIRED)

set(SOURCES
   linux_input.cpp
   input_api.cpp
   )

set(HEADERS  
  input_api.hpp
  common_input.hpp
  linux_input.hpp
  common_input.hpp
  linux_input.hpp
  )

if (CMAKE_ASM_MASM_COMPILER_LOADED)
    set_source_files_properties(SOURCES ${ASSEMBLY} PROPERTIES LANGUAGE ASM_MASM)
    set(SOURCES ${SOURCES} ${ASSEMBLY} ${Libevdev_INCLUDE_DIRS})
endif()

add_library(${TARGET} ${SOURCES} ${HEADERS} ${Libevdev_LIBRARIES})
add_library(Dobie::InputCommon ALIAS ${TARGET})

target_link_libraries(InputCommon ${Libevdev_LIBRARIES})

target_compile_options(${TARGET} PRIVATE ${DOBIE_FLAGS})
>>>>>>> ab3b997... Fixed several issues with windows. Namely added InputCommon to solution file. Removed code from the raw hpp and cpp files to ensure compilation Made several adjustments to Linux cmake files linking evdev and input common to DobieQT and InputCommon. Update CI recipe file. Update Readme to list current steps to use evdev in linux effectively

