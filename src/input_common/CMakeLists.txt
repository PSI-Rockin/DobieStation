set(TARGET InputCommon)

set(CMAKE_CXX_STANDARD 11)

set(SOURCES
   linux_input.cpp)

set(HEADERS
  linux_input.hpp
  )

if (CMAKE_ASM_MASM_COMPILER_LOADED)
    set_source_files_properties(SOURCES ${ASSEMBLY} PROPERTIES LANGUAGE ASM_MASM)
    set(SOURCES ${SOURCES} ${ASSEMBLY})
endif()

add_library(${TARGET} ${SOURCES} ${HEADERS})
add_library(Dobie::InputCommon ALIAS ${TARGET})

target_link_libraries(${TARGET} Ext::libdeflate Threads::Threads)
target_compile_options(${TARGET} PRIVATE ${DOBIE_FLAGS})

if (WIN32)
    # Needed to avoid ruining global scope with Windows.h on win32
    target_compile_definitions(${TARGET} PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
    if (MSVC)
        target_compile_definitions(${TARGET} PRIVATE VC_EXTRALEAN)
    endif()
endif()
