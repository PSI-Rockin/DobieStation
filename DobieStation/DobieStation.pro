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
    ../src/core/emotion.cpp \
    ../src/core/emulator.cpp \
    ../src/core/emotioninterpreter.cpp \
    ../src/core/cop0.cpp \
    ../src/core/cop1.cpp \
    ../src/core/emotion_mmi.cpp \
    ../src/core/bios_hle.cpp \
    ../src/core/vu0.cpp \
    ../src/core/emotion_special.cpp \
    ../src/core/gs.cpp \
    ../src/core/dmac.cpp \
    ../src/qt/emuwindow.cpp \
    ../src/core/gscontext.cpp \
    ../src/core/emotiondisasm.cpp

HEADERS += \
    ../src/core/emotion.hpp \
    ../src/core/emulator.hpp \
    ../src/core/emotioninterpreter.hpp \
    ../src/core/cop0.hpp \
    ../src/core/cop1.hpp \
    ../src/core/bios_hle.hpp \
    ../src/core/vu0.hpp \
    ../src/core/gs.hpp \
    ../src/core/dmac.hpp \
    ../src/qt/emuwindow.hpp \
    ../src/core/gscontext.hpp \
    ../src/core/emotiondisasm.hpp
