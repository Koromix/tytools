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
#include "ty/monitor.h"
#include "ty/optline.h"

TY_C_BEGIN

extern const char *executable_name;

void print_common_options(FILE *f);
bool parse_common_option(ty_optline_context *optl, char *arg);

int get_monitor(ty_monitor **rmonitor);
int get_board(ty_board **rboard);

TY_C_END

#endif
