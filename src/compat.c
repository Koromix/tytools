/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "ty/common.h"
#include "compat.h"
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

char *stpcpy(char *dest, const char *src)
{
    while ((*dest++ = *src++))
        continue;
    return dest - 1;
}

#endif

#ifndef HAVE_STPNCPY

char *stpncpy(char *dest, const char *src, size_t n)
{
    while ((*dest++ = *src++) && n--)
        continue;
    if (n)
        memset(dest, 0, n);
    return dest - 1;
}

#endif

#ifndef HAVE_STRNDUP

char *strndup(const char *s, size_t n)
{
    char *d;
    size_t len;

    len = strlen(s);
    if (len > n)
        len = n;

    d = malloc(len + 1);
    if (!d)
        return NULL;

    memcpy(d, s, len);
    d[len] = 0;

    return d;
}

#endif

#ifndef HAVE_MEMRCHR

void *memrchr(const void *s, int c, size_t n)
{
    if (!n)
        return NULL;

    unsigned char *p = (unsigned char *)s + n;

    while (--p >= (unsigned char *)s) {
        if (*p == c)
            return p;
    }

    return NULL;
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

#ifndef HAVE_GETDELIM

ssize_t getdelim(char **rbuf, size_t *rsize, int delim, FILE *fp)
{
    char *buf;
    size_t size, len;
    int c;

    if (!rbuf || !rsize || !fp) {
        errno = EINVAL;
        return -1;
    }

    if (!*rbuf)
        *rsize = 0;

    buf = *rbuf;
    size = *rsize;

    len = 0;
    do {
        c = fgetc(fp);
        if (c == EOF && ferror(fp))
            return -1;

        if (size <= len + 1) {
            if (size) {
                if (size == SSIZE_MAX) {
                    errno = EOVERFLOW;
                    return -1;
                }
                size *= 2;
                if (size > SSIZE_MAX)
                    size = SSIZE_MAX;
            } else {
                size = 120;
            }

            buf = realloc(buf, size);
            if (!buf)
                return -1;

            *rbuf = buf;
            *rsize = size;
        }

        if (c != EOF) {
            buf[len++] = (char)c;
        } else {
            buf[len] = 0;
        }

        if (c == delim)
            buf[len] = 0;
    } while (c != delim && c != EOF);

    return (ssize_t)len;
}

#endif

#ifndef HAVE_GETLINE

ssize_t getline(char **rbuf, size_t *rsize, FILE *fp)
{
    return getdelim(rbuf, rsize, '\n', fp);
}

#endif
