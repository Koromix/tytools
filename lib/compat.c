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

#include "common.h"
#include <stdarg.h>

char *strrpbrk(const char *s, const char *accept)
{
    const char *start = s;

    s += strlen(s);
    while (--s >= start)
    {
        const char *a = accept;
        while (*a != '\0') {
            if (*a++ == *s)
                return (char *)s;
        }
    }

    return NULL;
}

#ifndef HAVE_STPCPY
char *stpcpy(char *restrict dest, const char *restrict src)
{
    while ((*dest++ = *src++))
        continue;
    return dest - 1;
}
#endif

#ifndef HAVE_ASPRINTF
int asprintf(char **strp, const char *fmt, ...)
{
    va_list ap;
    int r;

    va_start(ap, fmt);
    r = vasprintf(strp, fmt, ap);
    va_end(ap);

    return r;
}

int vasprintf(char **strp, const char *fmt, va_list ap)
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
