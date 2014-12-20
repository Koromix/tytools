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

#ifndef TY_PROCESS_H
#define TY_PROCESS_H

#include "common.h"
#include "system.h"

TY_C_BEGIN

enum {
    TY_SPAWN_PATH = 1
};

enum {
    TY_PROCESS_SUCCESS = 1,
    TY_PROCESS_INTERRUPTED,
    TY_PROCESS_FAILURE
};

int ty_process_spawn(const char *name, const char *dir, const char * const args[], const ty_descriptor desc[3], uint32_t flags, ty_descriptor *rdesc);
int ty_process_wait(ty_descriptor desc, int timeout);

#ifdef __unix__
void ty_process_handle_sigchld(int signum);
#endif

TY_C_END

#endif
