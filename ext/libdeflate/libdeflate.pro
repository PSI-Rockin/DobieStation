win32:TARGET = libdeflate
else:TARGET = deflate

TEMPLATE = lib
CONFIG += staticlib c99

win32-msvc: QMAKE_CFLAGS += /MD /O2
else {
	QMAKE_CFLAGS += -O2 \
		-fomit-frame-pointer \
		-Wall -Wundef \
		-Wpedantic -Wdeclaration-after-statement -Wmissing-prototypes -Wstrict-prototypes -Wvla \
		-fvisibility=hidden -D_ANSI_SOURCE

	mingw: QMAKE_CFLAGS += -Wno-pedantic-ms-format
}

INCLUDEPATH += . common

HEADERS += \
# common headers
	libdeflate.h \
	common/common_defs.h \
	common/compiler_gcc.h \
	common/compiler_msc.h \
# library headers
	lib/adler32_vec_template.h \
	lib/aligned_malloc.h \
	lib/bt_matchfinder.h \
	lib/crc32_table.h \
	lib/crc32_vec_template.h \
	lib/decompress_template.h \
	lib/deflate_compress.h \
	lib/deflate_constants.h \
	lib/gzip_constants.h \
	lib/hc_matchfinder.h \
	lib/lib_common.h \
	lib/matchfinder_common.h \
	lib/unaligned.h \
	lib/zlib_constants.h \
	lib/arm/adler32_impl.h \
	lib/arm/cpu_features.h \
	lib/arm/crc32_impl.h \
	lib/arm/matchfinder_impl.h \
	lib/x86/adler32_impl.h \
	lib/x86/cpu_features.h \
	lib/x86/crc32_impl.h \
	lib/x86/crc32_pclmul_template.h \
	lib/x86/decompress_impl.h \
	lib/x86/matchfinder_impl.h

SOURCES += \
	lib/aligned_malloc.c \
	lib/deflate_decompress.c \
# uncomment for compression support
	#lib/deflate_compress.c \
# uncomment for zlib format support
	#lib/adler32.c \
	#lib/zlib_decompress.c \
	#lib/zlib_compress.c \
# uncomment for gzip support
	#lib/gzip_decompress.c \
	#lib/gzip_compress.c \
	lib/arm/cpu_features.c \
	lib/x86/cpu_features.c
