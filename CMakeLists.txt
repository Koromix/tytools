# ty, a collection of GUI and command-line tools to manage Teensy devices
#
# Distributed under the MIT license (see LICENSE.txt or http://opensource.org/licenses/MIT)
# Copyright (c) 2015 Niels Martignène <niels.martignene@gmail.com>

cmake_minimum_required(VERSION 2.8.12)
project(ty C CXX)

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
if(WIN32)
    set(HOST_OS "w64")
else()
    string(TOLOWER ${CMAKE_SYSTEM_NAME} HOST_OS)
endif()
if(MINGW)
    set(HOST_COMPILER "mingw32")
elseif(CMAKE_COMPILER_IS_GNUCC)
    set(HOST_COMPILER "gcc")
else()
    string(TOLOWER ${CMAKE_C_COMPILER_ID} HOST_COMPILER)
endif()
set(HOST "${HOST_CPU}-${HOST_OS}-${HOST_COMPILER}")

if(CMAKE_COMPILER_IS_GNUCC OR CMAKE_COMPILER_IS_CLANG)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=gnu99 -fvisibility=hidden -D_GNU_SOURCE -D_FILE_OFFSET_BITS=64 -Wall -Wextra -Wno-missing-field-initializers -Wno-missing-braces -Wshadow -Wconversion -Wformat=2")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=gnu++11 -fvisibility=hidden -D_GNU_SOURCE -D_FILE_OFFSET_BITS=64 -Wall -Wextra -Wno-missing-field-initializers -Wold-style-cast -Wformat=2")

    foreach(lang C CXX)
        foreach(mode RELEASE RELWITHDEBINFO MINSIZEREL)
            set(CMAKE_${lang}_FLAGS_${mode} "${CMAKE_${lang}_FLAGS_${mode}} -D_FORTIFY_SOURCE=2")
        endforeach()
    endforeach()
endif()

if(WIN32)
    add_definitions(-DWINVER=0x0602 -D_WIN32_WINNT=0x0602)

    if(MINGW)
        # Latest approach to force MinGW to statically link libwinpthread, until a new
        # version breaks this one too.
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static-libgcc -static-libstdc++ -Wl,-Bstatic")
        set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -static-libgcc -static-libstdc++ -Wl,-Bstatic")
        set(CMAKE_CXX_STANDARD_LIBRARIES "${CMAKE_C_STANDARD_LIBRARIES} -lstdc++ -lwinpthread")
    endif()
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

include_directories(libty/include ${CMAKE_BINARY_DIR})
set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(STATIC_BINARIES ON CACHE BOOL "Build statically linked binaries")
if(STATIC_BINARIES)
    set(TY_LIBRARIES ty_static)
else()
    set(TY_LIBRARIES ty_shared)
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
    set(CPACK_GENERATOR ZIP WIX)

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
    install(DIRECTORY contrib/win32/ DESTINATION . FILES_MATCHING PATTERN *.txt)

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
