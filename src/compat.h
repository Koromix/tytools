/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef TY_COMPAT_H
#define TY_COMPAT_H

#include "ty/common.h"
#include "config.h"
#include <stdarg.h>

char *strrpbrk(const char *s, const char *accept);

#ifndef HAVE_STPCPY
char *stpcpy(char *dest, const char *src);
#endif

#ifndef HAVE_STRNDUP
char *strndup(const char *s, size_t n);
#endif

#ifndef HAVE_MEMRCHR
void *memrchr(const void *s, int c, size_t n);
#endif

#ifndef HAVE_ASPRINTF
int asprintf(char **strp, const char *fmt, ...) TY_PRINTF_FORMAT(2, 3);
int vasprintf(char **strp, const char *fmt, va_list ap);
#endif

#ifndef HAVE_GETDELIM
ssize_t getdelim(char **rbuf, size_t *rsize, int delim, FILE *fp);
#endif

#ifndef HAVE_GETLINE
ssize_t getline(char **rbuf, size_t *rsize, FILE *fp);
#endif

#endif
