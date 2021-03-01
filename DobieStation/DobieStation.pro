TEMPLATE = subdirs
SUBDIRS = application \
    libFLAC \
    libchdr \
    lzma \
    zlib

application.depends = zlib libchdr lzma libFLAC
