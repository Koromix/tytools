/*
 * ty, a collection of GUI and command-line tools to manage Teensy devices
 *
 * Distributed under the MIT license (see LICENSE.txt or http://opensource.org/licenses/MIT)
 * Copyright (c) 2015 Niels Martignène <niels.martignene@gmail.com>
 */

#include <getopt.h>
#include "ty/firmware.h"
#include "ty/task.h"
#include "main.h"

enum {
    UPLOAD_OPTION_NOCHECK = 0x200,
    UPLOAD_OPTION_NORESET
};

static const char *short_options = COMMON_SHORT_OPTIONS"f:w";
static const struct option long_options[] = {
    COMMON_LONG_OPTIONS
    {"format",     required_argument, NULL, 'f'},
    {"nocheck",    no_argument,       NULL, UPLOAD_OPTION_NOCHECK},
    {"noreset",    no_argument,       NULL, UPLOAD_OPTION_NORESET},
    {"wait",       no_argument,       NULL, 'w'},
    {0}
};

static int upload_flags = 0;
static const char *firmware_format = NULL;

static void print_upload_usage(FILE *f)
{
    fprintf(f, "usage: %s upload [options] <firmwares>\n\n", executable_name);

    print_common_options(f);
    fprintf(f, "\n");

    fprintf(f, "Upload options:\n"
               "   -w, --wait               Wait for the bootloader instead of rebooting\n"
               "       --nocheck            Force upload even if the board is not compatible\n"
               "       --noreset            Do not reset the device once the upload is finished\n"
               "   -f, --format <format>    Firmware file format (autodetected by default)\n\n"
               "You can pass multiple firmwares, and the first compatible one will be used.\n");

    fprintf(f, "Supported firmware formats: ");
    for (const ty_firmware_format *format = ty_firmware_formats; format->name; format++)
        fprintf(f, "%s%s", format != ty_firmware_formats ? ", " : "", format->name);
    fprintf(f, ".\n");
}

int upload(int argc, char *argv[])
{
    ty_board *board = NULL;
    ty_firmware **fws;
    unsigned int fws_count;
    ty_task *task = NULL;
    int r;

    int c;
    while ((c = getopt_long(argc, argv, short_options, long_options, NULL)) != -1) {
        switch (c) {
        HANDLE_COMMON_OPTIONS(c, print_upload_usage);

        case UPLOAD_OPTION_NOCHECK:
            upload_flags |= TY_UPLOAD_NOCHECK;
            break;
        case UPLOAD_OPTION_NORESET:
            upload_flags |= TY_UPLOAD_NORESET;
            break;
        case 'w':
            upload_flags |= TY_UPLOAD_WAIT;
            break;

        case 'f':
            firmware_format = optarg;
            break;
        }
    }

    if (optind >= argc) {
        ty_log(TY_LOG_ERROR, "Missing firmware filename");
        print_upload_usage(stderr);
        return EXIT_FAILURE;
    } else if (argc - optind > TY_UPLOAD_MAX_FIRMWARES) {
        ty_log(TY_LOG_WARNING, "Too many firmwares, considering only %d files",
               TY_UPLOAD_MAX_FIRMWARES);
        argc = optind + TY_UPLOAD_MAX_FIRMWARES;
    }

    r = get_board(&board);
    if (r < 0)
        goto cleanup;

    fws = alloca((size_t)(argc - optind) * sizeof(*fws));
    fws_count = 0;
    for (unsigned int i = (unsigned int)optind; i < (unsigned int)argc; i++) {
        r = ty_firmware_load(argv[i], firmware_format, &fws[fws_count]);
        if (r < 0)
            goto cleanup;
        fws_count++;
    }

    r = ty_upload(board, fws, fws_count, upload_flags, &task);
    for (unsigned int i = 0; i < fws_count; i++)
        ty_firmware_unref(fws[i]);
    if (r < 0)
        goto cleanup;

    r = ty_task_join(task);

cleanup:
    ty_task_unref(task);
    ty_board_unref(board);
    return r < 0 ? EXIT_FAILURE : EXIT_SUCCESS;
}
