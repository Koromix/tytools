# TyTools - public domain
# Niels Martignène <niels.martignene@protonmail.com>
# https://koromix.dev/tytools

# This software is in the public domain. Where that dedication is not
# recognized, you are granted a perpetual, irrevocable license to copy,
# distribute, and modify this file as you see fit.

# See the LICENSE file for more details.

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
            set(HOST "${HOST_CPU}-win32-msvc")
        else()
            message(FATAL_ERROR "Only Visual Studio 2015 and later versions are supported")
        endif()
        if(NOT USE_SHARED_MSVCRT)
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

    if (CMAKE_CROSSCOMPILING)
        find_package(Qt5
            COMPONENTS Widgets Network PrintSupport
            HINTS "${CMAKE_SOURCE_DIR}/lib/qt5/${HOST}"
                  "${CMAKE_SOURCE_DIR}/qt5/${HOST}"
            NO_SYSTEM_ENVIRONMENT_PATH
            NO_CMAKE_SYSTEM_PATH
            NO_CMAKE_SYSTEM_PACKAGE_REGISTRY)
    else()
        find_package(Qt5
            COMPONENTS Widgets Network PrintSupport
            HINTS "${CMAKE_SOURCE_DIR}/lib/qt5/${HOST}"
                  "${CMAKE_SOURCE_DIR}/qt5/${HOST}"
                  "/usr/local/opt/qt5")
    endif()
endif()

if(Qt5_FOUND AND NOT TARGET EasyQt5)
    add_library(EasyQt5 INTERFACE)

    get_target_property(Qt5_TYPE Qt5::Core TYPE)
    get_target_property(Qt5_LOCATION Qt5::Core LOCATION)
    message(STATUS "Found Qt5: ${Qt5_LOCATION} (${Qt5_TYPE})")

    # Static libraries are painful. Be careful when you touch this, it was made
    # through an extremely evolved trial and error process :]
    if(Qt5_TYPE STREQUAL "STATIC_LIBRARY")
        get_filename_component(Qt5_DIRECTORY "${Qt5_LOCATION}" DIRECTORY)
        get_filename_component(Qt5_DIRECTORY "${Qt5_DIRECTORY}" DIRECTORY)
        set(Qt5_LIBRARY_DIRECTORIES
            "${Qt5_DIRECTORY}/lib" "${Qt5_DIRECTORY}/plugins/platforms" "${Qt5_DIRECTORY}/plugins/styles")

        if(WIN32)
            # Fix undefined reference to _imp__WSAAsyncSelect@16
            set_property(TARGET Qt5::Network APPEND PROPERTY INTERFACE_LINK_LIBRARIES ws2_32)

            # Why is there no config package for this?
            find_library(qtpcre_LIBRARIES NAMES qtpcre qtpcre2
                HINTS ${Qt5_LIBRARY_DIRECTORIES}
                NO_CMAKE_ENVIRONMENT_PATH NO_CMAKE_PATH NO_SYSTEM_ENVIRONMENT_PATH
                NO_CMAKE_SYSTEM_PATH NO_CMAKE_FIND_ROOT_PATH)
            find_library(qtpng_LIBRARIES NAMES qtpng qtlibpng
                HINTS ${Qt5_LIBRARY_DIRECTORIES}
                NO_CMAKE_ENVIRONMENT_PATH NO_CMAKE_PATH NO_SYSTEM_ENVIRONMENT_PATH
                NO_CMAKE_SYSTEM_PATH NO_CMAKE_FIND_ROOT_PATH)
            find_library(qtfreetype_LIBRARIES qtfreetype
                HINTS ${Qt5_LIBRARY_DIRECTORIES}
                NO_CMAKE_ENVIRONMENT_PATH NO_CMAKE_PATH NO_SYSTEM_ENVIRONMENT_PATH
                NO_CMAKE_SYSTEM_PATH NO_CMAKE_FIND_ROOT_PATH)
            find_library(qwindows_LIBRARIES qwindows
                HINTS ${Qt5_LIBRARY_DIRECTORIES}
                NO_CMAKE_ENVIRONMENT_PATH NO_CMAKE_PATH NO_SYSTEM_ENVIRONMENT_PATH
                NO_CMAKE_SYSTEM_PATH NO_CMAKE_FIND_ROOT_PATH)
            find_library(qwindowsvistastyle_LIBRARIES qwindowsvistastyle
                HINTS ${Qt5_LIBRARY_DIRECTORIES}
                NO_CMAKE_ENVIRONMENT_PATH NO_CMAKE_PATH NO_SYSTEM_ENVIRONMENT_PATH
                NO_CMAKE_SYSTEM_PATH NO_CMAKE_FIND_ROOT_PATH)
            find_library(Qt5WindowsUIAutomationSupport_LIBRARIES Qt5WindowsUIAutomationSupport
                HINTS ${Qt5_LIBRARY_DIRECTORIES}
                NO_CMAKE_ENVIRONMENT_PATH NO_CMAKE_PATH NO_SYSTEM_ENVIRONMENT_PATH
                NO_CMAKE_SYSTEM_PATH NO_CMAKE_FIND_ROOT_PATH)
            find_library(Qt5FontDatabaseSupport_LIBRARIES Qt5FontDatabaseSupport
                HINTS ${Qt5_LIBRARY_DIRECTORIES}
                NO_CMAKE_ENVIRONMENT_PATH NO_CMAKE_PATH NO_SYSTEM_ENVIRONMENT_PATH
                NO_CMAKE_SYSTEM_PATH NO_CMAKE_FIND_ROOT_PATH)
            find_library(Qt5EventDispatcherSupport_LIBRARIES Qt5EventDispatcherSupport
                HINTS ${Qt5_LIBRARY_DIRECTORIES}
                NO_CMAKE_ENVIRONMENT_PATH NO_CMAKE_PATH NO_SYSTEM_ENVIRONMENT_PATH
                NO_CMAKE_SYSTEM_PATH NO_CMAKE_FIND_ROOT_PATH)
            find_library(Qt5ThemeSupport_LIBRARIES Qt5ThemeSupport
                HINTS ${Qt5_LIBRARY_DIRECTORIES}
                NO_CMAKE_ENVIRONMENT_PATH NO_CMAKE_PATH NO_SYSTEM_ENVIRONMENT_PATH
                NO_CMAKE_SYSTEM_PATH NO_CMAKE_FIND_ROOT_PATH)

            target_link_libraries(EasyQt5 INTERFACE
                Qt5::QWindowsIntegrationPlugin Qt5::Core Qt5::Widgets Qt5::Network
                imm32 winmm dwmapi version wtsapi32 netapi32 userenv uxtheme
                ${Qt5WindowsUIAutomationSupport_LIBRARIES} ${Qt5FontDatabaseSupport_LIBRARIES}
                ${Qt5EventDispatcherSupport_LIBRARIES} ${Qt5ThemeSupport_LIBRARIES}
                ${qwindows_LIBRARIES} ${qwindowsvistastyle_LIBRARIES}
                ${qtpcre_LIBRARIES} ${qtpng_LIBRARIES} ${qtfreetype_LIBRARIES})
        elseif(APPLE)
            find_library(COCOA_LIBRARIES Cocoa)
            find_library(CARBON_LIBRARIES Carbon)
            find_library(SECURITY_LIBRARIES Security)
            find_library(SC_LIBRARIES SystemConfiguration)
            find_package(ZLIB REQUIRED)
            find_package(Cups REQUIRED)
            find_package(OpenGL REQUIRED)

            find_library(qtpcre_LIBRARIES NAMES qtpcre qtpcre2
                HINTS ${Qt5_LIBRARY_DIRECTORIES}
                NO_CMAKE_ENVIRONMENT_PATH NO_CMAKE_PATH NO_SYSTEM_ENVIRONMENT_PATH
                NO_CMAKE_SYSTEM_PATH NO_CMAKE_FIND_ROOT_PATH)
            find_library(qtpng_LIBRARIES NAMES qtpng qtlibpng
                HINTS ${Qt5_LIBRARY_DIRECTORIES}
                NO_CMAKE_ENVIRONMENT_PATH NO_CMAKE_PATH NO_SYSTEM_ENVIRONMENT_PATH
                NO_CMAKE_SYSTEM_PATH NO_CMAKE_FIND_ROOT_PATH)
            find_library(qcocoa_LIBRARIES qcocoa
                HINTS ${Qt5_LIBRARY_DIRECTORIES}
                NO_CMAKE_ENVIRONMENT_PATH NO_CMAKE_PATH NO_SYSTEM_ENVIRONMENT_PATH
                NO_CMAKE_SYSTEM_PATH NO_CMAKE_FIND_ROOT_PATH)
            find_library(Qt5FontDatabaseSupport_LIBRARIES Qt5FontDatabaseSupport
                HINTS ${Qt5_LIBRARY_DIRECTORIES}
                NO_CMAKE_ENVIRONMENT_PATH NO_CMAKE_PATH NO_SYSTEM_ENVIRONMENT_PATH
                NO_CMAKE_SYSTEM_PATH NO_CMAKE_FIND_ROOT_PATH)
            find_library(Qt5EventDispatcherSupport_LIBRARIES Qt5EventDispatcherSupport
                HINTS ${Qt5_LIBRARY_DIRECTORIES}
                NO_CMAKE_ENVIRONMENT_PATH NO_CMAKE_PATH NO_SYSTEM_ENVIRONMENT_PATH
                NO_CMAKE_SYSTEM_PATH NO_CMAKE_FIND_ROOT_PATH)
            find_library(Qt5ThemeSupport_LIBRARIES Qt5ThemeSupport
                HINTS ${Qt5_LIBRARY_DIRECTORIES}
                NO_CMAKE_ENVIRONMENT_PATH NO_CMAKE_PATH NO_SYSTEM_ENVIRONMENT_PATH
                NO_CMAKE_SYSTEM_PATH NO_CMAKE_FIND_ROOT_PATH)
            find_library(Qt5ClipboardSupport_LIBRARIES Qt5ClipboardSupport
                HINTS ${Qt5_LIBRARY_DIRECTORIES}
                NO_CMAKE_ENVIRONMENT_PATH NO_CMAKE_PATH NO_SYSTEM_ENVIRONMENT_PATH
                NO_CMAKE_SYSTEM_PATH NO_CMAKE_FIND_ROOT_PATH)
            find_library(Qt5GraphicsSupport_LIBRARIES Qt5GraphicsSupport
                HINTS ${Qt5_LIBRARY_DIRECTORIES}
                NO_CMAKE_ENVIRONMENT_PATH NO_CMAKE_PATH NO_SYSTEM_ENVIRONMENT_PATH
                NO_CMAKE_SYSTEM_PATH NO_CMAKE_FIND_ROOT_PATH)
            find_library(Qt5AccessibilitySupport_LIBRARIES Qt5AccessibilitySupport
                HINTS ${Qt5_LIBRARY_DIRECTORIES}
                NO_CMAKE_ENVIRONMENT_PATH NO_CMAKE_PATH NO_SYSTEM_ENVIRONMENT_PATH
                NO_CMAKE_SYSTEM_PATH NO_CMAKE_FIND_ROOT_PATH)
            find_library(Qt5CglSupport_LIBRARIES Qt5CglSupport
                HINTS ${Qt5_LIBRARY_DIRECTORIES}
                NO_CMAKE_ENVIRONMENT_PATH NO_CMAKE_PATH NO_SYSTEM_ENVIRONMENT_PATH
                NO_CMAKE_SYSTEM_PATH NO_CMAKE_FIND_ROOT_PATH)

            target_link_libraries(EasyQt5 INTERFACE
                Qt5::QCocoaIntegrationPlugin Qt5::PrintSupport
                Qt5::Core Qt5::Widgets Qt5::Network
                ${qcocoa_LIBRARIES} ${Qt5CglSupport_LIBRARIES}
                ${Qt5FontDatabaseSupport_LIBRARIES} ${Qt5ClipboardSupport_LIBRARIES}
                ${Qt5EventDispatcherSupport_LIBRARIES} ${Qt5ThemeSupport_LIBRARIES}
                ${Qt5GraphicsSupport_LIBRARIES} ${Qt5AccessibilitySupport_LIBRARIES}
                ${qtpcre_LIBRARIES} ${qtpng_LIBRARIES}
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
