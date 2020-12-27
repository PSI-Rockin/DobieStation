CONFIG -= qt

TEMPLATE = lib
CONFIG += staticlib c99

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0
DEFINES += _7ZIP_ST

INCLUDEPATH += \
        ../../ext/lzma/include

SOURCES += \
    ../../ext/lzma/src/Alloc.c \
    ../../ext/lzma/src/Bra86.c \
    ../../ext/lzma/src/BraIA64.c \
    ../../ext/lzma/src/CpuArch.c \
    ../../ext/lzma/src/Delta.c \
    ../../ext/lzma/src/LzFind.c \
    ../../ext/lzma/src/Lzma86Dec.c \
    ../../ext/lzma/src/Lzma86Enc.c \
    ../../ext/lzma/src/LzmaDec.c \
    ../../ext/lzma/src/LzmaEnc.c \
    ../../ext/lzma/src/LzmaLib.c \
    ../../ext/lzma/src/Sort.c

HEADERS += \
    ../../ext/lzma/include/7zTypes.h \
    ../../ext/lzma/include/Alloc.h \
    ../../ext/lzma/include/Bra.h \
    ../../ext/lzma/include/Compiler.h \
    ../../ext/lzma/include/CpuArch.h \
    ../../ext/lzma/include/Delta.h \
    ../../ext/lzma/include/LzFind.h \
    ../../ext/lzma/include/LzHash.h \
    ../../ext/lzma/include/Lzma86.h \
    ../../ext/lzma/include/LzmaDec.h \
    ../../ext/lzma/include/LzmaEnc.h \
    ../../ext/lzma/include/LzmaLib.h \
    ../../ext/lzma/include/Precomp.h \
    ../../ext/lzma/include/Sort.h

# Default rules for deployment.
unix {
    target.path = /usr/lib
}
!isEmpty(target.path): INSTALLS += target
