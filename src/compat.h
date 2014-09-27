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

#ifndef TY_COMPAT_H
#define TY_COMPAT_H

#include "ty/common.h"
#include "config.h"

char *strrpbrk(const char *s, const char *accept);

#ifndef HAVE_STPCPY
char *stpcpy(char *dest, const char *src);
#endif

#ifndef HAVE_STRNDUP
char *strndup(const char *s, size_t n);
#endif

#ifndef HAVE_ASPRINTF
int asprintf(char **strp, const char *fmt, ...);
int vasprintf(char **strp, const char *fmt, va_list ap);
#endif

#ifndef HAVE_GETDELIM
ssize_t getdelim(char **rbuf, size_t *rsize, int delim, FILE *fp);
#endif

#ifndef HAVE_GETLINE
ssize_t getline(char **rbuf, size_t *rsize, FILE *fp);
#endif

#endif
