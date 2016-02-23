/*
 * ty, a collection of GUI and command-line tools to manage Teensy devices
 *
 * Distributed under the MIT license (see LICENSE.txt or http://opensource.org/licenses/MIT)
 * Copyright (c) 2015 Niels Martignène <niels.martignene@gmail.com>
 */

#ifndef TY_COMPAT_H
#define TY_COMPAT_H

#include "util.h"
#include <stdarg.h>

TY_C_BEGIN

char *strrpbrk(const char *s, const char *accept);

#ifndef HAVE_ASPRINTF
int asprintf(char **strp, const char *fmt, ...) TY_PRINTF_FORMAT(2, 3);
int vasprintf(char **strp, const char *fmt, va_list ap);
#endif

TY_C_END

#endif
