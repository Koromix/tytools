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

extern const size_t ty_firmware_max_size;

int ty_firmware_load_ihex(const char *filename, ty_firmware **rfirmware);
void ty_firmware_free(ty_firmware *f);

TY_C_END

#endif
