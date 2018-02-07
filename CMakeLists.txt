cmake_minimum_required(VERSION 3.1)
project(DobieStation)

set(CMAKE_CXX_STANDARD 11)

set(CMAKE_AUTOMOC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

find_package(Qt5Core REQUIRED)
find_package(Qt5Widgets REQUIRED)

set(SOURCES
        src/core/bios_hle.cpp
        src/core/cop0.cpp
        src/core/cop1.cpp
        src/core/dmac.cpp
        src/core/emotion.cpp
        src/core/emotion_mmi.cpp
        src/core/emotion_special.cpp
        src/core/emotionasm.cpp
        src/core/emotiondisasm.cpp
        src/core/emotioninterpreter.cpp
        src/core/emulator.cpp
        src/core/gs.cpp
        src/core/gscontext.cpp
        src/core/vu0.cpp
        src/qt/emuwindow.cpp
        src/qt/main.cpp
        )

set(HEADERS
        src/core/bios_hle.hpp
        src/core/cop0.hpp
        src/core/cop1.hpp
        src/core/dmac.hpp
        src/core/emotion.hpp
        src/core/emotionasm.hpp
        src/core/emotiondisasm.hpp
        src/core/emotioninterpreter.hpp
        src/core/emulator.hpp
        src/core/gs.hpp
        src/core/gscontext.hpp
        src/core/vu0.hpp
        src/qt/emuwindow.hpp
        )

add_executable(DobieStation ${SOURCES} ${HEADERS})
target_link_libraries(DobieStation Qt5::Core Qt5::Widgets)
