/**
 * ty, command-line program to manage Teensy devices
 * http://github.com/Koromix/ty
 * Copyright (C) 2014 Niels Martignène
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "ty/common.h"
#include "compat.h"
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>
#include "ty/system.h"

struct child_report {
    ty_err err;
    char msg[512];
};

static struct termios orig_tio;

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

// Unlike ty_path_split, trailing slashes are ignored, so "a/b/" returns "b/". This is unusual
// but this way we don't have to allocate a new string or alter path itself.
const char *get_basename(const char *path)
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

int _ty_statat(int fd, const char *path, ty_file_info *info, bool follow)
{
    struct stat sb;
    int r;

    if (fd >= 0) {
        r = fstatat(fd, path, &sb, !follow ? AT_SYMLINK_NOFOLLOW : 0);
    } else {
        if (follow) {
            r = stat(path, &sb);
        } else {
            r = lstat(path, &sb);
        }
    }
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
#ifdef st_mtime
    info->mtime = (uint64_t)sb.st_mtim.tv_sec * 1000 + (uint64_t)sb.st_mtim.tv_nsec / 1000000;
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

int ty_stat(const char *path, ty_file_info *info, bool follow)
{
    assert(path && path[0]);
    assert(info);

    return _ty_statat(-1, path, info, follow);
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

static void child_send_error(ty_err err, const char *msg, void *udata)
{
    TY_UNUSED(err);

    struct child_report *report = udata;

    strncpy(report->msg, msg, sizeof(report->msg));
    report->msg[sizeof(report->msg) - 1] = 0;
}

static bool caught_sigint = false;

static void catch_sigint(int sig)
{
    TY_UNUSED(sig);

    caught_sigint = true;
}

TY_NORETURN static int child_exec(const char *path, const char *dir, const char * const args[],
                                  const int fds[3], uint32_t flags, int cpipe)
{
    struct child_report report;
    int r;

    ty_error_redirect(child_send_error, &report);

    if (dir) {
        r = chdir(dir);
        if (r < 0) {
            switch (errno) {
            case EACCES:
                r = ty_error(TY_ERROR_ACCESS, "Permission denied for '%s'", path);
                break;
            case EIO:
            case ENXIO:
                r = ty_error(TY_ERROR_IO, "I/O error while changing directory to '%s'", path);
                break;
            case ENOENT:
                r = ty_error(TY_ERROR_NOT_FOUND, "Directory '%s' does not exist", path);
                break;
            case ENOTDIR:
                r = ty_error(TY_ERROR_NOT_FOUND, "Part of '%s' is not a directory", path);
                break;
            default:
                r = ty_error(TY_ERROR_SYSTEM, "chdir('%s') failed: %s", path, strerror(errno));
                break;
            }
            goto error;
        }
    }

    if (fds) {
        for (int i = 0; i < 3; i++) {
            int fd = fds[i];

            if (fd < 0) {
                fd = open("/dev/null", O_RDWR);
                if (fd < 0) {
                    switch (errno) {
                    case EACCES:
                        r = ty_error(TY_ERROR_ACCESS, "Permission denied for '/dev/null'");
                        break;
                    case EIO:
                        r = ty_error(TY_ERROR_IO, "I/O error while opening '/dev/null'");
                        break;
                    case ENOENT:
                    case ENOTDIR:
                        r = ty_error(TY_ERROR_NOT_FOUND, "Device '/dev/null' does not exist");
                        break;

                    default:
                        r = ty_error(TY_ERROR_SYSTEM, "open('/dev/null') failed: %s", strerror(errno));
                        break;
                    }
                    goto error;
                }
            }

            if (fd != i) {
restart:
                r = dup2(fd, i);
                if (r < 0 && errno == EINTR)
                    goto restart;
                close(fd);
                if (r < 0) {
                    if (errno == EIO) {
                        r = ty_error(TY_ERROR_IO, "I/O error on file descriptor %d", i);
                    } else {
                        r = ty_error(TY_ERROR_SYSTEM, "dup2() failed: %s", strerror(errno));
                    }
                    goto error;
                }
            }
        }
    }

    if (flags & TY_SPAWN_PATH) {
        execvp(path, (char * const *)args);
    } else {
        execv(path, (char * const *)args);
    }
    switch (errno) {
    case EACCES:
        r = ty_error(TY_ERROR_ACCESS, "Permission denied to execute '%s'", path);
        break;
    case EIO:
        r = ty_error(TY_ERROR_IO, "I/O error while trying to execute '%s'", path);
        break;
    case ENOENT:
        r = ty_error(TY_ERROR_NOT_FOUND, "Executable '%s' not found", path);
        break;
    case ENOTDIR:
        r = ty_error(TY_ERROR_NOT_FOUND, "Part of '%s' is not a directory", path);
        break;

    default:
        r = ty_error(TY_ERROR_SYSTEM, "exec('%s') failed: %s", path, strerror(errno));
        break;
    }

error:
    report.err = r;
    (void)(write(cpipe, &report, sizeof(report)) < 0);
    _exit(-r);
}

int ty_spawn(const char *path, const char *dir, const char * const *args,
             const ty_descriptor desc[3], int *rcode, uint32_t flags)
{
    assert(path && path[0]);
    assert(args && args[0]);

    int cpipe[2];
    struct sigaction sa, oldsa;
    sigset_t mask, oldmask;
    pid_t pid;
    struct child_report report = {0};
    int status;
    ssize_t r;

    // If the pipe gets closed, the parent notices it (EOF) and knows the exec was successful
    r = pipe2(cpipe, O_CLOEXEC);
    if (r < 0)
        return ty_error(TY_ERROR_SYSTEM, "pipe() failed: %s", strerror(errno));

    sigemptyset(&mask);
    sigaddset(&mask, SIGCHLD);
    sigaddset(&mask, SIGINT);
    sigprocmask(SIG_BLOCK, &mask, &oldmask);

    pid = fork();
    if (pid < 0) {
        r = ty_error(TY_ERROR_SYSTEM, "fork() failed: %s", strerror(errno));
        goto cleanup;
    }

    if (!pid) {
        close(cpipe[0]);

        sigemptyset(&mask);
        sigprocmask(SIG_SETMASK, &mask, NULL);

        child_exec(path, dir, args, desc, flags, cpipe[1]);
        abort();
    }

    close(cpipe[1]);

    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = catch_sigint;
    sigaction(SIGINT, &sa, &oldsa);

    sigemptyset(&mask);
    sigaddset(&mask, SIGINT);
    sigprocmask(SIG_UNBLOCK, &mask, NULL);

    r = read(cpipe[0], &report, sizeof(report));
    close(cpipe[0]);
    if (r < 0) {
        r = ty_error(TY_ERROR_SYSTEM, "Unable to report from child: %s", strerror(errno));
        goto cleanup;
    } else if (r > 0) {
        // Don't trust the child too much
        report.msg[sizeof(report.msg) - 1] = 0;
        r = ty_error(report.err, "%s", report.msg);
        goto cleanup;
    }

restart:
    pid = waitpid(pid, &status, flags & TY_SPAWN_ASYNC ? WNOHANG : 0);
    if (pid < 0) {
        if (errno == EINTR)
            goto restart;

        r = ty_error(TY_ERROR_SYSTEM, "waitpid() failed: %s", strerror(errno));
        goto cleanup;
    }

    if (WIFEXITED(status)) {
        if (rcode)
            *rcode = WEXITSTATUS(status);

        r = 0;
    } else if (WIFSIGNALED(status)) {
        r = WTERMSIG(status);
    } else {
        // FIXME: better error message
        r = ty_error(TY_ERROR_SYSTEM, "Process failed");
    }

cleanup:
    sigaction(SIGINT, &oldsa, NULL);
    sigprocmask(SIG_SETMASK, &oldmask, NULL);
    if (r == SIGINT || caught_sigint)
        kill(getpid(), SIGINT);
    return (int)r;
}

int ty_poll(const ty_descriptor_set *set, int timeout)
{
    assert(set);
    assert(set->count);
    assert(set->count <= 64);

    struct pollfd pfd[64];
    int r;

    for (size_t i = 0; i < set->count; i++) {
        pfd[i].events = POLLIN;
        pfd[i].fd = set->desc[i];
    }

    if (timeout < 0)
        timeout = -1;

restart:
    r = poll(pfd, set->count, timeout);
    if (r < 0) {
        if (errno == EINTR)
            goto restart;
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

static void restore_terminal(void)
{
    tcsetattr(STDIN_FILENO, TCSADRAIN, &orig_tio);
}

int ty_terminal_change(uint32_t flags)
{
    struct termios tio;
    int r;

    r = tcgetattr(STDIN_FILENO, &tio);
    if (r < 0) {
        if (errno == ENOTTY)
            return ty_error(TY_ERROR_UNSUPPORTED, "Not a terminal");
        return ty_error(TY_ERROR_SYSTEM, "tcgetattr() failed: %s", strerror(errno));
    }

    static bool saved = false;
    if (!saved) {
        orig_tio = tio;
        saved = true;

        atexit(restore_terminal);
    }

    if (flags & TY_TERMINAL_RAW) {
        cfmakeraw(&tio);
        tio.c_oflag |= OPOST | ONLCR;
        tio.c_lflag |= ISIG;
        tio.c_cc[VMIN] = 1;
        tio.c_cc[VTIME] = 0;
    }

    tio.c_lflag |= ECHO;
    if (flags & TY_TERMINAL_SILENT)
        tio.c_lflag &= (unsigned int)~ECHO;

    r = tcsetattr(STDIN_FILENO, TCSADRAIN, &tio);
    if (r < 0)
        return ty_error(TY_ERROR_SYSTEM, "tcsetattr() failed: %s", strerror(errno));

    return 0;
}
