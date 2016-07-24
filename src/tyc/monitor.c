/*
 * ty, a collection of GUI and command-line tools to manage Teensy devices
 *
 * Distributed under the MIT license (see LICENSE.txt or http://opensource.org/licenses/MIT)
 * Copyright (c) 2015 Niels Martignène <niels.martignene@gmail.com>
 */

#include <unistd.h>
#ifdef _WIN32
    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>
#endif
#include "hs/serial.h"
#include "ty/system.h"
#include "main.h"

enum {
    DIRECTION_INPUT = 1,
    DIRECTION_OUTPUT = 2
};

#define BUFFER_SIZE 8192
#define ERROR_IO_TIMEOUT 5000

static int terminal_flags = 0;
static uint32_t device_rate = 115200;
static int device_flags = 0;
static int directions = DIRECTION_INPUT | DIRECTION_OUTPUT;
static bool reconnect = false;
static int timeout_eof = 200;

#ifdef _WIN32
static bool fake_echo;

static bool input_run = true;
static HANDLE input_thread;

static HANDLE input_available;
static HANDLE input_processed;

static char input_line[BUFFER_SIZE];
static ssize_t input_ret;
#endif

static void print_monitor_usage(FILE *f)
{
    fprintf(f, "usage: %s monitor [options]\n\n", executable_name);

    print_common_options(f);
    fprintf(f, "\n");

    fprintf(f, "Monitor options:\n"
               "   -b, --baud <rate>        Use baudrate for serial port\n"
               "   -d, --databits <bits>    Change number of bits for each character\n"
               "                            Must be one of 5, 6, 7 or 8 (default)\n"
               "   -D, --direction <dir>    Open serial connection in given direction\n"
               "                            Supports input, output, both (default)\n"
               "   -f, --flow <control>     Define flow-control mode\n"
               "                            Supports xonxoff (x), rtscts (h) and none (n)\n"
               "   -p, --parity <bits>      Change parity mode to use for the serial port\n"
               "                            Supports odd (o), even (e) and none (n)\n\n"
               "   -r, --raw                Disable line-buffering and line-editing\n"
               "   -s, --silent             Disable echoing of local input on terminal\n\n"
               "   -R, --reconnect          Try to reconnect on I/O errors\n"
               "       --noreset            Don't reset serial port when closing\n"
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

static unsigned int __stdcall stdin_thread(void *udata)
{
    TY_UNUSED(udata);

    DWORD len;
    BOOL success;
    int r;

    while (input_run) {
        WaitForSingleObject(input_processed, INFINITE);
        ResetEvent(input_processed);

        success = ReadFile(GetStdHandle(STD_INPUT_HANDLE), input_line, sizeof(input_line), &len, NULL);
        if (!success) {
            r = ty_error(TY_ERROR_IO, "I/O error while reading standard input");
            goto error;
        }
        if (!len) {
            r = 0;
            goto error;
        }

        input_ret = (ssize_t)len;
        SetEvent(input_available);
    }

    return 0;

error:
    input_ret = r;
    SetEvent(input_available);
    return 0;
}

static int start_stdin_thread(void)
{
    input_available = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (!input_available)
        return ty_error(TY_ERROR_SYSTEM, "CreateEvent() failed: %s", ty_win32_strerror(0));

    input_processed = CreateEvent(NULL, TRUE, TRUE, NULL);
    if (!input_processed)
        return ty_error(TY_ERROR_SYSTEM, "CreateEvent() failed: %s", ty_win32_strerror(0));

    input_thread = (HANDLE)_beginthreadex(NULL, 0, stdin_thread, NULL, 0, NULL);
    if (!input_thread)
        return ty_error(TY_ERROR_SYSTEM, "_beginthreadex() failed: %s", ty_win32_strerror(0));

    return 0;
}

static void stop_stdin_thread(void)
{
    if (input_thread) {
        CONSOLE_SCREEN_BUFFER_INFO sb;
        INPUT_RECORD ir = {0};
        DWORD written;

        // This is not enough because the background thread may be blocked in ReadFile
        input_run = false;
        SetEvent(input_processed);

        /* We'll soon push VK_RETURN to the console input, which will result in a newline,
           so move the cursor up one line to avoid showing it. */
        GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &sb);
        if (sb.dwCursorPosition.Y > 0) {
            sb.dwCursorPosition.Y--;
            SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), sb.dwCursorPosition);
        }

        ir.EventType = KEY_EVENT;
        ir.Event.KeyEvent.bKeyDown = TRUE;
        ir.Event.KeyEvent.dwControlKeyState = 0;
        ir.Event.KeyEvent.uChar.AsciiChar = '\r';
        ir.Event.KeyEvent.wRepeatCount = 1;

        // Write a newline to snap the background thread out of the blocking ReadFile call
        WriteConsoleInput(GetStdHandle(STD_INPUT_HANDLE), &ir, 1, &written);

        WaitForSingleObject(input_thread, INFINITE);
        CloseHandle(input_thread);
    }

    if (input_processed)
        CloseHandle(input_processed);
    if (input_available)
        CloseHandle(input_available);
}

#endif

static int fill_descriptor_set(ty_descriptor_set *set, ty_board *board)
{
    ty_descriptor_set_clear(set);

    ty_monitor_get_descriptors(ty_board_get_monitor(board), set, 1);
    if (directions & DIRECTION_INPUT) {
        ty_board_interface *iface;
        int r;

        r = ty_board_open_interface(board, TY_BOARD_CAPABILITY_SERIAL, &iface);
        if (r < 0)
            return r;

        ty_board_interface_get_descriptors(iface, set, 2);

        /* ty_board_interface_unref() keeps iface->open_count > 0 so the interface
           does not get closed, and we can monitor the handle. */
        ty_board_interface_unref(iface);
    }
#ifdef _WIN32
    if (directions & DIRECTION_OUTPUT) {
        if (input_available) {
            ty_descriptor_set_add(set, input_available, 3);
        } else {
            ty_descriptor_set_add(set, GetStdHandle(STD_INPUT_HANDLE), 3);
        }
    }
#else
    if (directions & DIRECTION_OUTPUT)
        ty_descriptor_set_add(set, STDIN_FILENO, 3);
#endif

    return 0;
}

static int loop(ty_board *board, int outfd)
{
    ty_descriptor_set set = {0};
    int timeout;
    char buf[BUFFER_SIZE];
    ssize_t r;

restart:
    r = ty_board_serial_set_attributes(board, device_rate, device_flags);
    if (r < 0)
        return (int)r;

    r = fill_descriptor_set(&set, board);
    if (r < 0)
        return (int)r;
    timeout = -1;

    ty_log(TY_LOG_INFO, "Monitoring '%s'", ty_board_get_tag(board));

    while (true) {
        if (!set.count)
            return 0;

        r = ty_poll(&set, timeout);
        if (r < 0)
            return (int)r;

        switch (r) {
        case 0:
            return 0;

        case 1:
            r = ty_monitor_refresh(ty_board_get_monitor(board));
            if (r < 0)
                return (int)r;

            if (!ty_board_has_capability(board, TY_BOARD_CAPABILITY_SERIAL)) {
                if (!reconnect)
                    return 0;

                ty_log(TY_LOG_INFO, "Waiting for '%s'...", ty_board_get_tag(board));
                r = ty_board_wait_for(board, TY_BOARD_CAPABILITY_SERIAL, -1);
                if (r < 0)
                    return (int)r;

                goto restart;
            }

            break;

        case 2:
            r = ty_board_serial_read(board, buf, sizeof(buf), 0);
            if (r < 0) {
                if (r == TY_ERROR_IO && reconnect) {
                    timeout = ERROR_IO_TIMEOUT;
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
#ifdef _WIN32
            if (input_available) {
                if (input_ret < 0)
                    return (int)input_ret;

                memcpy(buf, input_line, (size_t)input_ret);
                r = input_ret;

                ResetEvent(input_available);
                SetEvent(input_processed);
            } else {
                r = read(STDIN_FILENO, buf, sizeof(buf));
            }
#else
            r = read(STDIN_FILENO, buf, sizeof(buf));
#endif
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
                    timeout = ERROR_IO_TIMEOUT;
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
    ty_optline_context optl;
    char *opt;
    ty_board *board = NULL;
    int outfd = -1;
    int r;

    ty_optline_init_argv(&optl, argc, argv);
    while ((opt = ty_optline_next_option(&optl))) {
        if (strcmp(opt, "--help") == 0) {
            print_monitor_usage(stdout);
            return EXIT_SUCCESS;
        } else if (strcmp(opt, "--baud") == 0 || strcmp(opt, "-b") == 0) {
            char *value = ty_optline_get_value(&optl);
            if (!value) {
                ty_log(TY_LOG_ERROR, "Option '--baud' takes an argument");
                print_monitor_usage(stderr);
                return EXIT_FAILURE;
            }

            errno = 0;
            device_rate = (uint32_t)strtoul(value, NULL, 10);
            if (errno) {
                ty_log(TY_LOG_ERROR, "--baud requires a number");
                print_monitor_usage(stderr);
                return EXIT_FAILURE;
            }
        } else if (strcmp(opt, "--databits") == 0 || strcmp(opt, "-d") == 0) {
            char *value = ty_optline_get_value(&optl);
            if (!value) {
                ty_log(TY_LOG_ERROR, "Option '--databits' takes an argument");
                print_monitor_usage(stderr);
                return EXIT_FAILURE;
            }

            device_flags &= ~HS_SERIAL_MASK_CSIZE;
            if (strcmp(value, "5") == 0) {
                device_flags |= HS_SERIAL_CSIZE_5BITS;
            } else if (strcmp(value, "6") == 0) {
                device_flags |= HS_SERIAL_CSIZE_6BITS;
            } else if (strcmp(value, "7") == 0) {
                device_flags |= HS_SERIAL_CSIZE_7BITS;
            } else if (strcmp(value, "8") != 0) {
                ty_log(TY_LOG_ERROR, "--databits must be one off 5, 6, 7 or 8");
                print_monitor_usage(stderr);
                return EXIT_FAILURE;
            }
        } else if (strcmp(opt, "--direction") == 0 || strcmp(opt, "-D") == 0) {
            char *value = ty_optline_get_value(&optl);
            if (!value) {
                ty_log(TY_LOG_ERROR, "Option '--direction' takes an argument");
                print_monitor_usage(stderr);
                return EXIT_FAILURE;
            }

            if (strcmp(value, "input") == 0) {
                directions = DIRECTION_INPUT;
            } else if (strcmp(value, "output") == 0) {
                directions = DIRECTION_OUTPUT;
            } else if (strcmp(value, "both") == 0) {
                directions = DIRECTION_INPUT | DIRECTION_OUTPUT;
            } else {
                ty_log(TY_LOG_ERROR, "--direction must be one off input, output or both");
                print_monitor_usage(stderr);
                return EXIT_FAILURE;
            }
        } else if (strcmp(opt, "--flow") == 0 || strcmp(opt, "-f") == 0) {
            char *value = ty_optline_get_value(&optl);
            if (!value) {
                ty_log(TY_LOG_ERROR, "Option '--flow' takes an argument");
                print_monitor_usage(stderr);
                return EXIT_FAILURE;
            }

            device_flags &= ~HS_SERIAL_MASK_FLOW;
            if (strcmp(value, "x") == 0 || strcmp(value, "xonxoff") == 0) {
                device_flags |= HS_SERIAL_FLOW_XONXOFF;
            } else if (strcmp(value, "h") == 0 || strcmp(value, "rtscts") == 0) {
                device_flags |= HS_SERIAL_FLOW_RTSCTS;
            } else if (strcmp(value, "n") != 0 && strcmp(value, "none") == 0) {
                ty_log(TY_LOG_ERROR, "--flow must be one off x (xonxoff), h (rtscts) or n (none)");
                print_monitor_usage(stderr);
                return EXIT_FAILURE;
            }
        } else if (strcmp(opt, "--noreset") == 0) {
            device_flags |= HS_SERIAL_CLOSE_NOHUP;
        } else if (strcmp(opt, "--parity") == 0 || strcmp(opt, "-p") == 0) {
            char *value = ty_optline_get_value(&optl);
            if (!value) {
                ty_log(TY_LOG_ERROR, "Option '--parity' takes an argument");
                print_monitor_usage(stderr);
                return EXIT_FAILURE;
            }

            device_flags &= ~HS_SERIAL_MASK_PARITY;
            if (strcmp(value, "o") == 0 || strcmp(value, "odd") == 0) {
                device_flags |= HS_SERIAL_PARITY_ODD;
            } else if (strcmp(value, "e") == 0 || strcmp(value, "even") == 0) {
                device_flags |= HS_SERIAL_PARITY_EVEN;
            } else if (strcmp(value, "n") != 0 && strcmp(value, "none") != 0) {
                ty_log(TY_LOG_ERROR, "--parity must be one off o (odd), e (even) or n (none)");
                print_monitor_usage(stderr);
                return EXIT_FAILURE;
            }
        } else if (strcmp(opt, "--raw") == 0 || strcmp(opt, "-r") == 0) {
            terminal_flags |= TY_TERMINAL_RAW;
        } else if (strcmp(opt, "--reconnect") == 0 || strcmp(opt, "-R") == 0) {
            reconnect = true;
        } else if (strcmp(opt, "--silent") == 0 || strcmp(opt, "-s") == 0) {
            terminal_flags |= TY_TERMINAL_SILENT;
        } else if (strcmp(opt, "--timeout-eof") == 0) {
            char *value = ty_optline_get_value(&optl);
            if (!value) {
                ty_log(TY_LOG_ERROR, "Option '--timeout-eof' takes an argument");
                print_monitor_usage(stderr);
                return EXIT_FAILURE;
            }

            errno = 0;
            timeout_eof = (int)strtol(value, NULL, 10);
            if (errno) {
                ty_log(TY_LOG_ERROR, "--timeout requires a number");
                print_monitor_usage(stderr);
                return EXIT_FAILURE;
            }
            if (timeout_eof < 0)
                timeout_eof = -1;
        } else if (!parse_common_option(&optl, opt)) {
            print_monitor_usage(stderr);
            return EXIT_FAILURE;
        }
    }
    if (ty_optline_consume_non_option(&optl)) {
        ty_log(TY_LOG_ERROR, "No positional argument is allowed");
        print_monitor_usage(stderr);
        return EXIT_FAILURE;
    }

    if (ty_standard_get_modes(TY_STANDARD_INPUT) & TY_DESCRIPTOR_MODE_TERMINAL) {
#ifdef _WIN32
        if (terminal_flags & TY_TERMINAL_RAW && !(terminal_flags & TY_TERMINAL_SILENT)) {
            terminal_flags |= TY_TERMINAL_SILENT;

            if (ty_standard_get_modes(TY_STANDARD_OUTPUT) & TY_DESCRIPTOR_MODE_TERMINAL)
                fake_echo = true;
        }

        /* Unlike POSIX platforms, Windows does not implement the console line editing behavior
         * at the tty layer. Instead, ReadFile() takes care of it and blocks until return is hit.
         * The problem is that the Wait functions will return the stdin descriptor as soon as
         * something is typed but then, ReadFile() will block until return is pressed.
         * Overlapped I/O cannot be used because it is not supported on console descriptors.
         *
         * So the best way I found is to have a background thread handle the blocking ReadFile()
         * and pass the lines in a buffer. When a new line is entered, the input_available
         * event is set to signal the poll in loop(). I also tried to use an anonymous pipe to
         * make it simpler, but the Wait functions do not support them. */
        if (directions & DIRECTION_OUTPUT && !(terminal_flags & TY_TERMINAL_RAW)) {
            r = start_stdin_thread();
            if (r < 0)
                goto cleanup;
        }
#endif

        r = ty_terminal_setup(terminal_flags);
        if (r < 0)
            goto cleanup;
    }

    r = redirect_stdout(&outfd);
    if (r < 0)
        goto cleanup;

    r = get_board(&board);
    if (r < 0)
        goto cleanup;

    r = loop(board, outfd);

cleanup:
#ifdef _WIN32
    stop_stdin_thread();
#endif
    ty_board_unref(board);
    return r < 0 ? EXIT_FAILURE : EXIT_SUCCESS;
}
