/*
 * ty, a collection of GUI and command-line tools to manage Teensy devices
 *
 * Distributed under the MIT license (see LICENSE.txt or http://opensource.org/licenses/MIT)
 * Copyright (c) 2015 Niels Martignène <niels.martignene@gmail.com>
 */

#ifndef TY_FIRMWARE_H
#define TY_FIRMWARE_H

#include "common.h"

TY_C_BEGIN

typedef struct tyb_firmware tyb_firmware;

typedef struct tyb_firmware_format {
    const char *name;
    const char *ext;

    int (*load)(tyb_firmware *firmware, const char *filename);
} tyb_firmware_format;

TY_PUBLIC extern const tyb_firmware_format tyb_firmware_formats[];

#define TYB_FIRMWARE_MAX_SIZE (1024 * 1024)

TY_PUBLIC int tyb_firmware_load(const char *filename, const char *format_name, tyb_firmware **rfirmware);

TY_PUBLIC tyb_firmware *tyb_firmware_ref(tyb_firmware *firmware);
TY_PUBLIC void tyb_firmware_unref(tyb_firmware *firmware);

TY_PUBLIC const char *tyb_firmware_get_name(const tyb_firmware *firmware);

TY_PUBLIC size_t tyb_firmware_get_size(const tyb_firmware *firmware);
TY_PUBLIC const uint8_t *tyb_firmware_get_image(const tyb_firmware *firmware);

TY_C_END

#endif
