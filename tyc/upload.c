/*
 * ty, a collection of GUI and command-line tools to manage Teensy devices
 *
 * Distributed under the MIT license (see LICENSE.txt or http://opensource.org/licenses/MIT)
 * Copyright (c) 2015 Niels Martignène <niels.martignene@gmail.com>
 */

#ifdef _WIN32
    #include <malloc.h>
#else
    #include <alloca.h>
#endif
#include <getopt.h>
#include "ty/firmware.h"
#include "ty/task.h"
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

void print_upload_usage(FILE *f)
{
    fprintf(f, "usage: tyc upload [options] <firmwares>\n\n");

    print_main_options(f);
    fprintf(f, "\n");

    fprintf(f, "Upload options:\n"
               "   -w, --wait               Wait for the bootloader instead of rebooting\n"
               "       --nocheck            Force upload even if the board is not compatible\n"
               "       --noreset            Do not reset the device once the upload is finished\n"
               "   -f, --format <format>    Firmware file format (autodetected by default)\n\n"
               "You can pass multiple firmwares, and tyc will upload the first compatible.\n");

    fprintf(f, "Supported firmware formats: ");
    for (const tyb_firmware_format *format = tyb_firmware_formats; format->name; format++)
        fprintf(f, "%s%s", format != tyb_firmware_formats ? ", " : "", format->name);
    fprintf(f, ".\n");
}

int upload(int argc, char *argv[])
{
    tyb_board *board = NULL;
    tyb_firmware **fws;
    unsigned int fws_count;
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
            if (r)
                return r < 0 ? EXIT_FAILURE : EXIT_SUCCESS;
            break;
        }
    }

    if (optind >= argc) {
        ty_log(TY_LOG_ERROR, "Missing firmware filename");
        goto usage;
    } else if (argc - optind > TYB_UPLOAD_MAX_FIRMWARES) {
        ty_log(TY_LOG_WARNING, "Too many firmwares, considering only %d files",
               TYB_UPLOAD_MAX_FIRMWARES);
        argc = optind + TYB_UPLOAD_MAX_FIRMWARES;
    }

    r = get_board(&board);
    if (r < 0)
        goto cleanup;

    fws = alloca((size_t)(argc - optind) * sizeof(*fws));
    fws_count = 0;
    for (unsigned int i = (unsigned int)optind; i < (unsigned int)argc; i++) {
        r = tyb_firmware_load(argv[i], firmware_format, &fws[fws_count]);
        if (r < 0)
            goto cleanup;
        fws_count++;
    }

    r = tyb_upload(board, fws, fws_count, upload_flags, &task);
    for (unsigned int i = 0; i < fws_count; i++)
        tyb_firmware_unref(fws[i]);
    if (r < 0)
        goto cleanup;

    r = ty_task_join(task);

cleanup:
    ty_task_unref(task);
    tyb_board_unref(board);
    return r < 0 ? EXIT_FAILURE : EXIT_SUCCESS;

usage:
    print_upload_usage(stderr);
    return EXIT_FAILURE;
}
