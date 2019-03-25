/* TyTools - public domain
   Niels Martignène <niels.martignene@protonmail.com>
   https://koromix.dev/tytools

   This software is in the public domain. Where that dedication is not
   recognized, you are granted a perpetual, irrevocable license to copy,
   distribute, and modify this file as you see fit.

   See the LICENSE file for more details. */

#include "common_priv.h"
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include <termios.h>
#ifdef __APPLE__
    #include <mach/mach_time.h>
    #include <mach-o/dyld.h>
    #include <sys/select.h>
#else
    #include <poll.h>
#endif
#include "system.h"

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
    struct timespec ts;
    int r;

#ifdef CLOCK_MONOTONIC_RAW
    r = clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
#else
    r = clock_gettime(CLOCK_MONOTONIC, &ts);
#endif
    if (r < 0) {
        ty_log(TY_LOG_WARNING, "clock_gettime() failed: %s", strerror(errno));
        return 0;
    }

    return (uint64_t)ts.tv_sec * 1000 + (uint64_t)ts.tv_nsec / 10000000;
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

unsigned int ty_descriptor_get_modes(ty_descriptor desc)
{
    struct stat sb;
    int r;

    r = fstat(desc, &sb);
    if (r < 0)
        return 0;

    if (S_ISFIFO(sb.st_mode) || S_ISSOCK(sb.st_mode)) {
        return TY_DESCRIPTOR_MODE_FIFO;
    } else if (S_ISBLK(sb.st_mode) || S_ISCHR(sb.st_mode)) {
        if (isatty(desc)) {
            return TY_DESCRIPTOR_MODE_DEVICE | TY_DESCRIPTOR_MODE_TERMINAL;
        } else {
            return TY_DESCRIPTOR_MODE_DEVICE;
        }
    } else if (S_ISREG(sb.st_mode)) {
        return TY_DESCRIPTOR_MODE_FILE;
    }

    return 0;
}

ty_descriptor ty_standard_get_descriptor(ty_standard_stream std_stream)
{
    return std_stream;
}

#ifdef __APPLE__

unsigned int ty_standard_get_paths(ty_standard_path std_path, const char *suffix,
                                   char (*rpaths)[TY_PATH_MAX_SIZE], unsigned int max_paths)
{
    assert(rpaths);

    unsigned int paths_count = 0;

    if (!max_paths)
        return 0;

#define ADD_DIRECTORY(Fmt, ...) \
        do { \
            if (paths_count < max_paths) { \
                if (snprintf(rpaths[paths_count++], TY_PATH_MAX_SIZE, \
                             (Fmt), ## __VA_ARGS__) >= TY_PATH_MAX_SIZE) \
                    goto overflow; \
            } \
        } while (false)

    switch (std_path) {
        case TY_PATH_EXECUTABLE_DIRECTORY: {
            uint32_t tmp_size = TY_PATH_MAX_SIZE;
            int r = _NSGetExecutablePath(rpaths[0], &tmp_size);
            if (r == -1)
                goto overflow;

            size_t len = strlen(rpaths[0]);
            while (len && !strchr(TY_PATH_SEPARATORS, rpaths[0][--len]))
                continue;
            rpaths[0][len] = 0;

            paths_count = 1;
        } break;

        // FIXME: Use NSSearchPathForDirectoriesInDomains() to get proper paths
        case TY_PATH_CONFIG_DIRECTORY: {
            const char *home_dir = getenv("HOME");
            if (home_dir)
                ADD_DIRECTORY("%s/Library/Preferences", home_dir);
            ADD_DIRECTORY("/Library/Preferences");
        } break;
    }

#undef ADD_DIRECTORY

    if (suffix) {
        for (unsigned int i = 0; i < paths_count; i++) {
            size_t len = strlen(rpaths[i]);
            size_t suffix_len = (size_t)snprintf(rpaths[i] + len, TY_PATH_MAX_SIZE - len,
                                                 "/%s", suffix);
            if (suffix_len >= TY_PATH_MAX_SIZE - len)
                goto overflow;
        }
    }

    assert(paths_count);
    return paths_count;

overflow:
    ty_error(TY_ERROR_SYSTEM, "Ignoring truncated path in ty_standard_get_paths()");
    return 0;
}

#else

unsigned int ty_standard_get_paths(ty_standard_path std_path, const char *suffix,
                                   char (*rpaths)[TY_PATH_MAX_SIZE], unsigned int max_paths)
{
    assert(rpaths);

    unsigned int paths_count = 0;

    if (!max_paths)
        return 0;

#define ADD_DIRECTORY(Fmt, ...) \
        do { \
            if (paths_count < max_paths) { \
                if (snprintf(rpaths[paths_count++], TY_PATH_MAX_SIZE, \
                             (Fmt), ## __VA_ARGS__) >= TY_PATH_MAX_SIZE) \
                    goto overflow; \
            } \
        } while (false)

    switch (std_path) {
        case TY_PATH_EXECUTABLE_DIRECTORY: {
            ssize_t len = readlink("/proc/self/exe", rpaths[0], TY_PATH_MAX_SIZE);
            if (len < 0) {
                ty_error(TY_ERROR_SYSTEM, "readlink('/proc/self/exe') failed: %s", strerror(errno));
                return 0;
            }
            if (len >= TY_PATH_MAX_SIZE)
                goto overflow;

            while (len && !strchr(TY_PATH_SEPARATORS, rpaths[0][--len]))
                continue;
            rpaths[0][len] = 0;

            paths_count = 1;
        } break;

        case TY_PATH_CONFIG_DIRECTORY: {
            const char *config_home_dir = getenv("XDG_CONFIG_HOME");
            if (config_home_dir) {
                ADD_DIRECTORY("%s", config_home_dir);
            } else {
                const char *home_dir = getenv("HOME");
                if (home_dir)
                    ADD_DIRECTORY("%s/.config", home_dir);
            }

            const char *config_dirs = getenv("XDG_CONFIG_DIRS");
            if (!config_dirs)
                config_dirs = "/etc/xdg";
            while (config_dirs[0]) {
                size_t len = strcspn(config_dirs, ":");
                if (len)
                    ADD_DIRECTORY("%.*s", (int)len, config_dirs);
                config_dirs += len + !!config_dirs[len];
            }
        } break;
    }

#undef ADD_DIRECTORY

    if (suffix) {
        for (unsigned int i = 0; i < paths_count; i++) {
            size_t len = strlen(rpaths[i]);
            size_t suffix_len = (size_t)snprintf(rpaths[i] + len, TY_PATH_MAX_SIZE - len,
                                                 "/%s", suffix);
            if (suffix_len >= TY_PATH_MAX_SIZE - len)
                goto overflow;
        }
    }

    assert(paths_count);
    return paths_count;

overflow:
    ty_error(TY_ERROR_SYSTEM, "Ignoring truncated path in ty_standard_get_paths()");
    return 0;
}

#endif

#ifdef __APPLE__

int ty_poll(const ty_descriptor_set *set, int timeout)
{
    assert(set);
    assert(set->count);
    assert(set->count <= 64);

    fd_set fds;
    uint64_t start;
    struct timeval tv;
    int r;

    FD_ZERO(&fds);
    for (unsigned int i = 0; i < set->count; i++)
        FD_SET(set->desc[i], &fds);

    start = ty_millis();
restart:
    if (timeout >= 0) {
        int adjusted_timeout = ty_adjust_timeout(timeout, start);
        tv.tv_sec = adjusted_timeout / 1000;
        tv.tv_usec = (adjusted_timeout % 1000) * 1000;
        r = select(FD_SETSIZE, &fds, NULL, NULL, &tv);
    } else {
        r = select(FD_SETSIZE, &fds, NULL, NULL, NULL);
    }
    if (r < 0) {
        if (errno == EINTR)
            goto restart;

        return ty_error(TY_ERROR_SYSTEM, "poll() failed: %s", strerror(errno));
    }
    if (!r)
        return 0;

    for (unsigned int i = 0; i < set->count; i++) {
        if (FD_ISSET(set->desc[i], &fds))
            return set->id[i];
    }

    assert(false);
    __builtin_unreachable();
}

#else

int ty_poll(const ty_descriptor_set *set, int timeout)
{
    assert(set);
    assert(set->count);
    assert(set->count <= 64);

    struct pollfd pfd[64];
    uint64_t start;
    int r;

    for (unsigned int i = 0; i < set->count; i++) {
        pfd[i].events = POLLIN;
        pfd[i].fd = set->desc[i];
    }

    if (timeout < 0)
        timeout = -1;

    start = ty_millis();
restart:
    r = poll(pfd, (nfds_t)set->count, ty_adjust_timeout(timeout, start));
    if (r < 0) {
        if (errno == EINTR)
            goto restart;

        return ty_error(TY_ERROR_SYSTEM, "poll() failed: %s", strerror(errno));
    }
    if (!r)
        return 0;

    for (unsigned int i = 0; i < set->count; i++) {
        if (pfd[i].revents & (POLLIN | POLLERR | POLLHUP | POLLNVAL))
            return set->id[i];
    }

    assert(false);
    __builtin_unreachable();
}

#endif

bool ty_compare_paths(const char *path1, const char *path2)
{
    assert(path1);
    assert(path2);

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
}

int ty_terminal_setup(int flags)
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
    } else {
        tio.c_iflag = TTYDEF_IFLAG;
        tio.c_oflag = TTYDEF_OFLAG;
        tio.c_lflag = TTYDEF_LFLAG;
        tio.c_cflag = TTYDEF_CFLAG;
    }
    tio.c_cc[VMIN] = 1;
    tio.c_cc[VTIME] = 0;

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
