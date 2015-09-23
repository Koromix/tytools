# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

cmake_minimum_required(VERSION 2.8.12)
project(ty C CXX)

if(CMAKE_C_COMPILER_ID STREQUAL Clang)
    set(CMAKE_COMPILER_IS_CLANG 1)
endif()
if(CMAKE_SYSTEM_NAME STREQUAL Linux)
    set(LINUX 1)
endif()

if(CMAKE_COMPILER_IS_GNUCC OR CMAKE_COMPILER_IS_CLANG)
    set(CMAKE_C_FLAGS "-std=gnu99 -fvisibility=hidden -D_GNU_SOURCE -D_FILE_OFFSET_BITS=64 -Wall -Wextra -Wno-missing-field-initializers -Wno-missing-braces -Wshadow -Wconversion -Wformat=2")
    set(CMAKE_CXX_FLAGS "-std=gnu++11 -fvisibility=hidden -D_GNU_SOURCE -D_FILE_OFFSET_BITS=64 -Wall -Wextra -Wno-missing-field-initializers -Wold-style-cast -Wformat=2")

    foreach(lang C CXX)
        set(CMAKE_${lang}_FLAGS_DEBUG "-O1 -g -D_FORTIFY_SOURCE=2")
        set(CMAKE_${lang}_FLAGS_RELWITHDEBINFO "-O2 -g -D_FORTIFY_SOURCE=2")
        set(CMAKE_${lang}_FLAGS_RELEASE "-O2 -D_FORTIFY_SOURCE=2")
    endforeach()
endif()

if(WIN32)
    add_definitions(-DWINVER=0x0602 -D_WIN32_WINNT=0x0602)

    if(MINGW)
        set(CMAKE_C_STANDARD_LIBRARIES "${CMAKE_C_STANDARD_LIBRARIES} -static-libgcc -static-libstdc++ -Wl,-Bstatic -lwinpthread -Wl,-Bdynamic")
        set(CMAKE_CXX_STANDARD_LIBRARIES "${CMAKE_CXX_STANDARD_LIBRARIES} -static-libgcc -static-libstdc++ -Wl,-Bstatic -lstdc++ -lwinpthread -Wl,-Bdynamic")
    endif()
elseif(APPLE)
    if(POLICY CMP0042)
        cmake_policy(SET CMP0042 NEW)
    endif()

    set(CMAKE_INSTALL_NAME_DIR "@rpath")
    set(CMAKE_INSTALL_RPATH "@executable_path;@executable_path/../lib;@executable_path/../Frameworks")

    # Makes it easier for cross-compilation and testing. In my case, compile on Linux
    # and test on OSX using sshfs
    set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)
endif()

include_directories(libty/include ${CMAKE_BINARY_DIR})
set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(STATIC_BINARIES OFF CACHE BOOL "Build statically linked binaries")
if(STATIC_BINARIES)
    set(TY_LIBRARIES ty_static)
else()
    set(TY_LIBRARIES ty_shared)
endif()

add_subdirectory(libty)
add_subdirectory(tyc)
add_subdirectory(tyqt)

set(CPACK_PACKAGE_NAME "TyQt")
set(CPACK_PACKAGE_VENDOR "Koromix")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "GUI and command-line tools to manage Teensy devices")
set(CPACK_PACKAGE_DESCRIPTION_FILE "${CMAKE_SOURCE_DIR}/README.md")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_SOURCE_DIR}/LICENSE.txt")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "TyQt")
set(CPACK_PACKAGE_EXECUTABLES tyqt "TyQt")
set(CPACK_PACKAGE_ICON "${CMAKE_SOURCE_DIR}/tyqt/images/tyqt.png")

if(WIN32)
    set(CPACK_GENERATOR ZIP WIX)

    set(CPACK_WIX_UPGRADE_GUID 72663aca-47a7-4b9b-aa53-aa067b872b8a)
    set(CPACK_WIX_PRODUCT_ICON "${CMAKE_SOURCE_DIR}/tyqt/images/tyqt.ico")
    set(CPACK_WIX_UI_BANNER "${CMAKE_SOURCE_DIR}/contrib/win32/wix_banner.jpg")
    set(CPACK_WIX_UI_DIALOG "${CMAKE_SOURCE_DIR}/contrib/win32/wix_dialog.jpg")

    install(FILES README.md DESTINATION . RENAME README.txt)
    install(FILES LICENSE.txt DESTINATION .)
    install(DIRECTORY contrib/win32/ DESTINATION . FILES_MATCHING PATTERN *.txt)

    include(CPack)
elseif(APPLE)
    set(CPACK_GENERATOR Bundle)

    set(CPACK_BUNDLE_NAME "TyQt")
    set(CPACK_BUNDLE_ICON "${CMAKE_SOURCE_DIR}/tyqt/images/tyqt.icns")
    set(CPACK_BUNDLE_PLIST "${CMAKE_BINARY_DIR}/tyqt/tyqt_osx.plist")

    install(FILES README.md DESTINATION . RENAME README.txt)
    install(FILES LICENSE.txt DESTINATION .)

    include(CPack)
endif()
