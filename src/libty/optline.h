/* TyTools - public domain
   Niels Martignène <niels.martignene@protonmail.com>
   https://koromix.dev/tytools

   This software is in the public domain. Where that dedication is not
   recognized, you are granted a perpetual, irrevocable license to copy,
   distribute, and modify this file as you see fit.

   See the LICENSE file for more details. */

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

void ty_optline_init(ty_optline_context *ctx, char **args, unsigned int args_count);
void ty_optline_init_argv(ty_optline_context *ctx, int argc, char **argv);

char *ty_optline_next_option(ty_optline_context *ctx);

char *ty_optline_get_option(ty_optline_context *ctx);
char *ty_optline_get_value(ty_optline_context *ctx);
char *ty_optline_consume_non_option(ty_optline_context *ctx);

TY_C_END

#endif
