/*
 * ty, a collection of GUI and command-line tools to manage Teensy devices
 *
 * Distributed under the MIT license (see LICENSE.txt or http://opensource.org/licenses/MIT)
 * Copyright (c) 2015 Niels Martignène <niels.martignene@gmail.com>
 */

#ifndef MAIN_H
#define MAIN_H

#include "ty.h"

TY_C_BEGIN

struct tyb_monitor;
struct tyb_board;

void print_main_options(FILE *f);

int parse_main_option(int argc, char *argv[], int c);

int get_manager(tyb_monitor **rmanager);
int get_board(tyb_board **rboard);

enum {
    MAIN_OPTION_HELP = 0x100,
    MAIN_OPTION_VERSION,

    MAIN_OPTION_BOARD,

    MAIN_OPTION_EXPERIMENTAL
};

#define MAIN_SHORT_OPTIONS
#define MAIN_LONG_OPTIONS \
    {"help",         no_argument,       NULL, MAIN_OPTION_HELP}, \
    {"version",      no_argument,       NULL, MAIN_OPTION_VERSION}, \
    \
    {"board",        required_argument, NULL, MAIN_OPTION_BOARD}, \
    \
    {"experimental", no_argument,       NULL, MAIN_OPTION_EXPERIMENTAL},

TY_C_END

#endif
