/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <getopt.h>
#include "ty.h"
#include "main.h"

enum {
    OPTION_HELP = 0x100,
    OPTION_NORESET
};

static const char *short_options = "wf:";
static const struct option long_options[] = {
    {"format",  required_argument, NULL, 'f'},
    {"help",    no_argument,       NULL, OPTION_HELP},
    {"noreset", no_argument,       NULL, OPTION_NORESET},
    {"wait",    no_argument,       NULL, 'w'},
    {0}
};

static const int manual_reboot_delay = 4000;

static bool reset_after = true;
static bool wait_device = false;
static const char *image_format = NULL;
static const char *image_filename = NULL;

void print_upload_usage(void)
{
    fprintf(stderr, "usage: tyc upload [options] <filename>\n\n"
                    "Options:\n"
                    "   -f, --format <format>    Firmware file format (autodetected by default)\n"
                    "       --noreset            Do not reset the device once the upload is finished\n"
                    "   -w, --wait               Wait for the bootloader instead of rebooting\n\n"
                    "Supported firmware formats: ");

    for (const ty_firmware_format *format = ty_firmware_formats; format->name; format++)
        fprintf(stderr, "%s%s", format != ty_firmware_formats ? ", " : "", format->name);
    fprintf(stderr, "\n");
}

static int reload_firmware(ty_firmware **rfirmware, const char *filename, uint64_t *rmtime)
{
    ty_file_info info;
    ty_firmware *firmware;
    int r;

    r = ty_stat(filename, &info, true);
    if (r < 0)
        return r;

    if (!*rfirmware || info.mtime != *rmtime) {
        r = ty_firmware_load(filename, image_format, &firmware);
        if (r < 0)
            return r;

        if (*rfirmware)
            ty_firmware_free(*rfirmware);
        *rfirmware = firmware;
        *rmtime = info.mtime;

        return 1;
    }

    return 0;
}

static int show_progress(const ty_board *board, const ty_firmware *f, size_t uploaded, void *udata)
{
    TY_UNUSED(board);
    TY_UNUSED(udata);

    printf("\rUploading firmware... %zu%%", uploaded * 100 / f->size);
    fflush(stdout);

    return 0;
}

int upload(int argc, char *argv[])
{
    ty_board *board = NULL;
    ty_firmware *firmware = NULL;
    const ty_board_model *model;
    uint64_t mtime = 0;
    int r;

    int c;
    while ((c = getopt_long(argc, argv, short_options, long_options, NULL)) != -1) {
        switch (c) {
        case OPTION_HELP:
            print_upload_usage();
            return 0;

        case OPTION_NORESET:
            reset_after = false;
            break;
        case 'w':
            wait_device = true;
            break;

        case 'f':
            image_format = optarg;
            break;

        default:
            goto usage;
        }
    }

    if (optind >= argc) {
        ty_error(TY_ERROR_PARAM, "Missing firmware filename");
        goto usage;
    }

    image_filename = argv[optind++];

    // Test the file before doing anything else
    r = reload_firmware(&firmware, image_filename, &mtime);
    if (r < 0)
        return r;

    r = get_board(&board);
    if (r < 0)
        goto cleanup;

    // Can't upload directly, should we try to reboot or wait?
    if (!ty_board_has_capability(board, TY_BOARD_CAPABILITY_UPLOAD)) {
        if (wait_device) {
            printf("Waiting for device...\n"
                   "  (hint: press button to reboot)\n");
        } else {
            printf("Triggering board reboot\n");
            r = ty_board_reboot(board);
            if (r < 0)
                goto cleanup;
        }
    }

wait:
    r = ty_board_wait_for(board, TY_BOARD_CAPABILITY_UPLOAD, false, wait_device ? -1 : manual_reboot_delay);
    if (r < 0)
        goto cleanup;
    if (!r) {
        printf("Reboot didn't work, press button manually\n");
        wait_device = true;

        goto wait;
    }

    // Maybe it changed?
    r = reload_firmware(&firmware, image_filename, &mtime);
    if (r < 0)
        goto cleanup;

    model = ty_board_get_model(board);
    if (!model) {
        r = ty_error(TY_ERROR_MODE, "Unknown board model");
        goto cleanup;
    }

    printf("Model: %s\n", ty_board_model_get_desc(model));
    printf("Firmware: %s\n", image_filename);

    printf("Usage: %.1f%% (%zu bytes)\n", (double)firmware->size / (double)ty_board_model_get_code_size(model) * 100.0,
           firmware->size);

    r = ty_board_upload(board, firmware, 0, show_progress, NULL);
    if (r < 0)
        goto cleanup;
    printf("\n");

    if (reset_after) {
        printf("Sending reset command\n");
        r = ty_board_reset(board);
        if (r < 0)
            goto cleanup;
    } else {
        printf("Firmware uploaded, reset the board to use it\n");
    }

    r = 0;
cleanup:
    ty_firmware_free(firmware);
    ty_board_unref(board);
    return r;

usage:
    print_upload_usage();
    return TY_ERROR_PARAM;
}
