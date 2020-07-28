QT += core gui multimedia
greaterThan(QT_MAJOR_VERSION, 4) : QT += widgets

TEMPLATE = app
TARGET = ../DobieStation
CONFIG += console c++14
CONFIG -= app_bundle

INCLUDEPATH += ../../ext/libdeflate
LIBS += -L../libdeflate
win32:LIBS += -llibdeflate
else:LIBS += -ldeflate

QMAKE_CFLAGS_RELEASE -= -O
QMAKE_CFLAGS_RELEASE -= -O1
QMAKE_CFLAGS_RELEASE *= -O2
QMAKE_CFLAGS_RELEASE -= -O3

DEFINES += QT_DEPRECATED_WARNINGS
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000

target.path = /usr/local/bin/
INSTALLS += target

SOURCES += ../../src/qt/main.cpp \
    ../../src/core/errors.cpp \
    ../../src/core/ee/emotion.cpp \
    ../../src/core/emulator.cpp \
    ../../src/core/ee/emotioninterpreter.cpp \
    ../../src/core/ee/cop0.cpp \
    ../../src/core/ee/cop1.cpp \
    ../../src/core/ee/emotion_mmi.cpp \
    ../../src/core/ee/bios_hle.cpp \
    ../../src/core/ee/emotion_special.cpp \
    ../../src/core/gs.cpp \
    ../../src/core/gsregisters.cpp \
    ../../src/core/gsthread.cpp \
    ../../src/core/ee/dmac.cpp \
    ../../src/qt/emuwindow.cpp \
    ../../src/core/gscontext.cpp \
    ../../src/core/ee/emotiondisasm.cpp \
    ../../src/core/ee/emotionasm.cpp \
    ../../src/core/ee/emotion_fpu.cpp \
    ../../src/core/gif.cpp \
    ../../src/core/iop/iop.cpp \
    ../../src/core/iop/iop_cop0.cpp \
    ../../src/core/iop/iop_interpreter.cpp \
    ../../src/core/sif.cpp \
    ../../src/core/iop/iop_dma.cpp \
    ../../src/core/ee/timers.cpp \
    ../../src/core/iop/iop_timers.cpp \
    ../../src/core/ee/intc.cpp \
    ../../src/core/iop/cdvd/cdvd.cpp \
    ../../src/core/iop/cdvd/cso_reader.cpp\
    ../../src/core/iop/sio2.cpp \
    ../../src/core/ee/vu.cpp \
    ../../src/core/ee/emotion_vu0.cpp \
    ../../src/core/iop/gamepad.cpp \
    ../../src/core/iop/spu.cpp \
    ../../src/qt/emuthread.cpp \
    ../../src/core/tests/iop/alu.cpp \
    ../../src/core/ee/vif.cpp \
    ../../src/core/ee/ipu/ipu.cpp \
    ../../src/core/ee/ipu/vlc_table.cpp \
    ../../src/core/ee/ipu/mac_addr_inc.cpp \
    ../../src/core/ee/ipu/mac_i_pic.cpp \
    ../../src/core/ee/ipu/ipu_fifo.cpp \
    ../../src/core/ee/ipu/lumtable.cpp \
    ../../src/core/ee/ipu/dct_coeff.cpp \
    ../../src/core/ee/ipu/dct_coeff_table0.cpp \
    ../../src/core/ee/ipu/dct_coeff_table1.cpp \
    ../../src/core/ee/ipu/chromtable.cpp \
    ../../src/core/ee/ipu/mac_p_pic.cpp \
    ../../src/core/ee/ipu/motioncode.cpp \
    ../../src/core/ee/ipu/mac_b_pic.cpp \
    ../../src/core/ee/ipu/codedblockpattern.cpp \
    ../../src/core/ee/vu_interpreter.cpp \
    ../../src/core/ee/vu_disasm.cpp \
    ../../src/core/gsmem.cpp \
    ../../src/core/serialize.cpp \
    ../../src/core/iop/memcard.cpp \
    ../../src/qt/settings.cpp \
    ../../src/core/jitcommon/jitcache.cpp \
    ../../src/core/jitcommon/emitter64.cpp \
    ../../src/core/ee/vu_jittrans.cpp \
    ../../src/core/jitcommon/ir_block.cpp \
    ../../src/core/jitcommon/ir_instr.cpp \
    ../../src/core/ee/vu_jit.cpp \
    ../../src/core/ee/vu_jit64.cpp \
    ../../src/core/scheduler.cpp \
    ../../src/qt/renderwidget.cpp \
    ../../src/qt/settingswindow.cpp \
    ../../src/qt/bios.cpp \
    ../../src/qt/gamelistwidget.cpp \
    ../../src/core/ee/ee_jittrans.cpp \
    ../../src/core/ee/ee_jit.cpp \
    ../../src/core/ee/ee_jit64.cpp \
    ../../src/core/ee/ee_jit64_mmi.cpp \
    ../../src/core/ee/ee_jit64_cop2.cpp \
    ../../src/core/ee/ee_jit64_fpu.cpp \
    ../../src/core/ee/ee_jit64_fpu_avx.cpp \
    ../../src/core/ee/ee_jit64_gpr.cpp \
    ../../src/core/iop/cdvd/iso_reader.cpp \
    ../../src/core/iop/cdvd/bincuereader.cpp \
    ../../src/core/iop/iop_intc.cpp

HEADERS += \
    ../../src/core/errors.hpp \
    ../../src/core/ee/emotion.hpp \
    ../../src/core/emulator.hpp \
    ../../src/core/ee/emotioninterpreter.hpp \
    ../../src/core/ee/cop0.hpp \
    ../../src/core/ee/cop1.hpp \
    ../../src/core/ee/bios_hle.hpp \
    ../../src/core/gs.hpp \
    ../../src/core/circularFIFO.hpp \
    ../../src/core/gsthread.hpp \
    ../../src/core/gsregisters.hpp \
    ../../src/core/ee/dmac.hpp \
    ../../src/qt/emuwindow.hpp \
    ../../src/core/gscontext.hpp \
    ../../src/core/ee/emotiondisasm.hpp \
    ../../src/core/ee/emotionasm.hpp \
    ../../src/core/gif.hpp \
    ../../src/core/iop/iop.hpp \
    ../../src/core/iop/iop_cop0.hpp \
    ../../src/core/iop/iop_interpreter.hpp \
    ../../src/core/sif.hpp \
    ../../src/core/iop/iop_dma.hpp \
    ../../src/core/ee/timers.hpp \
    ../../src/core/iop/iop_timers.hpp \
    ../../src/core/ee/intc.hpp \
    ../../src/core/iop/cdvd/cdvd.hpp \
    ../../src/core/iop/cdvd/cso_reader.hpp\
    ../../src/core/iop/sio2.hpp \
    ../../src/core/ee/vu.hpp \
    ../../src/core/iop/gamepad.hpp \
    ../../src/core/iop/spu.hpp \
    ../../src/qt/emuthread.hpp \
    ../../src/core/ee/vif.hpp \
    ../../src/core/int128.hpp \
    ../../src/core/ee/ipu/ipu.hpp \
    ../../src/core/ee/ipu/vlc_table.hpp \
    ../../src/core/ee/ipu/mac_addr_inc.hpp \
    ../../src/core/ee/ipu/mac_i_pic.hpp \
    ../../src/core/ee/ipu/ipu_fifo.hpp \
    ../../src/core/ee/ipu/lumtable.hpp \
    ../../src/core/ee/ipu/dct_coeff.hpp \
    ../../src/core/ee/ipu/dct_coeff_table0.hpp \
    ../../src/core/ee/ipu/dct_coeff_table1.hpp \
    ../../src/core/ee/ipu/chromtable.hpp \
    ../../src/core/ee/ipu/mac_p_pic.hpp \
    ../../src/core/ee/ipu/motioncode.hpp \
    ../../src/core/ee/ipu/mac_b_pic.hpp \
    ../../src/core/ee/ipu/codedblockpattern.hpp \
    ../../src/core/ee/vu_interpreter.hpp \
    ../../src/core/ee/vu_disasm.hpp \
    ../../src/core/gsmem.hpp \
    ../../src/core/iop/memcard.hpp \
    ../../src/qt/settings.hpp \
    ../../src/core/jitcommon/jitcache.hpp \
    ../../src/core/jitcommon/emitter64.hpp \
    ../../src/core/ee/vu_jittrans.hpp \
    ../../src/core/jitcommon/ir_block.hpp \
    ../../src/core/jitcommon/ir_instr.hpp \
    ../../src/core/ee/vu_jit.hpp \
    ../../src/core/ee/vu_jit64.hpp \
    ../../src/core/scheduler.hpp \
    ../../src/qt/renderwidget.hpp \
    ../../src/qt/settingswindow.hpp \
    ../../src/qt/bios.hpp \
    ../../src/qt/gamelistwidget.hpp \
    ../../src/core/ee/ee_jittrans.hpp \
    ../../src/core/ee/ee_jit.hpp \
    ../../src/core/ee/ee_jit64.hpp \
    ../../src/core/iop/cdvd/cdvd_container.hpp \
    ../../src/core/iop/cdvd/iso_reader.hpp \
    ../../src/core/iop/cdvd/bincuereader.hpp \
    ../../src/core/iop/iop_intc.hpp
   
