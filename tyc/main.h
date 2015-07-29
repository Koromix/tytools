/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
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
    MAIN_OPTION_BOARD = 0x100,
    MAIN_OPTION_EXPERIMENTAL,
    MAIN_OPTION_HELP,
    MAIN_OPTION_VERSION
};

#define MAIN_SHORT_OPTIONS
#define MAIN_LONG_OPTIONS \
    {"help",         no_argument,       NULL, MAIN_OPTION_HELP}, \
    {"version",      no_argument,       NULL, MAIN_OPTION_VERSION}, \
    \
    {"board",        required_argument, NULL, MAIN_OPTION_BOARD}, \
    {"experimental", no_argument,       NULL, MAIN_OPTION_EXPERIMENTAL}, \

TY_C_END

#endif
