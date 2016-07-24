/*
 * ty, a collection of GUI and command-line tools to manage Teensy devices
 *
 * Distributed under the MIT license (see LICENSE.txt or http://opensource.org/licenses/MIT)
 * Copyright (c) 2015 Niels Martignène <niels.martignene@gmail.com>
 */

#ifndef TY_OPTLINE_H
#define TY_OPTLINE_H

#include "common.h"

TY_C_BEGIN

typedef struct ty_optline_context {
    char **args;
    unsigned int count;
    unsigned int index;
    unsigned int limit;
    size_t smallopt_offset;

    char *current_option;
    char *current_value;

    char buf[80];
} ty_optline_context;

TY_PUBLIC void ty_optline_init(ty_optline_context *ctx, char **args, unsigned int args_count);
TY_PUBLIC void ty_optline_init_argv(ty_optline_context *ctx, int argc, char **argv);

TY_PUBLIC char *ty_optline_next_option(ty_optline_context *ctx);

TY_PUBLIC char *ty_optline_get_option(ty_optline_context *ctx);
TY_PUBLIC char *ty_optline_get_value(ty_optline_context *ctx);
TY_PUBLIC char *ty_optline_consume_non_option(ty_optline_context *ctx);

TY_C_END

#endif
