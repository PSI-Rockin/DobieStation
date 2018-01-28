TEMPLATE = app
CONFIG += console c++11
CONFIG -= app_bundle
CONFIG -= qt

QMAKE_CFLAGS_RELEASE -= -O
QMAKE_CFLAGS_RELEASE -= -O1
QMAKE_CFLAGS_RELEASE *= -O2
QMAKE_CFLAGS_RELEASE -= -O3

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
    dmac.cpp

HEADERS += \
    emotion.hpp \
    emulator.hpp \
    emotioninterpreter.hpp \
    cop0.hpp \
    cop1.hpp \
    bios_hle.hpp \
    vu0.hpp \
    gs.hpp \
    dmac.hpp
