set(TARGET DobieQt)

set(CMAKE_CXX_STANDARD 14)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

if(CMAKE_VERSION VERSION_LESS "3.7.0")
    set(CMAKE_INCLUDE_CURRENT_DIR ON)
endif()

find_package(Qt5 COMPONENTS Core Widgets Gui REQUIRED)


set(SOURCES
    emuthread.cpp
    emuwindow.cpp
    settingswindow.cpp
    pad_configure.cpp
    renderwidget.cpp
    gamelistwidget.cpp
    memcardwindow.cpp
    main.cpp
    settings.cpp
    bios.cpp)

set(HEADERS
    emuthread.hpp
    emuwindow.hpp
    settingswindow.hpp
    pad_configure.hpp
    renderwidget.hpp
    gamelistwidget.hpp
    memcardwindow.hpp
    settings.hpp
    bios.hpp)

set(UIS
    memcardwindow.ui
)

add_executable(${TARGET} ${SOURCES} ${HEADERS})
set_target_properties(${TARGET} PROPERTIES OUTPUT_NAME "DobieStation") # Output as "DobieStation" instead of "DobieQt"

dobie_cxx_compile_options(${TARGET})
target_include_directories(${TARGET} PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${Qt5Gui_PRIVATE_INCLUDE_DIRS})
target_link_libraries(${TARGET} Dobie::Core InputCommon Qt5::Core Qt5::Widgets Qt5::Gui)

install(TARGETS DobieQt RUNTIME DESTINATION bin)
