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
#ifdef _WIN32
#include <windows.h>
#else
#include <poll.h>
#endif
#include <signal.h>
#include <unistd.h>
#include "board.h"
#include "main.h"
#include "system.h"

enum {
    OPTION_HELP = 0x100,
    OPTION_NORESET,
    OPTION_TIMEOUT_EOF
};

static const char *short_options = "b:d:f:p:rRs";
static const struct option long_options[] = {
    {"baud",        required_argument, NULL, 'b'},
    {"databits",    required_argument, NULL, 'd'},
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

static uint16_t terminal_flags = 0;
#ifdef _WIN32
static bool fake_echo = false;
#endif
static uint32_t device_rate = 115200;
static uint16_t device_flags = 0;
static bool reconnect = false;
static int timeout_eof = 200;

void print_monitor_usage(void)
{
    fprintf(stderr, "usage: ty monitor [--help] [options]\n\n"
                    "Options:\n"
                    "   -b, --baud <rate>        Use baudrate for serial port\n"
                    "   -d, --databits <bits>    Change number of bits for each character\n"
                    "                            Must be one of 5, 6, 7 or 8 (default)\n"
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

#ifdef _WIN32
static ssize_t wait_descriptor(HANDLE *handles, size_t count, int timeout)
{
    DWORD ret = WaitForMultipleObjects(count, handles, FALSE,
                                       timeout < 0 ? INFINITE : (DWORD)timeout);
    switch (ret) {
    case WAIT_FAILED:
        return ty_error(TY_ERROR_SYSTEM, "WaitForMultipleObjects() failed: %s",
                        ty_win32_strerror(0));
    case WAIT_TIMEOUT:
        return TY_ERROR_TIMEOUT;
    }

    return (ssize_t)(ret - WAIT_OBJECT_0);
}
#else
static ssize_t wait_descriptor(struct pollfd *pfd, size_t count, int timeout)
{
    int r;

    if (timeout < 0)
        timeout = -1;

restart:
    r = poll(pfd, count, timeout);
    if (r < 0) {
        if (errno == EINTR)
            goto restart;
        return ty_error(TY_ERROR_SYSTEM, "poll() failed: %s", strerror(errno));
    }
    if (!r)
        return TY_ERROR_TIMEOUT;

    for (size_t i = 0; i < count; i++) {
        if (pfd[i].revents & (POLLIN | POLLERR | POLLHUP))
            return (ssize_t)i;
    }
    assert(false);
}
#endif

static int loop(ty_board *board, int outfd)
{
#ifdef _WIN32
    HANDLE desc[2] = {NULL};
#else
    struct pollfd desc[2] = {{0}};
#endif
    size_t count = TY_COUNTOF(desc);
    int timeout = -1;
    char buf[64];
    ssize_t r;

#ifdef _WIN32
    desc[0] = board->h->ov->hEvent;
    desc[1] = GetStdHandle(STD_INPUT_HANDLE);
#else
    desc[0].events = POLLIN;
    desc[0].fd = board->h->fd;
    desc[1].events = POLLIN;
    desc[1].fd = STDIN_FILENO;
#endif

    while (true) {
        memset(buf, 0, sizeof(buf));

        r = wait_descriptor(desc, count, timeout);
        if (r < 0) {
            if (r == TY_ERROR_TIMEOUT)
                return 0;
            return (int)r;
        }

        switch (r) {
        case 0:
            r = ty_board_read_serial(board, buf, sizeof(buf));
            if (r < 0)
                return (int)r;

            r = write(outfd, buf, (size_t)r);
            if (r < 0) {
                if (errno == EIO)
                    return ty_error(TY_ERROR_IO, "I/O error on standard output");
                return ty_error(TY_ERROR_IO, "Failed to write to standard output: %s",
                                strerror(errno));
            }

            break;

        case 1:
            r = read(STDIN_FILENO, buf, sizeof(buf));
            if (r < 0) {
                if (errno == EIO)
                    return ty_error(TY_ERROR_IO, "I/O error on standard input");
                return ty_error(TY_ERROR_IO, "Failed to read from standard input: %s",
                                strerror(errno));
            }
            if (!r) {
                // EOF reached, don't listen to stdin anymore, and start timeout to give some
                // time for the device to send any data before closing down
                timeout = timeout_eof;
                count--;
                break;
            }

#ifdef _WIN32
            if (fake_echo) {
                r = write(outfd, buf, (size_t)r);
                if (r < 0)
                    return (int)r;
            }
#endif

            r = ty_board_write_serial(board, buf, (size_t)r);
            if (r < 0)
                return (int)r;

            break;
        }
    }
}

int monitor(int argc, char *argv[])
{
    ty_board *board = NULL;
    int outfd, r;

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
    r = ty_terminal_change(terminal_flags);
    ty_error_unmask();
    if (r < 0) {
        if (r != TY_ERROR_UNSUPPORTED)
            return r;
#ifdef _WIN32
        // We're not in a console, don't echo
        fake_echo = false;
#endif
    }

    r = redirect_stdout(&outfd);
    if (r < 0)
        return r;

reconnect:
    if (!board) {
        r = get_board(&board);
        if (r < 0)
            return r;
    } else {
        printf("Waiting for serial connection...\n");
        do {
            r = ty_board_probe(board, 500);
        } while (!r);
        if (r < 0)
            return r;
    }

    if (!ty_board_has_capability(board, TY_BOARD_CAPABILITY_SERIAL))
        return ty_error(TY_ERROR_MODE, "Serial connection is not available in this mode");

    printf("Connection ready\n");

    if (board->dev->type == TY_DEVICE_SERIAL) {
        r = ty_serial_set_control(board->h, device_rate, device_flags);
        if (r < 0)
            return r;
    }

    r = loop(board, outfd);
    if (reconnect && r == TY_ERROR_IO)
        goto reconnect;

    return r;

usage:
    print_monitor_usage();
    return TY_ERROR_PARAM;
}
