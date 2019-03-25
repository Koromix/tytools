# TyTools - public domain
# Niels Martignène <niels.martignene@protonmail.com>
# https://koromix.dev/tytools

# This software is in the public domain. Where that dedication is not
# recognized, you are granted a perpetual, irrevocable license to copy,
# distribute, and modify this file as you see fit.

# See the LICENSE file for more details.

set(TYCMD_SOURCES identify.c
                  list.c
                  main.c
                  main.h
                  monitor.c
                  reset.c
                  upload.c)

add_executable(tycmd ${TYCMD_SOURCES})
set_target_properties(tycmd PROPERTIES OUTPUT_NAME ${CONFIG_TYCMD_EXECUTABLE})
target_link_libraries(tycmd PRIVATE libhs libty)
enable_unity_build(tycmd)

if(WIN32)
    install(TARGETS tycmd DESTINATION .)
elseif(APPLE)
    if(CONFIG_TYCOMMANDER_BUILD)
        install(TARGETS tycmd RUNTIME DESTINATION "${CONFIG_TYCOMMANDER_EXECUTABLE}.app/Contents/MacOS")
    endif()
else()
    install(TARGETS tycmd RUNTIME DESTINATION bin)
endif()
