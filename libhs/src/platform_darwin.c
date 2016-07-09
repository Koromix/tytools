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
#include <mach/mach_time.h>
#include <sys/select.h>
#include <sys/utsname.h>
#include "hs/platform.h"

uint64_t hs_millis(void)
{
    static mach_timebase_info_data_t tb;
    if (!tb.numer)
        mach_timebase_info(&tb);

    return (uint64_t)mach_absolute_time() * tb.numer / tb.denom / 1000000;
}

int hs_poll(hs_poll_source *sources, unsigned int count, int timeout)
{
    assert(sources);
    assert(count);
    assert(count <= HS_POLL_MAX_SOURCES);

    fd_set fds;
    uint64_t start;
    int maxfd, r;

    FD_ZERO(&fds);
    maxfd = 0;
    for (unsigned int i = 0; i < count; i++) {
        if (sources[i].desc >= FD_SETSIZE) {
            for (unsigned int j = i; j < count; j++)
                sources[j].ready = 0;

            return hs_error(HS_ERROR_SYSTEM, "Cannot select() on descriptor %d (too big)",
                            sources[i].desc);
        }

        FD_SET(sources[i].desc, &fds);
        sources[i].ready = 0;

        if (sources[i].desc > maxfd)
            maxfd = sources[i].desc;
    }

    start = hs_millis();
restart:
    if (timeout >= 0) {
        int adjusted_timeout;
        struct timeval tv;

        adjusted_timeout = hs_adjust_timeout(timeout, start);
        tv.tv_sec = adjusted_timeout / 1000;
        tv.tv_usec = (adjusted_timeout % 1000) * 1000;

        r = select(maxfd + 1, &fds, NULL, NULL, &tv);
    } else {
        r = select(maxfd + 1, &fds, NULL, NULL, NULL);
    }
    if (r < 0) {
        if (errno == EINTR)
            goto restart;
        return hs_error(HS_ERROR_SYSTEM, "poll() failed: %s", strerror(errno));
    }
    if (!r)
        return 0;

    for (unsigned int i = 0; i < count; i++)
        sources[i].ready = !!FD_ISSET(sources[i].desc, &fds);

    return r;
}

uint32_t hs_darwin_version(void)
{
    static uint32_t version;

    if (!version) {
        struct utsname name;
        uint32_t major = 0, minor = 0, release = 0;

        uname(&name);
        sscanf(name.release, "%u.%u.%u", &major, &minor, &release);

        version = major * 10000 + minor * 100 + release;
    }

    return version;
}
