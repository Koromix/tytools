# See here: http://www.cmake.org/Wiki/CmakeMingw

set(CMAKE_SYSTEM_NAME Darwin)

# Go to https://github.com/tpoechtrager/osxcross and fall in love
execute_process(COMMAND xcrun --show-sdk-path
    OUTPUT_VARIABLE CMAKE_FIND_ROOT_PATH OUTPUT_STRIP_TRAILING_WHITESPACE)
execute_process(COMMAND xcrun -f clang
    OUTPUT_VARIABLE CMAKE_C_COMPILER OUTPUT_STRIP_TRAILING_WHITESPACE)
execute_process(COMMAND xcrun -f clang++
    OUTPUT_VARIABLE CMAKE_CXX_COMPILER OUTPUT_STRIP_TRAILING_WHITESPACE)
execute_process(COMMAND xcrun -f pkg-config
    OUTPUT_VARIABLE PKG_CONFIG_EXECUTABLE OUTPUT_STRIP_TRAILING_WHITESPACE)

if(NOT CMAKE_FIND_ROOT_PATH)
    message(FATAL_ERROR "Cannot find Apple SDK")
endif()
if(NOT CMAKE_C_COMPILER)
    message(FATAL_ERROR "Cannot find Apple Clang compiler")
endif()

# Know a better way to get the Darwin version?
string(REGEX REPLACE "^.*-darwin([0-9]+)-.*$" "\\1" DARWIN_MAJOR_VERSION "${CMAKE_C_COMPILER}")
set(CMAKE_SYSTEM_VERSION "${DARWIN_MAJOR_VERSION}.0")

# Adjust the default behaviour of the FIND_XXX() commands:
#  - search headers and libraries in the target environment
#  - search programs in the host environment
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE NEVER)
