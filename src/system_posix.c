/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "ty/common.h"
#include "compat.h"
#include <fcntl.h>
#include <poll.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#define TTYDEFCHARS
#include <termios.h>
#ifdef __APPLE__
#include <mach/mach_time.h>
#endif
#include "ty/system.h"

struct child_report {
    ty_err err;
    char msg[512];
};

static struct termios orig_termios;
static bool saved_termios;

#ifdef __APPLE__

uint64_t ty_millis(void)
{
    static mach_timebase_info_data_t tb;
    if (!tb.numer)
        mach_timebase_info(&tb);

    return (uint64_t)mach_absolute_time() * tb.numer / tb.denom / 1000000;
}

#else

uint64_t ty_millis(void)
{
    struct timespec spec;
    uint64_t millis;
    int r;

#ifdef CLOCK_MONOTONIC_RAW
    r = clock_gettime(CLOCK_MONOTONIC_RAW, &spec);
#else
    r = clock_gettime(CLOCK_MONOTONIC, &spec);
#endif
    assert(!r);

    millis = (uint64_t)spec.tv_sec * 1000 + (uint64_t)spec.tv_nsec / 10000000;

    return millis;
}

#endif

void ty_delay(unsigned int ms)
{
    struct timespec t, rem;
    int r;

    t.tv_sec = (int)(ms / 1000);
    t.tv_nsec = (int)((ms % 1000) * 1000000);

    do {
        r = nanosleep(&t, &rem);
        if (r < 0) {
            if (errno != EINTR) {
                ty_error(TY_ERROR_SYSTEM, "nanosleep() failed: %s", strerror(errno));
                return;
            }

            t = rem;
        }
    } while (r);
}

/* Unlike ty_path_split, trailing slashes are ignored, so "a/b/" returns "b/". This is unusual
   but this way we don't have to allocate a new string or alter path itself. */
static const char *get_basename(const char *path)
{
    assert(path);

    size_t len;
    const char *name;

    len = strlen(path);
    while (len && path[len - 1] == '/')
        len--;
    name = memrchr(path, '/', len);
    if (!name++)
        name = path;

    return name;
}

static int do_stat(int fd, const char *path, ty_file_info *info, bool follow)
{
    struct stat sb;
    int r;

#ifdef HAVE_FSTATAT
    r = fstatat(fd, path, &sb, !follow ? AT_SYMLINK_NOFOLLOW : 0);
#else
    if (follow) {
        r = stat(path, &sb);
    } else {
        r = lstat(path, &sb);
    }
#endif
    if (r < 0) {
        switch (errno) {
        case EACCES:
            return ty_error(TY_ERROR_ACCESS, "Permission denied for '%s'", path);
        case EIO:
            return ty_error(TY_ERROR_IO, "I/O error while stating '%s'", path);
        case ENOENT:
            return ty_error(TY_ERROR_NOT_FOUND, "Path '%s' does not exist", path);
        case ENOTDIR:
            return ty_error(TY_ERROR_NOT_FOUND, "Part of '%s' is not a directory", path);
        }
        return ty_error(TY_ERROR_SYSTEM, "Failed to stat '%s': %s", path, strerror(errno));
    }

    if (S_ISDIR(sb.st_mode)) {
        info->type = TY_FILE_DIRECTORY;
    } else if (S_ISREG(sb.st_mode)) {
        info->type = TY_FILE_REGULAR;
#ifdef S_ISLNK
    } else if (S_ISLNK(sb.st_mode)) {
        info->type = TY_FILE_LINK;
#endif
    } else {
        info->type = TY_FILE_SPECIAL;
    }

    info->size = (uint64_t)sb.st_size;
#if defined(HAVE_STAT_MTIM)
    info->mtime = (uint64_t)sb.st_mtim.tv_sec * 1000 + (uint64_t)sb.st_mtim.tv_nsec / 1000000;
#elif defined(HAVE_STAT_MTIMESPEC)
    info->mtime = (uint64_t)sb.st_mtimespec.tv_sec * 1000 + (uint64_t)sb.st_mtimespec.tv_nsec / 1000000;
#else
    info->mtime = (uint64_t)sb.st_mtime * 1000;
#endif

    info->dev = sb.st_dev;
    info->ino = sb.st_ino;

    info->flags = 0;
    if (*get_basename(path) == '.')
        info->flags |= TY_FILE_HIDDEN;

    return 0;
}

#ifdef HAVE_FSTATAT

int _ty_statat(int fd, const char *path, ty_file_info *info, bool follow)
{
    assert(path && path[0]);
    assert(info);

    if (fd < 0)
        fd = AT_FDCWD;

    return do_stat(fd, path, info, follow);
}

#endif

int ty_stat(const char *path, ty_file_info *info, bool follow)
{
    assert(path && path[0]);
    assert(info);

    return do_stat(AT_FDCWD, path, info, follow);
}

bool ty_file_unique(const ty_file_info *info1, const ty_file_info *info2)
{
    assert(info1);
    assert(info2);

    return info1->dev == info2->dev && info1->ino == info2->ino;
}

int ty_realpath(const char *path, const char *base, char **rpath)
{
    assert(path && path[0]);

    char *tmp = NULL, *real = NULL;
    int r;

    if (base && !ty_path_is_absolute(path)) {
        r = asprintf(&tmp, "%s/%s", base, path);
        if (r < 0)
            goto cleanup;

        path = tmp;
    }

    real = realpath(path, NULL);
    if (!real) {
        switch (errno) {
        case ENOMEM:
            r = ty_error(TY_ERROR_MEMORY, NULL);
            break;
        case EACCES:
            r = ty_error(TY_ERROR_ACCESS, "Permission denied for '%s'", path);
            break;
        case EIO:
            r = ty_error(TY_ERROR_IO, "I/O error while resolving path '%s'", path);
            break;
        case ENOENT:
            r = ty_error(TY_ERROR_NOT_FOUND, "Path '%s' does not exist", path);
            break;
        case ENOTDIR:
            r = ty_error(TY_ERROR_NOT_FOUND, "Part of '%s' is not a directory", path);
            break;

        default:
            r = ty_error(TY_ERROR_SYSTEM, "realpath('%s') failed: %s", path, strerror(errno));
            break;
        }
        goto cleanup;
    }

    if (rpath) {
        *rpath = real;
        real = NULL;
    }

    r = 0;
cleanup:
    free(real);
    free(tmp);
    return r;
}

int ty_delete(const char *path, bool tolerant)
{
    assert(path && path[0]);

    int r;

    r = remove(path);
    if (r < 0) {
        switch (errno) {
        case EACCES:
        case EPERM:
            return ty_error(TY_ERROR_ACCESS, "Permission denied to delete '%s'", path);
        case EBUSY:
            return ty_error(TY_ERROR_BUSY, "Failed to delete '%s' because it is busy", path);
        case EIO:
            return ty_error(TY_ERROR_IO, "I/O error while deleting '%s'", path);
        case ENOENT:
            if (tolerant)
                return 0;
            return ty_error(TY_ERROR_NOT_FOUND, "Path '%s' does not exist", path);
        case ENOTDIR:
            return ty_error(TY_ERROR_NOT_FOUND, "Part of '%s' is not a directory", path);
        case ENOTEMPTY:
            return ty_error(TY_ERROR_EXISTS, "Cannot remove non-empty directory '%s", path);
        }
        return ty_error(TY_ERROR_SYSTEM, "remove('%s') failed: %s", path, strerror(errno));
    }

    return 0;
}

int ty_poll(const ty_descriptor_set *set, int timeout)
{
    assert(set);
    assert(set->count);
    assert(set->count <= 64);

    struct pollfd pfd[64];
    uint64_t start;
    int r;

    for (size_t i = 0; i < set->count; i++) {
        pfd[i].events = POLLIN;
        pfd[i].fd = set->desc[i];
    }

    if (timeout < 0)
        timeout = -1;

    start = ty_millis();

restart:
    r = poll(pfd, (nfds_t)set->count, ty_adjust_timeout(timeout, start));
    if (r < 0) {
        switch (errno) {
        case EINTR:
            goto restart;
        case ENOMEM:
            return ty_error(TY_ERROR_MEMORY, NULL);
        }
        return ty_error(TY_ERROR_SYSTEM, "poll() failed: %s", strerror(errno));
    }
    if (!r)
        return 0;

    for (size_t i = 0; i < set->count; i++) {
        if (pfd[i].revents & (POLLIN | POLLERR | POLLHUP | POLLNVAL))
            return set->id[i];
    }
    assert(false);
}

int ty_terminal_setup(uint32_t flags)
{
    struct termios tio;
    int r;

    r = tcgetattr(STDIN_FILENO, &tio);
    if (r < 0) {
        if (errno == ENOTTY)
            return ty_error(TY_ERROR_UNSUPPORTED, "Not a terminal");
        return ty_error(TY_ERROR_SYSTEM, "tcgetattr() failed: %s", strerror(errno));
    }

    if (!saved_termios) {
        orig_termios = tio;
        saved_termios = true;

        atexit(ty_terminal_restore);
    }

    if (flags & TY_TERMINAL_RAW) {
        cfmakeraw(&tio);
        tio.c_oflag |= OPOST | ONLCR;
        tio.c_lflag |= ISIG;
        tio.c_cc[VMIN] = 1;
        tio.c_cc[VTIME] = 0;
    } else {
        tio.c_iflag = TTYDEF_IFLAG;
        tio.c_oflag = TTYDEF_OFLAG;
        tio.c_lflag = TTYDEF_LFLAG;
        tio.c_cflag = TTYDEF_CFLAG;
        memcpy(&tio.c_cc, ttydefchars, sizeof(ttydefchars));
    }

    tio.c_lflag |= ECHO;
    if (flags & TY_TERMINAL_SILENT)
        tio.c_lflag &= (unsigned int)~ECHO;

    r = tcsetattr(STDIN_FILENO, TCSADRAIN, &tio);
    if (r < 0)
        return ty_error(TY_ERROR_SYSTEM, "tcsetattr() failed: %s", strerror(errno));

    return 0;
}

void ty_terminal_restore(void)
{
    if (!saved_termios)
        return;

    tcsetattr(STDIN_FILENO, TCSADRAIN, &orig_termios);
}
