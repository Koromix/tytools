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
#include <poll.h>
#include <sys/utsname.h>
#include <time.h>
#include "hs/platform.h"

uint64_t hs_millis(void)
{
    struct timespec ts;
    int r _HS_POSSIBLY_UNUSED;

#ifdef CLOCK_MONOTONIC_RAW
    r = clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
#else
    r = clock_gettime(CLOCK_MONOTONIC, &ts);
#endif
    assert(!r);

    return (uint64_t)ts.tv_sec * 1000 + (uint64_t)ts.tv_nsec / 10000000;
}

int hs_poll(hs_poll_source *sources, unsigned int count, int timeout)
{
    assert(sources);
    assert(count);
    assert(count <= HS_POLL_MAX_SOURCES);

    struct pollfd pfd[HS_POLL_MAX_SOURCES];
    uint64_t start;
    int r;

    for (unsigned int i = 0; i < count; i++) {
        pfd[i].fd = sources[i].desc;
        pfd[i].events = POLLIN;
        sources[i].ready = 0;
    }

    start = hs_millis();
restart:
    r = poll(pfd, (nfds_t)count, hs_adjust_timeout(timeout, start));
    if (r < 0) {
        if (errno == EINTR)
            goto restart;
        return hs_error(HS_ERROR_SYSTEM, "poll() failed: %s", strerror(errno));
    }
    if (!r)
        return 0;

    for (unsigned int i = 0; i < count; i++)
        sources[i].ready = !!pfd[i].revents;

    return r;
}

#ifdef __linux__
uint32_t hs_linux_version(void)
{
    static uint32_t version;

    if (version)
        return version;

    struct utsname name;
    uint32_t major = 0, minor = 0, release = 0, patch = 0;

    uname(&name);
    sscanf(name.release, "%u.%u.%u.%u", &major, &minor, &release, &patch);

    if (major >= 3) {
        patch = release;
        release = 0;
    }
    version = major * 10000000 + minor * 100000 + release * 1000 + patch;

    return version;
}
#endif
