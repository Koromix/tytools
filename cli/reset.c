/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <getopt.h>
#include "ty.h"
#include "main.h"

static const char *short_options = MAIN_SHORT_OPTIONS;
static const struct option long_options[] = {
    MAIN_LONG_OPTIONS

    {0}
};

void print_reset_usage(void)
{
    fprintf(stderr, "usage: tyc reset\n");

    print_main_options();
}

int reset(int argc, char *argv[])
{
    ty_board *board;
    int r;

    int c;
    while ((c = getopt_long(argc, argv, short_options, long_options, NULL)) != -1) {
        r = parse_main_option(argc, argv, c);
        if (r <= 0)
            return r;
        break;
    }

    if (argc > optind) {
        ty_error(TY_ERROR_PARAM, "No positional argument is allowed");
        goto usage;
    }

    r = get_board(&board);
    if (r < 0)
        return r;

    if (!ty_board_has_capability(board, TY_BOARD_CAPABILITY_RESET)) {
        printf("Triggering board reboot\n");
        r = ty_board_reboot(board);
        if (r < 0)
            goto cleanup;

        r = ty_board_wait_for(board, TY_BOARD_CAPABILITY_RESET, false, -1);
        if (r < 0)
            goto cleanup;
    }

    if (!ty_board_has_capability(board, TY_BOARD_CAPABILITY_RESET)) {
        r = ty_error(TY_ERROR_MODE, "No way to trigger reset for this board");
        goto cleanup;
    }

    printf("Sending reset command\n");
    r = ty_board_reset(board);
    if (r < 0)
        goto cleanup;

    r = 0;
cleanup:
    ty_board_unref(board);
    return r;

usage:
    print_reset_usage();
    return TY_ERROR_PARAM;
}
