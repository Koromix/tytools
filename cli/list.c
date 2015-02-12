/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <getopt.h>
#include "ty.h"
#include "main.h"

struct capability_description {
    uint32_t cap;
    const char *desc;
};

enum {
    OPTION_HELP = 0x100
};

static const char *short_options = "vw";
static const struct option long_options[] = {
    {"help",    no_argument, NULL, OPTION_HELP},
    {"verbose", no_argument, NULL, 'v'},
    {"watch",   no_argument, NULL, 'w'},
    {0}
};

static const struct capability_description capability_names[] = {
    {TY_BOARD_CAPABILITY_UPLOAD,   "upload"},
    {TY_BOARD_CAPABILITY_RESET,    "reset"},
    {TY_BOARD_CAPABILITY_SERIAL,   "serial"},
    {TY_BOARD_CAPABILITY_REBOOT,   "reboot"},
    {0}
};

static bool list_verbose = false;
static bool watch = false;

void print_list_usage(void)
{
    fprintf(stderr, "usage: tyc list [--help] [options]\n\n"
                    "Options:\n"
                    "   -v, --verbose            Print detailed information about devices\n"
                    "   -w, --watch              Watch devices dynamically\n");
}

static void print_capabilities(uint16_t capabilities)
{
    bool first = true;
    for (const struct capability_description *c = capability_names; c->desc; c++) {
        if (capabilities & (1 << c->cap)) {
            printf("%s%s", first ? "" : ", ", c->desc);
            first = false;
        }
    }

    if (first)
        printf("(none)");
}

static int print_interface_info(ty_board *board, ty_board_interface *iface, void *udata)
{
    TY_UNUSED(board);
    TY_UNUSED(udata);

    printf("    * %s: %s\n", ty_board_interface_get_desc(iface),
           ty_device_get_path(ty_board_interface_get_device(iface)));

    return 0;
}

static void print_interfaces(ty_board *board)
{
    ty_board_list_interfaces(board, print_interface_info, NULL);
}

static int list_callback(ty_board *board, ty_board_event event, void *udata)
{
    TY_UNUSED(event);
    TY_UNUSED(udata);

    const ty_board_model *model = ty_board_get_model(board);

    // Suppress spurious warning on MinGW (c may be used undefined)
    int c = 0;

    switch (event) {
    case TY_BOARD_EVENT_ADDED:
        c = '+';
        break;
    case TY_BOARD_EVENT_CHANGED:
        c = '=';
        break;
    case TY_BOARD_EVENT_DISAPPEARED:
        c = '?';
        break;
    case TY_BOARD_EVENT_DROPPED:
        c = '-';
        break;
    }
    assert(c);

    printf("%c %s#%"PRIu64" %s\n", c, ty_board_get_location(board), ty_board_get_serial_number(board),
           model ? ty_board_model_get_name(model) : "(unknown)");

    if (list_verbose && event != TY_BOARD_EVENT_DROPPED) {
        printf("  - model: %s\n", model ? ty_board_model_get_desc(model) : "(unknown)");

        printf("  - capabilities: ");
        print_capabilities(ty_board_get_capabilities(board));
        printf("\n");

        if (event != TY_BOARD_EVENT_DISAPPEARED) {
            printf("  - interfaces: \n");
            print_interfaces(board);
        } else {
            printf("  - interfaces: (none)\n");
        }
    }

    return 0;
}

int list(int argc, char *argv[])
{
    ty_board_manager *manager;
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

        case 'w':
            watch = true;
            break;

        default:
            goto usage;
        }
    }

    r = get_manager(&manager);
    if (r < 0)
        return r;

    r = ty_board_manager_list(manager, list_callback, NULL);
    if (r < 0)
        return r;

    if (watch) {
        r = ty_board_manager_register_callback(manager, list_callback, NULL);
        if (r < 0)
            return r;

        r = ty_board_manager_wait(manager, NULL, NULL, -1);
        if (r < 0)
            return r;
    }

    return 0;

usage:
    print_list_usage();
    return TY_ERROR_PARAM;
}
