set(TARGET InputCommon)

set(CMAKE_CXX_STANDARD 14)

find_package(Libevdev REQUIRED)

set(SOURCES
   linux_input.cpp
   input_api.cpp
   )

set(HEADERS  
  input_api.hpp
  common_input.hpp
  linux_input.hpp
  common_input.hpp
  linux_input.hpp
  )

if (CMAKE_ASM_MASM_COMPILER_LOADED)
    set_source_files_properties(SOURCES ${ASSEMBLY} PROPERTIES LANGUAGE ASM_MASM)
    set(SOURCES ${SOURCES} ${ASSEMBLY} ${Libevdev_INCLUDE_DIRS})
endif()

add_library(${TARGET} ${SOURCES} ${HEADERS} ${Libevdev_LIBRARIES})
add_library(Dobie::InputCommon ALIAS ${TARGET})

target_link_libraries(InputCommon ${Libevdev_LIBRARIES})

target_compile_options(${TARGET} PRIVATE ${DOBIE_FLAGS})

if (WIN32)
    # Needed to avoid ruining global scope with Windows.h on win32
    target_compile_definitions(${TARGET} PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
    if (MSVC)
        target_compile_definitions(${TARGET} PRIVATE VC_EXTRALEAN)
    endif()
endif()
