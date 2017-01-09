# ty, a collection of GUI and command-line tools to manage Teensy devices
#
# Distributed under the MIT license (see LICENSE.txt or http://opensource.org/licenses/MIT)
# Copyright (c) 2015 Niels Martignène <niels.martignene@gmail.com>

set(ARGV)
math(EXPR argv_end "${CMAKE_ARGC} - 1")
foreach(i RANGE 1 ${argv_end})
    if(CMAKE_ARGV${i} STREQUAL "-P")
        math(EXPR argv_first "${i} + 2")
        foreach(j RANGE ${argv_first} ${argv_end})
            list(APPEND ARGV "${CMAKE_ARGV${j}}")
        endforeach()
        break()
    endif()
endforeach()

list(GET ARGV 0 SRC)
list(GET ARGV 1 DEST)

file(REMOVE "${DEST}")

get_filename_component(src_dir "${SRC}" DIRECTORY)
file(STRINGS "${SRC}" lines)

foreach(line IN LISTS lines)
    if(line MATCHES "#include \"([a-zA-Z0-9_\\-]+\\.[a-zA-Z]+)\"")
        set(include_file "${CMAKE_MATCH_1}")
        list(FIND EXCLUDE "${include_file}" exclude_index)
        if(exclude_index EQUAL -1)
            if(NOT IS_ABSOLUTE "${include_file}")
                set(include_file_full "${src_dir}/${include_file}")
            else()
                set(include_file_full "${include_file}")
            endif()

            file(READ "${include_file_full}" include_content)
            string(REGEX REPLACE "\n$" "" include_content "${include_content}")

            string(FIND "${include_content}" "#ifndef " offset_ifndef)
            string(FIND "${include_content}" "#include " offset_include)
            if(offset_ifndef EQUAL -1 AND offset_include EQUAL -1)
                set(offset 0)
            elseif(offset_include EQUAL -1)
                set(offset ${offset_ifndef})
            elseif(offset_ifndef EQUAL -1)
                set(offset ${offset_include})
            elseif(offset_include GREATER offset_ifndef)
                set(offset ${offset_ifndef})
            else()
                set(offset ${offset_include})
            endif()
            string(SUBSTRING "${include_content}" ${offset} -1 include_content)
            string(REGEX REPLACE "(#include \"[a-zA-Z0-9\\.-_]+\")" "// \\1" include_content "${include_content}")

            file(APPEND "${DEST}" "\
// ${include_file}\n\
// ------------------------------------\n\n\
${include_content}\n\n")
        else()
            file(APPEND "${DEST}" "${line}\n")
        endif()
    else()
        file(APPEND "${DEST}" "${line}\n")
    endif()
endforeach()
