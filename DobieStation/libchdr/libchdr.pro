CONFIG -= qt

TEMPLATE = lib
CONFIG += staticlib

CONFIG += c99
# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

INCLUDEPATH += ../../ext/zlib/include \
               ../../ext/lzma/include \
               ../../ext/libFLAC/include

INCLUDEPATH += \
        ../../ext/libchdr/include

SOURCES += \
    ../../ext/libchdr/src/libchdr_bitstream.c \
    ../../ext/libchdr/src/libchdr_cdrom.c \
    ../../ext/libchdr/src/libchdr_chd.c \
    ../../ext/libchdr/src/libchdr_flac.c \
    ../../ext/libchdr/src/libchdr_huffman.c

HEADERS += \
    ../../ext/libchdr/include/libchdr/bitstream.h \
    ../../ext/libchdr/include/libchdr/cdrom.h \
    ../../ext/libchdr/include/libchdr/chd.h \
    ../../ext/libchdr/include/libchdr/chdconfig.h \
    ../../ext/libchdr/include/libchdr/coretypes.h \
    ../../ext/libchdr/include/libchdr/flac.h \
    ../../ext/libchdr/include/libchdr/huffman.h

# Default rules for deployment.
unix {
    target.path = $$[QT_INSTALL_PLUGINS]/generic
}
!isEmpty(target.path): INSTALLS += target
