/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef MAIN_H
#define MAIN_H

#include "ty.h"

TY_C_BEGIN

struct ty_board_manager;
struct ty_board;

void print_main_options(void);
void print_supported_models(void);

int parse_main_option(int argc, char *argv[], int c);

int get_manager(ty_board_manager **rmanager);
int get_board(ty_board **rboard);

enum {
    MAIN_OPTION_EXPERIMENTAL = 0x100,
    MAIN_OPTION_HELP,
    MAIN_OPTION_VERSION
};

#define MAIN_SHORT_OPTIONS "b:"
#define MAIN_LONG_OPTIONS \
    {"help",         no_argument,       NULL, MAIN_OPTION_HELP}, \
    {"version",      no_argument,       NULL, MAIN_OPTION_VERSION}, \
    \
    {"board",        required_argument, NULL, 'b'}, \
    {"experimental", no_argument,       NULL, MAIN_OPTION_EXPERIMENTAL}, \

TY_C_END

#endif
