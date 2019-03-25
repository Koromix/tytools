/* TyTools - public domain
   Niels Martignène <niels.martignene@protonmail.com>
   https://koromix.dev/tytools

   This software is in the public domain. Where that dedication is not
   recognized, you are granted a perpetual, irrevocable license to copy,
   distribute, and modify this file as you see fit.

   See the LICENSE file for more details. */

#ifndef MAIN_H
#define MAIN_H

#include "../libty/common.h"
#include "../libty/board.h"
#include "../libty/class.h"
#include "../libty/monitor.h"
#include "../libty/optline.h"

TY_C_BEGIN

extern const char *tycmd_executable_name;

void print_common_options(FILE *f);
bool parse_common_option(ty_optline_context *optl, char *arg);

int get_monitor(ty_monitor **rmonitor);
int get_board(ty_board **rboard);

TY_C_END

#endif
