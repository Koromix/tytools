/*
 * ty, a collection of GUI and command-line tools to manage Teensy devices
 *
 * Distributed under the MIT license (see LICENSE.txt or http://opensource.org/licenses/MIT)
 * Copyright (c) 2015 Niels Martignène <niels.martignene@gmail.com>
 */

#include <getopt.h>
#include "ty.h"
#include "main.h"

enum {
    UPLOAD_OPTION_NOCHECK = 0x200,
    UPLOAD_OPTION_NORESET
};

static const char *short_options = MAIN_SHORT_OPTIONS "f:w";
static const struct option long_options[] = {
    MAIN_LONG_OPTIONS
    {"format",     required_argument, NULL, 'f'},
    {"nocheck",    no_argument,       NULL, UPLOAD_OPTION_NOCHECK},
    {"noreset",    no_argument,       NULL, UPLOAD_OPTION_NORESET},
    {"wait",       no_argument,       NULL, 'w'},
    {0}
};

static int upload_flags = 0;
static const char *firmware_format = NULL;
static const char *firmware_filename = NULL;

void print_upload_usage(FILE *f)
{
    fprintf(f, "usage: tyc upload [options] <filename>\n\n");

    print_main_options(f);
    fprintf(f, "\n");

    fprintf(f, "Upload options:\n"
               "   -w, --wait               Wait for the bootloader instead of rebooting\n"
               "       --nocheck            Force upload even if the board is not compatible\n"
               "       --noreset            Do not reset the device once the upload is finished\n\n"

               "   -f, --format <format>    Firmware file format (autodetected by default)\n\n");

    fprintf(f, "Supported firmware formats: ");
    for (const tyb_firmware_format *format = tyb_firmware_formats; format->name; format++)
        fprintf(f, "%s%s", format != tyb_firmware_formats ? ", " : "", format->name);
    fprintf(f, "\n");
}

int upload(int argc, char *argv[])
{
    tyb_board *board = NULL;
    ty_task *task = NULL;
    int r;

    int c;
    while ((c = getopt_long(argc, argv, short_options, long_options, NULL)) != -1) {
        switch (c) {
        case UPLOAD_OPTION_NOCHECK:
            upload_flags |= TYB_UPLOAD_NOCHECK;
            break;
        case UPLOAD_OPTION_NORESET:
            upload_flags |= TYB_UPLOAD_NORESET;
            break;
        case 'w':
            upload_flags |= TYB_UPLOAD_WAIT;
            break;

        case 'f':
            firmware_format = optarg;
            break;

        default:
            r = parse_main_option(argc, argv, c);
            if (r <= 0)
                return r;
            break;
        }
    }

    if (optind >= argc) {
        ty_error(TY_ERROR_PARAM, "Missing firmware filename");
        goto usage;
    } else if (argc > optind + 1) {
        ty_error(TY_ERROR_PARAM, "Only one positional argument is allowed");
        goto usage;
    }
    firmware_filename = argv[optind++];

    r = get_board(&board);
    if (r < 0)
        goto cleanup;

    r = tyb_upload2(board, firmware_filename, firmware_format, upload_flags, &task);
    if (r < 0)
        goto cleanup;

    r = ty_task_join(task);

cleanup:
    ty_task_unref(task);
    tyb_board_unref(board);
    return r;

usage:
    print_upload_usage(stderr);
    return TY_ERROR_PARAM;
}
