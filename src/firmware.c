/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "ty/common.h"
#include "compat.h"
#include "ty/firmware.h"
#include "ty/system.h"

int _ty_firmware_load_elf(const char *filename, ty_firmware **rfirmware);
int _ty_firmware_load_ihex(const char *filename, ty_firmware **rfirmware);

const ty_firmware_format ty_firmware_formats[] = {
    {"elf",  ".elf", _ty_firmware_load_elf},
    {"ihex", ".hex", _ty_firmware_load_ihex},
    {0}
};

const size_t ty_firmware_max_size = 1024 * 1024;

int ty_firmware_load(const char *filename, const char *format_name, ty_firmware **rfirmware)
{
    assert(filename);
    assert(rfirmware);

    const ty_firmware_format *format = ty_firmware_formats;

    if (format_name) {
        for (; format->name; format++) {
            if (strcasecmp(format->name, format_name) == 0)
                break;
        }
        if (!format->name)
            return ty_error(TY_ERROR_UNSUPPORTED, "Firmware file format '%s' unknown", format_name);
    } else {
        const char *ext = ty_path_ext(filename);

        for (; format->name; format++) {
            if (strcmp(format->ext, ext) == 0)
                break;
        }
        if (!format->name)
            return ty_error(TY_ERROR_UNSUPPORTED, "Firmware '%s' uses unrecognized file format", filename);
    }

    return (*format->load)(filename, rfirmware);
}

void ty_firmware_free(ty_firmware *f)
{
    free(f);
}
