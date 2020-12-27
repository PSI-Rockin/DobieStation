QT -= gui

TEMPLATE = lib
CONFIG += staticlib c99

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

INCLUDEPATH += \
        ../../ext/zlib/include

SOURCES += \
    ../../ext/zlib/src/adler32.c \
    ../../ext/zlib/src/compress.c \
    ../../ext/zlib/src/crc32.c \
    ../../ext/zlib/src/deflate.c \
    ../../ext/zlib/src/gzclose.c \
    ../../ext/zlib/src/gzlib.c \
    ../../ext/zlib/src/gzread.c \
    ../../ext/zlib/src/gzwrite.c \
    ../../ext/zlib/src/infback.c \
    ../../ext/zlib/src/inffast.c \
    ../../ext/zlib/src/inflate.c \
    ../../ext/zlib/src/inftrees.c \
    ../../ext/zlib/src/trees.c \
    ../../ext/zlib/src/uncompr.c \
    ../../ext/zlib/src/zutil.c

HEADERS += \
    ../../ext/zlib/src/crc32.h \
    ../../ext/zlib/src/deflate.h \
    ../../ext/zlib/src/gzguts.h \
    ../../ext/zlib/src/inffast.h \
    ../../ext/zlib/src/inffixed.h \
    ../../ext/zlib/src/inflate.h \
    ../../ext/zlib/src/inftrees.h \
    ../../ext/zlib/src/trees.h \
    ../../ext/zlib/src/zutil.h

# Default rules for deployment.
unix {
    target.path = $$[QT_INSTALL_PLUGINS]/generic
}
!isEmpty(target.path): INSTALLS += target
