/**
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
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

TY_PUBLIC int ty_process_spawn(const char *name, const char *dir, const char * const args[], const ty_descriptor desc[3], uint32_t flags, ty_descriptor *rdesc);
TY_PUBLIC int ty_process_wait(ty_descriptor desc, int timeout);

#if defined(__unix__) || defined(__APPLE__)
TY_PUBLIC void ty_process_handle_sigchld(int signum);
#endif

TY_C_END

#endif
