# The name of the target operating system
set(CMAKE_SYSTEM_NAME Linux)

# Try to find clang for C and C++
find_program(CMAKE_C_COMPILER   clang)
find_program(CMAKE_CXX_COMPILER clang++)

# Toolchain find root modes
#   NEVER - search host system only
#   ONLY  - search CMAKE_FIND_ROOT_PATH only
#   BOTH  - search both
# Probably not needed for a native toolchain so commenting these out
#set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
#set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
#set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
#set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
