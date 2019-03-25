/* TyTools - public domain
   Niels Martignène <niels.martignene@protonmail.com>
   https://koromix.dev/tytools

   This software is in the public domain. Where that dedication is not
   recognized, you are granted a perpetual, irrevocable license to copy,
   distribute, and modify this file as you see fit.

   See the LICENSE file for more details. */

#ifndef _WIN32
    #include <signal.h>
    #include <sys/wait.h>
#endif
#include "../libhs/common.h"
#include "../libty/system.h"
#include "main.h"

struct command {
    const char *name;
    int (*f)(int argc, char *argv[]);
    const char *description;
};

int identify(int argc, char *argv[]);
int list(int argc, char *argv[]);
int monitor(int argc, char *argv[]);
int reset(int argc, char *argv[]);
int upload(int argc, char *argv[]);

static const struct command commands[] = {
    {"identify", identify, "Identify models compatible with firmware"},
    {"list",     list,     "List available boards"},
    {"monitor",  monitor,  "Open serial (or emulated) connection with board"},
    {"reset",    reset,    "Reset board"},
    {"upload",   upload,   "Upload new firmware"},
    {0}
};

const char *tycmd_executable_name;

static const char *main_board_tag = NULL;

static ty_monitor *main_board_monitor;
static ty_board *main_board;

static void print_version(FILE *f)
{
    fprintf(f, "%s %s\n", tycmd_executable_name, ty_version_string());
}

static void print_main_usage(FILE *f)
{
    fprintf(f, "usage: %s <command> [options]\n\n", tycmd_executable_name);

    print_common_options(f);
    fprintf(f, "\n");

    fprintf(f, "Commands:\n");
    for (const struct command *c = commands; c->name; c++)
        fprintf(f, "   %-24s %s\n", c->name, c->description);
    fputc('\n', f);

    fprintf(f, "Supported models:\n");
    for (unsigned int i = 0; i < ty_models_count; i++) {
        if (ty_models[i].mcu)
            fprintf(f, "   - %-22s (%s)\n", ty_models[i].name, ty_models[i].mcu);
    }
}

void print_common_options(FILE *f)
{
    fprintf(f, "General options:\n"
               "       --help               Show help message\n"
               "       --version            Display version information\n\n"
               "   -B, --board <tag>        Work with board <tag> instead of first detected\n"
               "   -q, --quiet              Disable output, use -qqq to silence errors\n");
}

static inline unsigned int get_board_priority(ty_board *board)
{
    return ty_models[ty_board_get_model(board)].priority;
}

static int board_callback(ty_board *board, ty_monitor_event event, void *udata)
{
    TY_UNUSED(udata);

    switch (event) {
        case TY_MONITOR_EVENT_ADDED: {
            if ((!main_board || get_board_priority(board) > get_board_priority(main_board))
                    && ty_board_matches_tag(board, main_board_tag)) {
                ty_board_unref(main_board);
                main_board = ty_board_ref(board);
            }
        } break;

        case TY_MONITOR_EVENT_CHANGED:
        case TY_MONITOR_EVENT_DISAPPEARED: {
        } break;

        case TY_MONITOR_EVENT_DROPPED: {
            if (main_board == board) {
                ty_board_unref(main_board);
                main_board = NULL;
            }
        } break;
    }

    return 0;
}

static int init_monitor()
{
    if (main_board_monitor)
        return 0;

    ty_monitor *monitor = NULL;
    int r;

    r = ty_monitor_new(&monitor);
    if (r < 0)
        goto error;

    r = ty_monitor_register_callback(monitor, board_callback, NULL);
    if (r < 0)
        goto error;

    r = ty_monitor_start(monitor);
    if (r < 0)
        goto error;

    main_board_monitor = monitor;
    return 0;

error:
    ty_monitor_free(monitor);
    return r;
}

int get_monitor(ty_monitor **rmonitor)
{
    int r = init_monitor();
    if (r < 0)
        return r;

    *rmonitor = main_board_monitor;
    return 0;
}

int get_board(ty_board **rboard)
{
    int r = init_monitor();
    if (r < 0)
        return r;

    if (!main_board) {
        if (main_board_tag) {
            return ty_error(TY_ERROR_NOT_FOUND, "Board '%s' not found", main_board_tag);
        } else {
            return ty_error(TY_ERROR_NOT_FOUND, "No board available");
        }
    }

    *rboard = ty_board_ref(main_board);
    return 0;
}

bool parse_common_option(ty_optline_context *optl, char *arg)
{
    if (strcmp(arg, "--board") == 0 || strcmp(arg, "-B") == 0) {
        main_board_tag = ty_optline_get_value(optl);
        if (!main_board_tag) {
            ty_log(TY_LOG_ERROR, "Option '--board' takes an argument");
            return false;
        }
        return true;
    } else if (strcmp(arg, "--quiet") == 0 || strcmp(arg, "-q") == 0) {
        ty_config_verbosity--;
        return true;
    } else {
        ty_log(TY_LOG_ERROR, "Unknown option '%s'", arg);
        return false;
    }
}

int main(int argc, char *argv[])
{
    const struct command *cmd;
    int r;

    if (argc && *argv[0]) {
        tycmd_executable_name = argv[0] + strlen(argv[0]);
        while (tycmd_executable_name > argv[0] && !strchr(TY_PATH_SEPARATORS, tycmd_executable_name[-1]))
            tycmd_executable_name--;
    } else {
#ifdef _WIN32
        tycmd_executable_name = TY_CONFIG_TYCMD_EXECUTABLE ".exe";
#else
        tycmd_executable_name = TY_CONFIG_TYCMD_EXECUTABLE;
#endif
    }

    hs_log_set_handler(ty_libhs_log_handler, NULL);
    r = ty_models_load_patch(NULL);
    if (r == TY_ERROR_MEMORY)
        return EXIT_FAILURE;

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

    r = (*cmd->f)(argc - 1, argv + 1);

    ty_board_unref(main_board);
    ty_monitor_free(main_board_monitor);

    return r;
}
