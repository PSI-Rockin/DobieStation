cmake_minimum_required(VERSION 2.8.11)
project(DobieStation_master)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC ON)
find_package(Qt5Core REQUIRED)
find_package(Qt5Widgets REQUIRED)

set(SOURCES src/qt/main.cpp
            src/core/emotion.cpp
            src/core/emulator.cpp
            src/core/emotioninterpreter.cpp
            src/core/cop0.cpp
            src/core/cop1.cpp
            src/core/emotion_mmi.cpp
            src/core/bios_hle.cpp
            src/core/vu0.cpp
            src/core/emotion_special.cpp
            src/core/gs.cpp
            src/core/dmac.cpp
            src/qt/emuwindow.cpp
            src/core/gscontext.cpp
            src/core/emotiondisasm.cpp)

set(HEADERS src/core/emotion.hpp
            src/core/emulator.hpp
            src/core/emotioninterpreter.hpp
            src/core/cop0.hpp
            src/core/cop1.hpp
            src/core/bios_hle.hpp
            src/core/vu0.hpp
            src/core/gs.hpp
            src/core/dmac.hpp
            src/qt/emuwindow.hpp
            src/core/gscontext.hpp
            src/core/emotiondisasm.hpp)

add_executable(DobieStation_master ${SOURCES} ${HEADERS})

target_link_libraries(DobieStation_master Qt5::Core Qt5::Widgets)