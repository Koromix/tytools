/* TyTools - public domain
   Niels Martignène <niels.martignene@protonmail.com>
   https://koromix.dev/tytools

   This software is in the public domain. Where that dedication is not
   recognized, you are granted a perpetual, irrevocable license to copy,
   distribute, and modify this file as you see fit.

   See the LICENSE file for more details. */

#ifndef TY_COMMON_PRIV_H
#define TY_COMMON_PRIV_H

#include "common.h"
#include "compat_priv.h"

void _ty_refcount_increase(unsigned int *rrefcount);
unsigned int _ty_refcount_decrease(unsigned int *rrefcount);

#endif
