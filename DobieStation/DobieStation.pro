QT += core gui multimedia
greaterThan(QT_MAJOR_VERSION, 4) : QT += widgets

TEMPLATE = app
CONFIG += console c++11
CONFIG -= app_bundle

QMAKE_CFLAGS_RELEASE -= -O
QMAKE_CFLAGS_RELEASE -= -O1
QMAKE_CFLAGS_RELEASE *= -O2
QMAKE_CFLAGS_RELEASE -= -O3

DEFINES += QT_DEPRECATED_WARNINGS
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000

SOURCES += ../src/qt/main.cpp \
    ../src/core/ee/emotion.cpp \
    ../src/core/emulator.cpp \
    ../src/core/ee/emotioninterpreter.cpp \
    ../src/core/ee/cop0.cpp \
    ../src/core/ee/cop1.cpp \
    ../src/core/ee/emotion_mmi.cpp \
    ../src/core/ee/bios_hle.cpp \
    ../src/core/ee/emotion_special.cpp \
    ../src/core/gs.cpp \
    ../src/core/ee/dmac.cpp \
    ../src/qt/emuwindow.cpp \
    ../src/core/gscontext.cpp \
    ../src/core/ee/emotiondisasm.cpp \
    ../src/core/ee/emotionasm.cpp \
    ../src/core/ee/emotion_fpu.cpp \
    ../src/core/gif.cpp \
    ../src/core/iop/iop.cpp \
    ../src/core/iop/iop_cop0.cpp \
    ../src/core/iop/iop_interpreter.cpp \
    ../src/core/sif.cpp \
    ../src/core/iop/iop_dma.cpp \
    ../src/core/ee/timers.cpp \
    ../src/core/iop/iop_timers.cpp \
    ../src/core/ee/intc.cpp \
    ../src/core/iop/cdvd.cpp \
    ../src/core/iop/sio2.cpp

HEADERS += \
    ../src/core/ee/emotion.hpp \
    ../src/core/emulator.hpp \
    ../src/core/ee/emotioninterpreter.hpp \
    ../src/core/ee/cop0.hpp \
    ../src/core/ee/cop1.hpp \
    ../src/core/ee/bios_hle.hpp \
    ../src/core/gs.hpp \
    ../src/core/ee/dmac.hpp \
    ../src/qt/emuwindow.hpp \
    ../src/core/gscontext.hpp \
    ../src/core/ee/emotiondisasm.hpp \
    ../src/core/ee/emotionasm.hpp \
    ../src/core/gif.hpp \
    ../src/core/iop/iop.hpp \
    ../src/core/iop/iop_cop0.hpp \
    ../src/core/iop/iop_interpreter.hpp \
    ../src/core/sif.hpp \
    ../src/core/iop/iop_dma.hpp \
    ../src/core/ee/timers.hpp \
    ../src/core/iop/iop_timers.hpp \
    ../src/core/ee/intc.hpp \
    ../src/core/iop/cdvd.hpp \
    ../src/core/iop/sio2.hpp
