CONFIG -= qt

TEMPLATE = lib
CONFIG += staticlib

CONFIG += c99

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

DEFINES += PACKAGE_VERSION=\\\"1.3.2\\\" FLAC__NO_DLL FLAC__HAS_X86INTRIN=1 FLAC__HAS_OGG=0 HAVE_STDINT_H HAVE_STDLIB_H HAVE_LROUND

INCLUDEPATH += \
        ../../ext/libFLAC/include \
        ../../ext/libFLAC/src/include


SOURCES += \
    ../../ext/libFLAC/src/bitmath.c \
    ../../ext/libFLAC/src/bitreader.c \
    ../../ext/libFLAC/src/cpu.c \
    ../../ext/libFLAC/src/crc.c \
    ../../ext/libFLAC/src/fixed.c \
    ../../ext/libFLAC/src/fixed_intrin_sse2.c \
    ../../ext/libFLAC/src/fixed_intrin_ssse3.c \
    ../../ext/libFLAC/src/float.c \
    ../../ext/libFLAC/src/format.c \
    ../../ext/libFLAC/src/lpc.c \
    ../../ext/libFLAC/src/lpc_intrin_avx2.c \
    ../../ext/libFLAC/src/lpc_intrin_sse.c \
    ../../ext/libFLAC/src/lpc_intrin_sse2.c \
    ../../ext/libFLAC/src/lpc_intrin_sse41.c \
    ../../ext/libFLAC/src/md5.c \
    ../../ext/libFLAC/src/memory.c \
    ../../ext/libFLAC/src/metadata_iterators.c \
    ../../ext/libFLAC/src/metadata_object.c \
    ../../ext/libFLAC/src/stream_decoder.c

HEADERS += \
    ../../ext/libFLAC/include/FLAC/all.h \
    ../../ext/libFLAC/include/FLAC/assert.h \
    ../../ext/libFLAC/include/FLAC/callback.h \
    ../../ext/libFLAC/include/FLAC/export.h \
    ../../ext/libFLAC/include/FLAC/format.h \
    ../../ext/libFLAC/include/FLAC/metadata.h \
    ../../ext/libFLAC/include/FLAC/ordinals.h \
    ../../ext/libFLAC/include/FLAC/stream_decoder.h \
    ../../ext/libFLAC/include/FLAC/stream_encoder.h \
    ../../ext/libFLAC/src/include/private/all.h \
    ../../ext/libFLAC/src/include/private/bitmath.h \
    ../../ext/libFLAC/src/include/private/bitreader.h \
    ../../ext/libFLAC/src/include/private/cpu.h \
    ../../ext/libFLAC/src/include/private/crc.h \
    ../../ext/libFLAC/src/include/private/fixed.h \
    ../../ext/libFLAC/src/include/private/float.h \
    ../../ext/libFLAC/src/include/private/format.h \
    ../../ext/libFLAC/src/include/private/lpc.h \
    ../../ext/libFLAC/src/include/private/macros.h \
    ../../ext/libFLAC/src/include/private/md5.h \
    ../../ext/libFLAC/src/include/private/memory.h \
    ../../ext/libFLAC/src/include/private/metadata.h \
    ../../ext/libFLAC/src/include/private/window.h \
    ../../ext/libFLAC/src/include/protected/stream_decoder.h \
    ../../ext/libFLAC/src/include/share/alloc.h \
    ../../ext/libFLAC/src/include/share/compat.h \
    ../../ext/libFLAC/src/include/share/endswap.h \
    ../../ext/libFLAC/src/include/share/macros.h \
    ../../ext/libFLAC/src/include/share/safe_str.h \
    ../../ext/libFLAC/src/include/share/win_utf8_io.h \
    ../../ext/libFLAC/src/include/share/windows_unicode_filenames.h

# Default rules for deployment.
unix {
    target.path = $$[QT_INSTALL_PLUGINS]/generic
}
!isEmpty(target.path): INSTALLS += target
