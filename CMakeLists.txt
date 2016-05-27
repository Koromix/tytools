# ty, a collection of GUI and command-line tools to manage Teensy devices
#
# Distributed under the MIT license (see LICENSE.txt or http://opensource.org/licenses/MIT)
# Copyright (c) 2015 Niels Martignène <niels.martignene@gmail.com>

cmake_minimum_required(VERSION 2.8.12)
project(ty C CXX)

# Don't contaminate external projects with our variables, I know you're supposed to
# use ExternalProject... but add_subdirectory() avoids a lot of ExternalProject's mess.
if(NOT TY_STATIC_MSVCRT)
    set(HS_SHARED_MSVCRT ON CACHE BOOL "" FORCE)
endif()
set(HS_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
add_subdirectory(libhs)

if(CMAKE_C_COMPILER_ID STREQUAL Clang)
    set(CMAKE_COMPILER_IS_CLANG 1)
endif()
if(CMAKE_SYSTEM_NAME STREQUAL Linux)
    set(LINUX 1)
endif()

# Very simple host-compiler triplet, only valid for the handful of supported platforms
if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(HOST_CPU "x86_64")
else()
    set(HOST_CPU "i686")
endif()
if(MSVC)
    if(CMAKE_C_COMPILER_VERSION VERSION_GREATER 18)
        set(HOST "${HOST_CPU}-win32-msvc2015")
    else()
        message(FATAL_ERROR "Only Visual Studio 2015 and later versions are supported")
    endif()
    if(TY_STATIC_MSVCRT)
        set(HOST "${HOST}-mt")
    endif()
elseif(MINGW)
    set(HOST "${HOST_CPU}-w64-mingw32")
elseif(CMAKE_COMPILER_IS_GNUCC)
    string(TOLOWER "${HOST_CPU}-${CMAKE_SYSTEM_NAME}-gcc" HOST)
else()
    string(TOLOWER "${HOST_CPU}-${CMAKE_SYSTEM_NAME}-${CMAKE_C_COMPILER_ID}" HOST)
endif()

if(MSVC)
    add_definitions(-D_CRT_NONSTDC_NO_DEPRECATE -D_CRT_SECURE_NO_WARNINGS)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /MANIFEST:NO")

    set(TY_STATIC_MSVCRT OFF CACHE BOOL "Build with static version of MS CRT (/MT)")
    if(TY_STATIC_MSVCRT)
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

    set(CMAKE_INSTALL_NAME_DIR "@rpath")
    set(CMAKE_INSTALL_RPATH "@executable_path;@executable_path/../lib;@executable_path/../libty;@executable_path/../Frameworks")

    # Makes it easier for cross-compilation and testing. In my case, compile on Linux
    # and test on OSX using sshfs
    set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)
endif()

include_directories(libhs/include libty/include ${CMAKE_BINARY_DIR})
set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(TY_SHARED_LIBHS OFF CACHE BOOL "Use libhs as a dynamic library")
set(TY_SHARED_LIBTY OFF CACHE BOOL "Use libty as a dynamic library")
if(TY_SHARED_LIBHS)
    set(HS_LIBRARIES hs_shared)
else()
    set(HS_LIBRARIES hs_static)
endif()
if(TY_SHARED_LIBTY)
    set(TY_LIBRARIES ty_shared)
else()
    set(TY_LIBRARIES ty_static)
endif()

# Recompute the version string after each commit, does not work for tags though
if(EXISTS "${CMAKE_SOURCE_DIR}/.git/logs/HEAD")
    configure_file("${CMAKE_SOURCE_DIR}/.git/logs/HEAD" git_logs_HEAD COPYONLY)
endif()
if(WIN32 AND NOT CMAKE_CROSSCOMPILING)
    execute_process(COMMAND "${CMAKE_SOURCE_DIR}/scripts/git-version.bat"
        OUTPUT_VARIABLE TY_VERSION_HEADER)
else()
    execute_process(COMMAND "${CMAKE_SOURCE_DIR}/scripts/git-version.sh"
        OUTPUT_VARIABLE TY_VERSION_HEADER)
endif()
file(WRITE "${CMAKE_BINARY_DIR}/ty/version.h" "${TY_VERSION_HEADER}")
string(REGEX MATCH "[0-9]+\\.[0-9]+\\.[0-9]+(\\.[0-9]+)?(\\-[0-9]+-[a-zA-Z0-9]+)?" TY_VERSION "${TY_VERSION_HEADER}")
string(REGEX REPLACE "\\-.*$" "" CPACK_PACKAGE_VERSION ${TY_VERSION})

add_subdirectory(libty)
add_subdirectory(tyc)
add_subdirectory(tyqt)

set(CPACK_PACKAGE_NAME "TyQt")
set(CPACK_PACKAGE_VENDOR "Koromix")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "GUI and command-line tools to manage Teensy devices")
set(CPACK_PACKAGE_DESCRIPTION_FILE "${CMAKE_SOURCE_DIR}/README.md")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "TyQt")
set(CPACK_PACKAGE_EXECUTABLES tyqt "TyQt")
set(CPACK_PACKAGE_ICON "${CMAKE_SOURCE_DIR}/tyqt/images/tyqt.png")

if(WIN32)
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(CPACK_PACKAGE_FILE_NAME "TyQt-${TY_VERSION}-win64")
    else()
        set(CPACK_PACKAGE_FILE_NAME "TyQt-${TY_VERSION}-win32")
    endif()
    set(CPACK_GENERATOR ZIP)
    if (NOT CMAKE_CROSSCOMPILING)
        list(APPEND CPACK_GENERATOR WIX)
    endif()

    set(CPACK_WIX_UPGRADE_GUID 72663aca-47a7-4b9b-aa53-aa067b872b8a)
    set(CPACK_WIX_PRODUCT_ICON "${CMAKE_SOURCE_DIR}/tyqt/images/tyqt.ico")
    set(CPACK_WIX_UI_BANNER "${CMAKE_SOURCE_DIR}/contrib/win32/wix_banner.jpg")
    set(CPACK_WIX_UI_DIALOG "${CMAKE_SOURCE_DIR}/contrib/win32/wix_dialog.jpg")
    set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_SOURCE_DIR}/LICENSE.txt")

    # Someday, notepad will support LF newlines and this won't be necessary anymore
    file(WRITE "${CMAKE_BINARY_DIR}/dos2unix.cmake" "configure_file(\${IN} \${OUT} NEWLINE_STYLE CRLF)")
    add_custom_command(OUTPUT README_crlf.md COMMAND ${CMAKE_COMMAND}
        ARGS -DIN="${CMAKE_SOURCE_DIR}/README.md" -DOUT="${CMAKE_BINARY_DIR}/README_crlf.md" -P "${CMAKE_BINARY_DIR}/dos2unix.cmake")
    add_custom_command(OUTPUT LICENSE_crlf.txt COMMAND ${CMAKE_COMMAND}
        ARGS -DIN="${CMAKE_SOURCE_DIR}/LICENSE.txt" -DOUT="${CMAKE_BINARY_DIR}/LICENSE_crlf.txt" -P "${CMAKE_BINARY_DIR}/dos2unix.cmake")
    add_custom_target(crlf ALL DEPENDS README_crlf.md LICENSE_crlf.txt)

    install(FILES "${CMAKE_BINARY_DIR}/README_crlf.md" DESTINATION . RENAME README.txt)
    install(FILES "${CMAKE_BINARY_DIR}/LICENSE_crlf.txt" DESTINATION . RENAME LICENSE.txt)
    if(MINGW)
        install(DIRECTORY contrib/mingw/ DESTINATION . FILES_MATCHING PATTERN *.txt)
    endif()

    include(CPack)
elseif(APPLE)
    set(CPACK_PACKAGE_FILE_NAME "TyQt-${TY_VERSION}-osx")
    set(CPACK_GENERATOR Bundle)

    set(CPACK_BUNDLE_NAME "TyQt")
    set(CPACK_BUNDLE_ICON "${CMAKE_SOURCE_DIR}/tyqt/images/tyqt.icns")
    set(CPACK_BUNDLE_PLIST "${CMAKE_BINARY_DIR}/tyqt/tyqt_osx.plist")

    install(FILES README.md DESTINATION . RENAME README.txt)
    install(FILES LICENSE.txt DESTINATION .)

    include(CPack)
endif()
