/* libhs - public domain
   Niels Martignène <niels.martignene@protonmail.com>
   https://koromix.dev/libhs

   This software is in the public domain. Where that dedication is not
   recognized, you are granted a perpetual, irrevocable license to copy,
   distribute, and modify this file as you see fit.

   See the LICENSE file for more details. */

#include "common_priv.h"

#ifndef _HS_HAVE_STPCPY
char *_hs_stpcpy(char *dest, const char *src)
{
    while ((*dest++ = *src++))
        continue;
    return dest - 1;
}
#endif

#ifndef _HS_HAVE_ASPRINTF
int _hs_asprintf(char **strp, const char *fmt, ...)
{
    va_list ap;
    int r;

    va_start(ap, fmt);
    r = _hs_vasprintf(strp, fmt, ap);
    va_end(ap);

    return r;
}

int _hs_vasprintf(char **strp, const char *fmt, va_list ap)
{
    va_list ap_copy;
    char *s;
    int r;

    va_copy(ap_copy, ap);
    r = vsnprintf(NULL, 0, fmt, ap_copy);
    if (r < 0)
        return -1;
    va_end(ap_copy);

    s = (char *)malloc((size_t)r + 1);
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
