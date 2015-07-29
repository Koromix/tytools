/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "ty/common.h"
#include "compat.h"
#include "ty/firmware.h"
#include "ty/system.h"

int _tyb_firmware_load_elf(const char *filename, tyb_firmware **rfirmware);
int _tyb_firmware_load_ihex(const char *filename, tyb_firmware **rfirmware);

const tyb_firmware_format tyb_firmware_formats[] = {
    {"elf",  ".elf", _tyb_firmware_load_elf},
    {"ihex", ".hex", _tyb_firmware_load_ihex},
    {0}
};

const size_t tyb_firmware_max_size = 1024 * 1024;

int tyb_firmware_load(const char *filename, const char *format_name, tyb_firmware **rfirmware)
{
    assert(filename);
    assert(rfirmware);

    const tyb_firmware_format *format = tyb_firmware_formats;

    if (format_name) {
        for (; format->name; format++) {
            if (strcasecmp(format->name, format_name) == 0)
                break;
        }
        if (!format->name)
            return ty_error(TY_ERROR_UNSUPPORTED, "Firmware file format '%s' unknown", format_name);
    } else {
        const char *ext = strrchr(filename, '.');

        for (; format->name; format++) {
            if (strcmp(format->ext, ext) == 0)
                break;
        }
        if (!format->name)
            return ty_error(TY_ERROR_UNSUPPORTED, "Firmware '%s' uses unrecognized file format", filename);
    }

    return (*format->load)(filename, rfirmware);
}

void tyb_firmware_free(tyb_firmware *f)
{
    free(f);
}
