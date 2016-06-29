# ty, a collection of GUI and command-line tools to manage Teensy devices
#
# Distributed under the MIT license (see LICENSE.txt or http://opensource.org/licenses/MIT)
# Copyright (c) 2015 Niels Martignène <niels.martignene@gmail.com>

# I don't really know if this is any close to the "correct" way to make modules
# in modern CMake, but it works and... I don't care much beyond that.

if(NOT Qt5_FOUND)
    # Simple and stupid host-compiler triplet, only valid for the handful of
    # supported platforms.
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
        if(NOT TY_SHARED_MSVCRT)
            set(HOST "${HOST}-mt")
        endif()
    elseif(MINGW)
        set(HOST "${HOST_CPU}-w64-mingw32")
    elseif(CMAKE_COMPILER_IS_GNUCC)
        string(TOLOWER "${HOST_CPU}-${CMAKE_SYSTEM_NAME}-gcc" HOST)
    elseif(CMAKE_COMPILER_IS_CLANG)
        string(TOLOWER "${HOST_CPU}-${CMAKE_SYSTEM_NAME}-clang" HOST)
    else()
        string(TOLOWER "${HOST_CPU}-${CMAKE_SYSTEM_NAME}-${CMAKE_C_COMPILER_ID}" HOST)
    endif()

    find_package(Qt5 REQUIRED
        COMPONENTS Widgets Network PrintSupport
        HINTS "${CMAKE_SOURCE_DIR}/qt5/${HOST}"
        NO_CMAKE_FIND_ROOT_PATH)
endif()

if(Qt5_FOUND AND NOT TARGET EasyQt5)
    add_library(EasyQt5 INTERFACE)

    # Static libraries are painful. Be careful when you touch this, it was made
    # through an extremely evolved trial and error process :]
    get_target_property(QT5_TYPE Qt5::Core TYPE)
    if(QT5_TYPE STREQUAL "STATIC_LIBRARY")
        get_target_property(QT5_DIRECTORY Qt5::Core LOCATION)
        get_filename_component(QT5_DIRECTORY "${QT5_DIRECTORY}" DIRECTORY)
        get_filename_component(QT5_DIRECTORY "${QT5_DIRECTORY}" DIRECTORY)
        set(QT5_LIBRARY_DIRECTORIES "${QT5_DIRECTORY}/lib" "${QT5_DIRECTORY}/plugins/platforms")

        if(WIN32)
            # Fix undefined reference to _imp__WSAAsyncSelect@16
            set_property(TARGET Qt5::Network APPEND PROPERTY INTERFACE_LINK_LIBRARIES ws2_32)

            # Why is there no config package for this?
            find_library(Qt5PlatformSupport_LIBRARIES Qt5PlatformSupport
                HINTS ${QT5_LIBRARY_DIRECTORIES}
                NO_CMAKE_FIND_ROOT_PATH)
            find_library(qtpcre_LIBRARIES qtpcre
                HINTS ${QT5_LIBRARY_DIRECTORIES}
                NO_CMAKE_FIND_ROOT_PATH)
            find_library(qtfreetype_LIBRARIES qtfreetype
                HINTS ${QT5_LIBRARY_DIRECTORIES}
                NO_CMAKE_FIND_ROOT_PATH)
            find_library(qwindows_LIBRARIES qwindows
                HINTS ${QT5_LIBRARY_DIRECTORIES}
                NO_CMAKE_FIND_ROOT_PATH)

            target_link_libraries(EasyQt5 INTERFACE
                Qt5::QWindowsIntegrationPlugin imm32 winmm
                Qt5::Core Qt5::Widgets Qt5::Network
                ${Qt5PlatformSupport_LIBRARIES} ${qtpcre_LIBRARIES} ${qtfreetype_LIBRARIES})
        elseif(APPLE)
            find_library(COCOA_LIBRARIES Cocoa)
            find_library(CARBON_LIBRARIES Carbon)
            find_library(SECURITY_LIBRARIES Security)
            find_library(SC_LIBRARIES SystemConfiguration)
            find_package(ZLIB REQUIRED)
            find_package(Cups REQUIRED)
            find_package(OpenGL REQUIRED)

            find_library(Qt5PlatformSupport_LIBRARIES Qt5PlatformSupport
                HINTS ${QT5_LIBRARY_DIRECTORIES}
                NO_CMAKE_FIND_ROOT_PATH)
            find_library(qtpcre_LIBRARIES qtpcre
                HINTS ${QT5_LIBRARY_DIRECTORIES}
                NO_CMAKE_FIND_ROOT_PATH)

            target_link_libraries(EasyQt5 INTERFACE
                Qt5::QCocoaIntegrationPlugin Qt5::PrintSupport
                Qt5::Core Qt5::Widgets Qt5::Network
                ${Qt5PlatformSupport_LIBRARIES} ${qtpcre_LIBRARIES}
                ${COCOA_LIBRARIES} ${CARBON_LIBRARIES} ${SECURITY_LIBRARIES}
                ${ZLIB_LIBRARIES} ${CUPS_LIBRARIES} ${SC_LIBRARIES} ${OPENGL_LIBRARIES})
        endif()
    else()
        target_link_libraries(EasyQt5 INTERFACE Qt5::Core Qt5::Widgets Qt5::Network)

        if(WIN32 OR APPLE)
            message(WARNING "Cannot package Qt5 shared libraries")
        endif()
    endif()
endif()
