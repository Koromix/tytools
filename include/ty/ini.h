/**
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef TY_INI_H
#define TY_INI_H

#include "common.h"

TY_C_BEGIN

typedef struct ty_ini ty_ini;

typedef int ty_ini_callback_func(ty_ini *ini, const char *section, char *key, char *value, void *udata);

TY_PUBLIC int ty_ini_open(const char *path, ty_ini **rini);
TY_PUBLIC void ty_ini_free(ty_ini *ini);

TY_PUBLIC int ty_ini_next(ty_ini *ini, const char **rsection, char **rkey, char **rvalue);

TY_PUBLIC int ty_ini_walk(const char *path, ty_ini_callback_func *f, void *udata);

TY_C_END

#endif
