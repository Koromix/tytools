/* TyTools - public domain
   Niels Martignène <niels.martignene@protonmail.com>
   https://koromix.dev/tytools

   This software is in the public domain. Where that dedication is not
   recognized, you are granted a perpetual, irrevocable license to copy,
   distribute, and modify this file as you see fit.

   See the LICENSE file for more details. */

#include "common_priv.h"
#include "optline.h"

void ty_optline_init(ty_optline_context *ctx, char **args, unsigned int args_count)
{
    assert(ctx);
    assert(args || !args_count);

    memset(ctx, 0, sizeof(*ctx));
    ctx->args = args;
    ctx->limit = args_count;
    ctx->count = args_count;
}

void ty_optline_init_argv(ty_optline_context *ctx, int argc, char **argv)
{
    assert(ctx);
    assert(argc >= 0);
    assert(argv || !argc);

    if (argc > 0) {
        ty_optline_init(ctx, argv + 1, (unsigned int)argc - 1);
    } else {
        ty_optline_init(ctx, NULL, 0);
    }
}

static inline bool is_opt(const char *arg)
{
    return arg[0] == '-' && arg[1];
}

static inline bool is_longopt(const char *arg) {
    return arg[0] == '-' && arg[1] == '-' && arg[2];
}

static inline bool is_dashdash(const char *arg)
{
    return arg[0] == '-' && arg[1] == '-' && !arg[2];
}

static void reverse(char **args, unsigned int start, unsigned int end)
{
    for (unsigned int i = 0; i < (end - start) / 2; i++) {
        char *tmp = args[start + i];
        args[start + i] = args[end - i - 1];
        args[end - i - 1] = tmp;
    }
}

static void rotate(char **args, unsigned int start, unsigned int mid, unsigned int end)
{
    if (start == mid || mid == end)
        return;

    reverse(args, start, mid);
    reverse(args, mid, end);
    reverse(args, start, end);
}

char *ty_optline_next_option(ty_optline_context *ctx)
{
    assert(ctx);

    unsigned int next_index;
    char *opt;

    ctx->current_option = NULL;
    ctx->current_value = NULL;

    /* Support aggregate short options, such as '-fbar'. Note that this can also be parsed
       as the short option '-f' with value 'bar', if the user calls ty_optline_get_value(). */
    if (ctx->smallopt_offset) {
        opt = ctx->args[ctx->index];
        ctx->smallopt_offset++;
        if (opt[ctx->smallopt_offset]) {
            ctx->buf[1] = opt[ctx->smallopt_offset];
            ctx->current_option = ctx->buf;
            return ctx->current_option;
        } else {
            ctx->smallopt_offset = 0;
            ctx->index++;
        }
    }

    /* Skip non-options, and do the permutation once we reach an option or the
       end of args. */
    next_index = ctx->index;
    while (next_index < ctx->limit && !is_opt(ctx->args[next_index]))
        next_index++;
    rotate(ctx->args, ctx->index, next_index, ctx->count);
    ctx->limit -= (next_index - ctx->index);
    if (ctx->index >= ctx->limit)
        return NULL;
    opt = ctx->args[ctx->index];

    if (is_longopt(opt)) {
        char *needle = strchr(opt, '=');
        if (needle) {
            /* We can reorder args, but we don't want to change strings. So copy the
               option up to '=' in our buffer. And store the part after '=' as the
               current value. */
            size_t len = TY_MIN((size_t)(needle - opt), sizeof(ctx->buf) - 1);
            memcpy(ctx->buf, opt, len);
            ctx->buf[len] = 0;
            ctx->current_option = ctx->buf;
            ctx->current_value = needle + 1;
        } else {
            ctx->current_option = opt;
        }
        ctx->index++;
    } else if (is_dashdash(opt)) {
        /* We may have previously moved non-options to the end of args. For example,
           at this point 'a b c -- d e' is reordered to '-- d e a b c'. Fix it. */
        rotate(ctx->args, ctx->index + 1, ctx->limit, ctx->count);
        ctx->limit = ctx->index;
        ctx->index++;
    } else if (opt[2]) {
        /* We either have aggregated short options or one short option with a value,
           depending on whether or not the user calls ty_optline_get_opt_value(). */
        ctx->buf[0] = '-';
        ctx->buf[1] = opt[1];
        ctx->buf[2] = 0;
        ctx->current_option = ctx->buf;
        ctx->smallopt_offset = 1;
    } else {
        ctx->current_option = opt;
        ctx->index++;
    }

    return ctx->current_option;
}

char *ty_optline_get_option(ty_optline_context *ctx)
{
    assert(ctx);
    return ctx->current_option;
}

char *ty_optline_get_value(ty_optline_context *ctx)
{
    assert(ctx);

    if (!ctx->current_value) {
        char *arg = ctx->args[ctx->index];

        /* Support '-fbar' where bar is the value, but only for the first short option
           if it's an aggregate. */
        if (ctx->smallopt_offset == 1 && arg[2]) {
            ctx->smallopt_offset = 0;
            ctx->current_value = arg + 2;
            ctx->index++;
        /* Support '-f bar' and '--foo bar', see ty_optline_next_option() for '--foo=bar'. */
        } else if (!ctx->smallopt_offset && ctx->index < ctx->limit &&
                   !is_opt(ctx->args[ctx->index])) {
            ctx->current_value = ctx->args[ctx->index];
            ctx->index++;
        }
    }

    return ctx->current_value;
}

char *ty_optline_consume_non_option(ty_optline_context *ctx)
{
    assert(ctx);

    if (ctx->index >= ctx->count)
        return NULL;
    /* Beyond limit there are only non-options, the limit is moved when we move non-options to
       the end or upon encouteering a double dash '--'. */
    if (ctx->index < ctx->limit && is_opt(ctx->args[ctx->index]))
        return NULL;

    return ctx->args[ctx->index++];
}
