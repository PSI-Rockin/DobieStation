project(libdeflate C)
set(TARGET libdeflate)
set(CMAKE_C_STANDARD 99)

if (${CMAKE_C_COMPILER_ID} STREQUAL "GNU" OR
    ${CMAKE_C_COMPILER_ID} STREQUAL "Clang" OR
    ${CMAKE_C_COMPILER_ID} STREQUAL "AppleClang")

    set(FLAGS ${FLAGS} -fomit-frame-pointer)
    set(FLAGS ${FLAGS} -Wall -Wundef)
    set(FLAGS ${FLAGS} -Wpedantic -Wdeclaration-after-statement -Wmissing-prototypes -Wstrict-prototypes -Wvla)
    set(FLAGS ${FLAGS} -fvisibility=hidden -D_ANSI_SOURCE)

    if (MINGW)
        set(FLAGS ${FLAGS} -Wno-pedantic-ms-format)
    endif()
endif()

set(COMMON_HEADERS
    libdeflate.h

    common/common_defs.h
    common/compiler_gcc.h
    common/compiler_msc.h)

set(LIB_HEADERS
    lib/adler32_vec_template.h
    lib/aligned_malloc.h
    lib/bt_matchfinder.h
    lib/crc32_table.h
    lib/crc32_vec_template.h
    lib/decompress_template.h
    lib/deflate_compress.h
    lib/deflate_constants.h
    lib/gzip_constants.h
    lib/hc_matchfinder.h
    lib/lib_common.h
    lib/matchfinder_common.h
    lib/unaligned.h
    lib/zlib_constants.h

    lib/arm/adler32_impl.h
    lib/arm/cpu_features.h
    lib/arm/crc32_impl.h
    lib/arm/matchfinder_impl.h

    lib/x86/adler32_impl.h
    lib/x86/cpu_features.h
    lib/x86/crc32_impl.h
    lib/x86/crc32_pclmul_template.h
    lib/x86/decompress_impl.h
    lib/x86/matchfinder_impl.h)

set(LIB_SRC
    lib/aligned_malloc.c
    lib/deflate_decompress.c

    # uncomment for compression support
    #lib/deflate_compress.c

    # uncomment for zlib format support
    #lib/adler32.c
    #lib/zlib_decompress.c
    #lib/zlib_compress.c

    # uncomment for gzip support
    #lib/gzip_decompress.c
    #lib/gzip_compress.c

    lib/arm/cpu_features.c
    lib/x86/cpu_features.c)

add_library(${TARGET} STATIC ${LIB_SRC} ${LIB_HEADERS} ${COMMON_HEADERS})
add_library(Ext::libdeflate ALIAS ${TARGET})
set_target_properties(${TARGET} PROPERTIES PREFIX "")
set_property(TARGET ${TARGET} PROPERTY FOLDER External)

target_include_directories(${TARGET}
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/common)

if (FLAGS)
    target_compile_options(${TARGET} PRIVATE ${FLAGS})
endif()
