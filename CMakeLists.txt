# ty, a collection of GUI and command-line tools to manage Teensy devices
#
# Distributed under the MIT license (see LICENSE.txt or http://opensource.org/licenses/MIT)
# Copyright (c) 2015 Niels Martignène <niels.martignene@gmail.com>

cmake_minimum_required(VERSION 3.0)
project(ty C CXX)

set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake" ${CMAKE_MODULE_PATH})

if(TY_CONFIG_VARIANT)
    if(IS_ABSOLUTE ${TY_CONFIG_VARIANT})
        set(TY_CONFIG_VARIANT_PATH ${TY_CONFIG_VARIANT})
    else()
        set(TY_CONFIG_VARIANT_PATH "${CMAKE_CURRENT_BINARY_DIR}/variants/${TY_CONFIG_VARIANT}")
    endif()
    if(EXISTS "${TY_CONFIG_VARIANT_PATH}/settings.cmake")
        include("${TY_CONFIG_VARIANT_PATH}/settings.cmake")
    endif()
endif()

set(TY_CONFIG_VERSION_DEFAULT "0.7.5" CACHE STRING "Default version number")
set(TY_CONFIG_VERSION_AUTO ON CACHE BOOL "Compute version using git tags (fallback)")

set(TY_CONFIG_TYC_BUILD ON CACHE BOOL "Build tyc (Teensy command-line manager)")
set(TY_CONFIG_TYC_EXECUTABLE "tyc" CACHE STRING "Rename tyc executable")
set(TY_CONFIG_TYC_NAME "tyc" CACHE STRING "Rename tyc application")

set(TY_CONFIG_TYQT_BUILD ON CACHE BOOL "Build TyQt (Teensy GUI manager)")
set(TY_CONFIG_TYQT_EXECUTABLE "tyqt" CACHE STRING "Rename TyQt executable")
set(TY_CONFIG_TYQT_NAME "TyQt" CACHE STRING "Rename TyQt application")

set(TY_CONFIG_UPTY_BUILD ON CACHE BOOL "Build UpTy (Teensy GUI updater)")
set(TY_CONFIG_UPTY_EXECUTABLE "upty" CACHE STRING "Rename UpTy executable")
set(TY_CONFIG_UPTY_NAME "UpTy" CACHE STRING "Rename UpTy application")

set(TY_CONFIG_URL_WEBSITE "https://github.com/Koromix/ty" CACHE STRING "Project website")
set(TY_CONFIG_URL_BUGS "https://github.com/Koromix/ty/issues" CACHE STRING "Project bug report page")

set(TY_CONFIG_PACKAGE_NAME "TyQt" CACHE STRING "Package / installer name")

if(CMAKE_C_COMPILER_ID MATCHES "[Cc]lang")
    set(CMAKE_COMPILER_IS_CLANG 1)
endif()
if(CMAKE_SYSTEM_NAME STREQUAL Linux)
    set(LINUX 1)
endif()

set(TY_SHARED_LIBHS OFF CACHE BOOL "Use libhs as a dynamic library")
if(MSVC)
    set(TY_SHARED_MSVCRT OFF CACHE BOOL "Build with shared version of MS CRT (/MD)")
endif()

# Don't contaminate external projects with our variables, I know you're supposed to
# use ExternalProject... but add_subdirectory() avoids a lot of ExternalProject's mess.
if(TY_SHARED_MSVCRT)
    set(HS_SHARED_MSVCRT ON CACHE BOOL "" FORCE)
endif()
set(HS_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
add_subdirectory(libhs)
if(TY_SHARED_LIBHS)
    set(LIBHS_LIBRARIES hs_shared)
else()
    set(LIBHS_LIBRARIES hs_static)
endif()

if(MSVC)
    add_definitions(-D_CRT_NONSTDC_NO_DEPRECATE -D_CRT_SECURE_NO_WARNINGS)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /MANIFEST:NO")

    if(NOT TY_SHARED_MSVCRT)
        foreach(lang C CXX)
            foreach(mode DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
                string(REPLACE "/MD" "/MT" CMAKE_${lang}_FLAGS_${mode} "${CMAKE_${lang}_FLAGS_${mode}}")
            endforeach()
        endforeach()
    endif()

    include_directories(contrib/msvc)
elseif(CMAKE_COMPILER_IS_GNUCC OR CMAKE_COMPILER_IS_CLANG)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=gnu99 -fvisibility=hidden -D_GNU_SOURCE -D_FILE_OFFSET_BITS=64 -Wall -Wextra -Wno-missing-field-initializers -Wno-missing-braces -Wshadow -Wconversion -Wformat=2")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=gnu++11 -fvisibility=hidden -D_GNU_SOURCE -D_FILE_OFFSET_BITS=64 -Wall -Wextra -Wno-missing-field-initializers -Wold-style-cast -Wformat=2")

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
    set(CMAKE_INSTALL_RPATH "@executable_path;@executable_path/../lib;@executable_path/../libty;@executable_path/../Frameworks")
endif()

set(TY_CONFIG_VERSION ${TY_CONFIG_VERSION_DEFAULT})
if(TY_CONFIG_VERSION_AUTO AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/.git")
    find_package(Git)
    if(GIT_FOUND)
        # Recompute the version string after each commit, does not work for tags though
        if(EXISTS "${CMAKE_SOURCE_DIR}/.git/logs/HEAD")
            configure_file("${CMAKE_SOURCE_DIR}/.git/logs/HEAD" git_logs_HEAD COPYONLY)
        endif()
        execute_process(
            COMMAND ${GIT_EXECUTABLE} describe --tags
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
            OUTPUT_VARIABLE TY_CONFIG_VERSION
            OUTPUT_STRIP_TRAILING_WHITESPACE)
        string(SUBSTRING ${TY_CONFIG_VERSION} 1 -1 TY_CONFIG_VERSION)
    endif()
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")
add_subdirectory(src/libty)
if(TY_CONFIG_TYC_BUILD)
    add_subdirectory(src/tyc)
endif()
if(TY_CONFIG_TYQT_BUILD OR TY_CONFIG_UPTY_BUILD)
    add_subdirectory(src/libtyqt)
endif()
if(TY_CONFIG_TYQT_BUILD)
    add_subdirectory(src/tyqt)
    list(APPEND CPACK_PACKAGE_EXECUTABLES tyqt "${TY_CONFIG_TYQT_NAME}")
endif()
if(TY_CONFIG_UPTY_BUILD)
    add_subdirectory(src/upty)
    list(APPEND CPACK_PACKAGE_EXECUTABLES upty "${TY_CONFIG_UPTY_NAME}")
endif()

set(TY_BUILD_TESTS ON CACHE BOOL "Build unit tests and enable CTest")
if(TY_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests/libty)
endif()

set(CPACK_PACKAGE_NAME "${TY_CONFIG_PACKAGE_NAME}")
string(REGEX REPLACE "\\-.*$" "" CPACK_PACKAGE_VERSION "${TY_CONFIG_VERSION}")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "${CPACK_PACKAGE_NAME}")

if(WIN32)
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}-${TY_CONFIG_VERSION}-win64")
    else()
        set(CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}-${TY_CONFIG_VERSION}-win32")
    endif()
    if(TY_CONFIG_TYQT_BUILD)
        set(CPACK_PACKAGE_ICON "${TYQT_ICON_FILE_PREFIX}.ico")
    elseif(TY_CONFIG_UPTY_BUILD)
        set(CPACK_PACKAGE_ICON "${UPTY_ICON_FILE_PREFIX}.ico")
    endif()
    if (NOT TY_CONFIG_VARIANT AND NOT CMAKE_CROSSCOMPILING)
        set(CPACK_GENERATOR ZIP WIX)
    else()
        set(CPACK_GENERATOR ZIP)
    endif()

    set(CPACK_WIX_UPGRADE_GUID "72663aca-47a7-4b9b-aa53-aa067b872b8a")
    set(CPACK_WIX_PRODUCT_ICON "${CPACK_PACKAGE_ICON}")
    set(CPACK_WIX_UI_BANNER "${CMAKE_SOURCE_DIR}/contrib/images/wix_banner.jpg")
    set(CPACK_WIX_UI_DIALOG "${CMAKE_SOURCE_DIR}/contrib/images/wix_dialog.jpg")
    set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_SOURCE_DIR}/LICENSE.txt")

    if(TY_CONFIG_TYQT_BUILD OR TY_CONFIG_TYC_BUILD)
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
    set(CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}-${TY_CONFIG_VERSION}-osx")
    if(TY_CONFIG_TYQT_BUILD)
        set(CPACK_PACKAGE_ICON "${TYQT_ICON_FILE_PREFIX}.icns")
    elseif(TY_CONFIG_UPTY_BUILD)
        set(CPACK_PACKAGE_ICON "${UPTY_ICON_FILE_PREFIX}.icns")
    endif()
    set(CPACK_GENERATOR DragNDrop)

    if(TY_CONFIG_TYQT_BUILD OR TY_CONFIG_TYC_BUILD)
        install(FILES README.md DESTINATION . RENAME README.txt)
        install(FILES LICENSE.txt DESTINATION .)
    endif()

    include(CPack)
endif()
