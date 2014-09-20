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
    void (*usage)(void);

    const char *description;
};

enum {
    OPTION_HELP = 0x100,
    OPTION_VERSION
};

void print_list_usage(void);
void print_monitor_usage(void);
void print_reset_usage(void);
void print_upload_usage(void);

int list(int argc, char *argv[]);
int monitor(int argc, char *argv[]);
int reset(int argc, char *argv[]);
int upload(int argc, char *argv[]);

static const char *short_options = "+d:";
static const struct option long_options[] = {
    {"device",    required_argument, NULL, 'd'},
    {"help",      optional_argument, NULL, OPTION_HELP},
    {"version",   no_argument,       NULL, OPTION_VERSION},
    {0}
};

static const struct command commands[] = {
    {"list",    list,    print_list_usage,    "list available boards"},
    {"monitor", monitor, print_monitor_usage, "open serial (or emulated) connection with device"},
    {"reset",   reset,   print_reset_usage,   "reset device"},
    {"upload",  upload,  print_upload_usage,  "upload new firmware"},
    {0}
};

static ty_board_manager *board_manager;
static ty_board *main_board;

static const char *device_location = NULL;
static uint64_t device_serial = 0;

static void print_version(void)
{
    fprintf(stderr, "tyc version "TY_VERSION"\n");
}

static void print_usage(const char *cmd_name)
{
    if (cmd_name) {
        const struct command *cmd;
        for (cmd = commands; cmd->name; cmd++) {
            if (strcmp(cmd_name, cmd->name) == 0)
                break;
        }
        if (cmd->name) {
            cmd->usage();
            return;
        }

        ty_error(TY_ERROR_PARAM, "Invalid command '%s'", cmd_name);
    }

    fprintf(stderr, "usage: tyc [-d <device>] <command> [options]\n\n"
                    "Options:\n"
                    "   -C, --directory=<dir>    Change to <dir> before doing anything\n\n");

    fprintf(stderr, "Commands:\n");

    for (const struct command *c = commands; c->name; c++) {
        fprintf(stderr, "   %s ", c->name);
        for (size_t i = strlen(c->name); i < 9; i++)
            fputc(' ', stderr);
        fprintf(stderr, "%s\n", c->description);
    }

    fputc('\n', stderr);
    print_supported_models();
}

void print_supported_models(void)
{
    fprintf(stderr, "Supported models:\n");

    for (const ty_board_model **cur = ty_board_models; *cur; cur++) {
        const ty_board_model *m = *cur;
        fprintf(stderr, "   - %s", m->name);
        for (size_t i = strlen(m->name); i < 12; i++)
            fputc(' ', stderr);
        fprintf(stderr, "(%s)\n", m->mcu);
    }
}

static int parse_device_path(char *device, const char **rpath, uint64_t *rserial)
{
    *rpath = NULL;
    *rserial = 0;

    char *ptr = strchr(device, '#');

    if (ptr == device) {
        ptr++;
    } else {
        if (ptr > device)
            *ptr++ = 0;
        *rpath = device;
    }

    if (ptr) {
        errno = 0;
        *rserial = strtoull(ptr, NULL, 0);
        if (errno)
            return ty_error(TY_ERROR_PARAM, "Serial must be a number");
    }

    return 0;
}

static bool test_board(ty_board *board)
{
    if (device_location && strcmp(ty_board_get_location(board), device_location) != 0)
        return false;
    if (device_serial && ty_board_get_serial_number(board) != device_serial)
        return false;

    return true;
}

static int board_callback(ty_board *board, ty_board_event event, void *udata)
{
    TY_UNUSED(udata);

    switch (event) {
    case TY_BOARD_EVENT_ADDED:
        if (!main_board && test_board(board))
            main_board = ty_board_ref(board);
        break;

    case TY_BOARD_EVENT_CHANGED:
    case TY_BOARD_EVENT_CLOSED:
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

int get_manager(ty_board_manager **rmanager)
{
    *rmanager = board_manager;
    return 0;
}

int get_board(ty_board **rboard)
{
    if (!main_board)
        return ty_error(TY_ERROR_NOT_FOUND, "Board not found");

    static ty_board *previous_board = NULL;
    if (main_board != previous_board) {
        printf("Board at '%s#%"PRIu64"' (%s)\n", ty_board_get_location(main_board),
               ty_board_get_serial_number(main_board), ty_board_get_mode(main_board)->desc);
        previous_board = main_board;
    }

    *rboard = ty_board_ref(main_board);
    return 0;
}

#ifdef __unix__

static void handle_sigchld(int sig)
{
    TY_UNUSED(sig);

    pid_t pid;

    // Reap all children, we don't use SIG_IGN or SA_NOCLDWAIT because
    // we want to wait for some children and ignore others.
    do {
        pid = waitpid((pid_t)-1, 0, WNOHANG);
    } while (pid > 0);
}

static void setup_signals(void)
{
    signal(SIGCHLD, handle_sigchld);
}

#endif

int main(int argc, char *argv[])
{
    int r;

#ifdef __unix__
    setup_signals();
#endif

    int c;
    while ((c = getopt_long(argc, argv, short_options, long_options, NULL)) != -1) {
        switch (c) {
        case OPTION_HELP:
            if (optind < argc) {
                print_usage(argv[optind]);
            } else {
                print_usage(NULL);
            }
            return 0;
        case OPTION_VERSION:
            print_version();
            return 0;

        case 'd':
            r = parse_device_path(optarg, &device_location, &device_serial);
            break;

        default:
            goto usage;
        }
    }

    if (argc - optind < 1)
        goto usage;

    if (strcmp(argv[optind], "help") == 0) {
        if (optind + 1 < argc) {
            print_usage(argv[optind + 1]);
        } else {
            print_usage(NULL);
        }
        return 0;
    } else if (strcmp(argv[optind], "version") == 0) {
        print_version();
        return 0;
    }

    r = ty_board_manager_new(&board_manager);
    if (r < 0)
        return -r;

    r = ty_board_manager_register_callback(board_manager, board_callback, NULL);
    if (r < 0)
        return -r;

    r = ty_board_manager_refresh(board_manager);
    if (r < 0)
        return -r;

    r = 0;
    while (optind < argc) {
        const char *cmd_name = argv[optind];
        argv[optind] = argv[0];

        argv += optind;
        argc -= optind;
        optind = 1;

        const struct command *cmd;
        for (cmd = commands; cmd->name; cmd++) {
            if (strcmp(cmd_name, cmd->name) == 0)
                break;
        }
        if (cmd->name) {
            r = (*cmd->f)(argc, argv);
            if (r)
                break;
        } else {
            r = ty_error(TY_ERROR_PARAM, "Invalid command '%s'", cmd_name);
            break;
        }
    }

    ty_board_unref(main_board);
    ty_board_manager_free(board_manager);

    return -r;

usage:
    print_usage(NULL);
    return -TY_ERROR_PARAM;
}
