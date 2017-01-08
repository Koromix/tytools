# ty, a collection of GUI and command-line tools to manage Teensy devices
#
# Distributed under the MIT license (see LICENSE.txt or http://opensource.org/licenses/MIT)
# Copyright (c) 2015 Niels Martignène <niels.martignene@gmail.com>

if(CMAKE_BUILD_TYPE STREQUAL "Release" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo" OR
   CMAKE_BUILD_TYPE STREQUAL "MinSizeRel")
    set(USE_UNITY_BUILDS ON CACHE BOOL "Use single-TU builds (aka. Unity builds)")
else()
    set(USE_UNITY_BUILDS OFF CACHE BOOL "Use single-TU builds (aka. Unity builds)")
endif()
if(USE_UNITY_BUILDS)
    function(enable_unity_build TARGET)
        get_target_property(sources ${TARGET} SOURCES)
        string(GENEX_STRIP "${sources}" sources)

        set(unity_file_c "${CMAKE_CURRENT_BINARY_DIR}/${TARGET}_unity.c")
        set(unity_file_cpp "${CMAKE_CURRENT_BINARY_DIR}/${TARGET}_unity.cpp")
        file(REMOVE ${unity_file_c} ${unity_file_cpp})

        foreach(src ${sources})
            get_source_file_property(language ${src} LANGUAGE)
            if(IS_ABSOLUTE ${src})
                set(src_full ${src})
            else()
                set(src_full "${CMAKE_CURRENT_SOURCE_DIR}/${src}")
            endif()
            if(language STREQUAL "C")
                set_source_files_properties(${src} PROPERTIES HEADER_FILE_ONLY 1)
                file(APPEND ${unity_file_c} "#include \"${src_full}\"\n")
            elseif(language STREQUAL "CXX")
                set_source_files_properties(${src} PROPERTIES HEADER_FILE_ONLY 1)
                file(APPEND ${unity_file_cpp} "#include \"${src_full}\"\n")
            endif()
        endforeach()

        if(EXISTS ${unity_file_c})
            target_sources(${TARGET} PRIVATE ${unity_file_c})
        endif()
        if(EXISTS ${unity_file_cpp})
            target_sources(${TARGET} PRIVATE ${unity_file_cpp})
        endif()
    endfunction()
else()
    function(enable_unity_build TARGET)
    endfunction()
endif()

set(utility_source_dir "${CMAKE_CURRENT_LIST_DIR}")
function(add_amalgamated_file TARGET DEST SRC)
    cmake_parse_arguments("OPT" "" "" "EXCLUDE" ${ARGN})

    get_filename_component(DEST "${DEST}" REALPATH BASE_DIR "${CMAKE_CURRENT_BINARY_DIR}")
    get_filename_component(SRC "${SRC}" REALPATH BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")

    # Without that the semicolons are turned into spaces... Fuck CMake.
    string(REPLACE ";" "\\;" opt_exclude_escaped "${OPT_EXCLUDE}")
    add_custom_command(
        TARGET "${TARGET}" POST_BUILD
        COMMAND ${CMAKE_COMMAND}
            -DEXCLUDE="${opt_exclude_escaped}" -P "${utility_source_dir}/AmalgamateSourceFiles.cmake" "${SRC}" "${DEST}")

    target_sources(${TARGET} PRIVATE "${SRC}")
    set_source_files_properties("${SRC}" PROPERTIES HEADER_FILE_ONLY 1)
endfunction()
