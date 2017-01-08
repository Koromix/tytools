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

#include "common_priv.h"
#ifndef _WIN32
    #include <sys/stat.h>
#endif
#include "filter_priv.h"

int _hs_filter_init(_hs_filter *filter, const hs_match *matches, unsigned int count)
{
    if (!matches) {
        filter->matches = NULL;
        filter->count = 0;
        filter->types = UINT32_MAX;

        return 0;
    }

    // I promise to be careful
    filter->matches = count ? (hs_match *)matches : NULL;
    filter->count = count;

    filter->types = 0;
    for (unsigned int i = 0; i < count; i++) {
        if (!matches[i].type) {
            filter->types = UINT32_MAX;
            break;
        }

        filter->types |= (uint32_t)(1 << matches[i].type);
    }

    return 0;
}

void _hs_filter_release(_hs_filter *filter)
{
    _HS_UNUSED(filter);
}

static bool match_paths(const char *path1, const char *path2)
{
#ifdef _WIN32
    // This is mainly for COM ports, which exist as COMx files (with x < 10) and \\.\COMx files
    if (strncmp(path1, "\\\\.\\", 4) == 0 || strncmp(path1, "\\\\?\\", 4) == 0)
        path1 += 4;
    if (strncmp(path2, "\\\\.\\", 4) == 0 || strncmp(path2, "\\\\?\\", 4) == 0)
        path2 += 4;

    // Device nodes are not valid Win32 filesystem paths so a simple comparison is enough
    return strcasecmp(path1, path2) == 0;
#else
    struct stat sb1, sb2;
    int r;

    if (strcmp(path1, path2) == 0)
        return true;

    r = stat(path1, &sb1);
    if (r < 0)
        return false;
    r = stat(path2, &sb2);
    if (r < 0)
        return false;

    return sb1.st_dev == sb2.st_dev && sb1.st_ino == sb2.st_ino;
#endif
}

static bool test_match(const hs_match *match, const hs_device *dev)
{
    if (match->type && dev->type != (hs_device_type)match->type)
        return false;
    if (match->vid && dev->vid != match->vid)
        return false;
    if (match->pid && dev->pid != match->pid)
        return false;
    if (match->path && !match_paths(dev->path, match->path))
        return false;

    return true;
}

bool _hs_filter_match_device(const _hs_filter *filter, const hs_device *dev)
{
    // Do the fast checks first
    if (!_hs_filter_has_type(filter, dev->type))
        return false;
    if (!filter->count)
        return true;

    for (unsigned int i = 0; i < filter->count; i++) {
        if (test_match(&filter->matches[i], dev))
            return true;
    }

    return false;
}

bool _hs_filter_has_type(const _hs_filter *filter, hs_device_type type)
{
    return filter->types & (uint32_t)(1 << type);
}
