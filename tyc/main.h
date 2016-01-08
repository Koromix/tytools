/*
 * ty, a collection of GUI and command-line tools to manage Teensy devices
 *
 * Distributed under the MIT license (see LICENSE.txt or http://opensource.org/licenses/MIT)
 * Copyright (c) 2015 Niels Martignène <niels.martignene@gmail.com>
 */

#ifndef MAIN_H
#define MAIN_H

#include "ty/common.h"
#include "ty/board.h"

TY_C_BEGIN

struct tyb_monitor;
struct tyb_board;

void print_common_options(FILE *f);
bool parse_common_option(int argc, char *argv[], int c);

int get_manager(tyb_monitor **rmanager);
int get_board(tyb_board **rboard);

enum {
    COMMON_OPTION_HELP = 0x100,
    COMMON_OPTION_BOARD
};

#define COMMON_SHORT_OPTIONS ":q"
#define COMMON_LONG_OPTIONS \
    {"help",         no_argument,       NULL, COMMON_OPTION_HELP}, \
    {"board",        required_argument, NULL, COMMON_OPTION_BOARD}, \
    {"quiet",        no_argument,       NULL, 'q'},

#define HANDLE_COMMON_OPTIONS(c, usage) \
    case COMMON_OPTION_HELP: \
        usage(stdout); \
        return EXIT_SUCCESS; \
    default: \
        if (!parse_common_option(argc, argv, (c))) { \
            usage(stderr); \
            return EXIT_SUCCESS; \
        } \
        break;

TY_C_END

#endif
