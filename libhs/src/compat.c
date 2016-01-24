/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2015 Niels Martignène <niels.martignene@gmail.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "util.h"

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
char *stpcpy(char *dest, const char *src)
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
