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

INCLUDEPATH += \
	../../ext/libdeflate \
	../../ext/libdeflate/common

HEADERS += \
# common headers
	../../ext/libdeflate/libdeflate.h \
	../../ext/libdeflate/common/common_defs.h \
	../../ext/libdeflate/common/compiler_gcc.h \
	../../ext/libdeflate/common/compiler_msc.h \
# library headers
	../../ext/libdeflate/lib/adler32_vec_template.h \
	../../ext/libdeflate/lib/aligned_malloc.h \
	../../ext/libdeflate/lib/bt_matchfinder.h \
	../../ext/libdeflate/lib/crc32_table.h \
	../../ext/libdeflate/lib/crc32_vec_template.h \
	../../ext/libdeflate/lib/decompress_template.h \
	../../ext/libdeflate/lib/deflate_compress.h \
	../../ext/libdeflate/lib/deflate_constants.h \
	../../ext/libdeflate/lib/gzip_constants.h \
	../../ext/libdeflate/lib/hc_matchfinder.h \
	../../ext/libdeflate/lib/lib_common.h \
	../../ext/libdeflate/lib/matchfinder_common.h \
	../../ext/libdeflate/lib/unaligned.h \
	../../ext/libdeflate/lib/zlib_constants.h \
	../../ext/libdeflate/lib/arm/adler32_impl.h \
	../../ext/libdeflate/lib/arm/cpu_features.h \
	../../ext/libdeflate/lib/arm/crc32_impl.h \
	../../ext/libdeflate/lib/arm/matchfinder_impl.h \
	../../ext/libdeflate/lib/x86/adler32_impl.h \
	../../ext/libdeflate/lib/x86/cpu_features.h \
	../../ext/libdeflate/lib/x86/crc32_impl.h \
	../../ext/libdeflate/lib/x86/crc32_pclmul_template.h \
	../../ext/libdeflate/lib/x86/decompress_impl.h \
	../../ext/libdeflate/lib/x86/matchfinder_impl.h

SOURCES += \
	../../ext/libdeflate/lib/aligned_malloc.c \
	../../ext/libdeflate/lib/deflate_decompress.c \
# uncomment for compression support
	#../../ext/libdeflate/lib/deflate_compress.c \
# uncomment for zlib format support
	#../../ext/libdeflate/lib/adler32.c \
	#../../ext/libdeflate/lib/zlib_decompress.c \
	#../../ext/libdeflate/lib/zlib_compress.c \
# uncomment for gzip support
	#../../ext/libdeflate/lib/gzip_decompress.c \
	#../../ext/libdeflate/lib/gzip_compress.c \
	../../ext/libdeflate/lib/arm/cpu_features.c \
	../../ext/libdeflate/lib/x86/cpu_features.c
