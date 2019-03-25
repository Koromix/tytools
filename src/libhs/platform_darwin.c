/* libhs - public domain
   Niels Martignène <niels.martignene@protonmail.com>
   https://koromix.dev/libhs

   This software is in the public domain. Where that dedication is not
   recognized, you are granted a perpetual, irrevocable license to copy,
   distribute, and modify this file as you see fit.

   See the LICENSE file for more details. */

#include "common_priv.h"
#include <mach/mach_time.h>
#include <sys/select.h>
#include <sys/utsname.h>
#include "platform.h"

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
