/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
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
