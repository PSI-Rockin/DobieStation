TEMPLATE = subdirs
SUBDIRS = application libdeflate \
    libFLAC \
    libchdr \
    lzma \
    zlib

application.depends = libdeflate zlib libchdr lzma libFLAC
