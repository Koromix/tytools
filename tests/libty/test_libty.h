/* TyTools - public domain
   Niels Martignène <niels.martignene@protonmail.com>
   https://koromix.dev/tytools

   This software is in the public domain. Where that dedication is not
   recognized, you are granted a perpetual, irrevocable license to copy,
   distribute, and modify this file as you see fit.

   See the LICENSE file for more details. */

#ifndef TEST_LIBTY_H
#define TEST_LIBTY_H

#include "../../src/libty/common.h"

TY_C_BEGIN

#define ASSERT(pred) \
    report_test((pred), __FILE__, __LINE__, __func__, "'%s'", #pred)
#define ASSERT_STR_EQUAL(s1, s2) \
    do { \
        const char *a = (s1), *b = (s2); \
        if (!a) \
            a = "(none)"; \
        if (!b) \
            b = "(none)"; \
        report_test(strcmp(a, b) == 0, __FILE__, __LINE__, __func__, "'%s' == '%s'", a, b); \
    } while (0)

void report_test(bool pred, const char *file, unsigned int line, const char *fn,
                 const char *pred_fmt, ...) TY_PRINTF_FORMAT(5, 6);

TY_C_END

#endif
