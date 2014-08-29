/**
 * ty, command-line program to manage Teensy devices
 * http://github.com/Koromix/ty
 * Copyright (C) 2014 Niels Martignène
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "common.h"
#include <getopt.h>
#include "board.h"

struct list_context {
    bool verbose;
};

struct capability_description {
    uint32_t cap;
    const char *desc;
};

enum {
    OPTION_HELP = 0x100
};

static const char *short_options = "v";
static const struct option long_options[] = {
    {"help",    no_argument, NULL, OPTION_HELP},
    {"verbose", no_argument, NULL, 'v'},
    {0}
};

static const struct capability_description capabilities[] = {
    {TY_BOARD_CAPABILITY_IDENTIFY, "identify"},
    {TY_BOARD_CAPABILITY_UPLOAD,   "upload"},
    {TY_BOARD_CAPABILITY_RESET,    "reset"},
    {TY_BOARD_CAPABILITY_SERIAL,   "serial"},
    {TY_BOARD_CAPABILITY_REBOOT,   "reboot"},
    {0}
};

static bool list_verbose = false;

void print_list_usage(void)
{
    fprintf(stderr, "usage: ty list [--help] [options]\n\n"
                    "Options:\n"
                    "   -v, --verbose            Print detailed information about devices\n");
}

static void print_capabilities(ty_board *board)
{
    bool first = true;
    for (const struct capability_description *c = capabilities; c->desc; c++) {
        if (ty_board_has_capability(board, c->cap)) {
            printf("%s%s", first ? "" : ", ", c->desc);
            first = false;
        }
    }
    printf("\n");
}

static const char *read_model(ty_board *board)
{
    const ty_board_model *model;
    int r;

    if (!ty_board_has_capability(board, TY_BOARD_CAPABILITY_IDENTIFY))
        return "(unknown)";

    r = ty_board_probe(board, 0);
    if (r < 0)
        return "(error)";

    model = board->model;
    ty_board_close(board);

    // May happen if the device has changed mode since enumeration
    if (!model)
        return "(unknown)";

    return model->desc;
}

static int list_walker(ty_board *board, void *udata)
{
    TY_UNUSED(udata);

    printf("%s#%"PRIu64" (%s)\n", board->dev->path, board->serial, board->mode->desc);
    if (list_verbose) {
        printf("  - node: %s\n", board->dev->node);
        printf("  - model: %s\n", read_model(board));
        printf("  - capabilities: ");
        print_capabilities(board);
    }

    return 1;
}

int list(int argc, char *argv[])
{
    int r;

    int c;
    while ((c = getopt_long(argc, argv, short_options, long_options, NULL)) != -1) {
        switch (c) {
        case OPTION_HELP:
            print_list_usage();
            return 0;

        case 'v':
            list_verbose = true;
            break;

        default:
            goto usage;
        }
    }

    r = ty_board_list(list_walker, NULL);
    if (r < 0)
        return r;

    return 0;

usage:
    print_list_usage();
    return TY_ERROR_PARAM;
}
