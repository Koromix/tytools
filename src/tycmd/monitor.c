/* TyTools - public domain
   Niels Martignène <niels.martignene@protonmail.com>
   https://koromix.dev/tytools

   This software is in the public domain. Where that dedication is not
   recognized, you are granted a perpetual, irrevocable license to copy,
   distribute, and modify this file as you see fit.

   See the LICENSE file for more details. */

#include <unistd.h>
#ifdef _WIN32
    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>
#endif
#include "../libhs/device.h"
#include "../libhs/serial.h"
#include "../libty/system.h"
#include "main.h"

enum {
    DIRECTION_INPUT = 1,
    DIRECTION_OUTPUT = 2
};

#define BUFFER_SIZE 8192
#define ERROR_IO_TIMEOUT 5000

static int monitor_term_flags = 0;
static hs_serial_config monitor_serial_config = {
    .baudrate = 115200
};
static int monitor_directions = DIRECTION_INPUT | DIRECTION_OUTPUT;
static bool monitor_reconnect = false;
static int monitor_timeout_eof = 200;

#ifdef _WIN32
static bool monitor_fake_echo;

static bool monitor_input_run = true;
static HANDLE monitor_input_thread;

static HANDLE monitor_input_available;
static HANDLE monitor_input_processed;

static char monitor_input_line[BUFFER_SIZE];
static ssize_t monitor_input_ret;
#endif

static void print_monitor_usage(FILE *f)
{
    fprintf(f, "usage: %s monitor [options]\n\n", tycmd_executable_name);

    print_common_options(f);
    fprintf(f, "\n");

    fprintf(f, "Monitor options:\n"
               "   -r, --raw                Disable line-buffering and line-editing\n"
               "   -s, --silent             Disable echoing of local input on terminal\n\n"
               "   -R, --reconnect          Try to reconnect on I/O errors\n"
               "   -D, --direction <dir>    Open serial connection in given direction\n"
               "                            Supports input, output, both (default)\n"
               "       --timeout-eof <ms>   Time before closing after EOF on standard input\n"
               "                            Defaults to %d ms, use -1 to disable\n\n", monitor_timeout_eof);

    fprintf(f, "Serial settings:\n"
               "   -b, --baudrate <rate>    Use baudrate for serial port\n"
               "                            Default: %u bauds\n"
               "   -d, --databits <bits>    Change number of bits for every character\n"
               "                            Must be one of: 5, 6, 7 or 8\n"
               "   -p, --stopbits <bits>    Change number of stop bits for every character\n"
               "                            Must be one of: 1 or 2\n"
               "   -f, --flow <control>     Define flow-control mode\n"
               "                            Must be one of: off, rtscts or xonxoff\n"
               "   -y, --parity <bits>      Change parity mode to use for the serial port\n"
               "                            Must be one of: off, even, or odd\n\n"
               "These settings are mostly ignored by the USB serial emulation, but you can still\n"
               "access them in your embedded code (e.g. the Serial object API on Teensy).\n",
               monitor_serial_config.baudrate);
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

    while (monitor_input_run) {
        WaitForSingleObject(monitor_input_processed, INFINITE);
        ResetEvent(monitor_input_processed);

        success = ReadFile(GetStdHandle(STD_INPUT_HANDLE), monitor_input_line,
                           sizeof(monitor_input_line), &len, NULL);
        if (!success) {
            r = ty_error(TY_ERROR_IO, "I/O error while reading standard input");
            goto error;
        }
        if (!len) {
            r = 0;
            goto error;
        }

        monitor_input_ret = (ssize_t)len;
        SetEvent(monitor_input_available);
    }

    return 0;

error:
    monitor_input_ret = r;
    SetEvent(monitor_input_available);
    return 0;
}

static int start_stdin_thread(void)
{
    monitor_input_available = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (!monitor_input_available)
        return ty_error(TY_ERROR_SYSTEM, "CreateEvent() failed: %s", ty_win32_strerror(0));

    monitor_input_processed = CreateEvent(NULL, TRUE, TRUE, NULL);
    if (!monitor_input_processed)
        return ty_error(TY_ERROR_SYSTEM, "CreateEvent() failed: %s", ty_win32_strerror(0));

    monitor_input_thread = (HANDLE)_beginthreadex(NULL, 0, stdin_thread, NULL, 0, NULL);
    if (!monitor_input_thread)
        return ty_error(TY_ERROR_SYSTEM, "_beginthreadex() failed: %s", ty_win32_strerror(0));

    return 0;
}

static void stop_stdin_thread(void)
{
    if (monitor_input_thread) {
        CONSOLE_SCREEN_BUFFER_INFO sb;
        INPUT_RECORD ir = {0};
        DWORD written;

        // This is not enough because the background thread may be blocked in ReadFile
        monitor_input_run = false;
        SetEvent(monitor_input_processed);

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

        WaitForSingleObject(monitor_input_thread, INFINITE);
        CloseHandle(monitor_input_thread);
    }

    if (monitor_input_processed)
        CloseHandle(monitor_input_processed);
    if (monitor_input_available)
        CloseHandle(monitor_input_available);
}

#endif

static int open_serial_interface(ty_board *board, ty_board_interface **riface)
{
    ty_board_interface *iface;
    int r;

    r = ty_board_open_interface(board, TY_BOARD_CAPABILITY_SERIAL, &iface);
    if (r < 0)
        return r;
    if (!r)
        return ty_error(TY_ERROR_MODE, "Board '%s' is not available for serial I/O",
                        ty_board_get_tag(board));

    if (ty_board_interface_get_device(iface)->type == HS_DEVICE_TYPE_SERIAL) {
        r = hs_serial_set_config(ty_board_interface_get_handle(iface), &monitor_serial_config);
        if (r < 0)
            return (int)r;
    }

    *riface = iface;
    return 0;
}

static int fill_descriptor_set(ty_descriptor_set *set, ty_board *board)
{
    ty_board_interface *iface = NULL;
    int r;

    ty_descriptor_set_clear(set);

    // Board events / state changes
    ty_monitor_get_descriptors(ty_board_get_monitor(board), set, 1);

    r = open_serial_interface(board, &iface);
    if (r < 0)
        return r;

    if (monitor_directions & DIRECTION_INPUT)
        ty_board_interface_get_descriptors(iface, set, 2);
#ifdef _WIN32
    if (monitor_directions & DIRECTION_OUTPUT) {
        if (monitor_input_available) {
            ty_descriptor_set_add(set, monitor_input_available, 3);
        } else {
            ty_descriptor_set_add(set, GetStdHandle(STD_INPUT_HANDLE), 3);
        }
    }
#else
    if (monitor_directions & DIRECTION_OUTPUT)
        ty_descriptor_set_add(set, STDIN_FILENO, 3);
#endif

    /* ty_board_interface_unref() keeps iface->open_count > 0 so the device file does not
       get closed, and we can monitor the descriptor. When the refcount reaches 0, the
       device is closed anyway so we don't leak anything. */
    ty_board_interface_unref(iface);

    return 0;
}

static int loop(ty_board *board, int outfd)
{
    ty_descriptor_set set = {0};
    int timeout;
    char buf[BUFFER_SIZE];
    ssize_t r;

restart:
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
            case 0: {
                return 0;
            } break;

            case 1: {
                r = ty_monitor_refresh(ty_board_get_monitor(board));
                if (r < 0)
                    return (int)r;

                if (!ty_board_has_capability(board, TY_BOARD_CAPABILITY_SERIAL)) {
                    if (!monitor_reconnect)
                        return 0;

                    ty_log(TY_LOG_INFO, "Waiting for '%s'...", ty_board_get_tag(board));
                    r = ty_board_wait_for(board, TY_BOARD_CAPABILITY_SERIAL, -1);
                    if (r < 0)
                        return (int)r;

                    goto restart;
                }
            } break;

            case 2: {
                r = ty_board_serial_read(board, buf, sizeof(buf), 0);
                if (r < 0) {
                    if (r == TY_ERROR_IO && monitor_reconnect) {
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
            } break;

            case 3: {
#ifdef _WIN32
                if (monitor_input_available) {
                    if (monitor_input_ret < 0)
                        return (int)monitor_input_ret;

                    memcpy(buf, monitor_input_line, (size_t)monitor_input_ret);
                    r = monitor_input_ret;

                    ResetEvent(monitor_input_available);
                    SetEvent(monitor_input_processed);
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
                    if (monitor_timeout_eof >= 0) {
                        /* EOF reached, don't listen to stdin anymore, and start timeout to give some
                           time for the device to send any data before closing down. */
                        timeout = monitor_timeout_eof;
                        ty_descriptor_set_remove(&set, 1);
                        ty_descriptor_set_remove(&set, 3);
                    }
                    break;
                }

#ifdef _WIN32
                if (monitor_fake_echo) {
                    r = write(outfd, buf, (unsigned int)r);
                    if (r < 0)
                        return (int)r;
                }
#endif

                r = ty_board_serial_write(board, buf, (size_t)r);
                if (r < 0) {
                    if (r == TY_ERROR_IO && monitor_reconnect) {
                        timeout = ERROR_IO_TIMEOUT;
                        ty_descriptor_set_remove(&set, 2);
                        ty_descriptor_set_remove(&set, 3);
                        break;
                    }
                    return (int)r;
                }
            } break;
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
        } else if (strcmp(opt, "--baudrate") == 0 || strcmp(opt, "-b") == 0) {
            char *value = ty_optline_get_value(&optl);
            if (!value) {
                ty_log(TY_LOG_ERROR, "Option '--baudrate' takes an argument");
                print_monitor_usage(stderr);
                return EXIT_FAILURE;
            }

            errno = 0;
            monitor_serial_config.baudrate = (uint32_t)strtoul(value, NULL, 10);
            if (errno) {
                ty_log(TY_LOG_ERROR, "--baudrate requires a number");
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

            monitor_serial_config.databits = (unsigned int)strtoul(value, NULL, 10);
            if (monitor_serial_config.databits < 5 || monitor_serial_config.databits > 8) {
                ty_log(TY_LOG_ERROR, "--databits must be one of: 5, 6, 7 or 8");
                print_monitor_usage(stderr);
                return EXIT_FAILURE;
            }
        } else if (strcmp(opt, "--stopbits") == 0 || strcmp(opt, "-p") == 0) {
            char *value = ty_optline_get_value(&optl);
            if (!value) {
                ty_log(TY_LOG_ERROR, "Option '--stopbits' takes an argument");
                print_monitor_usage(stderr);
                return EXIT_FAILURE;
            }

            monitor_serial_config.stopbits = (unsigned int)strtoul(value, NULL, 10);
            if (monitor_serial_config.stopbits < 1 || monitor_serial_config.stopbits > 2) {
                ty_log(TY_LOG_ERROR, "--stopbits must be one of: 1 or 2");
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
                monitor_directions = DIRECTION_INPUT;
            } else if (strcmp(value, "output") == 0) {
                monitor_directions = DIRECTION_OUTPUT;
            } else if (strcmp(value, "both") == 0) {
                monitor_directions = DIRECTION_INPUT | DIRECTION_OUTPUT;
            } else {
                ty_log(TY_LOG_ERROR, "--direction must be one of: input, output or both");
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

            if (strcmp(value, "off") == 0) {
                monitor_serial_config.rts = HS_SERIAL_CONFIG_RTS_OFF;
                monitor_serial_config.xonxoff = HS_SERIAL_CONFIG_XONXOFF_OFF;
            } else if (strcmp(value, "xonxoff") == 0) {
                monitor_serial_config.rts = HS_SERIAL_CONFIG_RTS_OFF;
                monitor_serial_config.xonxoff = HS_SERIAL_CONFIG_XONXOFF_INOUT;
            } else if (strcmp(value, "rtscts") == 0) {
                monitor_serial_config.rts = HS_SERIAL_CONFIG_RTS_FLOW;
                monitor_serial_config.xonxoff = HS_SERIAL_CONFIG_XONXOFF_OFF;
            } else if (strcmp(value, "off") != 0) {
                ty_log(TY_LOG_ERROR, "--flow must be one of: off, rtscts or xonxoff");
                print_monitor_usage(stderr);
                return EXIT_FAILURE;
            }
        } else if (strcmp(opt, "--parity") == 0 || strcmp(opt, "-y") == 0) {
            char *value = ty_optline_get_value(&optl);
            if (!value) {
                ty_log(TY_LOG_ERROR, "Option '--parity' takes an argument");
                print_monitor_usage(stderr);
                return EXIT_FAILURE;
            }

            if (strcmp(value, "off") == 0) {
                monitor_serial_config.parity = HS_SERIAL_CONFIG_PARITY_OFF;
            } else if (strcmp(value, "even") == 0) {
                monitor_serial_config.parity = HS_SERIAL_CONFIG_PARITY_EVEN;
            } else if (strcmp(value, "odd") == 0) {
                monitor_serial_config.parity = HS_SERIAL_CONFIG_PARITY_ODD;
            } else if (strcmp(value, "mark") == 0) {
                monitor_serial_config.parity = HS_SERIAL_CONFIG_PARITY_MARK;
            } else if (strcmp(value, "space") == 0) {
                monitor_serial_config.parity = HS_SERIAL_CONFIG_PARITY_SPACE;
            } else {
                ty_log(TY_LOG_ERROR, "--parity must be one of: off, even, mark or space");
                print_monitor_usage(stderr);
                return EXIT_FAILURE;
            }
        } else if (strcmp(opt, "--raw") == 0 || strcmp(opt, "-r") == 0) {
            monitor_term_flags |= TY_TERMINAL_RAW;
        } else if (strcmp(opt, "--reconnect") == 0 || strcmp(opt, "-R") == 0) {
            monitor_reconnect = true;
        } else if (strcmp(opt, "--silent") == 0 || strcmp(opt, "-s") == 0) {
            monitor_term_flags |= TY_TERMINAL_SILENT;
        } else if (strcmp(opt, "--timeout-eof") == 0) {
            char *value = ty_optline_get_value(&optl);
            if (!value) {
                ty_log(TY_LOG_ERROR, "Option '--timeout-eof' takes an argument");
                print_monitor_usage(stderr);
                return EXIT_FAILURE;
            }

            errno = 0;
            monitor_timeout_eof = (int)strtol(value, NULL, 10);
            if (errno) {
                ty_log(TY_LOG_ERROR, "--timeout requires a number");
                print_monitor_usage(stderr);
                return EXIT_FAILURE;
            }
            if (monitor_timeout_eof < 0)
                monitor_timeout_eof = -1;
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

    if (ty_standard_get_modes(TY_STREAM_INPUT) & TY_DESCRIPTOR_MODE_TERMINAL) {
#ifdef _WIN32
        if (monitor_term_flags & TY_TERMINAL_RAW && !(monitor_term_flags & TY_TERMINAL_SILENT)) {
            monitor_term_flags |= TY_TERMINAL_SILENT;

            if (ty_standard_get_modes(TY_STREAM_OUTPUT) & TY_DESCRIPTOR_MODE_TERMINAL)
                monitor_fake_echo = true;
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
        if (monitor_directions & DIRECTION_OUTPUT && !(monitor_term_flags & TY_TERMINAL_RAW)) {
            r = start_stdin_thread();
            if (r < 0)
                goto cleanup;
        }
#endif

        r = ty_terminal_setup(monitor_term_flags);
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
