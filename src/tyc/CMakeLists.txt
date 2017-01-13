# Teensy Tools - public domain
# Niels Martignène <niels.martignene@gmail.com>
# https://neodd.com/teensytools

# This software is in the public domain. Where that dedication is not
# recognized, you are granted a perpetual, irrevocable license to copy,
# distribute, and modify this file as you see fit.

# See the LICENSE file for more details.

set(TYC_SOURCES identify.c
                list.c
                main.c
                monitor.c
                reset.c
                upload.c)

add_executable(tyc ${TYC_SOURCES})
set_target_properties(tyc PROPERTIES OUTPUT_NAME ${CONFIG_TYC_EXECUTABLE})
target_link_libraries(tyc PRIVATE libhs libty)
enable_unity_build(tyc)

if(WIN32)
    install(TARGETS tyc DESTINATION .)
elseif(APPLE)
    if(CONFIG_TYQT_BUILD)
        install(TARGETS tyc RUNTIME DESTINATION TyQt.app/Contents/MacOS)
    endif()
else()
    install(TARGETS tyc RUNTIME DESTINATION bin)
endif()
