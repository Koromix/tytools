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

#ifndef TY_INI_H
#define TY_INI_H

#include "common.h"

TY_C_BEGIN

typedef struct ty_ini ty_ini;

typedef int ty_ini_callback_func(ty_ini *ini, const char *section, char *key, char *value, void *udata);

int ty_ini_open(const char *path, ty_ini **rini);
void ty_ini_free(ty_ini *ini);

int ty_ini_next(ty_ini *ini, const char **rsection, char **rkey, char **rvalue);

int ty_ini_walk(const char *path, ty_ini_callback_func *f, void *udata);

TY_C_END

#endif
