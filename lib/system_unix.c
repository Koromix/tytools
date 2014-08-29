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

#include "common.h"
#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>
#include "system.h"

static struct termios orig_tio;

uint64_t ty_millis(void)
{
    struct timeval tv;
    uint64_t millis;

    gettimeofday(&tv, NULL);
    millis = (uint64_t)tv.tv_sec * 1000 + (uint64_t)tv.tv_usec / 1000;

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

int ty_find_config(char **rpath, const char *name)
{
    assert(rpath);
    assert(name && name[0]);

    char *path, *xdg;
    char *tok, *save = NULL;
    int r;

    r = ty_user_config(&path, name, false);
    if (r < 0)
        return r;

    if (access(path, F_OK) == 0) {
        *rpath = path;
        return 1;
    }

    free(path);
    path = NULL;

    xdg = getenv("XDG_CONFIG_DIRS");
    if (!xdg || !xdg[0])
        xdg = "/etc/xdg";
    xdg = strdup(xdg);
    if (!xdg)
        return ty_error(TY_ERROR_MEMORY, NULL);

    tok = strtok_r(xdg, ":", &save);
    do {
        r = asprintf(&path, "%s/%s", tok, name);
        if (r < 0) {
            r = ty_error(TY_ERROR_MEMORY, NULL);
            goto cleanup;
        }

        if (access(path, F_OK) == 0)
            break;

        free(path);
        path = NULL;

        tok = strtok_r(NULL, ":", &save);
    } while (tok);

    if (path) {
        *rpath = path;
        r = 1;
    } else {
        r = 0;
    }
cleanup:
    free(xdg);
    return r;
}

int ty_user_config(char **rpath, const char *name, bool make_parents)
{
    assert(rpath);
    assert(name && name[0]);

    char *path, *env;
    int r;

    env = getenv("XDG_CONFIG_HOME");
    if (env && env[0]) {
        r = asprintf(&path, "%s/%s", env, name);
    } else {
        env = getenv("HOME");
        if (!env || !env[0])
            return ty_error(TY_ERROR_SYSTEM, "$HOME is empty or not defined");

        r = asprintf(&path, "%s/.config/%s", env, name);
    }
    if (r < 0)
        return ty_error(TY_ERROR_MEMORY, NULL);

    if (make_parents) {
        r = ty_mkdir(path, 0755, TY_MKDIR_OMIT_LAST | TY_MKDIR_MAKE_PARENTS | TY_MKDIR_IGNORE_EXISTS);
        if (r < 0)
            goto error;
    }

    *rpath = path;
    return 0;

error:
    free(path);
    return r;
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
