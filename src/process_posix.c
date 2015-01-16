/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "ty/common.h"
#include "compat.h"
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>
#include "htable.h"
#include "ty/process.h"

struct process {
    ty_htable_head hnode;

    pid_t pid;
    int pipe[2];
};

struct child_report {
    ty_err err;
    char msg[512];
};

static ty_htable processes;

static void free_process(struct process *proc)
{
    if (proc) {
        close(proc->pipe[0]);
        close(proc->pipe[1]);
    }

    free(proc);
}

static void free_process_table(void)
{
    ty_htable_foreach(cur, &processes) {
        struct process *proc = ty_container_of(cur, struct process, hnode);
        free_process(proc);
    }
    ty_htable_release(&processes);
}

static int init_process_table(void)
{
    if (processes.size)
        return 0;

    int r = ty_htable_init(&processes, 32);
    if (r < 0)
        return r;

    atexit(free_process_table);

    return 0;
}

static void add_process(struct process *proc)
{
    sigset_t mask, oldmask;

// We can't stop sigaddset from tripping this (on OS X, at least), so disable it temporarily
TY_WARNING_DISABLE_SIGN_CONVERSION

    sigemptyset(&mask);
    sigaddset(&mask, SIGCHLD);
    sigprocmask(SIG_BLOCK, &mask, &oldmask);

TY_WARNING_RESTORE

    ty_htable_add(&processes, (uint32_t)proc->pid, &proc->hnode);

    sigprocmask(SIG_SETMASK, &oldmask, NULL);
}

static void remove_process(struct process *proc)
{
    sigset_t mask, oldmask;

TY_WARNING_DISABLE_SIGN_CONVERSION

    sigemptyset(&mask);
    sigaddset(&mask, SIGCHLD);
    sigprocmask(SIG_BLOCK, &mask, &oldmask);

TY_WARNING_RESTORE

    ty_htable_remove(&proc->hnode);

    sigprocmask(SIG_SETMASK, &oldmask, NULL);
}

static struct process *find_process(pid_t pid)
{
    ty_htable_foreach_hash(cur, &processes, (uint32_t)pid) {
        struct process *proc = ty_container_of(cur, struct process, hnode);

        if (proc->pid == pid)
            return proc;
    }

    return NULL;
}

#ifdef HAVE_PIPE2

static int create_pipe(int pfd[2], int flags)
{
    int r = pipe2(pfd, flags);
    if (r < 0)
        return ty_error(TY_ERROR_SYSTEM, "pipe() failed: %s", strerror(errno));

    return 0;
}

#else

// Racy alternative, not having CLOEXEC by default (and explictely disabled
// for standard descriptors) is one of the most pervasive design defects ever.
static int create_pipe(int pfd[2], int flags)
{
    int fds[2], r;

    r = pipe(fds);
    if (r < 0)
        return ty_error(TY_ERROR_SYSTEM, "pipe() failed: %s", strerror(errno));

    if (flags & O_CLOEXEC) {
        r = fcntl(fds[0], F_GETFD, 0);
        r = fcntl(fds[0], F_SETFD, r | FD_CLOEXEC);
        if (r < 0) {
            r = ty_error(TY_ERROR_SYSTEM, "fcntl() failed: %s", strerror(errno));
            goto error;
        }

        r = fcntl(fds[1], F_GETFD, 0);
        r = fcntl(fds[1], F_SETFD, r | FD_CLOEXEC);
        if (r < 0) {
            r = ty_error(TY_ERROR_SYSTEM, "fcntl() failed: %s", strerror(errno));
            goto error;
        }
    }

    if (flags & O_NONBLOCK) {
        r = fcntl(fds[0], F_GETFL, 0);
        r = fcntl(fds[0], F_SETFL, r | O_NONBLOCK);
        if (r < 0) {
            r = ty_error(TY_ERROR_SYSTEM, "fcntl() failed: %s", strerror(errno));
            goto error;
        }

        r = fcntl(fds[1], F_GETFL, 0);
        r = fcntl(fds[1], F_SETFL, r | O_NONBLOCK);
        if (r < 0) {
            r = ty_error(TY_ERROR_SYSTEM, "fcntl() failed: %s", strerror(errno));
            goto error;
        }
    }

    pfd[0] = fds[0];
    pfd[1] = fds[1];
    return 0;

error:
    close(fds[0]);
    close(fds[1]);
    return r;
}

#endif

static void child_send_error(ty_err err, const char *msg, void *udata)
{
    TY_UNUSED(err);

    struct child_report *report = udata;

    strncpy(report->msg, msg, sizeof(report->msg));
    report->msg[sizeof(report->msg) - 1] = 0;
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

int ty_process_spawn(const char *path, const char *dir, const char * const args[], const ty_descriptor desc[3], uint32_t flags, ty_descriptor *rdesc)
{
    assert(path && path[0]);
    assert(args && args[0]);

    struct process *proc = NULL;
    int cpipe[2] = {-1, -1};
    struct child_report report;
    int r;

    if (rdesc) {
        r = init_process_table();
        if (r < 0)
            goto cleanup;

        proc = calloc(1, sizeof(*proc));
        if (!proc) {
            r = ty_error(TY_ERROR_MEMORY, NULL);
            goto cleanup;
        }
        proc->pipe[0] = -1;
        proc->pipe[1] = -1;

        r = create_pipe(proc->pipe, O_CLOEXEC | O_NONBLOCK);
        if (r < 0)
            goto cleanup;

#ifdef F_SETNOSIGPIPE
        r = fcntl(proc->pipe[1], F_SETNOSIGPIPE, 1);
        if (r < 0) {
            r = ty_error(TY_ERROR_SYSTEM, "fcntl(F_SETNOSIGPIPE) failed: %s", strerror(errno));
            goto cleanup;
        }
#endif
    }

    r = create_pipe(cpipe, O_CLOEXEC);
    if (r < 0)
        goto cleanup;

    r = fork();
    if (r < 0) {
        r = ty_error(TY_ERROR_SYSTEM, "fork() failed: %s", strerror(errno));
        goto cleanup;
    }

    if (!r) {
        close(cpipe[0]);

        child_exec(path, dir, args, desc, flags, cpipe[1]);
        abort();
    }

    close(cpipe[1]);
    cpipe[1] = -1;

    r = (int)read(cpipe[0], &report, sizeof(report));
    if (r < 0) {
        r = ty_error(TY_ERROR_SYSTEM, "Unable to report from child: %s", strerror(errno));
        goto cleanup;
    } else if (r > 0) {
        // Don't trust the child too much
        report.msg[sizeof(report.msg) - 1] = 0;
        r = ty_error(report.err, "%s", report.msg);
        goto cleanup;
    }

    if (rdesc) {
        add_process(proc);

        *rdesc = proc->pipe[0];
        proc = NULL;
    }

    r = 0;
cleanup:
    close(cpipe[0]);
    close(cpipe[1]);
    free_process(proc);
    return r;
}

// desc is always closed unless it returns 0 (timeout)
int ty_process_wait(ty_descriptor desc, int timeout)
{
    assert(desc >= 0);

    struct pollfd pfd;
    int status[2], r;

    pfd.fd = desc;
    pfd.events = POLLIN;

restart:
    r = poll(&pfd, 1, timeout);
    if (r < 0) {
        switch (errno) {
        case EINTR:
            goto restart;
        case ENOMEM:
            r = ty_error(TY_ERROR_MEMORY, NULL);
            break;
        default:
            r = ty_error(TY_ERROR_SYSTEM, "poll() failed: %s", strerror(errno));
            break;
        }
        goto cleanup;
    }
    if (!r)
        return 0;
    assert(r == 1);

    r = (int)read(desc, status, sizeof(status));
    if (r < 0) {
        r = ty_error(TY_ERROR_SYSTEM, "read() failed: %s", strerror(errno));
        goto cleanup;
    }
    assert(r == sizeof(status));

    if (status[0] == CLD_EXITED && !status[1]) {
        r = TY_PROCESS_SUCCESS;
    } else if (status[0] == CLD_KILLED && status[1] == SIGINT) {
        r = TY_PROCESS_INTERRUPTED;
    } else {
        r = TY_PROCESS_FAILURE;
    }
cleanup:
    close(desc);
    return r;
}

static void signal_process(const siginfo_t *si)
{
#ifndef F_SETNOSIGPIPE
    sigset_t pending, block, oldmask;
#endif
    struct process *proc;
    int status[2], r;

#ifndef F_SETNOSIGPIPE
    sigemptyset(&pending);
    sigpending(&pending);

    sigemptyset(&block);
    sigaddset(&block, SIGPIPE);
    pthread_sigmask(SIG_BLOCK, &block, &oldmask);
#endif

    proc = find_process(si->si_pid);
    if (!proc)
        return;

    status[0] = si->si_code;
    status[1] = si->si_status;

    // Atomic since size <= PIPE_BUF (at least 512 bytes according to POSIX)
    r = (int)write(proc->pipe[1], status, sizeof(status));
    assert(r == EPIPE || r == sizeof(status));

#ifndef F_SETNOSIGPIPE
    if (!sigismember(&pending, SIGPIPE)) {
        sigemptyset(&pending);
        sigpending(&pending);

        if (sigismember(&pending, SIGPIPE)) {
            const struct timespec nowait = {0};
            sigtimedwait(&block, NULL, &nowait);
        }
    }

    pthread_sigmask(SIG_SETMASK, &oldmask, NULL);
#endif

    remove_process(proc);
    free_process(proc);
}

// Pass 0 as signum to avoid automatic child reaping if you want to do it yourself. This
// way you can register ty_process_handle_sigchld directly as the signal handler and it will
// reap children correctly.
void ty_process_handle_sigchld(int signum)
{
    int options;

    options = WNOHANG;
    if (!signum)
        options |= WNOWAIT;

    while (true) {
        siginfo_t si;
        int r;

        si.si_pid = 0;
        r = waitid(P_ALL, 0, &si, options);
        if (r < 0 || !si.si_pid)
            break;

        signal_process(&si);
    }
}
