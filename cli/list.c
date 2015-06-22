/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <getopt.h>
#include "ty.h"
#include "main.h"

static const char *short_options = MAIN_SHORT_OPTIONS "vw";
static const struct option long_options[] = {
    MAIN_LONG_OPTIONS

    {"verbose", no_argument, NULL, 'v'},
    {"watch",   no_argument, NULL, 'w'},
    {0}
};

static bool list_verbose = false;
static bool watch = false;

void print_list_usage(FILE *f)
{
    fprintf(f, "usage: tyc list [options]\n\n");

    print_main_options(f);
    fprintf(f, "\n");

    fprintf(f, "List options:\n"
               "   -v, --verbose            Print detailed information about devices\n"
               "   -w, --watch              Watch devices dynamically\n");
}

static void print_capabilities(int capabilities)
{
    bool first = true;
    for (unsigned int i = 0; i < TYB_BOARD_CAPABILITY_COUNT; i++) {
        if (capabilities & (1 << i)) {
            printf("%s%s", first ? "" : ", ", tyb_board_capability_get_name(i));
            first = false;
        }
    }

    if (first)
        printf("(none)");
}

static int print_interface_info(tyb_board_interface *iface, void *udata)
{
    TY_UNUSED(udata);

    printf("    * %s: %s\n", tyb_board_interface_get_desc(iface),
           tyd_device_get_path(tyb_board_interface_get_device(iface)));

    return 0;
}

static int list_callback(tyb_board *board, tyb_monitor_event event, void *udata)
{
    TY_UNUSED(event);
    TY_UNUSED(udata);

    const tyb_board_model *model = tyb_board_get_model(board);

    // Suppress spurious warning on MinGW (c may be used undefined)
    int c = 0;

    switch (event) {
    case TYB_MONITOR_EVENT_ADDED:
        c = '+';
        break;
    case TYB_MONITOR_EVENT_CHANGED:
        c = '=';
        break;
    case TYB_MONITOR_EVENT_DISAPPEARED:
        c = '?';
        break;
    case TYB_MONITOR_EVENT_DROPPED:
        c = '-';
        break;
    }
    assert(c);

    printf("%c %s %s\n", c, tyb_board_get_tag(board),
           model ? tyb_board_model_get_name(model) : "(unknown)");

    if (list_verbose && event != TYB_MONITOR_EVENT_DROPPED) {
        printf("  - model: %s\n", model ? tyb_board_model_get_desc(model) : "(unknown)");

        printf("  - capabilities: ");
        print_capabilities(tyb_board_get_capabilities(board));
        printf("\n");

        if (event != TYB_MONITOR_EVENT_DISAPPEARED) {
            printf("  - interfaces: \n");
            tyb_board_list_interfaces(board, print_interface_info, NULL);
        } else {
            printf("  - interfaces: (none)\n");
        }
    }

    return 0;
}

int list(int argc, char *argv[])
{
    tyb_monitor *manager;
    int r;

    int c;
    while ((c = getopt_long(argc, argv, short_options, long_options, NULL)) != -1) {
        switch (c) {
        case 'v':
            list_verbose = true;
            break;

        case 'w':
            watch = true;
            break;

        default:
            r = parse_main_option(argc, argv, c);
            if (r <= 0)
                return r;
            break;
        }
    }

    if (argc > optind) {
        ty_error(TY_ERROR_PARAM, "No positional argument is allowed");
        goto usage;
    }

    r = get_manager(&manager);
    if (r < 0)
        return r;

    r = tyb_monitor_list(manager, list_callback, NULL);
    if (r < 0)
        return r;

    if (watch) {
        r = tyb_monitor_register_callback(manager, list_callback, NULL);
        if (r < 0)
            return r;

        r = tyb_monitor_wait(manager, NULL, NULL, -1);
        if (r < 0)
            return r;
    }

    return 0;

usage:
    print_list_usage(stderr);
    return TY_ERROR_PARAM;
}
