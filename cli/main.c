/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <getopt.h>
#ifndef _WIN32
#include <signal.h>
#include <sys/wait.h>
#endif
#include "ty.h"
#include "main.h"

struct command {
    const char *name;

    int (*f)(int argc, char *argv[]);
    void (*usage)(FILE *f);

    const char *description;
};

void print_list_usage(FILE *f);
void print_monitor_usage(FILE *f);
void print_reset_usage(FILE *f);
void print_upload_usage(FILE *f);

int list(int argc, char *argv[]);
int monitor(int argc, char *argv[]);
int reset(int argc, char *argv[]);
int upload(int argc, char *argv[]);

static const struct command commands[] = {
    {"list",    list,    print_list_usage,    "list available boards"},
    {"monitor", monitor, print_monitor_usage, "open serial (or emulated) connection with device"},
    {"reset",   reset,   print_reset_usage,   "reset device"},
    {"upload",  upload,  print_upload_usage,  "upload new firmware"},
    {0}
};

static ty_board_manager *board_manager;
static ty_board *main_board;

static const struct command *current_command;
static const char *board_identity = NULL;

static void print_version(FILE *f)
{
    fprintf(f, "tyc "TY_VERSION"\n");
}

static void print_main_usage(FILE *f)
{
    fprintf(f, "usage: tyc <command> [options]\n\n");

    print_main_options(f);
    fprintf(f, "\n");

    fprintf(f, "Commands:\n");
    for (const struct command *c = commands; c->name; c++)
        fprintf(f, "   %-24s %s\n", c->name, c->description);
    fputc('\n', f);

    print_supported_models(f);
}

static void print_usage(FILE *f, const struct command *cmd)
{
    if (cmd) {
        cmd->usage(f);
    } else {
        print_main_usage(f);
    }
}

void print_main_options(FILE *f)
{
    fprintf(f, "General options:\n"
               "       --help               Show help message\n"
               "       --version            Display version information\n\n"

               "   -b, --board <id>         Work with board <id> instead of first detected\n"
               "       --experimental       Enable experimental features (use with caution)\n");
}

void print_supported_models(FILE *f)
{
    fprintf(f, "Supported models:\n");
    for (const ty_board_model **cur = ty_board_models; *cur; cur++) {
        const ty_board_model *model = *cur;
        fprintf(f, "   - %-22s (%s, %s)\n", ty_board_model_get_desc(model),
                ty_board_model_get_name(model), ty_board_model_get_mcu(model));
    }
}

static int board_callback(ty_board *board, ty_board_event event, void *udata)
{
    TY_UNUSED(udata);

    switch (event) {
    case TY_BOARD_EVENT_ADDED:
        if (!main_board) {
            int r = ty_board_matches_identity(board, board_identity);
            if (r < 0)
                return r;

            if (r)
                main_board = ty_board_ref(board);
        }
        break;

    case TY_BOARD_EVENT_CHANGED:
    case TY_BOARD_EVENT_DISAPPEARED:
        break;

    case TY_BOARD_EVENT_DROPPED:
        if (main_board == board) {
            ty_board_unref(main_board);
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

    ty_board_manager *manager = NULL;
    int r;

    r = ty_board_manager_new(&manager);
    if (r < 0)
        goto error;

    r = ty_board_manager_register_callback(manager, board_callback, NULL);
    if (r < 0)
        goto error;

    r = ty_board_manager_refresh(manager);
    if (r < 0)
        goto error;

    board_manager = manager;
    return 0;

error:
    ty_board_manager_free(manager);
    return r;
}

int get_manager(ty_board_manager **rmanager)
{
    int r = init_manager();
    if (r < 0)
        return r;

    *rmanager = board_manager;
    return 0;
}

int get_board(ty_board **rboard)
{
    int r = init_manager();
    if (r < 0)
        return r;

    if (!main_board) {
        if (board_identity) {
            return ty_error(TY_ERROR_NOT_FOUND, "Board '%s' not found", board_identity);
        } else {
            return ty_error(TY_ERROR_NOT_FOUND, "No board available");
        }
    }

    static ty_board *previous_board = NULL;
    if (main_board != previous_board) {
        printf("Board at '%s'\n", ty_board_get_identity(main_board));
        previous_board = main_board;
    }

    *rboard = ty_board_ref(main_board);
    return 0;
}

#if defined(__unix__) || defined(__APPLE__)

static void handle_sigchld(int sig)
{
    TY_UNUSED(sig);

    pid_t pid;

    /* Reap all children, we don't use SIG_IGN or SA_NOCLDWAIT because
       we want to wait for some children and ignore others. */
    do {
        pid = waitpid((pid_t)-1, 0, WNOHANG);
    } while (pid > 0);
}

static void setup_signals(void)
{
    signal(SIGCHLD, handle_sigchld);
}

#endif

int parse_main_option(int argc, char *argv[], int c)
{
    TY_UNUSED(argc);

    switch (c) {
    case MAIN_OPTION_HELP:
        print_usage(stdout, current_command);
        return 0;
    case MAIN_OPTION_VERSION:
        print_version(stdout);
        return 0;

    case MAIN_OPTION_EXPERIMENTAL:
        ty_config_experimental = true;
        return 1;

    case 'b':
        board_identity = optarg;
        return 1;
    }

    ty_error(TY_ERROR_PARAM, "Unknown option '%s'", argv[optind - 1]);
    print_usage(stderr, current_command);

    return TY_ERROR_PARAM;
}

static const struct command *find_command(const char *name)
{
    for (const struct command *cmd = commands; cmd->name; cmd++) {
        if (strcmp(cmd->name, name) == 0)
            return cmd;
    }

    return NULL;
}

int main(int argc, char *argv[])
{
    int r;

    if (argc < 2) {
        print_main_usage(stderr);
        return 0;
    }

#if defined(__unix__) || defined(__APPLE__)
    setup_signals();
#endif

    if (strcmp(argv[1], "help") == 0 || strcmp(argv[1], "--help") == 0) {
        if (argc > 2 && *argv[2] != '-') {
            const struct command *cmd = find_command(argv[2]);
            if (cmd) {
                print_usage(stdout, cmd);
            } else {
                ty_error(TY_ERROR_PARAM, "Unknown command '%s'", argv[2]);
                print_usage(stderr, NULL);
            }
        } else {
            print_usage(stdout, NULL);
        }

        return 0;
    } else if (strcmp(argv[1], "--version") == 0) {
        print_version(stdout);
        return 0;
    }

    current_command = find_command(argv[1]);
    if (!current_command) {
        ty_error(TY_ERROR_PARAM, "Unknown command '%s'", argv[1]);
        print_main_usage(stderr);
        return 1;
    }

    // We'll print our own, for consistency
    opterr = 0;

    r = (*current_command->f)(argc - 1, argv + 1);

    ty_board_unref(main_board);
    ty_board_manager_free(board_manager);
    return !!r;
}
