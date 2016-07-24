# ty, a collection of GUI and command-line tools to manage Teensy devices
#
# Distributed under the MIT license (see LICENSE.txt or http://opensource.org/licenses/MIT)
# Copyright (c) 2015 Niels Martignène <niels.martignene@gmail.com>

add_executable(test_libty test_libty.c
                          test_optline.c)
target_link_libraries(test_libty ${LIBHS_LIBRARIES} libty)
add_test(NAME libty COMMAND test_libty)
