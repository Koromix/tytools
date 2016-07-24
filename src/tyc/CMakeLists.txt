# ty, a collection of GUI and command-line tools to manage Teensy devices
#
# Distributed under the MIT license (see LICENSE.txt or http://opensource.org/licenses/MIT)
# Copyright (c) 2015 Niels Martignène <niels.martignene@gmail.com>

set(TYC_SOURCES list.c
                main.c
                monitor.c
                reset.c
                upload.c)

add_executable(tyc ${TYC_SOURCES})
set_target_properties(tyc PROPERTIES OUTPUT_NAME ${TY_CONFIG_TYC_EXECUTABLE})
target_link_libraries(tyc PRIVATE ${LIBHS_LIBRARIES} libty)

if(WIN32)
    install(TARGETS tyc DESTINATION .)
elseif(APPLE)
    if(TY_CONFIG_TYQT_BUILD)
        install(TARGETS tyc RUNTIME DESTINATION TyQt.app/Contents/MacOS)
    endif()
else()
    install(TARGETS tyc RUNTIME DESTINATION bin)
endif()
