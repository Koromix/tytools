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
#ifndef _WIN32
#include <signal.h>
#include <sys/wait.h>
#endif
#include "board.h"
#include "main.h"
#include "system.h"

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
    {"upload",  upload,  print_upload_usage,  "upload firmware (either local project or specific file)"},
    {0}
};

static const char *device_path = NULL;
static uint64_t device_serial = 0;

static ty_board *board = NULL;

static void print_version(void)
{
    fprintf(stderr, "ty version "TY_VERSION"\n");
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

    fprintf(stderr, "usage: ty [-d <device>] <command> [options]\n\n"
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

int get_board(ty_board **rboard)
{
    if (board) {
        *rboard = board;
        return 0;
    }

    int r;

    r = ty_board_find(device_path, device_serial, &board);
    if (r < 0) {
        return r;
    } else if (!r) {
        return ty_error(TY_ERROR_NOT_FOUND, "Board not found");
    }

    r = ty_board_probe(board, 0);
    if (r < 0)
        return r;

    printf("Board at '%s#%"PRIu64"' (%s)\n", board->dev->path, board->serial, board->mode->desc);

    *rboard = board;
    return 0;
}

#ifndef _WIN32
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

#ifndef _WIN32
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
            r = parse_device_path(optarg, &device_path, &device_serial);
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

    ty_board_unref(board);

    return -r;

usage:
    print_usage(NULL);
    return -TY_ERROR_PARAM;
}
