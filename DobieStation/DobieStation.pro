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

SOURCES += main.cpp \
    emotion.cpp \
    emulator.cpp \
    emotioninterpreter.cpp \
    cop0.cpp \
    cop1.cpp \
    emotion_mmi.cpp \
    bios_hle.cpp \
    vu0.cpp \
    emotion_special.cpp \
    gs.cpp \
    dmac.cpp \
    emuwindow.cpp \
    gscontext.cpp

HEADERS += \
    emotion.hpp \
    emulator.hpp \
    emotioninterpreter.hpp \
    cop0.hpp \
    cop1.hpp \
    bios_hle.hpp \
    vu0.hpp \
    gs.hpp \
    dmac.hpp \
    emuwindow.hpp \
    gscontext.hpp
