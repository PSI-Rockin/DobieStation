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
	src/core/ee/emotion_vu0.cpp
	src/core/ee/intc.cpp
	src/core/ee/ipu/chromtable.cpp
	src/core/ee/ipu/dct_coeff.cpp
	src/core/ee/ipu/dct_coeff_table0.cpp
	src/core/ee/ipu/dct_coeff_table1.cpp
	src/core/ee/ipu/ipu.cpp
	src/core/ee/ipu/ipu_fifo.cpp
	src/core/ee/ipu/lumtable.cpp
	src/core/ee/ipu/mac_addr_inc.cpp
	src/core/ee/ipu/mac_i_pic.cpp
	src/core/ee/ipu/mac_p_pic.cpp
	src/core/ee/ipu/motioncode.cpp
	src/core/ee/ipu/vlc_table.cpp
	src/core/ee/timers.cpp
	src/core/ee/vif.cpp
	src/core/ee/vu.cpp
	src/core/iop/cdvd.cpp
	src/core/iop/gamepad.cpp
	src/core/iop/iop.cpp
	src/core/iop/iop_cop0.cpp
	src/core/iop/iop_dma.cpp
	src/core/iop/iop_interpreter.cpp
	src/core/iop/iop_timers.cpp
	src/core/iop/sio2.cpp
	src/core/iop/spu.cpp
	src/core/tests/iop/alu.cpp
        src/core/emulator.cpp
        src/core/gif.cpp
        src/core/gs.cpp
        src/core/gscontext.cpp
	src/core/sif.cpp
	src/qt/emuthread.cpp
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
	src/core/ee/intc.hpp
	src/core/ee/ipu/chromtable.hpp
	src/core/ee/ipu/dct_coeff.hpp
	src/core/ee/ipu/dct_coeff_table0.hpp
	src/core/ee/ipu/dct_coeff_table1.hpp
	src/core/ee/ipu/ipu.hpp
	src/core/ee/ipu/ipu_fifo.hpp
	src/core/ee/ipu/lumtable.hpp
	src/core/ee/ipu/mac_addr_inc.hpp
	src/core/ee/ipu/mac_i_pic.hpp
	src/core/ee/ipu/mac_p_pic.hpp
	src/core/ee/ipu/motioncode.hpp
	src/core/ee/ipu/vlc_table.hpp
	src/core/ee/timers.hpp
	src/core/ee/vif.hpp
	src/core/ee/vu.hpp
	src/core/iop/cdvd.hpp
	src/core/iop/gamepad.hpp
	src/core/iop/iop.hpp
	src/core/iop/iop_cop0.hpp
	src/core/iop/iop_dma.hpp
	src/core/iop/iop_interpreter.hpp
	src/core/iop/iop_timers.hpp
	src/core/iop/sio2.hpp
	src/core/iop/spu.hpp
	src/core/emulator.hpp
        src/core/gif.hpp
        src/core/gs.hpp
	src/core/gscontext.hpp
	src/core/int128.hpp
	src/core/sif.hpp
	src/qt/emuthread.hpp
        src/qt/emuwindow.hpp
        )

add_executable(DobieStation ${SOURCES} ${HEADERS})
target_link_libraries(DobieStation Qt5::Core Qt5::Widgets)
