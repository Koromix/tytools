/*
 * ty, a collection of GUI and command-line tools to manage Teensy devices
 *
 * Distributed under the MIT license (see LICENSE.txt or http://opensource.org/licenses/MIT)
 * Copyright (c) 2015 Niels Martignène <niels.martignene@gmail.com>
 */

#ifndef TY_FIRMWARE_H
#define TY_FIRMWARE_H

#include "common.h"
#include "model.h"

TY_C_BEGIN

typedef struct ty_firmware ty_firmware;

typedef struct ty_firmware_format {
    const char *name;
    const char *ext;

    int (*load)(ty_firmware *fw);
} ty_firmware_format;

TY_PUBLIC extern const ty_firmware_format ty_firmware_formats[];
TY_PUBLIC extern const unsigned int ty_firmware_formats_count;

#define TY_FIRMWARE_MAX_SIZE (1024 * 1024)

TY_PUBLIC int ty_firmware_load(const char *filename, const char *format_name, ty_firmware **rfw);

TY_PUBLIC ty_firmware *ty_firmware_ref(ty_firmware *fw);
TY_PUBLIC void ty_firmware_unref(ty_firmware *fw);

TY_PUBLIC const char *ty_firmware_get_filename(const ty_firmware *fw);
TY_PUBLIC const char *ty_firmware_get_name(const ty_firmware *fw);

TY_PUBLIC size_t ty_firmware_get_size(const ty_firmware *fw);
TY_PUBLIC const uint8_t *ty_firmware_get_image(const ty_firmware *fw);

TY_PUBLIC unsigned int ty_firmware_identify(const ty_firmware *fw, ty_model *rmodels,
                                            unsigned int max_models);

TY_C_END

#endif
