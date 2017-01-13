/* Teensy Tools - public domain
   Niels Martignène <niels.martignene@gmail.com>
   https://neodd.com/teensytools

   This software is in the public domain. Where that dedication is not
   recognized, you are granted a perpetual, irrevocable license to copy,
   distribute, and modify this file as you see fit.

   See the LICENSE file for more details. */

#ifndef TY_COMPAT_PRIV_H
#define TY_COMPAT_PRIV_H

#include "common_priv.h"
#include <stdarg.h>

TY_C_BEGIN

#ifndef HAVE_ASPRINTF
int _ty_asprintf(char **strp, const char *fmt, ...) TY_PRINTF_FORMAT(2, 3);
#define asprintf _ty_asprintf
int _ty_vasprintf(char **strp, const char *fmt, va_list ap);
#define vasprintf _ty_vasprintf
#endif

TY_C_END

#endif
