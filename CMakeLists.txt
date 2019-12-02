# TyTools - public domain
# Niels Martignène <niels.martignene@protonmail.com>
# https://koromix.dev/tytools

# This software is in the public domain. Where that dedication is not
# recognized, you are granted a perpetual, irrevocable license to copy,
# distribute, and modify this file as you see fit.

# See the LICENSE file for more details.

set(CMAKE_OSX_DEPLOYMENT_TARGET "10.10" CACHE STRING "")
cmake_minimum_required(VERSION 3.1)
project(TyTools C CXX)

if(CONFIG_VARIANT)
    if(IS_ABSOLUTE ${CONFIG_VARIANT})
        set(CONFIG_VARIANT_PATH ${CONFIG_VARIANT})
    else()
        set(CONFIG_VARIANT_PATH "${CMAKE_CURRENT_SOURCE_DIR}/variants/${CONFIG_VARIANT}")
    endif()
    if(EXISTS "${CONFIG_VARIANT_PATH}/variant.cmake")
        include("${CONFIG_VARIANT_PATH}/variant.cmake")
    endif()

    set(USE_UNITY_BUILDS OFF CACHE BOOL "")
endif()

set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake" ${CMAKE_MODULE_PATH})
include("cmake/Utility.cmake")

set(VERSION_DEFAULT "0.9.0" CACHE STRING "Default version number")
set(VERSION_WITH_GIT ON CACHE BOOL "Compute version using git tags (fallback)")

set(CONFIG_TYCMD_BUILD ON CACHE BOOL "Build tycmd (Teensy command-line manager)")
set(CONFIG_TYCMD_EXECUTABLE "tycmd" CACHE STRING "Rename tycmd executable")
set(CONFIG_TYCMD_NAME "tycmd" CACHE STRING "Rename tycmd application")

set(CONFIG_TYCOMMANDER_BUILD ON CACHE BOOL "Build TyCommander (Teensy GUI manager)")
if(WIN32 OR APPLE)
    set(CONFIG_TYCOMMANDER_EXECUTABLE "TyCommander" CACHE STRING "Rename TyCommander executable")
else()
    set(CONFIG_TYCOMMANDER_EXECUTABLE "tycommander" CACHE STRING "Rename TyCommander executable")
endif()
set(CONFIG_TYCOMMANDER_NAME "TyCommander" CACHE STRING "Rename TyCommander application")

set(CONFIG_TYUPDATER_BUILD ON CACHE BOOL "Build TyUpdater (Teensy GUI updater)")
if(WIN32 OR APPLE)
    set(CONFIG_TYUPDATER_EXECUTABLE "TyUpdater" CACHE STRING "Rename TyUpdater executable")
else()
    set(CONFIG_TYUPDATER_EXECUTABLE "tyupdater" CACHE STRING "Rename TyUpdater executable")
endif()
set(CONFIG_TYUPDATER_NAME "TyUpdater" CACHE STRING "Rename TyUpdater application")

set(CONFIG_URL_WEBSITE "https://github.com/Koromix/tytools" CACHE STRING "Project website")
set(CONFIG_URL_BUGS "https://github.com/Koromix/tytools/issues" CACHE STRING "Project bug report page")

set(CONFIG_PACKAGE_NAME "TyTools" CACHE STRING "Package / installer name")
set(CONFIG_PACKAGE_FILENAME "TyTools" CACHE STRING "Package / installer filename")

if(MSVC)
    set(USE_SHARED_MSVCRT OFF CACHE BOOL "Build with shared version of MS CRT (/MD)")
endif()
set(BUILD_EXAMPLES ON CACHE BOOL "Build library examples")

if(MSVC)
    add_definitions(-D_CRT_NONSTDC_NO_DEPRECATE -D_CRT_SECURE_NO_WARNINGS)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /MANIFEST:NO")

    if(NOT USE_SHARED_MSVCRT)
        foreach(lang C CXX)
            foreach(mode DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
                string(REPLACE "/MD" "/MT" CMAKE_${lang}_FLAGS_${mode} "${CMAKE_${lang}_FLAGS_${mode}}")
            endforeach()
        endforeach()
    endif()

    include_directories(contrib/msvc)
elseif(CMAKE_COMPILER_IS_GNUCC OR CMAKE_COMPILER_IS_CLANG)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=gnu99 -fvisibility=hidden -D_FILE_OFFSET_BITS=64 -Wall -Wextra -Wno-missing-field-initializers -Wno-missing-braces -Wshadow -Wconversion -Wformat=2")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=gnu++11 -fvisibility=hidden -D_FILE_OFFSET_BITS=64 -Wall -Wextra -Wno-missing-field-initializers -Wold-style-cast -Wformat=2")

    foreach(lang C CXX)
        foreach(mode RELEASE RELWITHDEBINFO MINSIZEREL)
            set(CMAKE_${lang}_FLAGS_${mode} "${CMAKE_${lang}_FLAGS_${mode}} -D_FORTIFY_SOURCE=2")
        endforeach()
    endforeach()

    if(MINGW)
        add_definitions(-D__USE_MINGW_ANSI_STDIO=1)
        # Latest approach to force MinGW to statically link libwinpthread, until a new
        # version breaks this one too.
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static-libgcc -static-libstdc++ -Wl,-Bstatic")
        set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -static-libgcc -static-libstdc++ -Wl,-Bstatic")
        set(CMAKE_CXX_STANDARD_LIBRARIES "${CMAKE_CXX_STANDARD_LIBRARIES} -lstdc++ -lwinpthread")
    endif()
endif()

if(WIN32)
    add_definitions(-DWINVER=0x0602 -D_WIN32_WINNT=0x0602)
elseif(APPLE)
    if(POLICY CMP0042)
        cmake_policy(SET CMP0042 NEW)
    endif()

    # https://stackoverflow.com/questions/4929255/building-static-libraries-on-mac-using-cmake-and-gcc
    set(CMAKE_C_ARCHIVE_CREATE "<CMAKE_AR> Scr <TARGET> <LINK_FLAGS> <OBJECTS>")
    set(CMAKE_CXX_ARCHIVE_CREATE "<CMAKE_AR> Scr <TARGET> <LINK_FLAGS> <OBJECTS>")
    set(CMAKE_C_ARCHIVE_FINISH "<CMAKE_RANLIB> -no_warning_for_no_symbols -c <TARGET>")
    set(CMAKE_CXX_ARCHIVE_FINISH "<CMAKE_RANLIB> -no_warning_for_no_symbols -c <TARGET>")

    set(CMAKE_INSTALL_NAME_DIR "@rpath")
    set(CMAKE_INSTALL_RPATH "@executable_path;@executable_path/../lib;@executable_path/../Frameworks")
endif()

set(VERSION ${VERSION_DEFAULT})
if(VERSION_WITH_GIT AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/.git")
    find_package(Git)
    if(GIT_FOUND)
        # Recompute the version string after each commit, does not work for tags though
        if(EXISTS "${CMAKE_SOURCE_DIR}/.git/logs/HEAD")
            configure_file("${CMAKE_SOURCE_DIR}/.git/logs/HEAD" git_logs_HEAD COPYONLY)
        endif()
        execute_process(
            COMMAND ${GIT_EXECUTABLE} describe --tags
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
            OUTPUT_VARIABLE VERSION
            OUTPUT_STRIP_TRAILING_WHITESPACE)
        string(SUBSTRING ${VERSION} 1 -1 VERSION)
    endif()
endif()

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")

add_subdirectory(src/libhs)
add_subdirectory(src/libty)
if(CONFIG_TYCMD_BUILD)
    add_subdirectory(src/tycmd)
endif()
if(CONFIG_TYCOMMANDER_BUILD)
    add_subdirectory(src/tycommander)
    list(APPEND CPACK_PACKAGE_EXECUTABLES tycommander "${CONFIG_TYCOMMANDER_NAME}")
endif()
if(CONFIG_TYUPDATER_BUILD)
    add_subdirectory(src/tyupdater)
    list(APPEND CPACK_PACKAGE_EXECUTABLES tyupdater "${CONFIG_TYUPDATER_NAME}")
endif()

set(BUILD_TESTS ON CACHE BOOL "Build unit tests and enable CTest")
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests/libty)
endif()

set(CPACK_PACKAGE_NAME "${CONFIG_PACKAGE_NAME}")
string(REGEX REPLACE "\\-.*$" "" CPACK_PACKAGE_VERSION "${VERSION}")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "${CONFIG_PACKAGE_FILENAME}")

if(WIN32)
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(CPACK_PACKAGE_FILE_NAME "${CONFIG_PACKAGE_FILENAME}-${VERSION}-win64")
    else()
        set(CPACK_PACKAGE_FILE_NAME "${CONFIG_PACKAGE_FILENAME}-${VERSION}-win32")
    endif()
    if(CONFIG_TYCOMMANDER_BUILD)
        set(CPACK_PACKAGE_ICON "${TYCOMMANDER_ICON_FILE_PREFIX}.ico")
    elseif(CONFIG_TYUPDATER_BUILD)
        set(CPACK_PACKAGE_ICON "${TYUPDATER_ICON_FILE_PREFIX}.ico")
    endif()
    if (NOT CONFIG_VARIANT AND NOT CMAKE_CROSSCOMPILING)
        set(CPACK_GENERATOR ZIP WIX)
    else()
        set(CPACK_GENERATOR ZIP)
    endif()

    set(CPACK_WIX_UPGRADE_GUID "72663aca-47a7-4b9b-aa53-aa067b872b8a")
    set(CPACK_WIX_PRODUCT_ICON "${CPACK_PACKAGE_ICON}")
    set(CPACK_WIX_UI_BANNER "${CMAKE_SOURCE_DIR}/contrib/images/wix_banner.jpg")
    set(CPACK_WIX_UI_DIALOG "${CMAKE_SOURCE_DIR}/contrib/images/wix_dialog.jpg")
    set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_SOURCE_DIR}/LICENSE.txt")

    if(CONFIG_TYCOMMANDER_BUILD OR CONFIG_TYCMD_BUILD)
        # Someday, notepad will support LF newlines and this won't be necessary anymore
        file(WRITE "${CMAKE_BINARY_DIR}/dos2unix.cmake" "configure_file(\${IN} \${OUT} NEWLINE_STYLE CRLF)")
        add_custom_command(OUTPUT README_crlf.md COMMAND ${CMAKE_COMMAND}
            ARGS -DIN="${CMAKE_SOURCE_DIR}/README.md" -DOUT="${CMAKE_BINARY_DIR}/README_crlf.md" -P "${CMAKE_BINARY_DIR}/dos2unix.cmake")
        add_custom_command(OUTPUT LICENSE_crlf.txt COMMAND ${CMAKE_COMMAND}
            ARGS -DIN="${CMAKE_SOURCE_DIR}/LICENSE.txt" -DOUT="${CMAKE_BINARY_DIR}/LICENSE_crlf.txt" -P "${CMAKE_BINARY_DIR}/dos2unix.cmake")
        add_custom_target(crlf ALL DEPENDS README_crlf.md LICENSE_crlf.txt)

        install(FILES "${CMAKE_BINARY_DIR}/README_crlf.md" DESTINATION . RENAME README.txt)
        install(FILES "${CMAKE_BINARY_DIR}/LICENSE_crlf.txt" DESTINATION . RENAME LICENSE.txt)
    else()
        set(CPACK_INCLUDE_TOPLEVEL_DIRECTORY 0)
    endif()
    if(MINGW)
        install(DIRECTORY contrib/mingw/ DESTINATION . FILES_MATCHING PATTERN *.txt)
    endif()

    include(CPack)
elseif(APPLE)
    set(CPACK_PACKAGE_FILE_NAME "${CONFIG_PACKAGE_FILENAME}-${VERSION}-osx")
    if(CONFIG_TYCOMMANDER_BUILD)
        set(CPACK_PACKAGE_ICON "${TYCOMMANDER_ICON_FILE_PREFIX}.icns")
    elseif(CONFIG_TYUPDATER_BUILD)
        set(CPACK_PACKAGE_ICON "${TYUPDATER_ICON_FILE_PREFIX}.icns")
    endif()
    set(CPACK_GENERATOR DragNDrop)

    if(CONFIG_TYCOMMANDER_BUILD OR CONFIG_TYCMD_BUILD)
        install(FILES README.md DESTINATION . RENAME README.txt)
        install(FILES LICENSE.txt DESTINATION .)
    endif()

    include(CPack)
endif()
