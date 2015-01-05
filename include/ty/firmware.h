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
