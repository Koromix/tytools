/*
 * ty, a collection of GUI and command-line tools to manage Teensy devices
 *
 * Distributed under the MIT license (see LICENSE.txt or http://opensource.org/licenses/MIT)
 * Copyright (c) 2015 Niels Martignène <niels.martignene@gmail.com>
 */

#include <getopt.h>
#ifndef _WIN32
    #include <signal.h>
    #include <sys/wait.h>
#endif
#include "ty/version.h"
#include "main.h"

struct command {
    const char *name;
    int (*f)(int argc, char *argv[]);
    const char *description;
};

int list(int argc, char *argv[]);
int monitor(int argc, char *argv[]);
int reset(int argc, char *argv[]);
int upload(int argc, char *argv[]);

static const struct command commands[] = {
    {"list",    list,    "List available boards"},
    {"monitor", monitor, "Open serial (or emulated) connection with board"},
    {"reset",   reset,   "Reset board"},
    {"upload",  upload,  "Upload new firmware"},
    {0}
};

static const char *board_tag = NULL;

static tyb_monitor *board_manager;
static tyb_board *main_board;

static void print_version(FILE *f)
{
    fprintf(f, "tyc "TY_VERSION"\n");
}

static int print_family_model(const tyb_board_model *model, void *udata)
{
    FILE *f = udata;

    fprintf(f, "   - %-22s (%s)\n", tyb_board_model_get_name(model), tyb_board_model_get_mcu(model));
    return 0;
}

static void print_main_usage(FILE *f)
{
    fprintf(f, "usage: tyc <command> [options]\n\n");

    print_common_options(f);
    fprintf(f, "\n");

    fprintf(f, "Commands:\n");
    for (const struct command *c = commands; c->name; c++)
        fprintf(f, "   %-24s %s\n", c->name, c->description);
    fputc('\n', f);

    fprintf(f, "Supported models:\n");
    for (const tyb_board_family **cur = tyb_board_families; *cur; cur++) {
        const tyb_board_family *family = *cur;

        tyb_board_family_list_models(family, print_family_model, f);
    }
}

void print_common_options(FILE *f)
{
    fprintf(f, "General options:\n"
               "       --help               Show help message\n"
               "       --version            Display version information\n\n"
               "       --board <tag>        Work with board <tag> instead of first detected\n"
               "   -q, --quiet              Disable output, use -qqq to silence errors\n"
               "       --experimental       Enable experimental features (use with caution)\n");
}

static int board_callback(tyb_board *board, tyb_monitor_event event, void *udata)
{
    TY_UNUSED(udata);

    switch (event) {
    case TYB_MONITOR_EVENT_ADDED:
        if (!main_board && tyb_board_matches_tag(board, board_tag))
            main_board = tyb_board_ref(board);
        break;

    case TYB_MONITOR_EVENT_CHANGED:
    case TYB_MONITOR_EVENT_DISAPPEARED:
        break;

    case TYB_MONITOR_EVENT_DROPPED:
        if (main_board == board) {
            tyb_board_unref(main_board);
            main_board = NULL;
        }
        break;
    }

    return 0;
}

static int init_manager()
{
    if (board_manager)
        return 0;

    tyb_monitor *manager = NULL;
    int r;

    r = tyb_monitor_new(0, &manager);
    if (r < 0)
        goto error;

    r = tyb_monitor_register_callback(manager, board_callback, NULL);
    if (r < 0)
        goto error;

    r = tyb_monitor_refresh(manager);
    if (r < 0)
        goto error;

    board_manager = manager;
    return 0;

error:
    tyb_monitor_free(manager);
    return r;
}

int get_manager(tyb_monitor **rmanager)
{
    int r = init_manager();
    if (r < 0)
        return r;

    *rmanager = board_manager;
    return 0;
}

int get_board(tyb_board **rboard)
{
    int r = init_manager();
    if (r < 0)
        return r;

    if (!main_board) {
        if (board_tag) {
            return ty_error(TY_ERROR_NOT_FOUND, "Board '%s' not found", board_tag);
        } else {
            return ty_error(TY_ERROR_NOT_FOUND, "No board available");
        }
    }

    *rboard = tyb_board_ref(main_board);
    return 0;
}

bool parse_common_option(int argc, char *argv[], int c)
{
    TY_UNUSED(argc);

    switch (c) {
    case COMMON_OPTION_BOARD:
        board_tag = optarg;
        break;
    case 'q':
        ty_config_quiet++;
        break;
    case COMMON_OPTION_EXPERIMENTAL:
        ty_config_experimental = true;
        break;

    case ':':
        ty_log(TY_LOG_ERROR, "Option '%s' takes an argument", argv[optind - 1]);
        return false;
    case '?':
        ty_log(TY_LOG_ERROR, "Unknown option '%s'", argv[optind - 1]);
        return false;
    }

    return true;
}

int main(int argc, char *argv[])
{
    const struct command *cmd;
    int r;

    if (argc < 2) {
        print_main_usage(stderr);
        return EXIT_SUCCESS;
    }

    if (strcmp(argv[1], "help") == 0 || strcmp(argv[1], "--help") == 0) {
        if (argc > 2 && *argv[2] != '-') {
            argv[1] = argv[2];
            argv[2] = "--help";
        } else {
            print_main_usage(stdout);
            return EXIT_SUCCESS;
        }
    } else if (strcmp(argv[1], "--version") == 0) {
        print_version(stdout);
        return EXIT_SUCCESS;
    }

    for (cmd = commands; cmd->name; cmd++) {
        if (strcmp(cmd->name, argv[1]) == 0)
            break;
    }
    if (!cmd->name) {
        ty_log(TY_LOG_ERROR, "Unknown command '%s'", argv[1]);
        print_main_usage(stderr);
        return EXIT_FAILURE;
    }

    r = ty_init();
    if (r < 0)
        return EXIT_FAILURE;

    // We'll print our own, for consistency
    opterr = 0;

    r = (*cmd->f)(argc - 1, argv + 1);

    tyb_board_unref(main_board);
    tyb_monitor_free(board_manager);
    ty_release();

    return r;
}
