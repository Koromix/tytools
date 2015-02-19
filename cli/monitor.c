/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <getopt.h>
#include <unistd.h>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif
#include "ty.h"
#include "main.h"

enum {
    OPTION_HELP = 0x100,
    OPTION_NORESET,
    OPTION_TIMEOUT_EOF
};

static const char *short_options = "b:d:D:f:p:rRs";
static const struct option long_options[] = {
    {"baud",        required_argument, NULL, 'b'},
    {"databits",    required_argument, NULL, 'd'},
    {"direction",   required_argument, NULL, 'D'},
    {"flow",        required_argument, NULL, 'f'},
    {"help",        no_argument,       NULL, OPTION_HELP},
    {"noreset",     no_argument,       NULL, OPTION_NORESET},
    {"parity",      required_argument, NULL, 'p'},
    {"raw",         no_argument,       NULL, 'r'},
    {"reconnect",   no_argument,       NULL, 'R'},
    {"silent",      no_argument,       NULL, 's'},
    {"timeout-eof", required_argument, NULL, OPTION_TIMEOUT_EOF},
    {0}
};

enum {
    DIRECTION_INPUT = 1,
    DIRECTION_OUTPUT = 2
};

static const int error_io_timeout = 5000;

static uint16_t terminal_flags = 0;
#ifdef _WIN32
static bool fake_echo = false;
#endif
static uint32_t device_rate = 115200;
static uint16_t device_flags = 0;
static uint16_t directions = DIRECTION_INPUT | DIRECTION_OUTPUT;
static bool reconnect = false;
static int timeout_eof = 200;

void print_monitor_usage(void)
{
    fprintf(stderr, "usage: tyc monitor [--help] [options]\n\n"
                    "Options:\n"
                    "   -b, --baud <rate>        Use baudrate for serial port\n"
                    "   -d, --databits <bits>    Change number of bits for each character\n"
                    "                            Must be one of 5, 6, 7 or 8 (default)\n"
                    "   -D, --direction <dir>    Open serial connection in given direction\n"
                    "                            Supports input, output, both (default)\n"
                    "   -f, --flow <control>     Define flow-control mode\n"
                    "                            Supports xonxoff (x), rtscts (h) and none (n)\n"
                    "       --noreset            Don't reset serial port when closing\n"
                    "   -p, --parity <bits>      Change parity mode to use for the serial port\n"
                    "                            Supports odd (o), even (e) and none (n)\n"
                    "   -r, --raw                Disable line-buffering and line-editing\n"
                    "   -R, --reconnect          Try to reconnect on I/O errors\n"
                    "   -s, --silent             Disable echoing of local input on terminal\n"
                    "       --timeout-eof <ms>   Time before closing after EOF on standard input\n"
                    "                            Defaults to %d ms, use -1 to disable\n", timeout_eof);
}

static int redirect_stdout(int *routfd)
{
    int outfd, r;

    outfd = dup(STDOUT_FILENO);
    if (outfd < 0)
        return ty_error(TY_ERROR_SYSTEM, "dup() failed: %s", strerror(errno));

    r = dup2(STDERR_FILENO, STDOUT_FILENO);
    if (r < 0)
        return ty_error(TY_ERROR_SYSTEM, "dup2() failed: %s", strerror(errno));

    *routfd = outfd;
    return 0;
}

static void fill_descriptor_set(ty_descriptor_set *set, ty_board *board)
{
    ty_descriptor_set_clear(set);

    ty_board_manager_get_descriptors(ty_board_get_manager(board), set, 1);
    if (directions & DIRECTION_INPUT)
        ty_board_get_descriptors(board, TY_BOARD_CAPABILITY_SERIAL, set, 2);
#ifdef _WIN32
    if (directions & DIRECTION_OUTPUT)
        ty_descriptor_set_add(set, GetStdHandle(STD_INPUT_HANDLE), 3);
#else
    if (directions & DIRECTION_OUTPUT)
        ty_descriptor_set_add(set, STDIN_FILENO, 3);
#endif
}

static int loop(ty_board *board, int outfd)
{
    ty_descriptor_set set = {0};
    int timeout = -1;
    char buf[64];
    ssize_t r;

    fill_descriptor_set(&set, board);

    while (true) {
        memset(buf, 0, sizeof(buf));

        if (!set.count)
            return 0;

        r = ty_poll(&set, timeout);
        if (r < 0)
            return (int)r;

        switch (r) {
        case 0:
            return 0;

        case 1:
            r = ty_board_manager_refresh(ty_board_get_manager(board));
            if (r < 0)
                return (int)r;

            if (!ty_board_has_capability(board, TY_BOARD_CAPABILITY_SERIAL)) {
                printf("Waiting for device...\n");
                r = ty_board_wait_for(board, TY_BOARD_CAPABILITY_SERIAL, false, -1);
                if (r < 0)
                    return (int)r;

                fill_descriptor_set(&set, board);
                timeout = -1;

                printf("Connection ready\n");
            }

            break;

        case 2:
            r = ty_board_serial_read(board, buf, sizeof(buf));
            if (r < 0) {
                if (r == TY_ERROR_IO && reconnect) {
                    timeout = error_io_timeout;
                    ty_descriptor_set_remove(&set, 2);
                    ty_descriptor_set_remove(&set, 3);
                    break;
                }
                return (int)r;
            }

#ifdef _WIN32
            r = write(outfd, buf, (unsigned int)r);
#else
            r = write(outfd, buf, (size_t)r);
#endif
            if (r < 0) {
                if (errno == EIO)
                    return ty_error(TY_ERROR_IO, "I/O error on standard output");
                return ty_error(TY_ERROR_IO, "Failed to write to standard output: %s",
                                strerror(errno));
            }

            break;

        case 3:
            r = read(STDIN_FILENO, buf, sizeof(buf));
            if (r < 0) {
                if (errno == EIO)
                    return ty_error(TY_ERROR_IO, "I/O error on standard input");
                return ty_error(TY_ERROR_IO, "Failed to read from standard input: %s",
                                strerror(errno));
            }
            if (!r) {
                if (timeout_eof >= 0) {
                    /* EOF reached, don't listen to stdin anymore, and start timeout to give some
                       time for the device to send any data before closing down. */
                    timeout = timeout_eof;
                    ty_descriptor_set_remove(&set, 1);
                    ty_descriptor_set_remove(&set, 3);
                }
                break;
            }

#ifdef _WIN32
            if (fake_echo) {
                r = write(outfd, buf, (unsigned int)r);
                if (r < 0)
                    return (int)r;
            }
#endif

            r = ty_board_serial_write(board, buf, (size_t)r);
            if (r < 0) {
                if (r == TY_ERROR_IO && reconnect) {
                    timeout = error_io_timeout;
                    ty_descriptor_set_remove(&set, 2);
                    ty_descriptor_set_remove(&set, 3);
                    break;
                }
                return (int)r;
            }

            break;
        }
    }
}

int monitor(int argc, char *argv[])
{
    ty_board *board = NULL;
    int outfd = -1;
    int r;

    int c;
    while ((c = getopt_long(argc, argv, short_options, long_options, NULL)) != -1) {
        switch (c) {
        case OPTION_HELP:
            print_monitor_usage();
            return 0;

        case 's':
            terminal_flags |= TY_TERMINAL_SILENT;
            break;
        case 'r':
            terminal_flags |= TY_TERMINAL_RAW;
            break;

        case 'D':
            if (strcmp(optarg, "input") == 0) {
                directions = DIRECTION_INPUT;
            } else if (strcmp(optarg, "output") == 0) {
                directions = DIRECTION_OUTPUT;
            } else if (strcmp(optarg, "both") == 0) {
                directions = DIRECTION_INPUT | DIRECTION_OUTPUT;
            } else {
                return ty_error(TY_ERROR_PARAM, "--direction must be one off input, output or both");
            }
            break;

        case 'b':
            errno = 0;
            device_rate = (uint32_t)strtoul(optarg, NULL, 10);
            if (errno)
                return ty_error(TY_ERROR_PARAM, "--baud requires a number");
            break;
        case 'd':
           device_flags &= (uint16_t)~TY_SERIAL_CSIZE_MASK;
            if (strcmp(optarg, "5") == 0) {
                device_flags |= TY_SERIAL_5BITS_CSIZE;
            } else if (strcmp(optarg, "6") == 0) {
                device_flags |= TY_SERIAL_6BITS_CSIZE;
            } else if (strcmp(optarg, "7") == 0) {
                device_flags |= TY_SERIAL_7BITS_CSIZE;
            } else if (strcmp(optarg, "8") != 0) {
                return ty_error(TY_ERROR_PARAM, "--databits must be one off 5, 6, 7 or 8");
            }
        case 'f':
            device_flags &= (uint16_t)~TY_SERIAL_FLOW_MASK;
            if (strcmp(optarg, "x") == 0 || strcmp(optarg, "xonxoff") == 0) {
                device_flags |= TY_SERIAL_XONXOFF_FLOW;
            } else if (strcmp(optarg, "h") == 0 || strcmp(optarg, "rtscts") == 0) {
                device_flags |= TY_SERIAL_RTSCTS_FLOW;
            } else if (strcmp(optarg, "n") != 0 && strcmp(optarg, "none") == 0) {
                return ty_error(TY_ERROR_PARAM,
                                "--flow must be one off x (xonxoff), h (rtscts) or n (none)");
            }
            break;
        case OPTION_NORESET:
            device_flags |= TY_SERIAL_NOHUP_CLOSE;
            break;
        case 'p':
            device_flags &= (uint16_t)~TY_SERIAL_PARITY_MASK;
            if (strcmp(optarg, "o") == 0 || strcmp(optarg, "odd") == 0) {
                device_flags |= TY_SERIAL_ODD_PARITY;
            } else if (strcmp(optarg, "e") == 0 || strcmp(optarg, "even") == 0) {
                device_flags |= TY_SERIAL_EVEN_PARITY;
            } else if (strcmp(optarg, "n") != 0 && strcmp(optarg, "none") != 0) {
                return ty_error(TY_ERROR_PARAM,
                                "--parity must be one off o (odd), e (even) or n (none)");
            }
            break;

        case 'R':
            reconnect = true;
            break;

        case OPTION_TIMEOUT_EOF:
            errno = 0;
            timeout_eof = (int)strtol(optarg, NULL, 10);
            if (errno)
                return ty_error(TY_ERROR_PARSE, "--timeout requires a number");
            if (timeout_eof < 0)
                timeout_eof = -1;
            break;

        default:
            goto usage;
        }
    }

#ifdef _WIN32
    if (terminal_flags & TY_TERMINAL_RAW && !(terminal_flags & TY_TERMINAL_SILENT)) {
        terminal_flags |= TY_TERMINAL_SILENT;

        DWORD mode;
        if (GetConsoleMode(GetStdHandle(STD_OUTPUT_HANDLE), &mode))
            fake_echo = true;
    }
#endif

    ty_error_mask(TY_ERROR_UNSUPPORTED);
    r = ty_terminal_setup(terminal_flags);
    ty_error_unmask();
    if (r < 0) {
        if (r != TY_ERROR_UNSUPPORTED)
            goto cleanup;

#ifdef _WIN32
        // We're not in a console, don't echo
        fake_echo = false;
#endif
    }

    r = redirect_stdout(&outfd);
    if (r < 0)
        goto cleanup;

    r = get_board(&board);
    if (r < 0)
        goto cleanup;

    r = ty_board_serial_set_attributes(board, device_rate, device_flags);
    if (r < 0)
        goto cleanup;

    r = loop(board, outfd);

cleanup:
    ty_board_unref(board);
    return r;

usage:
    print_monitor_usage();
    return TY_ERROR_PARAM;
}
