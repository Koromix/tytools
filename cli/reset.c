/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <getopt.h>
#include "ty.h"
#include "main.h"

enum {
    OPTION_HELP = 0x100
};

static const char *short_options = "";
static const struct option long_options[] = {
    {"help", no_argument, NULL, OPTION_HELP},
    {0}
};

void print_reset_usage(void)
{
    fprintf(stderr, "usage: tyc reset [--help]\n");
}

int reset(int argc, char *argv[])
{
    ty_board *board;
    int r;

    int c;
    while ((c = getopt_long(argc, argv, short_options, long_options, NULL)) != -1) {
        switch (c) {
        case OPTION_HELP:
            print_reset_usage();
            return 0;

        default:
            goto usage;
        }
    }

    r = get_board(&board);
    if (r < 0)
        return r;

    if (!ty_board_has_capability(board, TY_BOARD_CAPABILITY_RESET)) {
        printf("Triggering board reboot\n");
        r = ty_board_reboot(board);
        if (r < 0)
            goto cleanup;

        r = ty_board_wait_for(board, TY_BOARD_CAPABILITY_RESET, -1);
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
