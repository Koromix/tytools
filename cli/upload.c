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
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include "board.h"
#include "firmware.h"
#include "main.h"
#include "system.h"

enum {
    OPTION_HELP = 0x100,
    OPTION_NORESET
};

static const char *short_options = "w";
static const struct option long_options[] = {
    {"help",    no_argument, NULL, OPTION_HELP},
    {"noreset", no_argument, NULL, OPTION_NORESET},
    {"wait",    no_argument, NULL, 'w'},
    {0}
};

static const uint64_t manual_reboot_delay = 4000;

static bool reset_after = true;
static bool wait_device = false;
static const char *image_filename = NULL;

void print_upload_usage(void)
{
    fprintf(stderr, "usage: ty upload [options] <filename>\n\n"
                    "Options:\n"
                    "       --noreset            Do not reset the device once the upload is finished\n"
                    "   -w, --wait               Wait for the bootloader instead of rebooting\n\n"
                    "Only Intel HEX files are suppported as of now.\n");
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
        r = ty_firmware_load_ihex(filename, &firmware);
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

static int wait_board(ty_board *board, bool warn)
{
    uint64_t warn_at = 0;
    int r;

    if (warn)
        warn_at = ty_millis() + manual_reboot_delay;

    r = 0;
    while (!r || !ty_board_has_capability(board, TY_BOARD_CAPABILITY_UPLOAD)) {
        if (r && warn_at && ty_millis() >= warn_at) {
            printf("Reboot didn't work, press button manually\n");
            warn_at = 0;
        }

        r = ty_board_probe(board, 500);
        if (r < 0)
            return r;
    }

    return 0;
}

int upload(int argc, char *argv[])
{
    ty_board *board;
    ty_firmware *firmware = NULL;
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
                   "     (hint: press button to reboot)\n");
        } else {
            if (ty_board_has_capability(board, TY_BOARD_CAPABILITY_REBOOT)) {
                printf("Triggering board reboot\n");
                r = ty_board_reboot(board);
                if (r < 0)
                    goto cleanup;
            } else {
                // We can't reboot either :/
                r = ty_error(TY_ERROR_MODE, "Cannot reboot the board");
                goto cleanup;
            }
        }

        r = wait_board(board, !wait_device);
        if (r < 0)
            goto cleanup;
    }

    r = reload_firmware(&firmware, image_filename, &mtime);
    if (r < 0)
        goto cleanup;

    if (!board->model) {
        r = ty_error(TY_ERROR_MODE, "Unknown board model");
        goto cleanup;
    }

    printf("Model: %s\n", board->model->desc);
    printf("Firmware: %s\n", image_filename);

    printf("Usage: %.1f%% (%zu bytes)\n",
           (double)firmware->size / (double)board->model->code_size * 100.0, firmware->size);

    printf("Uploading firmware...\n");
    r = ty_board_upload(board, firmware, 0);
    if (r < 0)
        goto cleanup;

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
    return r;

usage:
    print_upload_usage();
    return TY_ERROR_PARAM;
}
