/*
 * ty, a collection of GUI and command-line tools to manage Teensy devices
 *
 * Distributed under the MIT license (see LICENSE.txt or http://opensource.org/licenses/MIT)
 * Copyright (c) 2015 Niels Martignène <niels.martignene@gmail.com>
 */

#include "util.h"
#include <stdarg.h>

#ifndef HAVE_ASPRINTF

int _ty_asprintf(char **strp, const char *fmt, ...)
{
    va_list ap;
    int r;

    va_start(ap, fmt);
    r = vasprintf(strp, fmt, ap);
    va_end(ap);

    return r;
}

int _ty_vasprintf(char **strp, const char *fmt, va_list ap)
{
    char *s;
    int r;

    r = vsnprintf(NULL, 0, fmt, ap);
    if (r < 0)
        return -1;

    s = malloc((size_t)r + 1);
    if (!s)
        return -1;

    r = vsnprintf(s, (size_t)r + 1, fmt, ap);
    if (r < 0)
        return -1;

    *strp = s;
    return r;
}

#endif
