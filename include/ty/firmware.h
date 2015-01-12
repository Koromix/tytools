/**
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef TY_FIRMWARE_H
#define TY_FIRMWARE_H

#include "common.h"

TY_C_BEGIN

typedef struct ty_firmware {
    size_t size;
    uint8_t image[];
} ty_firmware;

typedef struct ty_firmware_format {
    const char *name;
    const char *ext;

    int (*load)(const char *filename, ty_firmware **rfirmware);
} ty_firmware_format;

TY_PUBLIC extern const ty_firmware_format ty_firmware_formats[];

TY_PUBLIC extern const size_t ty_firmware_max_size;

TY_PUBLIC int ty_firmware_load(const char *filename, const char *format_name, ty_firmware **rfirmware);
TY_PUBLIC void ty_firmware_free(ty_firmware *f);

TY_C_END

#endif
