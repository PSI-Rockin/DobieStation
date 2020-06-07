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
    bios.cpp
    emuthread.cpp
    emuwindow.cpp
    gamelistwidget.cpp
    main.cpp
    memcardwindow.cpp
    renderwidget.cpp
    settings.cpp
    settingswindow.cpp
    )

set(HEADERS
    bios.hpp
    emuthread.hpp
    emuwindow.hpp
    gamelistwidget.hpp
    memcardwindow.hpp
    renderwidget.hpp
    settings.hpp
    settingswindow.hpp
    )

set(UIS
    memcardwindow.ui
    )

add_executable(${TARGET} ${SOURCES} ${HEADERS})
set_target_properties(${TARGET} PROPERTIES OUTPUT_NAME "DobieStation") # Output as "DobieStation" instead of "DobieQt"

dobie_cxx_compile_options(${TARGET})
target_include_directories(${TARGET} PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${Qt5Gui_PRIVATE_INCLUDE_DIRS})
target_link_libraries(${TARGET} Dobie::Core Qt5::Core Qt5::Widgets Qt5::Gui)

install(TARGETS DobieQt RUNTIME DESTINATION bin)
