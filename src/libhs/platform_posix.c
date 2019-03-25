/* libhs - public domain
   Niels Martignène <niels.martignene@protonmail.com>
   https://koromix.dev/libhs

   This software is in the public domain. Where that dedication is not
   recognized, you are granted a perpetual, irrevocable license to copy,
   distribute, and modify this file as you see fit.

   See the LICENSE file for more details. */

#include "common_priv.h"
#include <poll.h>
#include <sys/utsname.h>
#include <time.h>
#include "platform.h"

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

    if (!version) {
        struct utsname name;
        uint32_t major = 0, minor = 0, release = 0, patch = 0;

        uname(&name);
        sscanf(name.release, "%u.%u.%u.%u", &major, &minor, &release, &patch);
        if (major >= 3) {
            patch = release;
            release = 0;
        }

        version = major * 10000000 + minor * 100000 + release * 1000 + patch;
    }

    return version;
}
#endif
