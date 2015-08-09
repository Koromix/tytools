# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

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
