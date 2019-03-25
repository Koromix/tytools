/* TyTools - public domain
   Niels Martignène <niels.martignene@protonmail.com>
   https://koromix.dev/tytools

   This software is in the public domain. Where that dedication is not
   recognized, you are granted a perpetual, irrevocable license to copy,
   distribute, and modify this file as you see fit.

   See the LICENSE file for more details. */

#ifndef TY_INI_H
#define TY_INI_H

#include "common.h"

TY_C_BEGIN

typedef int ty_ini_callback_func(const char *section, char *key, char *value, void *udata);

int ty_ini_walk_fp(FILE *fp, const char *path, ty_ini_callback_func *f, void *udata);
int ty_ini_walk(const char *path, ty_ini_callback_func *f, void *udata);

TY_C_END

#endif
