/* TyTools - public domain
   Niels Martignène <niels.martignene@protonmail.com>
   https://koromix.dev/tytools

   This software is in the public domain. Where that dedication is not
   recognized, you are granted a perpetual, irrevocable license to copy,
   distribute, and modify this file as you see fit.

   See the LICENSE file for more details. */

#include <stdarg.h>
#include "main.h"

enum output_format {
    OUTPUT_PLAIN,
    OUTPUT_JSON,
    OUTPUT_JSON_STREAM
};

static enum output_format list_output = OUTPUT_PLAIN;
static bool list_verbose = false;
static bool list_watch = false;

static bool json_comma = false;

static void print_list_usage(FILE *f)
{
    fprintf(f, "usage: %s list [options]\n\n", tycmd_executable_name);

    print_common_options(f);
    fprintf(f, "\n");

    fprintf(f, "List options:\n"
               "   -O, --output <format>    Output format, must be plain (default) or json\n"
               "   -v, --verbose            Print detailed information about devices\n\n"
               "   -w, --watch              Watch devices dynamically\n");
}

static int print_interface_info_plain(ty_board_interface *iface, void *udata)
{
    TY_UNUSED(udata);

    printf("    %s: %s\n", ty_board_interface_get_name(iface),
                           ty_board_interface_get_path(iface));

    return 0;
}

static int print_event_plain(ty_board *board, ty_monitor_event event)
{
    ty_model model = ty_board_get_model(board);
    const char *action = "";

    switch (event) {
        case TY_MONITOR_EVENT_ADDED: { action = "add"; } break;
        case TY_MONITOR_EVENT_CHANGED: { action = "change"; } break;
        case TY_MONITOR_EVENT_DISAPPEARED: { action = "miss"; } break;
        case TY_MONITOR_EVENT_DROPPED: { action = "remove"; } break;
    }

    if (ty_board_get_description(board)) {
        printf("%s %s %s (%s)\n", action, ty_board_get_tag(board), ty_models[model].name,
                                  ty_board_get_description(board));
    } else {
        printf("%s %s %s\n", action, ty_board_get_tag(board), ty_models[model].name);
    }

    if (list_verbose &&
            event != TY_MONITOR_EVENT_DROPPED && event != TY_MONITOR_EVENT_DISAPPEARED) {
        printf("  location: %s\n", ty_board_get_location(board));

        int capabilities = ty_board_get_capabilities(board);

        printf("  capabilities:\n");
        for (unsigned int i = 0; i < TY_BOARD_CAPABILITY_COUNT; i++) {
            if (capabilities & (1 << i))
                printf("    %s\n", ty_board_capability_get_name(i));
        }
        printf("\n");

        printf("  interfaces:\n");
        ty_board_list_interfaces(board, print_interface_info_plain, NULL);
        printf("\n");
    }

    fflush(stdout);

    return 0;
}

static void print_json_start(const char *key, char type, bool *comma)
{
    if (*comma)
        printf(", ");
    if (key)
        printf("\"%s\": ", key);

    putc(type, stdout);

    *comma = false;
}

static void print_json_end(char type, bool *comma)
{
    putc(type, stdout);
    *comma = true;
}

static void print_json_string(const char *key, const char *value, bool *comma)
{
    if (*comma)
        printf(", ");
    if (key)
        printf("\"%s\": ", key);

    putc('"', stdout);
    for (size_t i = 0; value[i]; i++) {
        switch (value[i]) {
            case '\b': { printf("\\b"); } break;
            case '\f': { printf("\\f"); } break;
            case '\n': { printf("\\n"); } break;
            case '\r': { printf("\\r"); } break;
            case '\t': { printf("\\t"); } break;
            case '"': { printf("\\\""); } break;
            case '\\': { printf("\\\\"); } break;
            default: { putc(value[i], stdout); } break;
        }
    }
    putc('"', stdout);

    *comma = true;
}

static int print_interface_info_json(ty_board_interface *iface, void *udata)
{
    bool *comma = (bool *)udata;

    print_json_start(NULL, '[', comma);
    print_json_string(NULL, ty_board_interface_get_name(iface), comma);
    print_json_string(NULL, ty_board_interface_get_path(iface), comma);
    print_json_end(']', comma);

    return 0;
}

static int print_event_json(ty_board *board, ty_monitor_event event, bool *comma)
{
    ty_model model = ty_board_get_model(board);
    const char *action = "";

    switch (event) {
        case TY_MONITOR_EVENT_ADDED: { action = "add"; } break;
        case TY_MONITOR_EVENT_CHANGED: { action = "change"; } break;
        case TY_MONITOR_EVENT_DISAPPEARED: { action = "miss"; } break;
        case TY_MONITOR_EVENT_DROPPED: { action = "remove"; } break;
    }

    print_json_start(NULL, '{', comma);

    print_json_string("action", action, comma);
    print_json_string("tag", ty_board_get_tag(board), comma);
    if (ty_board_get_serial_number(board))
        print_json_string("serial", ty_board_get_serial_number(board), comma);
    if (ty_board_get_description(board))
        print_json_string("description", ty_board_get_description(board), comma);
    print_json_string("model", ty_models[model].name, comma);

    if (list_verbose) {
        print_json_string("location", ty_board_get_location(board), comma);

        int capabilities = ty_board_get_capabilities(board);

        print_json_start("capabilities", '[', comma);
        for (unsigned int i = 0; i < TY_BOARD_CAPABILITY_COUNT; i++) {
            if (capabilities & (1 << i))
                print_json_string(NULL, ty_board_capability_get_name(i), comma);
        }
        print_json_end(']', comma);

        print_json_start("interfaces", '[', comma);
        ty_board_list_interfaces(board, print_interface_info_json, comma);
        print_json_end(']', comma);
    }

    print_json_end('}', comma);
    printf("\n");

    fflush(stdout);
    return 0;
}

static int list_callback(ty_board *board, ty_monitor_event event, void *udata)
{
    TY_UNUSED(udata);

    switch (list_output) {
        case OUTPUT_PLAIN: { return print_event_plain(board, event); } break;
        case OUTPUT_JSON: {
            printf("  ");
            return print_event_json(board, event, &json_comma);
        } break;
        case OUTPUT_JSON_STREAM: {
            json_comma = false;
            return print_event_json(board, event, &json_comma);
        } break;
    }

    assert(false);
    return 0;
}

int list(int argc, char *argv[])
{
    ty_optline_context optl;
    char *opt;
    ty_monitor *monitor;
    int r;

    ty_optline_init_argv(&optl, argc, argv);
    while ((opt = ty_optline_next_option(&optl))) {
        if (strcmp(opt, "--help") == 0) {
            print_list_usage(stdout);
            return EXIT_SUCCESS;
        } else if (strcmp(opt, "--output") == 0 || strcmp(opt, "-O") == 0) {
            char *value = ty_optline_get_value(&optl);
            if (!value) {
                ty_log(TY_LOG_ERROR, "Option '--output' takes an argument");
                print_list_usage(stderr);
                return EXIT_FAILURE;
            }

            if (strcmp(value, "plain") == 0) {
                list_output = OUTPUT_PLAIN;
            } else if (strcmp(value, "json") == 0) {
                list_output = OUTPUT_JSON;
            } else {
                ty_log(TY_LOG_ERROR, "--output must be one of plain or json");
                print_list_usage(stderr);
                return EXIT_FAILURE;
            }
        } else if (strcmp(opt, "--verbose") == 0 || strcmp(opt, "-v") == 0) {
            list_verbose = true;
        } else if (strcmp(opt, "--watch") == 0 || strcmp(opt, "-w") == 0) {
            list_watch = true;
        } else if (!parse_common_option(&optl, opt)) {
            print_list_usage(stderr);
            return EXIT_FAILURE;
        }
    }
    if (ty_optline_consume_non_option(&optl)) {
        ty_log(TY_LOG_ERROR, "No positional argument is allowed");
        print_list_usage(stderr);
        return EXIT_FAILURE;
    }

    if (list_watch && list_output == OUTPUT_JSON)
        list_output = OUTPUT_JSON_STREAM;

    r = get_monitor(&monitor);
    if (r < 0)
        return EXIT_FAILURE;

    if (list_output == OUTPUT_JSON) {
        printf("[\n");
        r = ty_monitor_list(monitor, list_callback, NULL);
        printf("]\n");
    } else {
        r = ty_monitor_list(monitor, list_callback, NULL);
    }
    if (r < 0)
        return EXIT_FAILURE;

    if (list_watch) {
        r = ty_monitor_register_callback(monitor, list_callback, NULL);
        if (r < 0)
            return EXIT_FAILURE;

        r = ty_monitor_wait(monitor, NULL, NULL, -1);
        if (r < 0)
            return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
