#[=======================================================================[.rst:
FindLibevdev
------------

Search for libevdev library and headers in the following places:

#. pkg-config when available.
#. System header & library paths.

Imported Targets
^^^^^^^^^^^^^^^^

This module provides the following imported targets, if found:

``Libevdev::Libevdev``
  Target for the libevdev library.

Result Variables
^^^^^^^^^^^^^^^^

This module defines the following variables:

``Libevdev_FOUND``
  True if libevdev was found on the system.

``Libevdev_VERSION``
  Version of libevdev found,
  currently only detected with pkg-config and thus
  shouldn't be relied upon for maximum portability.

``Libevdev_LIBRARIES``
  Libraries to link against to use libevdev.

``Libevdev_INCLUDE_DIRS``
  Directories to include to use libevdev.

``Libevdev_DEFINITIONS``
  Compiler definitions if specified in pkg-config.

Cache Variables
^^^^^^^^^^^^^^^

The following cache variables may also be set,
these are transitory and should not be relied upon:

``Libevdev_INCLUDE_DIR``
  Directory containing ``libevdev/libevdev.h``.
  (use ``Libevdev_INCLUDE_DIRS`` in your CMakeLists.txt)

``Libevdev_LIBRARY``
  Path to the libevdev library.
  (use ``Libevdev_LIBRARIES`` in your CMakeLists.txt)

]=======================================================================]


# try pkg-config if available
find_package(PkgConfig)
pkg_check_modules(_Libevdev_PC QUIET libevdev)


# find header & library files
find_path(Libevdev_INCLUDE_DIR
    NAMES libevdev/libevdev.h
    PATHS ${_Libevdev_PC_INCLUDE_DIRS}
    PATH_SUFFIXES libevdev-1.0)

find_library(Libevdev_LIBRARY
    NAMES evdev libevdev
    PATHS ${_Libevdev_PC_LIBRARY_DIR})

mark_as_advanced(Libevdev_INCLUDE_DIR Libevdev_LIBRARY)


# not sure how to reliably get the libevdev version without pkg-config
# so only the pkg-config version is used for now
set(Libevdev_VERSION "${_Libevdev_PC_VERSION}")
mark_as_advanced(Libevdev_VERSION)


# handle find_package arguments
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Libevdev
    FOUND_VAR Libevdev_FOUND
    REQUIRED_VARS
        Libevdev_LIBRARY
        Libevdev_INCLUDE_DIR
    VERSION_VAR Libevdev_VERSION)

mark_as_advanced(Libevdev_FOUND)


if (Libevdev_FOUND)
    # create the target
    if (NOT TARGET Libevdev::Libevdev)
        add_library(Libevdev::Libevdev UNKNOWN IMPORTED)
        set_target_properties(Libevdev::Libevdev PROPERTIES
            IMPORTED_LOCATION             "${Libevdev_LIBRARY}"
            INTERFACE_COMPILE_OPTIONS     "${SDL2_PC_CFLAGS_OTHER}"
            INTERFACE_INCLUDE_DIRECTORIES "${Libevdev_INCLUDE_DIR}")
    endif()


    # set traditional library variables
    set(Libevdev_LIBRARIES    "${Libevdev_LIBRARY}")
    set(Libevdev_INCLUDE_DIRS "${Libevdev_INCLUDE_DIR}")
    set(Libevdev_DEFINITIONS  "${SDL2_PC_CFLAGS_OTHER}")

    mark_as_advanced(Libevdev_LIBRARIES Libevdev_INCLUDE_DIRS Libevdev_DEFINITIONS)
endif()
