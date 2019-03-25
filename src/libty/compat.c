/* TyTools - public domain
   Niels Martignène <niels.martignene@protonmail.com>
   https://koromix.dev/tytools

   This software is in the public domain. Where that dedication is not
   recognized, you are granted a perpetual, irrevocable license to copy,
   distribute, and modify this file as you see fit.

   See the LICENSE file for more details. */

#include "common_priv.h"
#include <stdarg.h>

#ifndef _TY_HAVE_ASPRINTF

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
    va_list ap_copy;
    char *s;
    int r;

    va_copy(ap_copy, ap);
    r = vsnprintf(NULL, 0, fmt, ap_copy);
    if (r < 0)
        return -1;
    va_end(ap_copy);

    s = malloc((size_t)r + 1);
    if (!s)
        return -1;

    va_copy(ap_copy, ap);
    r = vsnprintf(s, (size_t)r + 1, fmt, ap_copy);
    if (r < 0)
        return -1;
    va_end(ap_copy);

    *strp = s;
    return r;
}

#endif
