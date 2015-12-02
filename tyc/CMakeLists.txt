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
target_link_libraries(tyc ${TY_LIBRARIES})

if(WIN32)
    install(TARGETS tyc DESTINATION .)
elseif(APPLE)
    # Kind of a hack, but it works (the Bundle generator want to put everything in Resources)
    install(TARGETS tyc DESTINATION ../MacOS)
else()
    install(TARGETS tyc RUNTIME DESTINATION bin)
endif()
