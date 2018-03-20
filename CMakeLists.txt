cmake_minimum_required(VERSION 3.1)
project(DobieStation)

set(CMAKE_CXX_STANDARD 11)

set(CMAKE_AUTOMOC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

find_package(Qt5Core REQUIRED)
find_package(Qt5Widgets REQUIRED)

set(SOURCES
        src/core/ee/bios_hle.cpp
        src/core/ee/cop0.cpp
        src/core/ee/cop1.cpp
        src/core/ee/dmac.cpp
        src/core/ee/emotion.cpp
        src/core/ee/emotion_fpu.cpp
        src/core/ee/emotion_mmi.cpp
        src/core/ee/emotion_special.cpp
        src/core/ee/emotionasm.cpp
        src/core/ee/emotiondisasm.cpp
        src/core/ee/emotioninterpreter.cpp
	src/core/iop/iop.cpp
	src/core/iop/iop_cop0.cpp
	src/core/iop/iop_dma.cpp
	src/core/iop/iop_interpreter.cpp
        src/core/emulator.cpp
        src/core/gif.cpp
        src/core/gs.cpp
        src/core/gscontext.cpp
	src/core/sif.cpp
        src/qt/emuwindow.cpp
        src/qt/main.cpp
        )

set(HEADERS
        src/core/ee/bios_hle.hpp
        src/core/ee/cop0.hpp
        src/core/ee/cop1.hpp
        src/core/ee/dmac.hpp
        src/core/ee/emotion.hpp
        src/core/ee/emotionasm.hpp
        src/core/ee/emotiondisasm.hpp
        src/core/ee/emotioninterpreter.hpp
        src/core/ee/emulator.hpp
	src/core/iop/iop.hpp
	src/core/iop/iop_cop0.hpp
	src/core/iop/iop_dma.hpp
	src/core/iop/iop_interpreter.hpp
        src/core/gif.hpp
        src/core/gs.hpp
	src/core/gscontext.hpp
	src/core/sif.hpp
        src/qt/emuwindow.hpp
        )

add_executable(DobieStation ${SOURCES} ${HEADERS})
target_link_libraries(DobieStation Qt5::Core Qt5::Widgets)
