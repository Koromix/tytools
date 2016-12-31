/*
 * ty, a collection of GUI and command-line tools to manage Teensy devices
 *
 * Distributed under the MIT license (see LICENSE.txt or http://opensource.org/licenses/MIT)
 * Copyright (c) 2015 Niels Martignène <niels.martignene@gmail.com>
 */

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

bool safe_strcmp(const char *s1, const char *s2);
void report_test(bool pred, const char *file, unsigned int line, const char *fn,
                 const char *pred_fmt, ...) TY_PRINTF_FORMAT(5, 6);

TY_C_END

#endif
