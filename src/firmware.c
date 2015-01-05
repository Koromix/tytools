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
