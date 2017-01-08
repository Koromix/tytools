/*
 * ty, a collection of GUI and command-line tools to manage Teensy devices
 *
 * Distributed under the MIT license (see LICENSE.txt or http://opensource.org/licenses/MIT)
 * Copyright (c) 2015 Niels Martignène <niels.martignene@gmail.com>
 */

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
